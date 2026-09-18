#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/Artifacts.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using ngm::check::mcp::Client;
using namespace std::chrono_literals;

void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream << value.dump(2) << '\n';
    stream.close();
}

class Session {
public:
    Session(const fs::path& server, const fs::path& payload, const std::string& name,
            const std::vector<std::string>& arguments, const std::vector<std::string>& environment) :
        client_(server.string(), "/usr/bin:/bin", arguments, false, environment, {30s, 30s}),
        transcript_(payload / (name + ".ndjson")), diagnostics_(payload / (name + ".stderr.txt")),
        shutdown_(payload / (name + ".shutdown.json")) {
        transcript_.exceptions(std::ios::badbit | std::ios::failbit);
    }
    ~Session() {
        if(!closed_) {
            try {
                close();
            } catch(...) {
                // close() retains diagnostics/outcome before propagating;
                // Client retains its bounded process-cleanup fallback.
            }
        }
    }
    void initialize() {
        const auto reply =
            request("initialize",
                    {{"protocolVersion", "2025-11-25"},
                     {"capabilities", Json::object()},
                     {"clientInfo", {{"name", "ngm-source-repair-integration"}, {"version", ngm::project_version()}}}});
        require(reply.at("result").at("serverInfo").at("version") == ngm::project_version(),
                "matching server identity");
        client_.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    }
    Json request(const char* method, const Json& params) {
        const auto id = next_id_++;
        transcript_ << Json{{"direction", "request"}, {"id", id}, {"method", method}, {"params", params}}.dump()
                    << '\n';
        transcript_.flush();
        auto response = client_.request(id, method, params);
        transcript_ << Json{{"direction", "response"}, {"message", response}}.dump() << '\n';
        transcript_.flush();
        return response;
    }
    Json tool(const char* name, const Json& arguments = Json::object()) {
        const auto reply = request("tools/call", {{"name", name}, {"arguments", arguments}});
        require(reply.contains("result") && !reply.at("result").value("isError", false),
                std::string(name) + " succeeds: " + reply.dump());
        const auto& result = reply.at("result");
        require(result.contains("structuredContent") && result.at("structuredContent").is_object() &&
                    result.at("content").size() == 1 &&
                    Json::parse(result.at("content").at(0).at("text").get<std::string>()) ==
                        result.at("structuredContent"),
                "bounded structured result agrees with text fallback");
        return result.at("structuredContent");
    }
    Json text_json(const std::string& artifact, const std::string& path) {
        const auto result = tool("artifact_read", {{"artifact_id", artifact}, {"path", path}});
        require(result.at("status") == "text", "bounded evidence is available as text");
        return Json::parse(result.at("text").get<std::string>());
    }
    void close() {
        if(closed_)
            return;
        closed_ = true;
        try {
            const auto status = client_.finish();
            std::ofstream(diagnostics_) << client_.diagnostics();
            save(shutdown_, {{"status", status == 0 ? "pass" : "fail"}, {"exit_code", status}});
            require(status == 0, "server drains protocol and exits normally on EOF");
        } catch(const std::exception& error) {
            std::ofstream(diagnostics_) << client_.diagnostics();
            save(shutdown_, {{"status", "fail"}, {"error", error.what()}});
            throw;
        }
    }

private:
    Client client_;
    std::ofstream transcript_;
    fs::path diagnostics_;
    fs::path shutdown_;
    int next_id_ = 1;
    bool closed_ = false;
};
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    if(argc != 11) {
        std::cerr << "SERVER ORIGINAL_FIXTURE REPAIRED_FIXTURE SHADERS REPAIR_RECORD NSIGHT_ROOT ARTIFACT_ROOT "
                     "RUN_ROOT REFERENCE_SCENARIO FAULT_SCENARIO required\n";
        return 2;
    }
    fs::path failure_output;
    std::string phase = "preflight";
    try {
        const auto server_source = fs::canonical(argv[1]);
        const auto fixture = fs::canonical(argv[2]);
        const auto repaired_fixture = fs::canonical(argv[3]);
        const auto shaders = fs::canonical(argv[4]);
        const std::string reference_scenario = argv[9], fault_scenario = argv[10];
        require((reference_scenario == "combined-reference" && fault_scenario == "combined-pass-error") ||
                    (reference_scenario == "multipass-reference" && fault_scenario == "pass-output-error"),
                "qualified postpass source-repair scenario pair");
        const auto repair_record = fs::canonical(argv[5]);
        const auto installation = fs::canonical(argv[6]);
        const auto store_root = fs::weakly_canonical(fs::absolute(argv[7]));
        const auto run_root = fs::weakly_canonical(fs::absolute(argv[8]));
        // The report import must be separate from the managed store.
        const auto inside = [](const fs::path& parent, const fs::path& child) {
            return std::mismatch(parent.begin(), parent.end(), child.begin(), child.end()).first == parent.end();
        };
        require(!inside(store_root, run_root) && !inside(run_root, store_root), "disjoint evidence and run roots");
        fs::create_directories(run_root);
        auto pattern = (run_root / "source-repair-XXXXXX").string();
        const auto* allocated = mkdtemp(pattern.data());
        require(allocated != nullptr, "allocate isolated source repair validation run");
        const fs::path run(allocated);
        const auto payload = run / "payload";
        fs::create_directories(payload / "inputs/shaders");
        failure_output = payload / "preflight.json";
        save(failure_output, {{"status", "running"}, {"phase", phase}});
        std::cout << "Run directory: " << run << '\n' << std::flush;
        const auto target = payload / "inputs/fixture";
        const auto server = payload / "inputs/nsight-graphics-mcp";
        fs::copy_file(server_source, server);
        fs::permissions(server, fs::perms::owner_read | fs::perms::owner_exec);
        fs::copy_file(fixture, target);
        fs::permissions(target, fs::perms::owner_read | fs::perms::owner_exec);
        fs::copy(shaders, payload / "inputs/shaders", fs::copy_options::recursive);
        const auto repaired_target = payload / "inputs/repaired-fixture";
        fs::copy_file(repaired_fixture, repaired_target);
        fs::permissions(repaired_target, fs::perms::owner_read | fs::perms::owner_exec);
        fs::copy(repair_record, payload / "repair-record", fs::copy_options::recursive);
        const auto repair = Json::parse(ngm::read_regular_file(payload / "repair-record/repair.json", 65536));
        require(repair.at("schema_version") == 1 && repair.at("kind") == "post-channel-order",
                "qualified source repair record");
        const auto original_identity =
            Json::parse(ngm::read_regular_file(payload / "repair-record/original-identity.json", 262144));
        const auto repaired_identity =
            Json::parse(ngm::read_regular_file(payload / "repair-record/repaired-identity.json", 262144));
        const auto before_source = ngm::read_regular_file(payload / "repair-record/original-Fixture.cpp", 256 * 1024);
        const auto after_source = ngm::read_regular_file(payload / "repair-record/repaired-Fixture.cpp", 256 * 1024);
        std::string expected_source = before_source;
        const std::string before_line = "result.channel_order = scenario == \"pass-output-error\" || scenario == "
                                        "\"combined-pass-error\" ? 1u : 0u;";
        const auto changed = expected_source.find(before_line);
        require(changed != std::string::npos && expected_source.find(before_line, changed + 1) == std::string::npos,
                "single implicated channel-order assignment");
        expected_source.replace(changed, before_line.size(), "result.channel_order = 0u;");
        require(expected_source == after_source,
                "only the responsible C++ assignment changed; scenario selection remains intact");
        const auto before_source_hash = ngm::sha256_file(payload / "repair-record/original-Fixture.cpp");
        const auto after_source_hash = ngm::sha256_file(payload / "repair-record/repaired-Fixture.cpp");
        const auto before_executable = ngm::sha256_file(target), after_executable = ngm::sha256_file(repaired_target);
        require(before_executable != after_executable && repair.at("before_executable_sha256") == before_executable &&
                    repair.at("after_executable_sha256") == after_executable &&
                    repair.at("before_source_sha256") == before_source_hash &&
                    repair.at("after_source_sha256") == after_source_hash,
                "repair record binds actual source and distinct executable bytes");
        auto expected_identity = original_identity;
        std::size_t changed_inputs = 0;
        for(auto& input : expected_identity.at("inputs")) {
            if(input.at("path") == "src/fixture/Fixture.cpp") {
                require(input.at("sha256") == before_source_hash, "original build identifies original source");
                input["sha256"] = after_source_hash;
                ++changed_inputs;
            }
        }
        require(changed_inputs == 1 && expected_identity == repaired_identity,
                "complete build identity differs only in Fixture.cpp input; same compiler, options, and other inputs");
        bool compiled_source = false;
        for(const auto& command : repaired_identity.at("compile_commands")) {
            const auto file = command.at("file").get<std::string>();
            if(fs::path(file).filename() == "Fixture.cpp") {
                compiled_source = true;
                require(ngm::sha256_file(file) == after_source_hash,
                        "normal CMake compiler input is the actual edited source");
                require(command.at("command").get<std::string>().find("-c " + file) != std::string::npos,
                        "recorded compiler command compiles the edited translation unit");
            }
        }
        require(compiled_source, "fixture compilation is recorded");
        for(const auto* role : {"original", "repaired"}) {
            const auto result = Json::parse(
                ngm::read_regular_file(payload / "repair-record" / (std::string(role) + "-build.result.json"), 65536));
            require(result.at("exit_code") == 0, "normal CMake source build succeeded");
            const auto log =
                ngm::read_regular_file(payload / "repair-record" / (std::string(role) + "-build.log"), 4 * 1024 * 1024);
            require(log.find("Fixture.cpp.o -c ") != std::string::npos &&
                        log.find("-o ngm-vulkan-fixture") != std::string::npos,
                    "verbose build retains actual source compile and executable link");
        }
        require(repair.at("source_patch") == "source.patch", "retained source patch reference");
        const auto patch = ngm::read_regular_file(payload / "repair-record/source.patch", 65536);
        require(patch.find("-    " + before_line) != std::string::npos &&
                    patch.find("+    result.channel_order = 0u;") != std::string::npos,
                "retained patch corresponds to the actual one-line edit");
        Json actual_shaders = Json::object();
        for(const auto& file : fs::directory_iterator(payload / "inputs/shaders")) {
            require(file.is_regular_file(), "flat frozen shader bundle");
            actual_shaders[file.path().filename().string()] = ngm::sha256_file(file.path());
        }
        require(actual_shaders == repair.at("shader_files"), "every role uses exactly the same frozen shader bundle");
        save(failure_output, {{"status", "pass"}, {"phase", phase}});
        phase = "workflow_or_retention";
        failure_output = payload / "outer-failure.json";
        fs::copy_file(__FILE__, payload / "SourceRepairIntegration.cpp");
        for(const auto* header : {"McpClient.hpp", "Check.hpp"}) {
            fs::copy_file(fs::path(__FILE__).parent_path() / header, payload / header);
        }
        Json report{
            {"schema_version", 1},
            {"project_version", ngm::project_version()},
            {"evidence_origin", "real_mcp_source_repair_validation"},
            {"status", "running"},
            {"server_sha256", ngm::sha256_file(server)},
            {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
            {"host_compiler", __VERSION__},
            {"target_sha256", before_executable},
            {"repaired_target_sha256", after_executable},
            {"original_build_identity", original_identity},
            {"repaired_build_identity", repaired_identity},
            {"reference_scenario", reference_scenario},
            {"fault_scenario", fault_scenario},
            {"repair", repair},
            {"nsight_root", installation.string()},
            {"captures", Json::array()},
            {"inputs", {{"seed", 42}, {"width", 192}, {"height", 128}, {"wait_frames", 2}, {"final_frame", 120}}},
            {"limits",
             {"Postpass C++ source-control repair only; other defect families remain unqualified",
              "No standalone GPU replay",
              "Generated source retrieval does not decode resource blobs or reconstruct arbitrary event state"}}};
        save(payload / "report.json", report);
        std::vector<std::string> environment;
        for(const auto& [key, value] : ngm::capture_environment()) {
            if(key != "PATH")
                environment.push_back(key + '=' + value);
        }
        std::vector<std::pair<std::string, Json>> retained;
        std::vector<std::string> screenshot_hashes;
        std::vector<ngm::ExperimentResult> baselines;
        try {
            for(std::size_t case_index = 0; case_index < 3; ++case_index) {
                ngm::ExperimentOptions options;
                options.fixture = case_index == 2 ? repaired_target : target;
                options.output_root = payload / "application-baselines";
                options.shader_directory = payload / "inputs/shaders";
                options.scenario = case_index == 0 ? reference_scenario : fault_scenario;
                options.seed = 42;
                options.width = 192;
                options.height = 128;
                options.frame = 2;
                options.validation = true;
                baselines.push_back(ngm::run_experiment(options));
                require(baselines.back().report.at("status") == "pass", "standalone frame-2 baseline succeeds");
                const auto& observed_workload = baselines.back().report.at("result").at("workload");
                const bool combined = reference_scenario == "combined-reference";
                require(observed_workload.at("offscreen_render_target") == true &&
                            observed_workload.at("post_processing") == true &&
                            observed_workload.at("render_pass_count") == 2 &&
                            observed_workload.at("bindless_storage_buffers") == combined &&
                            observed_workload.at("indirect_draw") == combined,
                        "requested standalone or combined workload features are actually reported");
                require(baselines.back().report.at("result").at("application").at("build") ==
                            (case_index == 2 ? repaired_identity : original_identity),
                        "running executable embeds the retained build identity for its source role");
            }
            {
                Session session(server, payload, "capture",
                                {"--artifact-root", store_root.string(), "--nsight-root", installation.string()},
                                environment);
                session.initialize();
                Json baseline_ids = Json::array();
                for(const auto& baseline : baselines) {
                    const auto imported =
                        session.tool("artifact_import", {{"source", baseline.directory.string()},
                                                         {"required_outputs", {"report.json", "output/image.ppm"}},
                                                         {"pin", true}});
                    baseline_ids.push_back(imported.at("id"));
                }
                report["application_baseline_ids"] = baseline_ids;
                for(std::size_t case_index = 0; case_index < 3; ++case_index) {
                    const std::string scenario = case_index == 0 ? reference_scenario : fault_scenario;
                    const auto selected_shaders = payload / "inputs/shaders";
                    const auto selected_target = case_index == 2 ? repaired_target : target;
                    const auto submission = session.tool(
                        "capture_cpp", {{"executable", selected_target.string()},
                                        {"working_directory", payload.string()},
                                        {"arguments",
                                         {"--scenario", scenario, "--seed", "42", "--width", "192", "--height", "128",
                                          "--frame", "120", "--shader-dir", selected_shaders.string()}},
                                        {"wait_frames", 2},
                                        {"timeout_ms", 120000},
                                        {"pin", true},
                                        {"application_output_option", "--output"}});
                    const auto capture_id = submission.at("artifact_id").get<std::string>();
                    report["captures"].push_back({{"scenario", scenario},
                                                  {"role", case_index == 0   ? "reference"
                                                           : case_index == 1 ? "before"
                                                                             : "repaired"},
                                                  {"submission", submission},
                                                  {"status", "running"}});
                    save(payload / "report.json", report);
                    auto& capture = report["captures"].back();
                    const auto deadline = std::chrono::steady_clock::now() + 150s;
                    Json job;
                    while(true) {
                        job = session.tool("job_status", {{"job_id", submission.at("identity").at("job_id")}});
                        if(job.at("state") != "queued" && job.at("state") != "running" &&
                           job.at("worker_running") == false && job.at("finalization_pending") == false)
                            break;
                        if(std::chrono::steady_clock::now() >= deadline) {
                            session.tool("job_cancel", {{"job_id", submission.at("identity").at("job_id")}});
                            throw std::runtime_error("MCP C++ capture observation deadline; cancellation requested");
                        }
                        std::this_thread::sleep_for(100ms);
                    }
                    capture["job"] = job;
                    save(payload / "report.json", report);
                    require(job.at("state") == "succeeded" && job.at("cleanup_confirmed") == true &&
                                job.at("gpu_reserved") == false,
                            "C++ capture succeeds with owned-process cleanup: " + job.dump());
                    const auto info = session.tool("artifact_info", {{"artifact_id", capture_id}});
                    require(info.at("status") == "complete" && info.at("pinned") == true &&
                                info.at("quarantined") == false,
                            "C++ capture is published and pinned");
                    const auto index = session.text_json(capture_id, "derived/cpp-project.json");
                    require(index.at("job") == submission.at("identity"),
                            "generated index belongs to submitted capture");
                    const auto source = index.at("project_directory").get<std::string>() + "/CommandList00.cpp";
                    const auto read = session.tool("artifact_read", {{"artifact_id", capture_id}, {"path", source}});
                    require(read.at("status") == "text" &&
                                read.at("text").get<std::string>().find("\"frame.2\"") != std::string::npos,
                            "real generated command source is retrievable and labels the captured frame");
                    const auto draw_page = session.tool("capture_cpp_draws", {{"capture_id", capture_id}});
                    require(draw_page.at("draws_total") == 2 && draw_page.at("unsupported_recordings_total") == 0 &&
                                draw_page.at("unsupported_objects_total") == 0 && draw_page.at("next_offset").is_null(),
                            "fresh source query resolves both scene and postpass draws completely");
                    require(draw_page.at("draws").at(0).at("function") == (reference_scenario == "combined-reference"
                                                                               ? "VulkanReplay_CmdDrawIndirect"
                                                                               : "VulkanReplay_CmdDraw") &&
                                draw_page.at("draws").at(1).at("function") == "VulkanReplay_CmdDraw",
                            "generated calls preserve the requested scene draw mode and postpass");
                    capture["draws"] = draw_page;
                    const auto setup_path = index.at("project_directory").get<std::string>() + "/FrameSetup00.cpp";
                    const auto setup =
                        session.tool("artifact_read", {{"artifact_id", capture_id}, {"path", setup_path}});
                    require(setup.at("status") == "text", "fresh shader debug-name source retrievable");
                    const auto setup_text = setup.at("text").get<std::string>();
                    const auto& post_draw = draw_page.at("draws").back();
                    require(post_draw.at("association_status") == "resolved_source_relationship",
                            "qualified post draw pipeline association");
                    bool post_fragment = false;
                    for(const auto& stage : post_draw.at("pipeline").at("stages")) {
                        if(stage.at("stage") != "VK_SHADER_STAGE_FRAGMENT_BIT")
                            continue;
                        const auto module = stage.at("module").get<std::string>();
                        const auto marker =
                            "uint64_t(" + module + "),\n    /* pObjectName = */ \"fixture.post.frag.spv\"";
                        post_fragment = setup_text.find(marker) != std::string::npos;
                    }
                    require(post_fragment &&
                                read.at("text").get<std::string>().find("\"post.present\"") != std::string::npos,
                            "postpass label and fragment module name belong to this fresh capture");
                    const auto& span = post_draw.at("source");
                    capture["post_draw_source"] =
                        session.tool("capture_cpp_source", {{"capture_id", capture_id},
                                                            {"source_path", span.at("path")},
                                                            {"start_line", span.at("start_line")},
                                                            {"max_lines", 3}});
                    const auto& excerpt = capture.at("post_draw_source");
                    require(excerpt.at("source").at("path") == span.at("path") &&
                                excerpt.at("source").at("sha256").get<std::string>() ==
                                    ngm::sha256_file(store_root / "bundles" / capture_id /
                                                     span.at("path").get<std::string>()) &&
                                !excerpt.at("lines").empty() &&
                                excerpt.at("lines").at(0).at("number") == span.at("start_line") &&
                                excerpt.at("lines").at(0).at("text").get<std::string>().find(
                                    post_draw.at("function").get<std::string>()) != std::string::npos,
                            "numbered excerpt identifies the fresh post draw in the hashed source");
                    // Explicitly pinned terminal evidence remains protected while its local binary reference is hashed.
                    const auto image =
                        store_root / "bundles" / capture_id / index.at("screenshot_path").get<std::string>();
                    screenshot_hashes.push_back(ngm::sha256_file(image));
                    capture["screenshot_sha256"] = screenshot_hashes.back();
                    capture["index"] = index;
                    capture["report"] = session.text_json(capture_id, "raw/report.json");
                    require(
                        capture.at("report").at("application").at("sha256_before_launch") ==
                                (case_index == 2 ? report.at("repaired_target_sha256") : report.at("target_sha256")) &&
                            capture.at("report").at("application").at("sha256_after_launch") ==
                                (case_index == 2 ? report.at("repaired_target_sha256") : report.at("target_sha256")),
                        "every launch uses the frozen executable identity");
                    const auto correspondence = session.tool(
                        "artifact_compare_images",
                        {{"reference",
                          {{"artifact_id", baseline_ids.at(case_index)}, {"path", "raw/imported/output/image.ppm"}}},
                         {"candidate", {{"artifact_id", capture_id}, {"path", index.at("screenshot_path")}}},
                         {"channel_tolerance", 0}});
                    require(correspondence.at("matches_within_tolerance") == true,
                            "captured frame-2 RGB exactly matches its independent application readback");
                    capture["application_correspondence"] = correspondence;
                    retained.emplace_back(capture_id, index);
                    capture["status"] = "pass";
                    save(payload / "report.json", report);
                }
                const auto compare = [&](std::size_t candidate) {
                    return session.tool("artifact_compare_images",
                                        {{"reference",
                                          {{"artifact_id", retained.at(0).first},
                                           {"path", retained.at(0).second.at("screenshot_path")}}},
                                         {"candidate",
                                          {{"artifact_id", retained.at(candidate).first},
                                           {"path", retained.at(candidate).second.at("screenshot_path")}}},
                                         {"channel_tolerance", 0}});
                };
                report["before_vs_reference"] = compare(1);
                report["repaired_vs_reference"] = compare(2);
                require(
                    report.at("before_vs_reference").at("matches_within_tolerance") == false &&
                        report.at("repaired_vs_reference").at("matches_within_tolerance") == true,
                    "fault differs; actual rebuilt C++ source under the SAME faulty scenario matches reference RGB");
                session.close();
            }
            {
                Session restarted(server, payload, "restart", {"--artifact-root", store_root.string()}, {});
                restarted.initialize();
                for(const auto& [id, index] : retained) {
                    require(restarted.tool("artifact_info", {{"artifact_id", id}}).at("pinned") == true &&
                                restarted.text_json(id, "derived/cpp-project.json") == index,
                            "generated evidence and pins survive restart without desktop or Nsight override");
                }
                for(const auto& id : report.at("application_baseline_ids"))
                    require(restarted.tool("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                            "application baseline pins survive restart");
                const auto repeated = restarted.tool(
                    "artifact_compare_images",
                    {{"reference",
                      {{"artifact_id", retained.at(0).first}, {"path", retained.at(0).second.at("screenshot_path")}}},
                     {"candidate",
                      {{"artifact_id", retained.at(2).first}, {"path", retained.at(2).second.at("screenshot_path")}}}});
                require(repeated == report.at("repaired_vs_reference"), "repair pixel comparison survives restart");
                restarted.close();
            }
            report["status"] = "pass";
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
            if(!report["captures"].empty() && report["captures"].back().at("status") == "running") {
                report["captures"].back()["status"] = "fail";
            }
        }
        save(payload / "report.json", report);
        ngm::ArtifactOptions options;
        options.root = store_root;
        ngm::ArtifactStore store(options);
        const auto imported = store.import_directory(
            payload, {{"label", "real-mcp-source-repair-validation"}, {"status", report.at("status")}},
            {"report.json", "SourceRepairIntegration.cpp"}, true);
        save(run / "receipt.json", imported);
        std::cout << Json{{"status", report.at("status")}, {"artifact", imported}, {"run", run.string()}}.dump(2)
                  << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        if(!failure_output.empty()) {
            try {
                save(failure_output, {{"status", "fail"}, {"phase", phase}, {"error", error.what()}});
            } catch(...) { /* Keep the original diagnostic if the output filesystem also failed. */
            }
        }
        std::cerr << error.what() << '\n';
        return 2;
    }
}
