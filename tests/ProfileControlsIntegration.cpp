#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path.string() + ".tmp");
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << value.dump(2) << '\n';
    stream.close();
    fs::rename(path.string() + ".tmp", path);
}
Json load(const fs::path& path) {
    return Json::parse(ngm::read_regular_file(path, 4U * 1024U * 1024U));
}
} // namespace
int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    fs::path run;
    Json report{{"status", "running"},
                {"project_version", ngm::project_version()},
                {"scope", "Actual profile control requests/export verification; no draw correlation, overhead or "
                          "per-draw counters are qualified"},
                {"cases", Json::array()}};
    return ngm::check::run([&] {
        require(argc == 7, "SERVER FIXTURE SHADERS NSIGHT_ROOT OUTPUT_PARENT LAUNCH_ENV_JSON required");
        const auto server = fs::canonical(argv[1]), fixture = fs::canonical(argv[2]), shaders = fs::canonical(argv[3]);
        const auto nsight = fs::canonical(argv[4]), parent = fs::absolute(argv[5]);
        const auto overlay = load(fs::canonical(argv[6]));
        (void)ngm::validate_launch_environment(overlay.get<ngm::LaunchEnvironment>());
        fs::create_directories(parent);
        auto pattern = (parent / "profile-controls-XXXXXX").string();
        require(mkdtemp(pattern.data()), "unique control qualification directory");
        run = pattern;
        std::cout << run << '\n' << std::flush;
        fs::copy_file(__FILE__, run / "ProfileControlsIntegration.cpp");
        std::vector<std::string> environment;
        for(const auto& [key, value] : ngm::capture_environment())
            if(key != "PATH")
                environment.push_back(key + '=' + value);
        report["server_sha256"] = ngm::sha256_file(server);
        report["fixture_sha256"] = ngm::sha256_file(fixture);
        report["harness_sha256"] = ngm::sha256_file("/proc/self/exe");
        report["launch_environment"] = overlay;
        report["desktop_environment"] = ngm::capture_environment();
        report["shader_provenance"] = load(shaders / "provenance.json");
        save(run / "report.json", report);
        std::ofstream transcript(run / "transcript.ndjson");
        transcript.exceptions(std::ios::failbit | std::ios::badbit);
        try {
            for(unsigned pass = 0; pass < 2; ++pass) {
                ngm::check::mcp::Client client(
                    server, "/usr/bin:/bin",
                    {"--artifact-root", (run / "store").string(), "--nsight-root", nsight.string()}, false, environment,
                    {30s, 30s});
                int sequence = 1;
                const auto request = [&](const char* method, const Json& params) {
                    transcript << Json{{"pass", pass},
                                       {"direction", "request"},
                                       {"id", sequence},
                                       {"method", method},
                                       {"params", params}}
                                      .dump()
                               << '\n';
                    const auto response = client.request(sequence++, method, params);
                    transcript << Json{{"pass", pass}, {"direction", "response"}, {"message", response}}.dump() << '\n';
                    transcript.flush();
                    require(response.contains("result"), "profile controls protocol result");
                    return response.at("result");
                };
                const auto call = [&](const char* tool, const Json& args) {
                    const auto result = request("tools/call", {{"name", tool}, {"arguments", args}});
                    require(!result.value("isError", false) && result.contains("structuredContent"),
                            "profile control call: " + result.dump());
                    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                                result.at("structuredContent"),
                            "profile control text/structured parity");
                    return result.at("structuredContent");
                };
                const auto initialized =
                    request("initialize",
                            {{"protocolVersion", "2025-06-18"},
                             {"capabilities", Json::object()},
                             {"clientInfo",
                              {{"name", "ngm-profile-controls-integration"}, {"version", ngm::project_version()}}}});
                require(initialized.at("serverInfo").at("version") == ngm::project_version(),
                        "current source-built server version");
                client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
                if(pass == 0)
                    report["tools"] = request("tools/list", Json::object());
                for(unsigned index = 0; index < 2; ++index) {
                    const bool enabled = index != 0;
                    if(pass == 0) {
                        const Json settings{{"architecture", "Ampere GA10x"},
                                            {"delimiter", "frames"},
                                            {"start_after", 5},
                                            {"limit", 3},
                                            {"duration_ms", 1000},
                                            {"time_every_action", enabled},
                                            {"hes_enabled", false}};
                        const Json arguments{
                            {"executable", fixture.string()},
                            {"working_directory", run.string()},
                            {"arguments",
                             {"--scenario", "state-descriptors-reuse", "--seed", "42", "--width", "192", "--height",
                              "128", "--frame", "300", "--shader-dir", shaders.string()}},
                            {"application_output_option", "--output"},
                            {"launch_environment", overlay},
                            {"settings", settings},
                            {"timeout_ms", 120000},
                            {"pin", true}};
                        const auto submitted = call("profile", arguments);
                        report["cases"].push_back({{"request", arguments}, {"submission", submitted}});
                        save(run / "report.json", report);
                        Json state;
                        const auto deadline = std::chrono::steady_clock::now() + 150s;
                        for(;;) {
                            state = call("job_status", {{"job_id", submitted.at("identity").at("job_id")}});
                            if(state.at("state") != "queued" && state.at("state") != "running" &&
                               state.at("finalization_pending") == false)
                                break;
                            require(std::chrono::steady_clock::now() < deadline, "native profile control deadline");
                            std::this_thread::sleep_for(100ms);
                        }
                        auto& item = report["cases"].back();
                        item["job"] = state;
                        const auto id = submitted.at("artifact_id");
                        item["artifact_info"] = call("artifact_info", {{"artifact_id", id}});
                        item["capture_report"] =
                            load(run / "store/bundles" / id.get<std::string>() / "raw/report.json");
                        save(run / "report.json", report);
                        require(state.at("cleanup_confirmed") == true && state.at("gpu_reserved") == false &&
                                    state.at("worker_running") == false &&
                                    item.at("artifact_info").at("pinned") == true,
                                "exact terminal cleanup and independent profile pin");
                        require(state.at("state") == "succeeded",
                                "native profiling failed; inspect retained request/logs/report before retrying");
                    }
                    auto& item = report["cases"][index];
                    const auto id = item.at("submission").at("artifact_id");
                    const auto metadata = call("profile_metadata", {{"profile_id", id}});
                    const auto& controls = metadata.at("collection_controls");
                    require(controls.at("time_every_action").at("requested") == enabled &&
                                controls.at("time_every_action").at("exported") == (enabled ? "Yes" : "No") &&
                                controls.at("time_every_action").at("verification") == "verified_export" &&
                                controls.at("hes_enabled").at("requested") == false &&
                                controls.at("hes_enabled").at("exported").is_null(),
                            "actual per-action export matches request; active HES state remains unverified");
                    if(pass == 0) {
                        item["metadata"] = metadata;
                        item["events"] = call("profile_metrics", {{"profile_id", id}, {"table", "event_durations"}});
                        item["frame_duration"] =
                            call("profile_metrics", {{"profile_id", id}, {"table", "frame_duration"}});
                        const auto& ref = metadata.at("tables").at("regime_metrics");
                        const auto raw_path =
                            run / "store/bundles" / id.get<std::string>() / ref.at("path").get<std::string>();
                        require(ngm::sha256_file(raw_path) == ref.at("sha256").get<std::string>(),
                                "independent raw regime table identity");
                        std::istringstream lines(ngm::read_regular_file(raw_path, 16U * 1024U * 1024U));
                        std::string line;
                        require(static_cast<bool>(std::getline(lines, line)), "raw regime header");
                        std::size_t rows = 0, available = 0, unavailable = 0;
                        Json first_known, first_missing;
                        while(std::getline(lines, line)) {
                            if(line.ends_with('\r'))
                                line.pop_back();
                            std::istringstream cells(line);
                            std::string label, token;
                            require(static_cast<bool>(std::getline(cells, label, '\t')), "raw regime row label");
                            std::size_t column = 0;
                            while(std::getline(cells, token, '\t')) {
                                const bool missing = token == "nan";
                                missing ? ++unavailable : ++available;
                                auto& first = missing ? first_missing : first_known;
                                if(first.is_null())
                                    first = {{"row", rows}, {"column", column}, {"text", token}};
                                ++column;
                            }
                            ++rows;
                        }
                        const auto& coverage = metadata.at("table_coverage").at("regime_metrics");
                        require(coverage.at("rows") == rows && coverage.at("available_values") == available &&
                                    coverage.at("unavailable_values") == unavailable,
                                "independent raw finite/missing position counts");
                        item["regime_audit"] = {{"rows", rows},
                                                {"available", available},
                                                {"unavailable", unavailable},
                                                {"samples", Json::array()}};
                        for(const auto& sample : {first_known, first_missing}) {
                            if(sample.is_null())
                                continue;
                            const auto result = call("profile_metrics", {{"profile_id", id},
                                                                         {"table", "regime_metrics"},
                                                                         {"offset", sample.at("row")},
                                                                         {"limit", 1},
                                                                         {"column_offset", sample.at("column")},
                                                                         {"column_limit", 1}});
                            const auto& cell = result.at("rows")[0].at("values")[0];
                            require(cell.at("text") == sample.at("text") &&
                                        cell.at("value").is_null() == (sample.at("text") == "nan") &&
                                        cell.at("status") == (sample.at("text") == "nan" ? "unavailable" : "available"),
                                    "raw selected regime positions preserve exact text and explicit numeric coverage");
                            item["regime_audit"]["samples"].push_back(result);
                        }
                    } else
                        require(metadata == item.at("metadata"), "retained control provenance survives restart");
                    require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                            "profile pin survives restart");
                    save(run / "report.json", report);
                }
                require(client.finish() == 0, "profile control server exits cleanly");
            }
            report["status"] = "pass";
            save(run / "report.json", report);
        } catch(...) {
            report["status"] = "failed";
            save(run / "report.json", report);
            throw;
        }
    });
}
