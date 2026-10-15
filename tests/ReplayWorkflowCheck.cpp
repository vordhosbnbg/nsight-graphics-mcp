#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

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
        auto pattern = (fs::temp_directory_path() / "ngm-replay-XXXXXX").string();
        const auto p = mkdtemp(pattern.data());
        require(p != nullptr, "create replay scratch directory");
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
    const auto state = service.wait(job.identity.job_id, 10s);
    require(state && ngm::job_terminal(state->state), "replay finishes before check deadline");
    require(state->identity == job.identity && state->cleanup_confirmed && !state->worker_running &&
                !state->gpu_reserved && !state->finalization_pending,
            "exact replay identity, cleanup and retired callable");
    return *state;
}
template <typename Function>
void rejected(Function function) {
    try {
        function();
    } catch(const std::exception&) {
        return;
    }
    require(false, "invalid replay input must be rejected");
}
void await_file(const fs::path& file) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while(std::chrono::steady_clock::now() < deadline) {
        std::error_code error;
        if(fs::file_size(file, error) > 0 && !error)
            return;
        std::this_thread::sleep_for(1ms);
    }
    require(false, "replay process readiness");
}
void gone(const fs::path& file) {
    std::ifstream input(file);
    int pid = 0;
    std::size_t count = 0;
    while(input >> pid) {
        require(kill(pid, 0) == -1 && errno == ESRCH, "owned replay child reaped");
        ++count;
    }
    require(count == 2, "replay and escaped child both observed");
}
Json call(ngm::check::mcp::Client& client, const char* tool, Json args) {
    const auto response = client.request(20, "tools/call", {{"name", tool}, {"arguments", std::move(args)}});
    require(response.contains("result") && !response.at("result").value("isError", false),
            "actual MCP replay call: " + response.dump());
    const auto& result = response.at("result");
    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) == result.at("structuredContent"),
            "equivalent structured/text replay result");
    return result.at("structuredContent");
}
void initialize(ngm::check::mcp::Client& client) {
    require(client
                .request(1, "initialize",
                         {{"protocolVersion", "2025-11-25"},
                          {"capabilities", Json::object()},
                          {"clientInfo", {{"name", "replay-check"}, {"version", "1"}}}})
                .contains("result"),
            "initialize replay MCP client");
    client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "stand-in and actual server paths");
        Scratch scratch;
        const auto installation = scratch.path / "installation";
        for(const auto name : {"ngfx", "ngfx-capture", "ngfx-replay"})
            copy_executable(fs::absolute(argv[1]), installation / "host/linux-desktop-nomad-x64" / name);
        const auto target = scratch.path / "target with spaces";
        copy_executable(fs::absolute(argv[1]), target);
        ngm::CaptureServiceOptions options;
        options.nsight_root = installation;
        options.artifacts.root = scratch.path / "store";
        options.environment = {{"NGM_TARGET_RECORD", (scratch.path / "targets.txt").string()}};
        ngm::CaptureRequest capture;
        capture.executable = target;
        capture.working_directory = scratch.path;
        capture.timeout = 10s;
        capture.pin = true;
        ngm::ReplayRequest replay;
        replay.timeout = 10s;
        replay.pin = true;
        replay.loop_count = 3;
        replay.launch_environment = {{"QT_QPA_PLATFORM", "xcb"}};
        std::string retained;
        std::string replay_id;
        {
            ngm::CaptureService service(options);
            const auto source = service.capture(capture);
            require(completed(service, source).state == ngm::JobState::succeeded, "native source capture");
            retained = replay.capture_id = source.artifact_id;
            const auto target_record = ngm::read_regular_file(scratch.path / "targets.txt", 65536);
            const auto job = service.replay(replay);
            const auto state = completed(service, job);
            require(state.state == ngm::JobState::succeeded, "supervised finite replay succeeds: " + state.error);
            replay_id = job.artifact_id;
            const auto report = Json::parse(service.artifacts().read(job.artifact_id, "raw/report.json"));
            require(report.at("evidence_origin") == "nsight_gpu_replay" && report.at("replayed") == true &&
                        report.at("job") == Json(job.identity) && report.at("source").at("capture_id") == retained &&
                        report.at("source").at("observed_capture_sha256") ==
                            report.at("source").at("original_capture_sha256") &&
                        report.at("environment").at("QT_QPA_PLATFORM") == "xcb",
                    "separate replay origin, immutable source identity and overlay");
            const auto args = report.at("replay").at("arguments").get<std::vector<std::string>>();
            require(args.size() == 4 && args.front() == "--loop-count=3" &&
                        fs::path(args.back()).lexically_relative(options.artifacts.root).string().find(retained) ==
                            std::string::npos,
                    "replayer receives only its own staged capture snapshot and finite controls");
            require(ngm::read_regular_file(scratch.path / "targets.txt", 65536) == target_record,
                    "replay never launches the original target again");
            require(service.artifacts().inspect(replay_id).summary.pinned &&
                        !service.artifacts().inspect(retained).summary.in_use,
                    "replay pin persists and source lease retires after finalization");
            auto invalid = replay;
            for(const auto loops : {std::uint64_t{0}, std::uint64_t{601}}) {
                invalid.loop_count = loops;
                rejected([&] { service.replay(invalid); });
            }
            invalid = replay;
            invalid.timeout = 0ms;
            rejected([&] { service.replay(invalid); });
            invalid = replay;
            invalid.capture_id = replay_id;
            rejected([&] { service.replay(invalid); });
            const auto imported = scratch.path / "imported";
            fs::create_directory(imported);
            std::ofstream(imported / "capture.ngfx-capture") << "unverified";
            invalid.capture_id = service.artifacts().import_directory(imported, Json::object()).summary.id;
            rejected([&] { service.replay(invalid); });
            auto failed = service.artifacts().begin(Json::object());
            service.artifacts().publish_failure(failed, "synthetic failure");
            invalid.capture_id = failed.id();
            rejected([&] { service.replay(invalid); });
            for(const auto format : {ngm::CaptureFormat::Cpp, ngm::CaptureFormat::Profile}) {
                auto other = capture;
                other.format = format;
                other.profile.architecture = format == ngm::CaptureFormat::Profile ? "Ampere GA10x" : "";
                const auto job = service.capture(other);
                require(completed(service, job).state == ngm::JobState::succeeded, "other native evidence format");
                invalid.capture_id = job.artifact_id;
                rejected([&] { service.replay(invalid); });
            }
        }
        for(const auto* mode : {"metadata-only", "fail", "empty"}) {
            auto failed_options = options;
            failed_options.environment["NGM_STANDIN_BATCH_REPLAY_MODE"] = mode;
            ngm::CaptureService service(failed_options);
            const auto job = service.replay(replay);
            require(completed(service, job).state == ngm::JobState::failed &&
                        service.artifacts().inspect(job.artifact_id).summary.status == "failed",
                    "exit code alone or metadata-only output cannot complete GPU replay");
            require(!service.artifacts().read(job.artifact_id, "raw/input/capture.ngfx-capture").empty(),
                    "failed replay retains its exact source input");
        }
        {
            auto mismatched = options;
            mismatched.environment["NGM_PROFILE_OLD_RELEASE"] = "1";
            ngm::CaptureService service(mismatched);
            const auto job = service.replay(replay);
            const auto result = completed(service, job);
            require(result.state == ngm::JobState::failed && result.error.find("match") != std::string::npos,
                    "mismatched installed release cannot replay a retained capture");
        }
        // Use the same replayer with an escaped child at the real process boundary.
        auto hanging = options;
        const auto pids = scratch.path / "replay.pids";
        hanging.environment["NGM_STANDIN_BATCH_REPLAY_MODE"] = "hang";
        hanging.environment["NGM_STANDIN_PIDS"] = pids.string();
        {
            auto now = std::chrono::system_clock::now();
            auto aging = hanging;
            aging.artifacts.clock = [&] { return now; };
            ngm::CaptureService service(aging);
            const auto active = service.replay(replay);
            await_file(pids);
            const auto queued = service.replay(replay);
            auto expiring = replay;
            expiring.timeout = 100ms;
            const auto expired = service.replay(expiring);
            require(completed(service, expired).state == ngm::JobState::timed_out &&
                        service.artifacts().inspect(expired.artifact_id).summary.status == "failed",
                    "queued replay deadline retires its source lease without a process quarantine");
            const auto queued_capture = service.capture(capture);
            auto profile = capture;
            profile.format = ngm::CaptureFormat::Profile;
            profile.profile.architecture = "Ampere GA10x";
            const auto queued_profile = service.capture(profile);
            require(service.status(queued.identity.job_id)->state == ngm::JobState::queued &&
                        service.status(queued_capture.identity.job_id)->state == ngm::JobState::queued &&
                        service.status(queued_profile.identity.job_id)->state == ngm::JobState::queued,
                    "replay, capture and profile share the same GPU reservation");
            service.artifacts().pin(retained, false);
            now += std::chrono::hours(24 * 31);
            service.artifacts().prune();
            require(service.artifacts().inspect(retained).summary.status == "complete" &&
                        service.artifacts().inspect(retained).summary.in_use,
                    "queued and active replay source survives unpin and age pruning");
            service.artifacts().pin(retained, true);
            service.cancel(queued.identity.job_id);
            require(completed(service, queued).state == ngm::JobState::cancelled &&
                        service.artifacts().inspect(queued.artifact_id).summary.status == "failed",
                    "queued cancellation publishes normal failed evidence");
            service.cancel(active.identity.job_id);
            require(completed(service, active).state == ngm::JobState::cancelled, "active replay cancellation");
            gone(pids);
            require(completed(service, queued_capture).state == ngm::JobState::succeeded &&
                        completed(service, queued_profile).state == ngm::JobState::succeeded,
                    "capture/profile proceed only after replay owned cleanup");
            service.artifacts().pin(retained, true);
        }
        fs::remove(pids);
        {
            ngm::CaptureService service(hanging);
            auto timed = replay;
            timed.timeout = 1500ms;
            const auto job = service.replay(timed);
            await_file(pids);
            require(completed(service, job).state == ngm::JobState::timed_out, "finite replay deadline");
            gone(pids);
        }
        fs::remove(pids);
        {
            ngm::CaptureService service(hanging);
            const auto active = service.replay(replay);
            await_file(pids);
            const auto queued = service.replay(replay);
            const auto result = service.shutdown(5s);
            require(result.workers_joined && result.cleanup_confirmed &&
                        completed(service, active).state == ngm::JobState::cancelled &&
                        completed(service, queued).state == ngm::JobState::cancelled,
                    "shutdown joins active replay and retires queued source leases");
            gone(pids);
            require(!service.artifacts().inspect(retained).summary.in_use, "shutdown releases source lease");
        }
        // A retained source changed after queueing must fail before GPU execution.
        fs::remove(pids);
        {
            ngm::CaptureService service(hanging);
            const auto active = service.replay(replay);
            await_file(pids);
            const auto queued = service.replay(replay);
            const auto path = service.artifacts().lease(retained).directory() / "raw/capture.ngfx-capture";
            const auto original = ngm::read_regular_file(path, 65536);
            auto changed = original;
            changed.front() = '?';
            std::ofstream(path, std::ios::binary) << changed;
            service.cancel(active.identity.job_id);
            (void)completed(service, active);
            const auto failure = completed(service, queued);
            require(failure.state == ngm::JobState::failed && failure.error.find("source changed") != std::string::npos,
                    "same-size queued source mutation rejected by capture-time hash");
            require(service.artifacts().read(queued.artifact_id, "raw/input/capture.ngfx-capture") == changed,
                    "rejected changed snapshot remains explicit failed evidence");
            std::ofstream(path, std::ios::binary) << original;
        }
        {
            ngm::CaptureService service(options);
            require(service.artifacts().inspect(replay_id).summary.status == "complete" &&
                        service.artifacts().inspect(replay_id).summary.pinned,
                    "replay evidence and pin survive all service restarts");
        }
        // Exercise the actual stdio transport against the already retained store.
        ngm::check::mcp::Client client(
            fs::absolute(argv[2]), scratch.path,
            {"--artifact-root", options.artifacts.root.string(), "--nsight-root", installation.string()});
        initialize(client);
        const auto submitted = call(client, "replay", {{"capture_id", retained}, {"pin", true}});
        Json state;
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        do {
            state = call(client, "job_status", {{"job_id", submitted.at("identity").at("job_id")}});
            if(state.at("state") == "succeeded" || state.at("state") == "failed")
                break;
            std::this_thread::sleep_for(5ms);
        } while(std::chrono::steady_clock::now() < deadline);
        require(state.at("state") == "succeeded" && state.at("cleanup_confirmed") == true,
                "actual stdio replay success: " + state.dump());
        const auto result = call(client, "artifact_info", {{"artifact_id", submitted.at("artifact_id")}});
        require(result.at("pinned") == true && result.at("provenance").at("evidence_origin") == "nsight_gpu_replay",
                "actual stdio retains replay evidence and structured provenance");
        const auto invalid = client.request(
            2, "tools/call", {{"name", "replay"}, {"arguments", {{"capture_id", retained}, {"loop_count", 0}}}});
        require(invalid.at("result").at("isError") == true, "actual stdio rejects invalid replay bounds");
    });
}
