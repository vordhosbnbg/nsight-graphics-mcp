#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ProfileInspection.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using ngm::check::require;
using Json = nlohmann::json;
using namespace std::chrono_literals;
struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-profile-XXXXXX").string();
        const auto p = mkdtemp(pattern.data());
        require(p != nullptr, "create scratch directory");
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
    require(state && ngm::job_terminal(state->state), "profile finishes before test deadline");
    require(state->identity == job.identity && state->cleanup_confirmed && !state->worker_running &&
                !state->gpu_reserved && !state->finalization_pending,
            "exact completion identity and owned cleanup");
    return *state;
}
template <typename Function>
void rejected(Function function) {
    try {
        function();
    } catch(const std::exception&) {
        return;
    }
    require(false, "invalid retained evidence must be rejected");
}
Json call(ngm::check::mcp::Client& client, const char* tool, Json args) {
    const auto response = client.request(20, "tools/call", {{"name", tool}, {"arguments", std::move(args)}});
    require(response.contains("result") && !response.at("result").value("isError", false),
            "MCP profile tool succeeds: " + response.dump());
    const auto& result = response.at("result");
    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) == result.at("structuredContent"),
            "equivalent structured/text profile result");
    return result.at("structuredContent");
}
void initialize(ngm::check::mcp::Client& client) {
    require(client
                .request(1, "initialize",
                         {{"protocolVersion", "2025-06-18"},
                          {"capabilities", Json::object()},
                          {"clientInfo", {{"name", "profile-check"}, {"version", "1"}}}})
                .contains("result"),
            "initialize MCP");
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
        ngm::CaptureRequest request;
        request.format = ngm::CaptureFormat::Profile;
        request.executable = target;
        request.working_directory = scratch.path;
        request.timeout = 5s;
        request.profile.architecture = "Ampere GA10x";
        request.pin = true;
        std::string retained;
        {
            ngm::CaptureService service(options);
            const auto first = service.capture(request);
            auto frames = request;
            frames.profile.delimiter = "frames";
            frames.profile.start_after = 5;
            frames.application_output_option = "--many-evidence";
            const auto second = service.capture(frames);
            for(const auto& job : {first, second}) {
                const auto status = completed(service, job);
                require(status.state == ngm::JobState::succeeded, "successful profiling job: " + status.error);
                const auto info = service.artifacts().inspect(job.artifact_id);
                require(info.summary.status == "complete" && info.summary.pinned, "profile complete and pinned");
                const auto report = Json::parse(service.artifacts().read(job.artifact_id, "raw/report.json"));
                require(report.at("profiled") == true && report.at("readable_capture") == false &&
                            report.at("generated_cpp_project") == false,
                        "profile success is distinct from graphics capture and generated replay");
                const auto args = report.at("profile").at("arguments").get<std::vector<std::string>>();
                require(std::find(args.begin(), args.end(), "--set-gpu-clocks=unaltered") != args.end(),
                        "no clock change requested");
                ngm::ProfileInspection inspection(service.artifacts());
                const auto metadata = inspection.metadata(job.artifact_id);
                require(metadata.at("gpu") == "CPU stand-in (not GPU evidence)",
                        "observed identity is not a real compatibility claim");
                const auto page = inspection.metrics(job.artifact_id, "regime_metrics", 0, 1, 0, 1);
                require(page.at("total_rows") == 2 && page.at("total_columns") == 2 && page.at("next_offset") == 1 &&
                            page.at("next_column_offset") == 1 &&
                            page.at("rows")[0].at("values")[0].at("unit").is_null(),
                        "independent row/column pagination and unresolved units");
                const auto tail = inspection.metrics(job.artifact_id, "regime_metrics", 1, 1, 1, 1);
                require(tail.at("rows")[0].at("values")[0].at("value") == 4 && tail.at("next_offset").is_null(),
                        "last cell traversed");
                const auto times = inspection.metrics(job.artifact_id, "event_durations");
                require(times.at("rows")[0].at("label") == times.at("rows")[1].at("label") &&
                            times.at("rows")[0].at("values")[0].at("unit") == "ms",
                        "duplicate events retain explicit duration units");
                rejected([&] { inspection.metrics(job.artifact_id, "missing"); });
                rejected([&] { inspection.metrics(job.artifact_id, "frame_metrics", 0, 0); });
                retained = job.artifact_id;
            }
            // Same-length edits bypass file-size checks and must be rejected by
            // provenance/hash validation rather than silently qualifying evidence.
            auto lease = service.artifacts().lease(retained);
            const auto tamper = [&](const std::string& relative, const std::string& before, const std::string& after,
                                    auto query) {
                require(before.size() == after.size(), "same-size evidence corruption");
                const auto original = service.artifacts().read(retained, relative);
                auto altered = original;
                const auto at = altered.find(before);
                require(at != std::string::npos, "tamper marker exists");
                altered.replace(at, before.size(), after);
                const auto path = lease.directory() / relative;
                const auto permissions = fs::status(path).permissions();
                fs::permissions(path, fs::perms::owner_write, fs::perm_options::add);
                const auto write = [&](const std::string& bytes) {
                    std::ofstream file(path, std::ios::binary | std::ios::trunc);
                    file.exceptions(std::ios::badbit | std::ios::failbit);
                    file << bytes;
                };
                write(altered);
                bool refused = false;
                try {
                    query();
                } catch(const ngm::InspectionError&) {
                    refused = true;
                }
                write(original);
                fs::permissions(path, permissions);
                require(refused, "same-size altered evidence is refused");
            };
            ngm::ProfileInspection inspection(service.artifacts());
            const auto metadata = [&] { return inspection.metadata(retained); };
            tamper("raw/report.json", "\"interface_ready\": true", "\"interface_ready\": null", metadata);
            tamper("raw/report.json", "documented_nsight_cli", "documented_nsight_BAD", metadata);
            tamper("raw/report.json", "--set-gpu-clocks=unaltered", "--set-gpu-clocks=base00000", metadata);
            tamper("derived/profile.json", "REPRO_INFO.xls", "WRONG_INFO.xls", metadata);
            const auto metric_path = metadata().at("tables").at("event_durations").at("path").get<std::string>();
            tamper(metric_path, "0.1", "9.1", [&] { return inspection.metrics(retained, "event_durations"); });
            auto invalid = request;
            invalid.profile.architecture.clear();
            rejected([&] { service.capture(invalid); });
            invalid = request;
            invalid.profile.duration_ms = 10001;
            rejected([&] { service.capture(invalid); });
            invalid = request;
            invalid.capture_frame = 3;
            rejected([&] { service.capture(invalid); });
        }
        {
            auto offline = options;
            offline.nsight_root = scratch.path / "missing-tools";
            ngm::CaptureService service(offline);
            require(ngm::ProfileInspection(service.artifacts()).metadata(retained).at("profile_id") == retained,
                    "retained profile query works after restart without tools");
            const auto source = scratch.path / "caller-import";
            fs::create_directory(source);
            std::ofstream(source / "note") << "not a server profile";
            const auto imported = service.artifacts().import_directory(source, Json::object(), {"note"}, true);
            rejected([&] { ngm::ProfileInspection(service.artifacts()).metadata(imported.summary.id); });
        }
        for(const auto mode : {"fail", "missing", "malformed", "settings", "wrong-producer", "duplicate-trace",
                               "oversize", "symlink", "hardlink"}) {
            auto failure = options;
            failure.artifacts.root = scratch.path / (std::string("failure-") + mode);
            failure.environment["NGM_STANDIN_PROFILE_MODE"] = mode;
            ngm::CaptureService service(failure);
            const auto job = service.capture(request);
            require(completed(service, job).state == ngm::JobState::failed, "bad profile result fails");
            const auto info = service.artifacts().inspect(job.artifact_id);
            require(info.summary.status == "failed" && !info.summary.quarantined,
                    "failed output retained after cleanup");
            rejected([&] { ngm::ProfileInspection(service.artifacts()).metadata(job.artifact_id); });
        }
        for(const auto& [key, value] : std::map<std::string, std::string>{
                {"NGM_OMIT_PROFILE_OPTION", "--set-gpu-clocks"}, {"NGM_MISMATCH_BUILD", "ngfx"}}) {
            auto unsupported = options;
            unsupported.artifacts.root = scratch.path / key;
            unsupported.environment[key] = value;
            ngm::CaptureService service(unsupported);
            const auto job = service.capture(request);
            require(completed(service, job).state == ngm::JobState::failed, "missing profile interface rejected");
            const auto report = Json::parse(service.artifacts().read(job.artifact_id, "raw/report.json"));
            require(!report.contains("profile") || report.at("profile").at("launched") == false,
                    "unsupported profiling never launches the target");
        }
        for(const bool cancel : {false, true}) {
            auto hanging = options;
            hanging.artifacts.root = scratch.path / (cancel ? "cancel" : "timeout");
            hanging.environment["NGM_STANDIN_PROFILE_MODE"] = "hang";
            const auto pids = scratch.path / (cancel ? "cancel.pids" : "timeout.pids");
            hanging.environment["NGM_STANDIN_PIDS"] = pids.string();
            ngm::CaptureService service(hanging);
            auto timed = request;
            timed.timeout = cancel ? 5s : 500ms;
            const auto job = service.capture(timed);
            const auto deadline = std::chrono::steady_clock::now() + 4s;
            while(!fs::exists(pids) && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(1ms);
            require(fs::exists(pids), "profile owns a hanging tool and descendant");
            auto graphics = request;
            graphics.format = ngm::CaptureFormat::Graphics;
            graphics.profile = {};
            const auto queued = service.capture(graphics);
            require(service.status(queued.identity.job_id)->state == ngm::JobState::queued,
                    "capture shares profile GPU reservation");
            service.cancel(queued.identity.job_id);
            if(cancel)
                service.cancel(job.identity.job_id);
            require(completed(service, job).state == (cancel ? ngm::JobState::cancelled : ngm::JobState::timed_out),
                    "profile cancellation/deadline terminal state");
            require(completed(service, queued).state == ngm::JobState::cancelled, "queued cross-format cancellation");
        }
        {
            using ngm::check::mcp::Client;
            const auto store = scratch.path / "mcp-store";
            const std::vector<std::string> args{"--nsight-root", installation.string(), "--artifact-root",
                                                store.string()};
            Client client(fs::absolute(argv[2]), scratch.path, args);
            initialize(client);
            const auto submitted = call(client, "profile",
                                        {{"executable", target.string()},
                                         {"working_directory", scratch.path.string()},
                                         {"settings", {{"architecture", "Ampere GA10x"}}},
                                         {"pin", true}});
            Json state;
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            do {
                state = call(client, "job_status", {{"job_id", submitted.at("identity").at("job_id")}});
                if(state.at("state") == "succeeded" || state.at("state") == "failed")
                    break;
                std::this_thread::sleep_for(1ms);
            } while(std::chrono::steady_clock::now() < deadline);
            require(state.at("state") == "succeeded" && state.at("cleanup_confirmed") == true,
                    "actual MCP profile workflow succeeds");
            const auto id = submitted.at("artifact_id");
            require(call(client, "profile_metadata", {{"profile_id", id}}).at("producer").is_string(),
                    "typed profile metadata");
            require(
                call(client, "profile_metrics", {{"profile_id", id}, {"table", "frame_metrics"}}).at("rows").size() ==
                    2,
                "typed profile metric query");
            require(client.finish() == 0, "protocol-only stdout and clean server shutdown");
            Client restart(fs::absolute(argv[2]), scratch.path, {"--artifact-root", store.string()});
            initialize(restart);
            require(call(restart, "profile_metadata", {{"profile_id", id}}).at("profile_id") == id,
                    "retained MCP query after restart");
            require(restart.finish() == 0, "restart clean exit");
        }
    });
}
