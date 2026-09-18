#include "Check.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <set>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using ngm::check::require;
using namespace std::chrono_literals;

struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-capture-check-XXXXXX").string();
        const auto result = mkdtemp(pattern.data());
        require(result != nullptr, "allocate isolated capture check");
        path = result;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

void copy_executable(const fs::path& source, const fs::path& destination) {
    fs::create_directories(destination.parent_path());
    fs::copy_file(source, destination);
    fs::permissions(destination, fs::perms::owner_all);
}

ngm::JobSnapshot completed(ngm::CaptureService& service, const ngm::CaptureSubmission& submission) {
    const auto result = service.wait(submission.identity.job_id, 10s);
    require(result && ngm::job_terminal(result->state), "capture reaches a terminal state");
    require(result->identity == submission.identity, "completion has the submitted job/capture/attempt identity");
    require(result->cleanup_confirmed && !result->gpu_reserved && !result->worker_running &&
                !result->finalization_pending,
            "terminal capture joins owned processes and finalizes evidence");
    return *result;
}

void await_file(const fs::path& file) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while(std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if(fs::file_size(file, error) > 0 && !error) {
            return;
        }
        std::this_thread::sleep_for(1ms);
    }
    throw std::runtime_error("stand-in readiness record did not arrive");
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "expected Nsight executable stand-in");
        Scratch scratch;
        const auto standin = fs::absolute(argv[1]);
        const auto installation = scratch.path / "installation";
        for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
            copy_executable(standin, installation / "host/linux-desktop-nomad-x64" / name);
        }
        const auto target = scratch.path / "target with spaces";
        copy_executable(standin, target);
        ngm::CaptureServiceOptions options;
        options.artifacts.root = scratch.path / "store";
        options.nsight_root = installation;
        options.environment = {{"NGM_TARGET_RECORD", (scratch.path / "targets.txt").string()},
                               {"NGM_EXPECT_XDG_DATA_DIRS", "/usr/local/share:/usr/share"}};
        ngm::CaptureRequest request;
        request.executable = target;
        request.working_directory = scratch.path;
        request.arguments = {"--scenario", "reference"};
        request.timeout = 5s;
        request.pin = true;
        std::string retained_id;
        {
            ngm::CaptureService service(options);
            const auto first = service.capture(request);
            const auto second = service.capture(request);
            require(first.identity.job_id != second.identity.job_id && first.artifact_id != second.artifact_id,
                    "fresh jobs have separate captures and bundles");
            for(const auto& submission : {first, second}) {
                const auto result = completed(service, submission);
                require(result.state == ngm::JobState::succeeded,
                        "complete capture and matching metadata succeed: " + result.error);
                require(result.artifact_ids == std::vector<std::string>{submission.artifact_id},
                        "coordinator attaches only this capture's artifact");
                const auto artifact = service.artifacts().inspect(submission.artifact_id);
                require(artifact.summary.status == "complete" && artifact.summary.pinned,
                        "successful bundle is published and persistently pinned");
                const auto report =
                    nlohmann::json::parse(service.artifacts().read(submission.artifact_id, "raw/report.json"));
                for(const auto& exported : report.at("exports")) {
                    require(exported.at("outcome") == "success",
                            "every happy-path export actually succeeds: " + exported.dump());
                }
                require(report.at("readable_capture") == true &&
                            report.at("job") == nlohmann::json(submission.identity),
                        "bundle links readable capture evidence to exact job identity");
                require(report.at("exports").size() == 5 && report.at("cleanup_confirmed") == true,
                        "each independent export and confirmed cleanup is retained");
                require(report.at("application").at("sha256_before_launch") ==
                            report.at("application").at("sha256_after_launch"),
                        "application path identity is checked around tool execution");
                require(service.artifacts()
                                .read(submission.artifact_id, report.at("capture").at("stdout").get<std::string>())
                                .find("target stdout") != std::string::npos,
                        "application stdout remains in bundle logs");
            }
            retained_id = first.artifact_id;
            std::istringstream lines(ngm::read_regular_file(scratch.path / "targets.txt", 65536));
            std::set<std::string> pids;
            std::size_t data_path_observations = 0;
            for(std::string line; std::getline(lines, line);) {
                if(line.starts_with("pid=")) {
                    pids.insert(line);
                }
                if(line == "xdg_data_dirs=/usr/local/share:/usr/share") {
                    ++data_path_observations;
                }
            }
            require(pids.size() == 2, "two capture requests actually launch two distinct target processes");
            require(data_path_observations == 2, "both targets receive system driver data-directory defaults");
        }
        {
            ngm::CaptureService service(options);
            const auto saved = service.artifacts().inspect(retained_id);
            require(saved.summary.pinned && saved.summary.status == "complete",
                    "capture evidence and pins survive service restart");
        }

        // The service supplies defaults for explicit empty values too, and
        // preserves an ordered caller path through discovery/capture/replay.
        for(const auto& data_dirs : {std::string(), std::string("/opt/test-driver/share:/usr/share")}) {
            auto environment_options = options;
            environment_options.artifacts.root = scratch.path / (data_dirs.empty() ? "empty-data" : "custom-data");
            environment_options.environment["XDG_DATA_DIRS"] = data_dirs;
            const auto expected = data_dirs.empty() ? "/usr/local/share:/usr/share" : data_dirs;
            environment_options.environment["NGM_EXPECT_XDG_DATA_DIRS"] = expected;
            const auto target_record = scratch.path / (data_dirs.empty() ? "empty-target.txt" : "custom-target.txt");
            environment_options.environment["NGM_TARGET_RECORD"] = target_record.string();
            ngm::CaptureService service(environment_options);
            const auto submission = service.capture(request);
            const auto result = completed(service, submission);
            require(result.state == ngm::JobState::succeeded,
                    "data-directory environment reaches every backend process: " + result.error);
            const auto report =
                nlohmann::json::parse(service.artifacts().read(submission.artifact_id, "raw/report.json"));
            for(const auto& exported : report.at("exports")) {
                require(exported.at("outcome") == "success",
                        "each export preserves the selected data-directory environment: " + exported.dump());
            }
            require(ngm::read_regular_file(target_record, 65536).find("xdg_data_dirs=" + expected + '\n') !=
                        std::string::npos,
                    "caller/default data-directory path reaches the launched application");
        }

        for(const auto* mode : {"fail", "missing"}) {
            auto failed_options = options;
            failed_options.artifacts.root = scratch.path / (std::string("failure-") + mode);
            failed_options.environment["NGM_STANDIN_MODE"] = mode;
            ngm::CaptureService service(failed_options);
            const auto submitted = service.capture(request);
            const auto result = completed(service, submitted);
            require(result.state == ngm::JobState::failed, "process success alone cannot establish capture success");
            const auto artifact = service.artifacts().inspect(submitted.artifact_id);
            require(artifact.summary.status == "failed" && artifact.summary.pinned,
                    "failed attempt retains pinned logs and explicit status");
            require(!service.artifacts().read(submitted.artifact_id, "raw/report.json").empty(),
                    "failed evidence remains inspectable");
        }
        {
            auto failed_options = options;
            failed_options.artifacts.root = scratch.path / "replay-failure";
            failed_options.environment["NGM_STANDIN_REPLAY_MODE"] = "fail";
            ngm::CaptureService service(failed_options);
            const auto submitted = service.capture(request);
            require(completed(service, submitted).state == ngm::JobState::failed,
                    "a saved capture without matching readable metadata is a failed job");
            require(service.artifacts().inspect(submitted.artifact_id).summary.status == "failed",
                    "replay failure does not publish a complete bundle");
        }
        {
            auto large_options = options;
            large_options.artifacts.root = scratch.path / "large-provenance";
            auto large_request = request;
            large_request.application_provenance = {{"input", std::string(60000, 'a')}};
            ngm::CaptureService service(large_options);
            const auto submitted = service.capture(large_request);
            require(completed(service, submitted).state == ngm::JobState::succeeded,
                    "accepted caller provenance cannot overflow the manifest after capture observations grow");
            const auto artifact = service.artifacts().inspect(submitted.artifact_id);
            require(artifact.provenance.dump().size() < 8192 &&
                        artifact.provenance.at("report_path") == "raw/report.json",
                    "manifest keeps compact provenance and a full-report reference");
            const auto report =
                nlohmann::json::parse(service.artifacts().read(submitted.artifact_id, "raw/report.json"));
            require(report.at("caller_provided_application_provenance") == large_request.application_provenance,
                    "large caller provenance remains intact in retained raw evidence");
        }
        {
            auto quota_options = options;
            quota_options.artifacts.root = scratch.path / "quota";
            quota_options.artifacts.max_bytes = 5000;
            auto quota_request = request;
            quota_request.pin = false;
            ngm::CaptureService service(quota_options);
            const auto submitted = service.capture(quota_request);
            const auto result = completed(service, submitted);
            require(result.state == ngm::JobState::failed && result.error.find("budget") != std::string::npos,
                    "full captured evidence exceeding quota reports a failed job");
            const auto artifact = service.artifacts().inspect(submitted.artifact_id);
            require(artifact.summary.status == "failed" && !artifact.summary.quarantined && !artifact.summary.pinned,
                    "clean publication failure does not become a permanent cleanup quarantine");
            service.artifacts().pin(submitted.artifact_id, true);
            service.artifacts().pin(submitted.artifact_id, false);
            const auto report =
                nlohmann::json::parse(service.artifacts().read(submitted.artifact_id, "raw/report.json"));
            require(report.at("worker_outcome") == "failed", "retained report records publication failure");
            const auto next = service.capture(quota_request);
            require(next.artifact_id != submitted.artifact_id,
                    "eligible failed evidence can be pruned for a fresh attempt");
            service.cancel(next.identity.job_id);
            (void)completed(service, next);
        }
        {
            auto fifo_options = options;
            fifo_options.artifacts.root = scratch.path / "replaced-executable";
            fifo_options.environment["NGM_STANDIN_MODE"] = "hang";
            fifo_options.environment["NGM_STANDIN_PIDS"] = (scratch.path / "fifo-pids.txt").string();
            auto fifo_request = request;
            fifo_request.executable = scratch.path / "replaceable-target";
            copy_executable(standin, fifo_request.executable);
            ngm::CaptureService service(fifo_options);
            const auto running = service.capture(fifo_request);
            await_file(scratch.path / "fifo-pids.txt");
            const auto queued = service.capture(fifo_request);
            fs::remove(fifo_request.executable);
            require(mkfifo(fifo_request.executable.c_str(), 0700) == 0, "replace queued executable with a FIFO");
            service.cancel(running.identity.job_id);
            (void)completed(service, running);
            const auto result = completed(service, queued);
            require(result.state == ngm::JobState::failed && result.error.find("regular file") != std::string::npos,
                    "post-submission executable replacement cannot block hashing or shutdown");
            require(!service.artifacts().inspect(queued.artifact_id).summary.quarantined,
                    "failed prelaunch hashing confirms no process is owned");
        }
        for(const bool deadline : {true, false}) {
            auto queued_options = options;
            queued_options.artifacts.root = scratch.path / (deadline ? "queued-deadline" : "queued-shutdown");
            queued_options.environment["NGM_STANDIN_MODE"] = "hang";
            const auto pids = scratch.path / (deadline ? "deadline-pids.txt" : "shutdown-pids.txt");
            queued_options.environment["NGM_STANDIN_PIDS"] = pids.string();
            ngm::CaptureService service(queued_options);
            const auto running = service.capture(request);
            await_file(pids);
            auto queued_request = request;
            queued_request.timeout = deadline ? 20ms : 5s;
            const auto queued = service.capture(queued_request);
            if(!deadline) {
                const auto stopped = service.shutdown(5s);
                require(stopped.workers_joined && stopped.cleanup_confirmed,
                        "capture service shutdown joins active work and retires queued evidence");
            }
            require(completed(service, queued).state ==
                        (deadline ? ngm::JobState::timed_out : ngm::JobState::cancelled),
                    "queued deadline/shutdown preserves the selected job outcome");
            const auto artifact = service.artifacts().inspect(queued.artifact_id);
            require(artifact.summary.status == "failed" && !artifact.summary.quarantined,
                    "never-launched deadline/shutdown retains ordinary failed evidence");
            if(deadline) {
                service.cancel(running.identity.job_id);
            }
            (void)completed(service, running);
        }
        {
            auto cancelled_options = options;
            cancelled_options.artifacts.root = scratch.path / "cancelled";
            cancelled_options.environment["NGM_STANDIN_MODE"] = "hang";
            cancelled_options.environment["NGM_STANDIN_PIDS"] = (scratch.path / "owned-pids.txt").string();
            ngm::CaptureService service(cancelled_options);
            request.pin = false;
            const auto running = service.capture(request);
            await_file(scratch.path / "owned-pids.txt");
            const auto queued = service.capture(request);
            require(service.cancel(queued.identity.job_id) == ngm::JobCancelResult::requested,
                    "queued cancellation accepted");
            require(completed(service, queued).state == ngm::JobState::cancelled,
                    "queued job cancelled without launching");
            const auto abandoned = service.artifacts().inspect(queued.artifact_id);
            require(abandoned.summary.status == "failed" && !abandoned.summary.quarantined,
                    "never-launched capture is a failed attempt, not an unresolved-process quarantine");
            require(service.cancel(running.identity.job_id) == ngm::JobCancelResult::requested,
                    "running cancellation accepted");
            require(completed(service, running).state == ngm::JobState::cancelled,
                    "running capture cancellation cleans its children");
            std::ifstream pids(scratch.path / "owned-pids.txt");
            pid_t pid = -1;
            int count = 0;
            while(pids >> pid) {
                errno = 0;
                require(pid > 1 && kill(pid, 0) == -1 && errno == ESRCH,
                        "owned stand-in processes are gone after cancellation");
                ++count;
            }
            require(count == 2, "cancellation exercised launcher and escaped descendant");
        }
    });
}
