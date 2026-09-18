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
                     {"clientInfo", {{"name", "ngm-shader-repair-integration"}, {"version", ngm::project_version()}}}});
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
    if(argc != 9) {
        std::cerr << "SERVER FIXTURE ORIGINAL_SHADERS REPAIRED_SHADERS REPAIR_RECORD NSIGHT_ROOT ARTIFACT_ROOT "
                     "RUN_ROOT required\n";
        return 2;
    }
    try {
        const auto server_source = fs::canonical(argv[1]);
        const auto fixture = fs::canonical(argv[2]);
        const auto shaders = fs::canonical(argv[3]);
        const auto repaired_shaders = fs::canonical(argv[4]);
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
        auto pattern = (run_root / "shader-repair-XXXXXX").string();
        const auto* allocated = mkdtemp(pattern.data());
        require(allocated != nullptr, "allocate isolated shader repair validation run");
        const fs::path run(allocated);
        const auto payload = run / "payload";
        fs::create_directories(payload / "inputs/shaders");
        const auto target = payload / "inputs/fixture";
        const auto server = payload / "inputs/nsight-graphics-mcp";
        fs::copy_file(server_source, server);
        fs::permissions(server, fs::perms::owner_read | fs::perms::owner_exec);
        fs::copy_file(fixture, target);
        fs::permissions(target, fs::perms::owner_read | fs::perms::owner_exec);
        fs::copy(shaders, payload / "inputs/shaders", fs::copy_options::recursive);
        fs::copy(repaired_shaders, payload / "inputs/repaired-shaders", fs::copy_options::recursive);
        fs::copy(repair_record, payload / "repair-record", fs::copy_options::recursive);
        const auto before_source = ngm::sha256_file(payload / "inputs/shaders/shader-error.frag");
        const auto after_source = ngm::sha256_file(payload / "inputs/repaired-shaders/shader-error.frag");
        const auto before_spirv = ngm::sha256_file(payload / "inputs/shaders/shader-error.frag.spv");
        const auto after_spirv = ngm::sha256_file(payload / "inputs/repaired-shaders/shader-error.frag.spv");
        require(before_source != after_source && before_spirv != after_spirv,
                "actual changed source and rebuilt shader");
        const auto repair = Json::parse(ngm::read_regular_file(payload / "repair-record/repair.json", 65536));
        require(repair.at("before_source_sha256") == before_source &&
                    repair.at("after_source_sha256") == after_source &&
                    repair.at("before_spirv_sha256") == before_spirv &&
                    repair.at("after_spirv_sha256") == after_spirv && repair.at("compiler_exit_code") == 0,
                "repair record binds edited/rebuilt shader inputs");
        const auto original_manifest =
            Json::parse(ngm::read_regular_file(payload / "inputs/shaders/provenance.json", 65536));
        const auto repaired_manifest =
            Json::parse(ngm::read_regular_file(payload / "inputs/repaired-shaders/provenance.json", 65536));
        const auto& compiler = original_manifest.at("shader_compiler");
        require(repaired_manifest.at("shader_compiler") == compiler &&
                    repair.at("compiler_sha256") == compiler.at("sha256"),
                "repair uses the same recorded compiler identity/options as both shader bundles");
        const auto compiler_argv = repair.at("compiler_argv").get<std::vector<std::string>>();
        const auto flags = compiler.at("arguments").get<std::vector<std::string>>();
        require(compiler_argv.size() == flags.size() + 4 &&
                    std::equal(flags.begin(), flags.end(), compiler_argv.begin() + 1) &&
                    compiler_argv.at(flags.size() + 1) == "-o" &&
                    ngm::sha256_file(fs::canonical(compiler_argv.front())) ==
                        repair.at("compiler_sha256").get<std::string>() &&
                    ngm::sha256_file(fs::canonical(compiler_argv.at(flags.size() + 2))) == after_spirv &&
                    ngm::sha256_file(fs::canonical(compiler_argv.back())) == after_source,
                "recorded compiler invocation binds actual compiler and edited source/output files");
        require(after_spirv != ngm::sha256_file(payload / "inputs/shaders/scene.frag.spv"),
                "repair is not a copied prebuilt reference shader");
        auto expected_source = ngm::read_regular_file(payload / "inputs/shaders/shader-error.frag", 65536);
        const auto swizzle = expected_source.find("vertex_color.bgr");
        require(swizzle != std::string::npos &&
                    expected_source.find("vertex_color.bgr", swizzle + 1) == std::string::npos,
                "fixed basic case has one implicated swizzle");
        expected_source.replace(swizzle, std::string("vertex_color.bgr").size(), "vertex_color.rgb");
        require(expected_source == ngm::read_regular_file(payload / "inputs/repaired-shaders/shader-error.frag", 65536),
                "only implicated GLSL swizzle changed");
        require(repair.at("source_patch") == "shader.patch", "fixed local patch artifact");
        const auto patch = ngm::read_regular_file(payload / "repair-record/shader.patch", 65536);
        require(patch.find("-    output_color = vec4(vertex_color.bgr") != std::string::npos &&
                    patch.find("+    output_color = vec4(vertex_color.rgb") != std::string::npos,
                "retained patch records the actual source change");
        for(const auto& file : fs::directory_iterator(payload / "inputs/shaders")) {
            const auto name = file.path().filename();
            if(name != "shader-error.frag" && name != "shader-error.frag.spv" && name != "provenance.json") {
                require(ngm::sha256_file(file.path()) == ngm::sha256_file(payload / "inputs/repaired-shaders" / name),
                        "other shader inputs stay identical across this shader-only repair");
            }
        }
        fs::copy_file(__FILE__, payload / "ShaderRepairIntegration.cpp");
        for(const auto* header : {"McpClient.hpp", "Check.hpp"}) {
            fs::copy_file(fs::path(__FILE__).parent_path() / header, payload / header);
        }
        Json report{
            {"schema_version", 1},
            {"project_version", ngm::project_version()},
            {"evidence_origin", "real_mcp_shader_repair_validation"},
            {"status", "running"},
            {"server_sha256", ngm::sha256_file(server)},
            {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
            {"host_compiler", __VERSION__},
            {"target_sha256", ngm::sha256_file(target)},
            {"repair", repair},
            {"nsight_root", installation.string()},
            {"captures", Json::array()},
            {"inputs", {{"seed", 42}, {"width", 192}, {"height", 128}, {"wait_frames", 2}, {"final_frame", 120}}},
            {"limits",
             {"Basic shader-calculation repair only; no other defect variants qualified", "No standalone GPU replay",
              "Generated source retrieval does not decode resource blobs or reconstruct arbitrary event state"}}};
        save(payload / "report.json", report);
        std::vector<std::string> environment;
        for(const auto& [key, value] : ngm::capture_environment()) {
            if(key != "PATH")
                environment.push_back(key + '=' + value);
        }
        std::cout << "Run directory: " << run << '\n' << std::flush;
        std::vector<std::pair<std::string, Json>> retained;
        std::vector<std::string> screenshot_hashes;
        std::vector<ngm::ExperimentResult> baselines;
        try {
            for(std::size_t case_index = 0; case_index < 3; ++case_index) {
                ngm::ExperimentOptions options;
                options.fixture = target;
                options.output_root = payload / "application-baselines";
                options.shader_directory = payload / (case_index == 2 ? "inputs/repaired-shaders" : "inputs/shaders");
                options.scenario = case_index == 0 ? "reference" : "shader-error";
                options.seed = 42;
                options.width = 192;
                options.height = 128;
                options.frame = 2;
                options.validation = true;
                baselines.push_back(ngm::run_experiment(options));
                require(baselines.back().report.at("status") == "pass", "standalone frame-2 baseline succeeds");
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
                    const std::string scenario = case_index == 0 ? "reference" : "shader-error";
                    const auto selected_shaders =
                        payload / (case_index == 2 ? "inputs/repaired-shaders" : "inputs/shaders");
                    const auto submission = session.tool(
                        "capture_cpp", {{"executable", target.string()},
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
                    // Explicitly pinned terminal evidence remains protected while its local binary reference is hashed.
                    const auto image =
                        store_root / "bundles" / capture_id / index.at("screenshot_path").get<std::string>();
                    screenshot_hashes.push_back(ngm::sha256_file(image));
                    capture["screenshot_sha256"] = screenshot_hashes.back();
                    capture["index"] = index;
                    capture["report"] = session.text_json(capture_id, "raw/report.json");
                    require(capture.at("report").at("application").at("sha256_before_launch") ==
                                    report.at("target_sha256") &&
                                capture.at("report").at("application").at("sha256_after_launch") ==
                                    report.at("target_sha256"),
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
                require(report.at("before_vs_reference").at("matches_within_tolerance") == false &&
                            report.at("repaired_vs_reference").at("matches_within_tolerance") == true,
                        "fault differs; actual rebuilt shader under the SAME faulty scenario matches reference RGB");
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
            payload, {{"label", "real-mcp-shader-repair-validation"}, {"status", report.at("status")}},
            {"report.json", "ShaderRepairIntegration.cpp"}, true);
        save(run / "receipt.json", imported);
        std::cout << Json{{"status", report.at("status")}, {"artifact", imported}, {"run", run.string()}}.dump(2)
                  << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
