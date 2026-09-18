#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/Artifacts.hpp"
#include "ngm/CaptureService.hpp"
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
                     {"clientInfo", {{"name", "ngm-cpp-capture-integration"}, {"version", ngm::project_version()}}}});
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
        std::cerr
            << "SERVER FIXTURE SHADERS NSIGHT_ROOT ARTIFACT_ROOT RUN_ROOT REFERENCE_SCENARIO FAULT_SCENARIO required\n";
        return 2;
    }
    try {
        const auto server_source = fs::canonical(argv[1]);
        const auto fixture = fs::canonical(argv[2]);
        const auto shaders = fs::canonical(argv[3]);
        const auto installation = fs::canonical(argv[4]);
        const auto store_root = fs::weakly_canonical(fs::absolute(argv[5]));
        const auto run_root = fs::weakly_canonical(fs::absolute(argv[6]));
        // The report import must be separate from the managed store.
        const auto inside = [](const fs::path& parent, const fs::path& child) {
            return std::mismatch(parent.begin(), parent.end(), child.begin(), child.end()).first == parent.end();
        };
        require(!inside(store_root, run_root) && !inside(run_root, store_root), "disjoint evidence and run roots");
        fs::create_directories(run_root);
        auto pattern = (run_root / "cpp-validation-XXXXXX").string();
        const auto* allocated = mkdtemp(pattern.data());
        require(allocated != nullptr, "allocate isolated C++ capture validation run");
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
        fs::copy_file(__FILE__, payload / "CppCaptureIntegration.cpp");
        for(const auto* header : {"McpClient.hpp", "Check.hpp"}) {
            fs::copy_file(fs::path(__FILE__).parent_path() / header, payload / header);
        }
        Json report{
            {"schema_version", 1},
            {"project_version", ngm::project_version()},
            {"evidence_origin", "real_mcp_cpp_capture_validation"},
            {"status", "running"},
            {"server_sha256", ngm::sha256_file(server)},
            {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
            {"host_compiler", __VERSION__},
            {"target_sha256", ngm::sha256_file(target)},
            {"nsight_root", installation.string()},
            {"captures", Json::array()},
            {"inputs", {{"seed", 42}, {"width", 192}, {"height", 128}, {"wait_frames", 2}, {"final_frame", 120}}},
            {"limits",
             {"No source repair or GPU replay", "Screenshot file equality/difference, not decoded pixel validation",
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
        try {
            {
                Session session(server, payload, "capture",
                                {"--artifact-root", store_root.string(), "--nsight-root", installation.string()},
                                environment);
                session.initialize();
                for(const auto& scenario : {std::string(argv[7]), std::string(argv[7]), std::string(argv[8])}) {
                    const auto submission = session.tool(
                        "capture_cpp", {{"executable", target.string()},
                                        {"working_directory", payload.string()},
                                        {"arguments",
                                         {"--scenario", scenario, "--seed", "42", "--width", "192", "--height", "128",
                                          "--frame", "120", "--shader-dir", (payload / "inputs/shaders").string()}},
                                        {"wait_frames", 2},
                                        {"timeout_ms", 120000},
                                        {"pin", true},
                                        {"application_output_option", "--output"}});
                    const auto capture_id = submission.at("artifact_id").get<std::string>();
                    report["captures"].push_back(
                        {{"scenario", scenario}, {"submission", submission}, {"status", "running"}});
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
                    retained.emplace_back(capture_id, index);
                    capture["status"] = "pass";
                    save(payload / "report.json", report);
                }
                session.close();
            }
            require(screenshot_hashes.at(0) == screenshot_hashes.at(1) &&
                        screenshot_hashes.at(0) != screenshot_hashes.at(2),
                    "reference screenshot repeats exactly and faulty scenario differs");
            {
                Session restarted(server, payload, "restart", {"--artifact-root", store_root.string()}, {});
                restarted.initialize();
                for(const auto& [id, index] : retained) {
                    require(restarted.tool("artifact_info", {{"artifact_id", id}}).at("pinned") == true &&
                                restarted.text_json(id, "derived/cpp-project.json") == index,
                            "generated evidence and pins survive restart without desktop or Nsight override");
                }
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
            payload, {{"label", "real-mcp-cpp-capture-validation"}, {"status", report.at("status")}},
            {"report.json", "CppCaptureIntegration.cpp"}, true);
        save(run / "receipt.json", imported);
        std::cout << Json{{"status", report.at("status")}, {"artifact", imported}, {"run", run.string()}}.dump(2)
                  << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
