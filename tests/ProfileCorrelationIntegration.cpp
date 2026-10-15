#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <cmath>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
Json load(const fs::path& path) {
    return Json::parse(ngm::read_regular_file(path, 8U * 1024U * 1024U));
}
void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path.string() + ".tmp");
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << value.dump(2) << '\n';
    stream.close();
    fs::rename(path.string() + ".tmp", path);
}
void verify_witness(const Json& witness, const std::string& scenario, const std::string& fixture_hash) {
    require(witness.at("evidence_origin") == "private_fixture_timestamps" &&
                witness.at("executable_sha256") == fixture_hash && witness.at("scenario") == scenario &&
                !witness.at("submissions").empty(),
            "private API/timestamp witness identity");
    const auto bits = witness.at("timestamp_valid_bits").get<unsigned>();
    const auto period = witness.at("timestamp_period_ns").get<double>();
    require(bits >= 36 && bits <= 64 && std::isfinite(period) && period > 0, "private timestamp properties");
    const auto mask = bits == 64 ? UINT64_MAX : (std::uint64_t{1} << bits) - 1;
    std::size_t ordinal = 0;
    for(const auto& item : witness.at("submissions")) {
        const auto a = item.at("timestamp_start").get<std::uint64_t>();
        const auto b = item.at("timestamp_end").get<std::uint64_t>();
        const auto ticks = (b - a) & mask;
        require(a <= mask && b <= mask && ticks == item.at("elapsed_ticks") &&
                    static_cast<double>(ticks) * period == item.at("duration_ns") &&
                    item.at("host_submit_wait_ns").get<long double>() < std::ldexp(period, static_cast<int>(bits)),
                "independent timestamp conversion and wrap bound");
        const auto repeats = scenario == "timing-reuse" ? 2u : 1u;
        require(item.at("frame") == ordinal / repeats && item.at("submission_occurrence") == ordinal % repeats &&
                    item.at("draws").size() == 6 && item.at("markers").size() >= 10 && item.at("markers").size() <= 11,
                "complete ordered fresh-frame/reused-submission API witness");
        if(item.at("markers").size() == 11)
            require(item.at("markers")[10].at("name") == "application.readback" &&
                        item.at("markers")[10].at("parent_ordinal") == 0 &&
                        item.at("markers")[10].at("path").size() == 2,
                    "selected-frame readback marker stays outside the timed scene");
        for(std::size_t draw = 0; draw < 6; ++draw) {
            const auto& call = item.at("draws")[draw];
            const bool indexed = scenario == "timing-indexed" || (scenario == "timing-reuse" && draw % 2 == 1);
            require(call.at("ordinal") == draw && call.at("command") == (indexed ? "vkCmdDrawIndexed" : "vkCmdDraw") &&
                        call.at("marker_path").size() == 5,
                    "actual direct/indexed call and nested marker path");
            require(item.at("markers").at(call.at("marker_ordinal").get<std::size_t>()).at("path") ==
                        call.at("marker_path"),
                    "draw references an actual recorded begin-label occurrence");
        }
        require(item.at("draws")[1].at("marker_ordinal") != item.at("draws")[2].at("marker_ordinal") &&
                    item.at("draws")[4].at("marker_ordinal") == item.at("draws")[5].at("marker_ordinal"),
                "repeated sibling markers are distinct; a two-draw marker stays one occurrence");
        ++ordinal;
    }
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 7, "SERVER FIXTURE SHADERS NSIGHT_ROOT OUTPUT_PARENT LAUNCH_ENV_JSON required");
        const auto server = fs::canonical(argv[1]), fixture = fs::canonical(argv[2]), shaders = fs::canonical(argv[3]);
        const auto nsight = fs::canonical(argv[4]), parent = fs::absolute(argv[5]);
        const auto overlay = load(fs::canonical(argv[6])).get<ngm::LaunchEnvironment>();
        const auto selection = ngm::validate_launch_environment(overlay);
        fs::create_directories(parent);
        auto pattern = (parent / "profile-correlation-XXXXXX").string();
        require(mkdtemp(pattern.data()), "unique correlation validation directory");
        const fs::path run(pattern);
        fs::copy_file(__FILE__, run / "ProfileCorrelationIntegration.cpp");
        fs::create_directories(run / "native-witnesses");
        const auto fixture_hash = ngm::sha256_file(fixture);
        Json report{{"status", "running"},
                    {"scope", "Collection/feasibility matrix and independent private timestamp witnesses; public "
                              "correlation, counter scopes and timing sanity remain a separate export audit"},
                    {"project_version", ngm::project_version()},
                    {"server_sha256", ngm::sha256_file(server)},
                    {"fixture_sha256", fixture_hash},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"shader_provenance", load(shaders / "provenance.json")},
                    {"launch_environment", overlay},
                    {"launch_selection", selection},
                    {"desktop_environment", ngm::capture_environment()},
                    {"timestamp_sanity_policy",
                     {{"relative_tolerance", 0.25},
                      {"absolute_tolerance_ms", 0.025},
                      {"scope", "whole scene envelope; same profile only"}}},
                    {"baselines", Json::array()},
                    {"profiles", Json::array()}};
        save(run / "report.json", report);
        std::cout << run << '\n' << std::flush;
        try {
            for(const bool validation : {true, false})
                for(const auto* scenario : {"timing-direct", "timing-indexed", "timing-reuse"}) {
                    const auto directory = run / (std::string(validation ? "validation-" : "baseline-") + scenario);
                    fs::create_directory(directory);
                    auto environment = ngm::capture_environment();
                    for(const auto& [key, value] : overlay)
                        environment[key] = value;
                    for(const auto* key :
                        {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "TMPDIR"}) {
                        const auto path = directory / "config" / key;
                        fs::create_directories(path);
                        environment[key] = path.string();
                    }
                    if(validation) {
                        environment["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation";
                        environment["VK_LAYER_SETTINGS_PATH"] = (directory / "config").string();
                        std::ofstream settings(directory / "config/vk_layer_settings.txt");
                        settings << "khronos_validation.validate_sync = true\n"
                                    "khronos_validation.report_flags = error,warn,info\n"
                                    "khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG\n";
                        settings.close();
                        require(static_cast<bool>(settings), "write isolated synchronization validation settings");
                    }
                    ngm::ProcessOptions process;
                    process.executable = fixture;
                    process.working_directory = directory;
                    process.arguments = {"--scenario",       scenario,
                                         "--seed",           "42",
                                         "--width",          "1024",
                                         "--height",         "512",
                                         "--frame",          validation ? "3" : "11",
                                         "--shader-dir",     shaders.string(),
                                         "--output",         (directory / "application").string(),
                                         "--timing-witness", (directory / "witness.json").string()};
                    process.environment = environment;
                    process.stdout_path = directory / "stdout.log";
                    process.stderr_path = directory / "stderr.log";
                    process.timeout = 60s;
                    const auto result = ngm::run_process(process);
                    report["baselines"].push_back(
                        {{"directory", directory.string()},
                         {"scenario", scenario},
                         {"validation", validation},
                         {"arguments", process.arguments},
                         {"environment", environment},
                         {"exit_code", result.exit_code ? Json(*result.exit_code) : Json(nullptr)},
                         {"cleanup_confirmed", result.cleanup_confirmed},
                         {"timed_out", result.timed_out},
                         {"error", result.error}});
                    save(run / "report.json", report);
                    require(result.exit_code == 0 && result.cleanup_confirmed && !result.timed_out,
                            "fresh timing fixture baseline; inspect retained logs before retrying");
                    auto& item = report["baselines"].back();
                    item["application_result"] = load(directory / "application/result.json");
                    require(item["application_result"]["status"] == "pass", "baseline application result");
                    const auto witness = load(directory / "witness.json");
                    verify_witness(witness, scenario, fixture_hash);
                    item["witness"] = witness;
                    if(validation) {
                        const auto logs = ngm::read_regular_file(process.stdout_path, 16U * 1024U * 1024U) +
                                          ngm::read_regular_file(process.stderr_path, 16U * 1024U * 1024U);
                        const bool clean = logs.find("CURRENT-VALIDATION-ENABLED") != std::string::npos &&
                                           logs.find("  - Synchronization") != std::string::npos &&
                                           logs.find("Validation Error") == std::string::npos &&
                                           logs.find("SYNC-HAZARD") == std::string::npos &&
                                           logs.find("VUID-") == std::string::npos;
                        item["synchronization_validation_active_and_clean"] = clean;
                        save(run / "report.json", report);
                        require(clean, "synchronization validation active/clean");
                    }
                    save(run / "report.json", report);
                }
            std::vector<std::string> desktop;
            for(const auto& [key, value] : ngm::capture_environment())
                if(key != "PATH")
                    desktop.push_back(key + '=' + value);
            std::ofstream transcript(run / "transcript.ndjson");
            transcript.exceptions(std::ios::badbit | std::ios::failbit);
            for(unsigned pass = 0; pass < 2; ++pass) {
                ngm::check::mcp::Client client(
                    server, "/usr/bin:/bin",
                    {"--artifact-root", (run / "store").string(), "--nsight-root", nsight.string()}, false, desktop,
                    {30s, 30s});
                int sequence = 1;
                const auto request = [&](const char* method, const Json& parameters) {
                    transcript << Json{{"pass", pass},
                                       {"request", {{"id", sequence}, {"method", method}, {"params", parameters}}}}
                                      .dump()
                               << '\n';
                    const auto response = client.request(sequence++, method, parameters);
                    transcript << Json{{"pass", pass}, {"response", response}}.dump() << '\n';
                    transcript.flush();
                    require(response.contains("result"), "correlation protocol result");
                    return response.at("result");
                };
                const auto call = [&](const char* name, const Json& arguments) {
                    const auto result = request("tools/call", {{"name", name}, {"arguments", arguments}});
                    require(!result.value("isError", false) && result.contains("structuredContent"),
                            "correlation tool: " + result.dump());
                    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                                result.at("structuredContent"),
                            "MCP result text/structured parity");
                    return result.at("structuredContent");
                };
                require(request("initialize", {{"protocolVersion", "2025-06-18"},
                                               {"capabilities", Json::object()},
                                               {"clientInfo",
                                                {{"name", "ngm-profile-correlation-integration"},
                                                 {"version", ngm::project_version()}}}})
                                .at("serverInfo")
                                .at("version") == ngm::project_version(),
                        "source-built server version");
                client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
                if(pass == 0) {
                    report["tools"] = request("tools/list", Json::object());
                    for(const auto* scenario : {"timing-direct", "timing-indexed", "timing-reuse"})
                        for(unsigned collection = 0; collection < 5; ++collection) {
                            const bool single = collection == 4, action = collection % 2 == 1 || single;
                            const std::string delimiter = collection < 2 || single ? "frames" : "submits";
                            const auto key = std::string(scenario) + '-' + std::to_string(collection);
                            const auto witness_path = run / "native-witnesses" / (key + ".json");
                            const Json arguments{{"executable", fixture.string()},
                                                 {"working_directory", run.string()},
                                                 {"arguments",
                                                  {"--scenario", scenario, "--seed", "42", "--width", "1024",
                                                   "--height", "512", "--frame", "300", "--shader-dir",
                                                   shaders.string(), "--timing-witness", witness_path.string()}},
                                                 {"application_output_option", "--output"},
                                                 {"launch_environment", overlay},
                                                 {"pin", true},
                                                 {"timeout_ms", 120000},
                                                 {"settings",
                                                  {{"architecture", "Ampere GA10x"},
                                                   {"delimiter", delimiter},
                                                   {"start_after", 5},
                                                   {"limit", single ? 1 : 3},
                                                   {"duration_ms", 1000},
                                                   {"time_every_action", action},
                                                   {"hes_enabled", false}}}};
                            const auto submitted = call("profile", arguments);
                            report["profiles"].push_back(
                                {{"key", key}, {"request", arguments}, {"submission", submitted}});
                            save(run / "report.json", report);
                            Json state;
                            const auto deadline = std::chrono::steady_clock::now() + 150s;
                            for(;;) {
                                state = call("job_status", {{"job_id", submitted.at("identity").at("job_id")}});
                                if(state.at("state") != "queued" && state.at("state") != "running" &&
                                   state.at("finalization_pending") == false)
                                    break;
                                require(std::chrono::steady_clock::now() < deadline, "bounded correlation profile job");
                                std::this_thread::sleep_for(100ms);
                            }
                            auto& item = report["profiles"].back();
                            item["job"] = state;
                            const auto id = submitted.at("artifact_id");
                            item["artifact_info"] = call("artifact_info", {{"artifact_id", id}});
                            item["capture_report"] =
                                load(run / "store/bundles" / id.get<std::string>() / "raw/report.json");
                            save(run / "report.json", report);
                            require(state.at("state") == "succeeded" && state.at("cleanup_confirmed") == true &&
                                        state.at("gpu_reserved") == false && state.at("worker_running") == false &&
                                        item["artifact_info"]["pinned"] == true,
                                    "successful native collection, cleanup and source pin; inspect failure before "
                                    "retrying");
                            item["metadata"] = call("profile_metadata", {{"profile_id", id}});
                            item["events"] = call("profile_metrics",
                                                  {{"profile_id", id}, {"table", "event_durations"}, {"limit", 100}});
                            require(item["events"]["next_offset"].is_null(),
                                    "complete bounded marker export fits audit page");
                            item["witness"] = load(witness_path);
                            verify_witness(item["witness"], scenario, fixture_hash);
                            save(run / "report.json", report);
                            std::cout << key << ' ' << id << '\n' << std::flush;
                        }
                } else
                    for(const auto& item : report.at("profiles")) {
                        const auto id = item.at("submission").at("artifact_id");
                        require(call("profile_metadata", {{"profile_id", id}}) == item.at("metadata"),
                                "identical offline metadata");
                        require(call("profile_metrics",
                                     {{"profile_id", id}, {"table", "event_durations"}, {"limit", 100}}) ==
                                    item.at("events"),
                                "identical offline marker export");
                        require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                                "independent source pins survive restart");
                    }
                require(client.finish() == 0, "correlation server exits cleanly");
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
