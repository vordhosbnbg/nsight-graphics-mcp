#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/LaunchEnvironment.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
void save(const fs::path& path, const Json& json) {
    std::ofstream file(path);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << json.dump(2) << '\n';
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 8, "SERVER FIXTURE SHADERS NSIGHT_ROOT STORE REPORT_ROOT LAUNCH_ENVIRONMENT_JSON required");
        const auto server = fs::canonical(argv[1]), fixture = fs::canonical(argv[2]), shaders = fs::canonical(argv[3]);
        const auto installation = fs::canonical(argv[4]), store = fs::absolute(argv[5]), output = fs::absolute(argv[6]);
        const auto overlay_file = fs::canonical(argv[7]);
        const auto overlay = Json::parse(ngm::read_regular_file(overlay_file, 32768)).get<ngm::LaunchEnvironment>();
        const auto selection = ngm::validate_launch_environment(overlay);
        fs::create_directories(output);
        auto pattern = (output / "launch-environment-XXXXXX").string();
        require(mkdtemp(pattern.data()), "isolated hardware validation directory");
        const fs::path run(pattern);
        fs::copy_file(__FILE__, run / "LaunchEnvironmentIntegration.cpp");
        fs::copy_file(overlay_file, run / "launch-environment.json");
        Json report{
            {"status", "running"},
            {"project_version", ngm::project_version()},
            {"evidence_origin", "hardware_launch_environment_mcp_validation"},
            {"scope", "Fresh XCB/Xwayland Vulkan fixture capture; does not qualify SDL Wayland, "
                      "profiling permission, new draw state or driver machine settings"},
            {"server", {{"path", server.string()}, {"sha256", ngm::sha256_file(server)}}},
            {"fixture", {{"path", fixture.string()}, {"sha256", ngm::sha256_file(fixture)}}},
            {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
            {"shader_provenance", Json::parse(ngm::read_regular_file(shaders / "provenance.json", 1024 * 1024))},
            {"launch_environment", overlay},
            {"selection", selection},
            {"artifact_root", store.string()}};
        std::vector<std::string> desktop;
        report["desktop_environment"] = Json::object();
        for(const auto* key : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE",
                               "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS"})
            if(const auto* value = std::getenv(key); value && *value) {
                desktop.emplace_back(std::string(key) + '=' + value);
                report["desktop_environment"][key] = value;
            }
        save(run / "report.json", report);
        std::cout << run << '\n' << std::flush;
        try {
            // Independent fresh fixture launch separates desktop/driver startup
            // from Nsight injection. It is application evidence, never an export.
            auto baseline_environment = ngm::capture_environment();
            for(const auto& [key, value] : overlay)
                baseline_environment[key] = value;
            for(const auto* key :
                {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "TMPDIR"}) {
                const auto path = run / "baseline-config" / key;
                fs::create_directories(path);
                baseline_environment[key] = path.string();
            }
            ngm::ProcessOptions baseline;
            baseline.executable = fixture;
            baseline.working_directory = run;
            baseline.arguments = {"--scenario",   "reference",
                                  "--seed",       "42",
                                  "--width",      "192",
                                  "--height",     "128",
                                  "--frame",      "2",
                                  "--shader-dir", shaders.string(),
                                  "--output",     (run / "baseline-output").string()};
            baseline.environment = baseline_environment;
            baseline.stdout_path = run / "baseline.stdout";
            baseline.stderr_path = run / "baseline.stderr";
            baseline.timeout = 30s;
            const auto baseline_result = ngm::run_process(baseline);
            report["baseline"] = {
                {"evidence_origin", "application_provided"},
                {"arguments", baseline.arguments},
                {"environment", baseline_environment},
                {"exit_code", baseline_result.exit_code ? Json(*baseline_result.exit_code) : Json(nullptr)},
                {"timed_out", baseline_result.timed_out},
                {"cleanup_confirmed", baseline_result.cleanup_confirmed},
                {"error", baseline_result.error}};
            save(run / "report.json", report);
            require(baseline_result.exit_code == 0 && baseline_result.cleanup_confirmed && !baseline_result.timed_out,
                    "independent matching-environment fixture baseline succeeds");
            report["baseline"]["result"] =
                Json::parse(ngm::read_regular_file(run / "baseline-output/result.json", 1024 * 1024));
            require(report.at("baseline").at("result").at("status") == "pass", "fixture baseline output contract");
            std::ofstream transcript(run / "transcript.ndjson");
            transcript.exceptions(std::ios::badbit | std::ios::failbit);
            ngm::check::mcp::Client client(server, "/usr/bin:/bin",
                                           {"--artifact-root", store.string(), "--nsight-root", installation.string()},
                                           false, desktop, {30s, 30s});
            int sequence = 1;
            const auto request = [&](const char* method, const Json& params) {
                transcript
                    << Json{{"direction", "request"}, {"id", sequence}, {"method", method}, {"params", params}}.dump()
                    << '\n';
                const auto response = client.request(sequence++, method, params);
                transcript << Json{{"direction", "response"}, {"message", response}}.dump() << '\n';
                transcript.flush();
                require(response.contains("result") && !response.at("result").value("isError", false),
                        "MCP request succeeds: " + response.dump());
                return response.at("result");
            };
            const auto call = [&](const char* tool, const Json& args) {
                const auto result = request("tools/call", {{"name", tool}, {"arguments", args}});
                require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                            result.at("structuredContent"),
                        "structured/text parity");
                return result.at("structuredContent");
            };
            const auto init =
                request("initialize",
                        {{"protocolVersion", "2025-06-18"},
                         {"capabilities", Json::object()},
                         {"clientInfo",
                          {{"name", "ngm-launch-environment-integration"}, {"version", ngm::project_version()}}}});
            require(init.at("serverInfo").at("version") == ngm::project_version(), "current server identity");
            client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
            report["capabilities"] = call("capabilities", Json::object());
            report["submission"] =
                call("capture", {{"executable", fixture.string()},
                                 {"working_directory", run.string()},
                                 {"arguments",
                                  {"--scenario", "reference", "--seed", "42", "--width", "192", "--height", "128",
                                   "--frame", "20", "--shader-dir", shaders.string()}},
                                 {"capture_frame", 2},
                                 {"timeout_ms", 120000},
                                 {"pin", true},
                                 {"application_output_option", "--output"},
                                 {"launch_environment", overlay}});
            save(run / "report.json", report);
            const auto deadline = std::chrono::steady_clock::now() + 150s;
            for(;;) {
                const auto state =
                    call("job_status", {{"job_id", report.at("submission").at("identity").at("job_id")}});
                report["job"] = state;
                save(run / "report.json", report);
                if(state.at("state") != "queued" && state.at("state") != "running" &&
                   state.at("worker_running") == false && state.at("gpu_reserved") == false &&
                   state.at("finalization_pending") == false)
                    break;
                require(std::chrono::steady_clock::now() < deadline, "hardware job polling deadline");
                std::this_thread::sleep_for(250ms);
            }
            const auto id = report.at("submission").at("artifact_id");
            const auto retained = Json::parse(call("artifact_read", {{"artifact_id", id}, {"path", "raw/report.json"}})
                                                  .at("text")
                                                  .get<std::string>());
            report["capture_report"] = retained;
            require(report.at("job").at("state") == "succeeded" && report.at("job").at("cleanup_confirmed") == true,
                    "fresh Vulkan capture succeeds: " + report.at("job").dump());
            require(retained.at("launch_environment").at("requested") == Json(overlay) &&
                        retained.at("launch_environment").at("selection") == selection,
                    "current MCP request retains exact selected paths and hashes");
            for(const auto& [key, value] : overlay)
                require(retained.at("environment").at(key) == value, "real capture uses the applied overlay");
            report["metadata"] = call("capture_metadata", {{"capture_id", id}});
            report["artifact"] = call("artifact_info", {{"artifact_id", id}});
            require(report.at("artifact").at("pinned") == true, "source capture pin");
            require(ngm::sha256_file(server) == report.at("server").at("sha256").get<std::string>() &&
                        ngm::sha256_file(fixture) == report.at("fixture").at("sha256").get<std::string>(),
                    "unchanged executable path identities");
            require(client.finish() == 0, "normal MCP shutdown");
            std::ofstream(run / "server.stderr") << client.diagnostics();
            report["status"] = "pass";
            save(run / "report.json", report);
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
            save(run / "report.json", report);
            throw;
        }
    });
}
