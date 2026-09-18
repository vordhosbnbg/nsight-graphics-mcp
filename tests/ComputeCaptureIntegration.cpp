#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
using ngm::check::require;
using ngm::check::mcp::Client;
using namespace std::chrono_literals;
void save(const fs::path& path, const Json& value) {
    std::ofstream out(path.string() + ".tmp");
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << value.dump(2) << '\n';
    out.close();
    fs::rename(path.string() + ".tmp", path);
}
struct Session {
    Client client;
    int next = 1;
    fs::path transcript;
    bool closed = false;
    Session(const fs::path& server, const fs::path& nsight, const fs::path& store, const fs::path& directory) :
        client(server.string(), "/usr/bin:/bin",
               {"--nsight-root", nsight.string(), "--artifact-root", store.string(), "--artifact-max-bytes", "0",
                "--artifact-max-age-seconds", "0"},
               false, environment(directory), {10s, 15s}),
        transcript(directory / "protocol.jsonl") {
        try {
            const auto response =
                request("initialize", {{"protocolVersion", "2025-11-25"},
                                       {"capabilities", Json::object()},
                                       {"clientInfo", {{"name", "compute-qualification"}, {"version", "1"}}}});
            require(response.contains("result"), "MCP initialization");
            client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        } catch(...) {
            try {
                close();
            } catch(...) {
            }
            throw;
        }
    }
    ~Session() {
        try {
            close();
        } catch(...) {
        }
    }
    void preserve_diagnostics() {
        std::ofstream output(transcript.parent_path() / "server.stderr");
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << client.diagnostics();
        output.close();
    }
    void close() {
        if(closed)
            return;
        closed = true;
        try {
            const auto status = client.finish();
            preserve_diagnostics();
            require(status == 0, "server EOF cleanup");
        } catch(...) {
            preserve_diagnostics();
            throw;
        }
    }
    static std::vector<std::string> environment(const fs::path& directory) {
        std::vector<std::string> result{"LANG=C", "LC_ALL=C", "HOME=" + directory.string()};
        for(const auto& [key, value] : ngm::capture_environment())
            result.push_back(key + "=" + value);
        return result;
    }
    Json request(const char* method, const Json& params) {
        const auto id = next++;
        {
            std::ofstream log(transcript, std::ios::app);
            log.exceptions(std::ios::badbit | std::ios::failbit);
            log << Json{{"direction", "request"}, {"id", id}, {"method", method}, {"params", params}}.dump() << '\n';
        }
        const auto response = client.request(id, method, params);
        std::ofstream log(transcript, std::ios::app);
        log.exceptions(std::ios::badbit | std::ios::failbit);
        log << Json{{"direction", "response"}, {"message", response}}.dump() << '\n';
        return response;
    }
    Json tool(const char* name, const Json& args) {
        const auto reply = request("tools/call", {{"name", name}, {"arguments", args}});
        require(reply.contains("result") && !reply.at("result").value("isError", false),
                std::string(name) + ": " + reply.dump());
        return reply.at("result").at("structuredContent");
    }
    std::string read(const std::string& id, const std::string& path, bool binary = false) {
        const auto result =
            tool("artifact_read", {{"artifact_id", id}, {"path", path}, {"max_bytes", binary ? 1 : 65536}});
        if(result.at("status") == "text")
            return result.at("text").get<std::string>();
        const auto bytes = ngm::read_regular_file(result.at("local_path").get<std::string>(), 16 * 1024 * 1024);
        require(bytes.size() == result.at("bytes"), "local artifact reference byte count");
        return bytes;
    }
    Json json(const std::string& id, const std::string& path) {
        return Json::parse(read(id, path));
    }
};
} // namespace

int main(int argc, char** argv) {
    fs::path directory;
    Json report{{"status", "failed"}, {"runs", Json::array()}, {"evidence_origin", "private_compute_capture_harness"}};
    try {
        require(argc == 6 || argc == 8, "usage: ngm_compute_capture_integration SERVER FIXTURE NSIGHT_ROOT STORE "
                                        "OUTPUT_ROOT [SHADER_DIR original|repaired]");
        const auto output = fs::absolute(argv[5]);
        fs::create_directories(output);
        auto pattern = (output / "capture-XXXXXX").string();
        require(mkdtemp(pattern.data()) != nullptr, "allocate isolated compute capture run");
        directory = pattern;
        const auto server = fs::canonical(argv[1]);
        const auto fixture = fs::canonical(argv[2]);
        const auto nsight = fs::canonical(argv[3]);
        const auto store = fs::absolute(argv[4]);
        const bool repaired = argc == 8 && std::string_view(argv[7]) == "repaired";
        require(argc != 8 || repaired || std::string_view(argv[7]) == "original",
                "original or repaired oracle mode required");
        report["oracle_mode"] = repaired ? "repaired" : "original";
        report["server_sha256"] = ngm::sha256_file(server);
        report["fixture_sha256"] = ngm::sha256_file(fixture);
        Session session(server, nsight, store, directory);
        for(const auto* scenario : {"compute-reference", "compute-index-error", "compute-arithmetic-error"}) {
            ngm::ExperimentOptions baseline;
            baseline.fixture = fixture;
            baseline.output_root = directory / "baselines";
            baseline.scenario = scenario;
            baseline.width = 33;
            baseline.height = 35;
            baseline.seed = 42;
            baseline.frame = 2;
            baseline.validation = true;
            if(argc == 8)
                baseline.shader_directory = fs::canonical(argv[6]);
            const auto observed = ngm::run_experiment(baseline);
            report["runs"].push_back(
                {{"scenario", scenario}, {"baseline", observed.directory.string()}, {"status", "running"}});
            save(directory / "report.json", report);
            if(observed.report.at("status") == "unsupported") {
                report["status"] = "unsupported";
                report["reason"] = "standalone prerequisite unavailable";
                report["runs"].back()["status"] = "unsupported";
                session.close();
                save(directory / "report.json", report);
                return 3;
            }
            require(observed.report.at("status") == "pass", "standalone compute baseline execution");
            const auto validation = ngm::read_regular_file(observed.directory / "logs/stdout.log", 16 * 1024 * 1024) +
                                    ngm::read_regular_file(observed.directory / "logs/stderr.log", 16 * 1024 * 1024);
            require(validation.find("CURRENT-VALIDATION-ENABLED") != std::string::npos &&
                        validation.find("  - Synchronization") != std::string::npos &&
                        validation.find("Validation Error") == std::string::npos &&
                        validation.find("SYNC-HAZARD") == std::string::npos &&
                        validation.find("VUID-") == std::string::npos,
                    "baseline synchronization validation enabled and clean");
            report["runs"].back()["synchronization_validation"] = "pass";
            const auto baseline_row =
                Json::parse(ngm::read_regular_file(observed.directory / "output/compute-frame-2.json", 1024 * 1024));
            Json arguments = {"--scenario",
                              scenario,
                              "--seed",
                              "42",
                              "--width",
                              "33",
                              "--height",
                              "35",
                              "--frame",
                              "120",
                              "--compute-boundary",
                              "vk_frame_boundary"};
            if(argc == 8) {
                arguments.push_back("--shader-dir");
                arguments.push_back(baseline.shader_directory.string());
            }
            const auto submission = session.tool("capture", {{"executable", fixture.string()},
                                                             {"working_directory", directory.string()},
                                                             {"arguments", arguments},
                                                             {"capture_frame", 2},
                                                             {"delimiter", "vk_frame_boundary"},
                                                             {"timeout_ms", 90000},
                                                             {"application_output_option", "--output"},
                                                             {"pin", true}});
            const auto id = submission.at("artifact_id").get<std::string>();
            report["runs"].back()["capture"] = id;
            save(directory / "report.json", report);
            Json job;
            const auto deadline = std::chrono::steady_clock::now() + 110s;
            while(std::chrono::steady_clock::now() < deadline) {
                job = session.tool("job_status", {{"job_id", submission.at("identity").at("job_id")}});
                if(job.at("state") != "running" && job.at("state") != "queued" && job.at("worker_running") == false &&
                   job.at("finalization_pending") == false)
                    break;
                std::this_thread::sleep_for(200ms);
            }
            report["runs"].back()["job"] = job;
            save(directory / "report.json", report);
            if(job.at("state") == "failed" && job.at("cleanup_confirmed") == true) {
                std::optional<Json> refusal;
                try {
                    const auto app = session.json(id, "raw/application/result.json");
                    if(app.at("status") == "unsupported")
                        refusal = app;
                } catch(const std::exception&) {
                }
                if(refusal) {
                    report["status"] = "unsupported";
                    report["runs"].back()["status"] = "unsupported";
                    report["runs"].back()["application_failure"] = *refusal;
                    session.close();
                    save(directory / "report.json", report);
                    return 3;
                }
            }
            require(job.at("state") == "succeeded" && job.at("cleanup_confirmed") == true &&
                        job.at("gpu_reserved") == false,
                    "capture success and bounded cleanup");
            const auto meta = session.tool("capture_metadata", {{"capture_id", id}});
            const auto events = session.tool("capture_events", {{"capture_id", id}, {"limit", 100}});
            const auto objects = session.tool("capture_objects", {{"capture_id", id}, {"limit", 100}});
            require(meta.at("metadata").at("captured_frame") == "2", "retained delimiter ordinal");
            require(events.at("next_offset").is_null() &&
                        std::count_if(events.at("events").begin(), events.at("events").end(),
                                      [](const Json& event) { return event.at("function_name") == "vkCmdDispatch"; }) ==
                            1,
                    "one captured dispatch");
            require(objects.at("next_offset").is_null(), "complete bounded compute object inventory");
            for(const auto& [label, type] :
                std::array<std::pair<const char*, const char*>, 4>{{{"compute.input", "Buffer"},
                                                                    {"compute.output", "Buffer"},
                                                                    {"compute.affine.shader", "ShaderModule"},
                                                                    {"compute.affine.pipeline", "Pipeline"}}}) {
                require(std::count_if(objects.at("objects").begin(), objects.at("objects").end(),
                                      [&](const Json& object) {
                                          return object.at("object_name") == label && object.at("type_name") == type &&
                                                 object.at("api") == "Vulkan";
                                      }) == 1,
                        "named compute object present exactly once");
            }
            const auto setup = session.json(id, "raw/application/compute-setup.json");
            const auto row = session.json(id, "raw/application/compute-frame-2.json");
            require(setup.at("workload").at("presentation") == false && row.at("frame_boundary_id") == 2 &&
                        row.at("evidence_origin") == "application_readback" &&
                        row.at("compute").at("evidence_origin") == "application_observation",
                    "application evidence origins and selected frame");
            require(row.at("input") == baseline_row.at("input") && row.at("output") == baseline_row.at("output") &&
                        row.at("compute").at("spirv_sha256") == baseline_row.at("compute").at("spirv_sha256") &&
                        row.at("executable_sha256") == report.at("fixture_sha256"),
                    "capture/readback agrees with independent launch and shader/build identity");
            const auto source =
                session.read(id, "raw/application/" + row.at("compute").at("shader_source").get<std::string>());
            require(ngm::sha256(std::as_bytes(std::span(source))) ==
                        row.at("compute").at("source_sha256").get<std::string>(),
                    "source retrievable through MCP matches application observation");
            require(row.at("frame").is_number_unsigned() && row.at("frame") == 2 &&
                        row.at("schema_version").is_number_unsigned() && row.at("schema_version") == 1 &&
                        row.at("phase") == "readback_before_frame_end",
                    "captured readback frame schema");
            for(const auto* key : {"input", "output"}) {
                require(row.at(key).is_array() && row.at(key).size() == 1155, "captured numerical array length");
                for(const auto& value : row.at(key))
                    require(value.is_number_unsigned() && value.get<uint64_t>() <= UINT32_MAX, "captured uint32 value");
            }
            auto expected_compute = baseline_row.at("compute");
            expected_compute["boundary_enabled"] = true;
            require(row.at("compute") == expected_compute && setup.at("compute") == expected_compute,
                    "captured application shader and dispatch observations match standalone observations");
            const auto spirv =
                session.read(id, "raw/application/" + row.at("compute").at("shader_spirv").get<std::string>(), true);
            require(ngm::sha256(std::as_bytes(std::span(spirv))) ==
                        row.at("compute").at("spirv_sha256").get<std::string>(),
                    "retained application SPIR-V bytes match observed dispatch shader identity");
            report["runs"].back()["association_scope"] =
                "application-recorded dispatch/shader association; Nsight named object and dispatch inventories are "
                "corroborating, not executed-state reconstruction";
            uint64_t mismatches = 0;
            for(std::size_t i = 0; i < 1155; ++i) {
                const uint64_t expected = (((i * 13ULL) ^ 42ULL) + 14ULL) * 3ULL + 7ULL;
                mismatches += row.at("output").at(i).get<uint64_t>() != (expected & 0xffffffffULL);
            }
            require((repaired || std::string_view(scenario) == "compute-reference") ? mismatches == 0 : mismatches > 0,
                    "independent numerical correctness/fault oracle");
            report["runs"].back()["metadata"] = meta;
            report["runs"].back()["events"] = events;
            report["runs"].back()["objects"] = objects;
            report["runs"].back()["source_sha256"] = row.at("compute").at("source_sha256");
            report["runs"].back()["mismatches_to_reference"] = mismatches;
            report["runs"].back()["absolute_tolerance"] = 0;
            report["runs"].back()["status"] = "pass";
            save(directory / "report.json", report);
        }
        session.close();
        report["status"] = "pass";
        save(directory / "report.json", report);
        std::cout << (directory / "report.json").string() << '\n';
        return 0;
    } catch(const std::exception& error) {
        report["error"] = error.what();
        if(!directory.empty())
            save(directory / "report.json", report);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
