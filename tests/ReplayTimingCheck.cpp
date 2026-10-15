#include "Check.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ProfileInspection.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <thread>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-timing-XXXXXX").string();
        const auto p = mkdtemp(pattern.data());
        require(p != nullptr, "create timing check directory");
        path = p;
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
ngm::JobSnapshot completed(ngm::CaptureService& service, const ngm::CaptureSubmission& job) {
    const auto state = service.wait(job.identity.job_id, 15s);
    require(state && ngm::job_terminal(state->state), "timing job completes");
    require(state->identity == job.identity && state->cleanup_confirmed && !state->worker_running &&
                !state->gpu_reserved && !state->finalization_pending,
            "timing job retires its exact worker/process/evidence identity");
    return *state;
}
template <typename Function>
void rejected(Function function) {
    try {
        function();
    } catch(const std::exception&) {
        return;
    }
    require(false, "invalid timing evidence or request must refuse");
}
void await_file(const fs::path& file) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while(std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if(fs::file_size(file, error) > 0 && !error)
            return;
        std::this_thread::sleep_for(1ms);
    }
    require(false, "owned profile process readiness");
}
void gone(const fs::path& path) {
    std::ifstream input(path);
    std::size_t count = 0;
    for(int pid; input >> pid;) {
        require(kill(pid, 0) == -1 && errno == ESRCH, "owned timing process reaped");
        ++count;
    }
    require(count == 2, "timing cleanup includes escaped child");
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "Nsight stand-in required");
        Scratch scratch;
        const auto root = scratch.path / "installation";
        for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay", "ngfx-replay.bin"})
            copy_executable(fs::absolute(argv[1]), root / "host/linux-desktop-nomad-x64" / name);
        const auto target = scratch.path / "target with spaces";
        copy_executable(fs::absolute(argv[1]), target);
        ngm::CaptureServiceOptions options;
        options.nsight_root = root;
        options.artifacts.root = scratch.path / "store";
        options.environment = {{"NGM_TARGET_RECORD", (scratch.path / "targets.txt").string()}};
        ngm::CaptureRequest capture;
        capture.executable = target;
        capture.working_directory = scratch.path;
        capture.capture_frame = 7;
        capture.frame_count = 3;
        capture.timeout = 10s;
        capture.pin = true;
        ngm::ReplayTimingRequest request;
        request.architecture = "Ampere GA10x";
        request.timeout = 10s;
        request.pin = true;
        std::string retained;
        std::string analysis;
        {
            ngm::CaptureService service(options);
            const auto source = service.capture(capture);
            require(completed(service, source).state == ngm::JobState::succeeded, "native timing source");
            retained = request.capture_id = source.artifact_id;
            const auto before = ngm::read_regular_file(scratch.path / "targets.txt", 65536);
            for(std::uint64_t interval = 0; interval != 3; ++interval) {
                request.interval_index = interval;
                const auto job = service.analyze_timing(request);
                const auto result = completed(service, job);
                require(result.state == ngm::JobState::succeeded, "same-input timing collection: " + result.error);
                analysis = job.artifact_id;
                const auto info = service.artifacts().inspect(analysis);
                const auto report = Json::parse(service.artifacts().read(analysis, "raw/report.json"));
                const auto index = Json::parse(service.artifacts().read(analysis, "derived/timing-analysis.json"));
                const auto profile = Json::parse(service.artifacts().read(analysis, "derived/replay-profile.json"));
                require(info.summary.status == "complete" && info.summary.pinned &&
                            info.provenance.at("evidence_origin") == "nsight_replay_timing" &&
                            report.at("timing_collected") == true && report.at("generated_cpp_project") == true &&
                            report.at("profiled") == true && report.at("replayed") == false,
                        "timing collection is distinct from ordinary captures, live profiles and plain replay");
                require(index.at("job") == Json(job.identity) && index.at("source") == report.at("source") &&
                            index.at("requested") == report.at("timing_settings") &&
                            index.at("requested").at("interval_index") == interval &&
                            index.at("requested").at("source_interval_count") == 3 &&
                            index.at("requested").at("cpp_loop_count") == 20 &&
                            index.at("requested").at("profile_loop_count") == 60 &&
                            index.at("requested").at("cpp_wait_frames") == 3 + interval,
                        "all phase identities and interval selection remain tied to the same source");
                require(report.at("source").at("original_capture_sha256") ==
                                report.at("source").at("observed_capture_sha256") &&
                            report.at("replay_target").at("sha256_before") ==
                                report.at("replay_target").at("sha256_after"),
                        "source and replayer contents are checked around both launches");
                const auto bundle = service.artifacts().lease(analysis).directory();
                for(const auto* key : {"cpp_index", "profile_index"}) {
                    const auto ref = index.at(key);
                    require(ngm::sha256_file(bundle / ref.at("path").get<std::string>()) ==
                                ref.at("sha256").get<std::string>(),
                            "both child indexes have retained content identities");
                }
                require(profile.at("settings").at("Start After") ==
                            (interval == 0 ? "Replay Pass Start" : std::to_string(interval) + " Frames"),
                        "SDK and frame triggers retain distinct exact exported contracts");
                rejected([&] { ngm::ProfileInspection(service.artifacts()).metadata(analysis); });
                rejected([&] { ngm::InspectionService(service.artifacts()).metadata(analysis); });
            }
            require(ngm::read_regular_file(scratch.path / "targets.txt", 65536) == before,
                    "analysis never relaunches the original application");
            require(!service.artifacts().inspect(retained).summary.in_use, "completed source leases retire");
            auto unqualified_capture = capture;
            unqualified_capture.frame_count = 2;
            const auto unqualified_source = service.capture(unqualified_capture);
            require(completed(service, unqualified_source).state == ngm::JobState::succeeded,
                    "capture collection remains independent of timing interval qualification");
            auto unqualified_timing = request;
            unqualified_timing.capture_id = unqualified_source.artifact_id;
            unqualified_timing.interval_index = 0;
            rejected([&] { service.analyze_timing(unqualified_timing); });
            auto invalid = request;
            invalid.interval_index = 3;
            rejected([&] { service.analyze_timing(invalid); });
            invalid = request;
            invalid.architecture.clear();
            rejected([&] { service.analyze_timing(invalid); });
            invalid = request;
            invalid.capture_id = analysis;
            rejected([&] { service.analyze_timing(invalid); });
        }
        request.interval_index = 0;
        for(const auto* mode : {"wrong-replay-trigger", "wrong-replay-input", "wrong-replay-target",
                                "missing-action-control", "settings"}) {
            auto selected = options;
            selected.environment["NGM_STANDIN_PROFILE_MODE"] = mode;
            ngm::CaptureService service(selected);
            const auto job = service.analyze_timing(request);
            const auto state = completed(service, job);
            require(state.state == ngm::JobState::failed &&
                        service.artifacts().inspect(job.artifact_id).summary.status == "failed" &&
                        !service.artifacts().inspect(retained).summary.in_use,
                    "changed source/target/settings never publish successful analysis: " + state.error);
            require(!service.artifacts().read(job.artifact_id, "raw/input/capture.ngfx-capture").empty() &&
                        !service.artifacts().read(job.artifact_id, "derived/cpp-project.json").empty(),
                    "failure after C++ collection retains the exact input and completed first phase");
        }
        auto hanging = options;
        const auto pids = scratch.path / "profile.pids";
        hanging.environment["NGM_STANDIN_PROFILE_MODE"] = "hang";
        hanging.environment["NGM_STANDIN_PIDS"] = pids.string();
        {
            auto now = std::chrono::system_clock::now();
            hanging.artifacts.clock = [&] { return now; };
            ngm::CaptureService service(hanging);
            const auto active = service.analyze_timing(request);
            await_file(pids);
            const auto queued = service.analyze_timing(request);
            const auto queued_capture = service.capture(capture);
            require(service.status(queued.identity.job_id)->state == ngm::JobState::queued &&
                        service.status(queued_capture.identity.job_id)->state == ngm::JobState::queued,
                    "all collection phases share the capture/replay/profile GPU queue");
            service.artifacts().pin(retained, false);
            now += std::chrono::hours(24 * 31);
            service.artifacts().prune();
            require(service.artifacts().inspect(retained).summary.status == "complete" &&
                        service.artifacts().inspect(retained).summary.in_use,
                    "queued and active analysis protect the unpinned source");
            service.artifacts().pin(retained, true);
            service.cancel(queued.identity.job_id);
            require(completed(service, queued).state == ngm::JobState::cancelled, "queued timing cancellation");
            service.cancel(active.identity.job_id);
            require(completed(service, active).state == ngm::JobState::cancelled, "active timing cancellation");
            gone(pids);
            require(completed(service, queued_capture).state == ngm::JobState::succeeded,
                    "capture proceeds after analysis owned cleanup");
        }
        hanging.artifacts.clock = std::chrono::system_clock::now;
        fs::remove(pids);
        {
            ngm::CaptureService service(hanging);
            auto timed = request;
            timed.timeout = 1500ms;
            const auto job = service.analyze_timing(timed);
            await_file(pids);
            require(completed(service, job).state == ngm::JobState::timed_out, "analysis deadline owns both phases");
            gone(pids);
        }
        fs::remove(pids);
        {
            ngm::CaptureService service(hanging);
            const auto active = service.analyze_timing(request);
            await_file(pids);
            const auto queued = service.analyze_timing(request);
            const auto shutdown = service.shutdown(5s);
            require(shutdown.workers_joined && shutdown.cleanup_confirmed &&
                        completed(service, active).state == ngm::JobState::cancelled &&
                        completed(service, queued).state == ngm::JobState::cancelled,
                    "shutdown joins analysis and retires queued source protection");
            gone(pids);
        }
        {
            ngm::CaptureService service(options);
            require(service.artifacts().inspect(analysis).summary.status == "complete" &&
                        service.artifacts().inspect(analysis).summary.pinned &&
                        !service.artifacts().inspect(retained).summary.in_use,
                    "analysis evidence and pins survive restart");
        }
    });
}
