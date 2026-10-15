#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/DrawState.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << value.dump(2) << '\n';
}
Json load(const fs::path& path) {
    return Json::parse(ngm::read_regular_file(path, 4U * 1024U * 1024U));
}
void inspect_retained(const fs::path& run) {
    const auto report = load(run / "report.json");
    Json results{
        {"status", "running"},
        {"project_version", ngm::project_version()},
        {"scope", "Pure recorded-state interpretation versus private fixture API inputs; not observed GPU event state"},
        {"cases", Json::array()}};
    save(run / "recorded-state.json", results);
    for(const auto& record : report.at("cases")) {
        std::vector<ngm::CppSource> sources;
        const auto bundle = run / "store/bundles" / record.at("submission").at("artifact_id").get<std::string>();
        for(const auto& value : record.at("index").at("source_files")) {
            const auto path = value.get<std::string>(), name = fs::path(path).filename().string();
            if(name.starts_with("CommandList") || name.starts_with("Resources") || name.starts_with("Frame"))
                sources.push_back({path, ngm::read_regular_file(bundle / path, 4U * 1024U * 1024U)});
        }
        const auto parsed = ngm::inspect_draw_executions(sources);
        results["cases"].push_back({{"scenario", record.at("scenario")}, {"parsed", parsed}});
        save(run / "recorded-state.json", results);
        require(parsed.at("executions").size() == 1, "one recorded submitted draw: " + parsed.at("unsupported").dump());
        require(parsed.at("captured_gpu_execution_correspondence") == false &&
                    parsed.at("full_effective_state_coverage") == false,
                "recorded-state scope stays explicit");
        const auto& draw = parsed.at("executions")[0];
        const auto& witness = record.at("witness");
        const auto label = draw.at("labels")[0];
        const auto submitted = std::find_if(witness.at("submissions").begin(), witness.at("submissions").end(),
                                            [&](const auto& value) { return value.at("label") == label; });
        require(submitted != witness.at("submissions").end(),
                "capture label selects its independent successful submission witness");
        Json expected = witness.at("pipeline_api_inputs");
        require(submitted->at("draw").at("bound_pipeline_handle") == expected.at("pipeline_handle") &&
                    submitted->at("draw").at("active_render_pass_handle") == expected.at("render_pass_handle") &&
                    expected.at("pipeline_handle").get<std::uint64_t>() != 0 && expected.at("subpass") == 0,
                "independent successful creation/bind/render-pass API handle association");
        for(const auto& [key, value] : submitted->at("draw").at("setters").items())
            expected[key] = value;
        const auto& state = draw.at("state");
        for(const auto* key : {"pipeline", "shaders", "depth_test", "depth_write", "depth_compare", "cull_mode",
                               "front_face", "viewports", "scissors"})
            require(state.at(key).at("status") == "qualified_recorded_state",
                    std::string(key) + " available: " + state.at(key).dump());
        require(state.at("depth_test").at("value") == Json(expected.at("depthTestEnable").get<unsigned>() == 1) &&
                    state.at("depth_write").at("value") == Json(expected.at("depthWriteEnable").get<unsigned>() == 1),
                "depth API enable values");
        const std::map<std::string, unsigned> compare{{"VK_COMPARE_OP_LESS", 1}, {"VK_COMPARE_OP_GREATER", 4}};
        const std::map<std::string, unsigned> cull{{"VK_CULL_MODE_NONE", 0}, {"VK_CULL_MODE_FRONT_BIT", 1}};
        const std::map<std::string, unsigned> front{{"VK_FRONT_FACE_COUNTER_CLOCKWISE", 0},
                                                    {"VK_FRONT_FACE_CLOCKWISE", 1}};
        require(compare.at(state.at("depth_compare").at("value")) == expected.at("depthCompareOp") &&
                    cull.at(state.at("cull_mode").at("value")) == expected.at("cullMode") &&
                    front.at(state.at("front_face").at("value")) == expected.at("frontFace"),
                "depth/culling enum API values");
        require(state.at("viewports").at("value") == Json::array({expected.at("viewport")}) &&
                    state.at("scissors").at("value") == Json::array({expected.at("scissor")}),
                "viewport/scissor API arrays");
        require(draw.at("function") ==
                    "VulkanReplay_" + submitted->at("draw").at("command").get<std::string>().substr(2),
                "indexed/direct API draw command");
        require(state.at("shaders").at("value").size() == 2 &&
                    state.at("uniform_material_values").at("status") == "unavailable",
                "two module identities and explicit execution-value limitation");
    }
    results["status"] = "pass";
    save(run / "recorded-state.json", results);
}
std::string hex(const std::string& bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for(const auto value : bytes) {
        result += digits[static_cast<unsigned char>(value) >> 4];
        result += digits[static_cast<unsigned char>(value) & 15];
    }
    return result;
}
void verify_mcp(const fs::path& server, const fs::path& worker, const fs::path& run) {
    inspect_retained(run);
    const auto captured = load(run / "report.json");
    const auto pure = load(run / "recorded-state.json");
    Json report{{"status", "running"},
                {"project_version", ngm::project_version()},
                {"scope", "Retained fresh fixture captures, actual MCP recorded-state queries and exact serialized "
                          "shader reads; no GPU event observation"},
                {"server_sha256", ngm::sha256_file(server)},
                {"worker_sha256", ngm::sha256_file(worker)},
                {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                {"passes", Json::array()}};
    fs::copy_file(__FILE__, run / "DrawStateQueryIntegration.cpp", fs::copy_options::overwrite_existing);
    std::ofstream transcript(run / "state-query-transcript.ndjson");
    transcript.exceptions(std::ios::failbit | std::ios::badbit);
    std::vector<std::string> read_bundles;
    Json first;
    for(unsigned pass = 0; pass < 2; ++pass) {
        ngm::check::mcp::Client client(
            server, "/usr/bin:/bin",
            {"--artifact-root", (run / "store").string(), "--resource-worker-2026-3", worker.string()}, false, {},
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
            require(response.contains("result"), "MCP protocol result");
            return response.at("result");
        };
        const auto call = [&](const char* tool, const Json& args) {
            const auto result = request("tools/call", {{"name", tool}, {"arguments", args}});
            require(!result.value("isError", false) && result.contains("structuredContent"),
                    "state/reader MCP call: " + result.dump());
            require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                        result.at("structuredContent"),
                    "MCP text/structured parity");
            return result.at("structuredContent");
        };
        const auto initialized =
            request("initialize",
                    {{"protocolVersion", "2025-06-18"},
                     {"capabilities", Json::object()},
                     {"clientInfo", {{"name", "ngm-draw-state-integration"}, {"version", ngm::project_version()}}}});
        require(initialized.at("serverInfo").at("version") == ngm::project_version(), "current server version");
        client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        if(pass == 0)
            report["tools"] = request("tools/list", Json::object());
        Json cases = Json::array();
        for(std::size_t at = 0; at < captured.at("cases").size(); ++at) {
            const auto& record = captured.at("cases")[at];
            const auto id = record.at("submission").at("artifact_id");
            const auto listed = call("capture_draw_executions", {{"capture_id", id}, {"limit", 1}});
            require(listed.at("executions_total") == 1 && listed.at("unsupported_total") == 0 &&
                        listed.at("next_offset").is_null(),
                    "one qualified fresh draw");
            const auto ref = listed.at("executions")[0].at("execution_ref");
            const auto selected = call("capture_draw_state", {{"capture_id", id}, {"execution_ref", ref}});
            const auto& state = selected.at("execution").at("state");
            const auto& expected = pure.at("cases")[at].at("parsed").at("executions")[0].at("state");
            for(const auto* key : {"pipeline", "depth_test", "depth_write", "depth_compare", "cull_mode", "front_face",
                                   "viewports", "scissors"})
                require(state.at(key) == expected.at(key), "MCP fields match independently witnessed pure result");
            require(selected.at("captured_gpu_execution_correspondence") == false &&
                        selected.at("full_effective_state_coverage") == false &&
                        selected.at("selection_domain") == "generated_cpp_recorded_submission",
                    "MCP scope is explicit");
            require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true, "source capture pin");
            Json item{{"capture_id", id}, {"listing", listed}, {"selected", selected}};
            if(pass == 0) {
                item["shader_reads"] = Json::array();
                for(const auto& stage : state.at("shaders").at("value")) {
                    const auto resource_ref = stage.at("serialized_byte_read").at("resource_ref");
                    require(resource_ref.is_string(), "unique shader reference is linked to the reader");
                    const auto bytes = stage.at("declared_bytes").get<std::size_t>();
                    require(bytes > 0 && bytes <= 65536, "fixture module fits one bounded whole-resource read");
                    const auto read =
                        call("capture_cpp_resource",
                             {{"capture_id", id}, {"resource_ref", resource_ref}, {"length", bytes}, {"pin", true}});
                    const auto filename =
                        stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? "scene.vert.spv" : "scene.frag.spv";
                    const auto oracle_path =
                        run / "store/bundles" / id.get<std::string>() / "raw/application/shaders" / filename;
                    const auto oracle = ngm::read_regular_file(oracle_path, 65536);
                    const auto& witness_stages = record.at("witness").at("pipeline_api_inputs").at("stages");
                    const auto witness_stage =
                        std::find_if(witness_stages.begin(), witness_stages.end(), [&](const auto& value) {
                            return value.at("stage") == (stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? 1u : 16u);
                        });
                    require(witness_stage != witness_stages.end() &&
                                witness_stage->at("sha256") == ngm::sha256_file(oracle_path) &&
                                witness_stage->at("bytes") == oracle.size() && witness_stage->at("flags") == 0 &&
                                witness_stage->at("entry_point") == "main",
                            "actual shader-module API bytes and stage inputs match the independent retained oracle");
                    require(read.at("encoding") == "hex" && read.at("data") == hex(oracle) &&
                                read.at("total_bytes") == oracle.size() && read.at("returned_bytes") == oracle.size() &&
                                read.at("next_offset").is_null() && read.at("sha256") == ngm::sha256_file(oracle_path),
                            "exact independent shader bytes/hash");
                    for(const auto* name : {"data.bin", "data.bin.rec"}) {
                        const auto path = record.at("index").at("project_directory").get<std::string>() + '/' + name;
                        const auto& anchors = selected.at("input_files");
                        const auto found = std::find_if(anchors.begin(), anchors.end(),
                                                        [&](const auto& input) { return input.at("path") == path; });
                        require(found != anchors.end() &&
                                    found->at("sha256") == read.at("inputs").at(name).at("sha256"),
                                "reader snapshot matches capture-time selected-state payload identity");
                    }
                    const auto read_id = read.at("artifact_id").get<std::string>();
                    require(call("artifact_info", {{"artifact_id", read_id}}).at("pinned") == true, "shader read pin");
                    read_bundles.push_back(read_id);
                    item["shader_reads"].push_back(read);
                }
            } else {
                require(listed == first[at].at("listing") && selected == first[at].at("selected"),
                        "reference/value/provenance stability after restart");
            }
            const auto bad =
                request("tools/call", {{"name", "capture_draw_state"},
                                       {"arguments", {{"capture_id", id}, {"execution_ref", std::string(64, '0')}}}});
            require(bad.value("isError", false), "foreign/stale reference refuses");
            cases.push_back(std::move(item));
        }
        if(pass == 0)
            first = cases;
        else
            for(const auto& id : read_bundles)
                require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                        "read evidence remains pinned after restart");
        require(client.finish() == 0, "MCP server exits cleanly");
        report["passes"].push_back(std::move(cases));
        save(run / "state-query-report.json", report);
    }
    report["status"] = "pass";
    save(run / "state-query-report.json", report);
}
void verify_push_mcp(const fs::path& server, const fs::path& worker, const fs::path& run) {
    const auto captured = load(run / "report.json");
    require(captured.at("status") == "pass" && captured.at("cases").size() == 2, "two fresh push captures");
    Json report{{"status", "running"},
                {"project_version", ngm::project_version()},
                {"scope", "Recorded opaque push bytes versus private API witnesses; no GPU-observed event state or "
                          "typed material values"},
                {"server_sha256", ngm::sha256_file(server)},
                {"worker_sha256", ngm::sha256_file(worker)},
                {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                {"passes", Json::array()}};
    fs::copy_file(__FILE__, run / "PushStateQueryIntegration.cpp", fs::copy_options::overwrite_existing);
    std::ofstream transcript(run / "push-query-transcript.ndjson");
    transcript.exceptions(std::ios::failbit | std::ios::badbit);
    std::vector<std::string> read_artifacts;
    Json original;
    for(unsigned pass = 0; pass < 2; ++pass) {
        ngm::check::mcp::Client client(
            server, "/usr/bin:/bin",
            {"--artifact-root", (run / "store").string(), "--resource-worker-2026-3", worker.string()}, false, {},
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
            require(response.contains("result"), "MCP protocol response");
            return response.at("result");
        };
        const auto call = [&](const char* tool, const Json& args) {
            const auto response = request("tools/call", {{"name", tool}, {"arguments", args}});
            require(!response.value("isError", false) && response.contains("structuredContent"),
                    "push MCP call: " + response.dump());
            require(Json::parse(response.at("content")[0].at("text").get<std::string>()) ==
                        response.at("structuredContent"),
                    "push text/structured parity");
            return response.at("structuredContent");
        };
        const auto initialized =
            request("initialize",
                    {{"protocolVersion", "2025-06-18"},
                     {"capabilities", Json::object()},
                     {"clientInfo", {{"name", "ngm-push-state-integration"}, {"version", ngm::project_version()}}}});
        require(initialized.at("serverInfo").at("version") == ngm::project_version(), "current push server identity");
        client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        if(pass == 0)
            report["tools"] = request("tools/list", Json::object());
        Json cases = Json::array();
        std::set<std::string> execution_refs;
        for(std::size_t case_at = 0; case_at < captured.at("cases").size(); ++case_at) {
            const auto& source = captured.at("cases")[case_at];
            const auto id = source.at("submission").at("artifact_id");
            Json pages = Json::array(), executions = Json::array();
            std::size_t offset = 0;
            for(;;) {
                const auto page =
                    call("capture_draw_executions", {{"capture_id", id}, {"offset", offset}, {"limit", 2}});
                require(page.at("unsupported_total") == 0, "qualified push source grammar");
                pages.push_back(page);
                for(const auto& row : page.at("executions"))
                    executions.push_back(row);
                if(page.at("next_offset").is_null())
                    break;
                offset = page.at("next_offset").get<std::size_t>();
            }
            require(executions.size() == (case_at == 0 ? 3 : 6),
                    "three draws per recorded submission, including reuse");
            Json item{{"capture_id", id}, {"pages", pages}, {"draws", Json::array()}};
            for(std::size_t draw_at = 0; draw_at < executions.size(); ++draw_at) {
                const auto& execution = executions[draw_at];
                const auto ref = execution.at("execution_ref");
                require(execution_refs.insert(ref.get<std::string>()).second,
                        "each capture/submission/draw occurrence has a distinct reference");
                const auto usage = call("artifact_usage", Json::object());
                const auto state = call("capture_draw_state", {{"capture_id", id}, {"execution_ref", ref}});
                require(state.at("execution").at("selection") == execution.at("selection"),
                        "selected reference preserves its exact recorded occurrence");
                require(call("artifact_usage", Json::object()).at("completed_bytes") == usage.at("completed_bytes"),
                        "default query retains no byte snapshots");
                const auto& push = state.at("execution").at("state").at("push_constants");
                require(push.at("status") == "unavailable" && push.at("value").is_null() &&
                            push.at("coverage").at("input_coverage_complete") == true,
                        "known writes remain explicitly unread by default");
                const auto& label = execution.at("labels")[0];
                const auto occurrence = execution.at("selection").at("submission_index");
                const auto& submissions = source.at("witness").at("submissions");
                const auto submitted = std::find_if(submissions.begin(), submissions.end(), [&](const auto& entry) {
                    return entry.at("label") == label && entry.at("submission_occurrence") == occurrence;
                });
                require(submitted != submissions.end(),
                        "selected recorded occurrence matches private successful submission");
                const auto& expected_draws = submitted->at("constant_draws");
                const auto expected = std::find_if(expected_draws.begin(), expected_draws.end(), [&](const auto& row) {
                    return row.at("label") == execution.at("labels").back();
                });
                require(expected != expected_draws.end(), "unique nested draw label in private API witness");
                const auto& pipelines = source.at("witness").at("pipelines");
                const auto pipeline = std::find_if(pipelines.begin(), pipelines.end(), [&](const auto& row) {
                    return row.at("pipeline_handle") == expected->at("pipeline_handle");
                });
                require(pipeline != pipelines.end() && pipeline->at("layout_handle") == expected->at("layout_handle") &&
                            pipeline->at("push_size") == 16,
                        "private pipeline/layout creation and bind association");
                Json checked{{"execution_ref", ref}, {"default", state}, {"witness", *expected}};
                if(pass == 0 || draw_at == 0) {
                    const auto bytes = call(
                        "capture_draw_state",
                        {{"capture_id", id}, {"execution_ref", ref}, {"read_push_constants", true}, {"pin", true}});
                    const auto& resolved = bytes.at("execution").at("state").at("push_constants");
                    require(resolved.at("status") == "qualified_recorded_state" && resolved.at("value").size() == 1 &&
                                resolved.at("value")[0].at("stage") == "VK_SHADER_STAGE_FRAGMENT_BIT" &&
                                resolved.at("value")[0].at("data") == expected->at("data") &&
                                resolved.at("value")[0].at("offset") == expected->at("offset") &&
                                resolved.at("value")[0].at("length") == expected->at("length"),
                            "exact reconstructed per-stage byte bank matches private API writes");
                    require(bytes.at("captured_gpu_execution_correspondence") == false &&
                                bytes.at("full_effective_state_coverage") == false,
                            "push reconstruction does not claim GPU event observation");
                    for(const auto& read_id : resolved.at("read_artifacts")) {
                        require(call("artifact_info", {{"artifact_id", read_id}}).at("pinned") == true,
                                "push read pin");
                        read_artifacts.push_back(read_id.get<std::string>());
                    }
                    checked["resolved"] = bytes;
                }
                if(pass != 0)
                    require(state == original[case_at].at("draws")[draw_at].at("default"),
                            "selection/coverage/provenance survive server restart");
                item["draws"].push_back(std::move(checked));
            }
            require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true, "source capture remains pinned");
            if(pass == 0) {
                const auto& stages =
                    item.at("draws")[0].at("default").at("execution").at("state").at("shaders").at("value");
                const auto fragment = std::find_if(stages.begin(), stages.end(), [](const auto& stage) {
                    return stage.at("stage") == "VK_SHADER_STAGE_FRAGMENT_BIT";
                });
                require(fragment != stages.end(), "push fixture fragment declaration");
                const auto shader = call("capture_cpp_resource",
                                         {{"capture_id", id},
                                          {"resource_ref", fragment->at("serialized_byte_read").at("resource_ref")},
                                          {"length", fragment->at("declared_bytes")},
                                          {"pin", true}});
                const auto oracle =
                    run / "store/bundles" / id.get<std::string>() / "raw/application/shaders/state-constants.frag.spv";
                require(shader.at("data") == hex(ngm::read_regular_file(oracle, 65536)) &&
                            shader.at("sha256") == ngm::sha256_file(oracle),
                        "exact independently retained push-consuming shader");
                item["fragment_read"] = shader;
                read_artifacts.push_back(shader.at("artifact_id").get<std::string>());
            }
            cases.push_back(std::move(item));
        }
        if(pass == 0)
            original = cases;
        else
            for(const auto& id : read_artifacts)
                require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                        "all read pins survive restart");
        require(client.finish() == 0, "push validation server clean exit");
        report["passes"].push_back(std::move(cases));
        save(run / "push-query-report.json", report);
    }
    report["status"] = "pass";
    save(run / "push-query-report.json", report);
}
} // namespace

#include "DescriptorStateIntegration.hpp"

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        if(argc == 4 && std::string_view(argv[1]) == "--verify-descriptors") {
            verify_descriptors_mcp(fs::canonical(argv[2]), fs::canonical(argv[3]));
            return;
        }
        if(argc == 3 && std::string_view(argv[1]) == "--inspect-retained") {
            inspect_retained(fs::canonical(argv[2]));
            return;
        }
        if(argc == 5 && std::string_view(argv[1]) == "--verify-mcp") {
            verify_mcp(fs::canonical(argv[2]), fs::canonical(argv[3]), fs::canonical(argv[4]));
            return;
        }
        if(argc == 5 && std::string_view(argv[1]) == "--verify-push") {
            verify_push_mcp(fs::canonical(argv[2]), fs::canonical(argv[3]), fs::canonical(argv[4]));
            return;
        }
        const bool constants = argc == 7 && std::string_view(argv[6]) == "--constants";
        const bool descriptors = argc == 7 && std::string_view(argv[6]) == "--descriptors";
        require(argc == 6 || constants || descriptors,
                "FIXTURE SHADERS NSIGHT_ROOT OUTPUT_ROOT LAUNCH_ENVIRONMENT_JSON [--constants|--descriptors] required");
        const auto fixture = fs::canonical(argv[1]), shaders = fs::canonical(argv[2]);
        const auto nsight = fs::canonical(argv[3]), output = fs::absolute(argv[4]);
        const auto overlay = load(fs::canonical(argv[5])).get<ngm::LaunchEnvironment>();
        const auto selection = ngm::validate_launch_environment(overlay);
        fs::create_directories(output);
        auto pattern = (output / "draw-state-XXXXXX").string();
        require(mkdtemp(pattern.data()), "unique state validation run");
        const fs::path run(pattern);
        fs::copy_file(__FILE__, run / "DrawStateIntegration.cpp");
        Json report{{"status", "running"},
                    {"project_version", ngm::project_version()},
                    {"evidence_origin", "fixture_cpp_capture_probe"},
                    {"state_query_qualification", false},
                    {"fixture", {{"path", fixture.string()}, {"sha256", ngm::sha256_file(fixture)}}},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"shaders", load(shaders / "provenance.json")},
                    {"launch_environment", overlay},
                    {"selection", selection},
                    {"desktop", ngm::capture_environment()},
                    {"cases", Json::array()}};
        save(run / "report.json", report);
        std::cout << run << '\n' << std::flush;
        ngm::CaptureServiceOptions options;
        options.artifacts.root = run / "store";
        options.nsight_root = nsight;
        options.environment = ngm::capture_environment();
        ngm::CaptureService service(options);
        try {
            const std::vector<const char*> scenarios =
                descriptors ? std::vector<const char*>{"state-descriptors", "state-descriptors-reuse"}
                : constants ? std::vector<const char*>{"state-constants", "state-constants-reuse"}
                            : std::vector<const char*>{"state-static", "state-dynamic", "state-indexed", "state-count"};
            for(const auto* scenario : scenarios) {
                const auto directory = run / scenario;
                fs::create_directory(directory);
                std::vector<std::string> arguments{"--scenario", scenario, "--seed",       "42",
                                                   "--width",    "192",    "--height",     "128",
                                                   "--frame",    "20",     "--shader-dir", shaders.string()};
                auto environment = options.environment;
                for(const auto& [key, value] : overlay)
                    environment[key] = value;
                for(const auto* key :
                    {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME", "TMPDIR"}) {
                    const auto path = directory / "baseline-config" / key;
                    fs::create_directories(path);
                    environment[key] = path.string();
                }
                ngm::ProcessOptions baseline;
                baseline.executable = fixture;
                baseline.working_directory = directory;
                baseline.arguments = arguments;
                baseline.arguments.insert(baseline.arguments.end(),
                                          {"--output", (directory / "baseline").string(), "--state-witness",
                                           (directory / "baseline-witness.json").string()});
                baseline.environment = environment;
                baseline.stdout_path = directory / "baseline.stdout";
                baseline.stderr_path = directory / "baseline.stderr";
                baseline.timeout = 30s;
                const auto result = ngm::run_process(baseline);
                Json item{{"scenario", scenario},
                          {"baseline",
                           {{"arguments", baseline.arguments},
                            {"environment", environment},
                            {"exit_code", result.exit_code ? Json(*result.exit_code) : Json(nullptr)},
                            {"cleanup_confirmed", result.cleanup_confirmed},
                            {"timed_out", result.timed_out},
                            {"error", result.error}}}};
                report["cases"].push_back(item);
                save(run / "report.json", report);
                require(result.exit_code == 0 && result.cleanup_confirmed && !result.timed_out,
                        "independent state baseline");
                auto& record = report["cases"].back();
                record["baseline"]["result"] = load(directory / "baseline/result.json");
                require(record.at("baseline").at("result").at("status") == "pass", "baseline result");
                ngm::CaptureRequest request;
                request.executable = fixture;
                request.arguments = arguments;
                request.arguments.insert(request.arguments.end(),
                                         {"--state-witness", (directory / "capture-witness.json").string()});
                request.working_directory = directory;
                request.format = ngm::CaptureFormat::Cpp;
                request.cpp_wait_frames = 2;
                request.timeout = 120s;
                request.pin = true;
                request.application_output_option = "--output";
                request.launch_environment = overlay;
                const auto submitted = service.capture(request);
                record["submission"] = submitted;
                save(run / "report.json", report);
                const auto deadline = std::chrono::steady_clock::now() + 150s;
                for(;;) {
                    const auto state = service.wait(submitted.identity.job_id, 250ms);
                    require(state.has_value(), "owned state job exists");
                    record["job"] = *state;
                    save(run / "report.json", report);
                    if(ngm::job_terminal(state->state) && !state->worker_running && !state->gpu_reserved &&
                       !state->finalization_pending)
                        break;
                    require(std::chrono::steady_clock::now() < deadline, "state capture polling deadline");
                }
                const auto id = submitted.artifact_id;
                service.artifacts().pin(id, true);
                record["capture_report"] =
                    Json::parse(service.artifacts().read(id, "raw/report.json", 4U * 1024U * 1024U));
                save(run / "report.json", report);
                require(record.at("job").at("state") == "succeeded", "fresh state C++ capture");
                record["index"] =
                    Json::parse(service.artifacts().read(id, "derived/cpp-project.json", 4U * 1024U * 1024U));
                record["witness"] = load(directory / "capture-witness.json");
                require(record.at("witness").at("submissions").size() >= 3,
                        "captured frame has a submitted API witness");
                save(run / "report.json", report);
            }
            report["status"] = "pass";
            report["scope"] = "Fresh documented C++ captures and private API witnesses; does not qualify a state query";
        } catch(...) {
            report["status"] = "failed";
            save(run / "report.json", report);
            throw;
        }
        save(run / "report.json", report);
    });
}
