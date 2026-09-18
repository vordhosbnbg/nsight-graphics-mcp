#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <map>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
void require(bool condition, const std::string& reason) {
    if(!condition)
        throw std::runtime_error(reason);
}
void save(const fs::path& path, const Json& value) {
    std::ofstream file(path.string() + ".tmp");
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << value.dump(2) << '\n';
    file.close();
    fs::rename(path.string() + ".tmp", path);
}
bool validation_clean(const fs::path& run) {
    const auto messages = ngm::read_regular_file(run / "logs/stdout.log", 16 * 1024 * 1024) +
                          ngm::read_regular_file(run / "logs/stderr.log", 16 * 1024 * 1024);
    return messages.find("CURRENT-VALIDATION-ENABLED") != std::string::npos &&
           messages.find("  - Synchronization") != std::string::npos &&
           messages.find("Validation Error") == std::string::npos &&
           messages.find("SYNC-HAZARD") == std::string::npos && messages.find("VUID-") == std::string::npos;
}
// Private harness oracle. It does not consume the shader implementation or put
// expected diagnoses in the application/MCP observations. uint32 arithmetic is
// evaluated in uint64 and explicitly reduced modulo 2^32.
Json oracle(const Json& row, uint32_t count, uint32_t seed, uint32_t frame, const std::string& scenario) {
    constexpr uint64_t mask = 0xffffffffULL;
    uint64_t wrong = 0;
    for(uint32_t i = 0; i < count; ++i) {
        const auto input = (((uint64_t(i) * 13) & mask) ^ seed) + uint64_t(frame) * 7;
        require(row.at("input")[i] == (input & mask), "input oracle mismatch");
        const auto correct = ((input & mask) * 3 + 7) & mask;
        const auto output = row.at("output")[i].get<uint64_t>();
        wrong += output != correct;
        uint64_t expected = correct;
        if(scenario == "compute-index-error") {
            const auto next = (i + 1) % count;
            expected = (((((uint64_t(next) * 13) & mask) ^ seed) + uint64_t(frame) * 7) & mask) * 3 + 7;
        } else if(scenario == "compute-arithmetic-error") {
            expected = (input & mask) * 2 + 7;
        }
        require(output == (expected & mask), "selected numerical fixture behavior differs from independent oracle");
    }
    require(scenario == "compute-reference" ? wrong == 0 : wrong > 0, "reference/fault distinction missing");
    return {{"frame", frame}, {"elements", count}, {"mismatches_to_reference", wrong}, {"absolute_tolerance", 0}};
}
} // namespace

int main(int argc, char** argv) {
    Json report{
        {"status", "failed"}, {"evidence_origin", "private_compute_validation_harness"}, {"runs", Json::array()}};
    fs::path root;
    try {
        require(argc == 3, "usage: ngm_compute_integration FIXTURE OUTPUT_ROOT");
        const auto base = fs::absolute(argv[2]);
        fs::create_directories(base);
        const auto candidate = base / ("run-" + std::to_string(getpid()) + "-" +
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(candidate), "could not allocate new output directory");
        root = candidate; // Failure writes are allowed only after ownership is established.
        ngm::ExperimentOptions options;
        options.fixture = fs::absolute(argv[1]);
        options.output_root = root / "runs";
        options.validation = true;
        options.timeout = std::chrono::seconds(30);
        for(unsigned config = 0; config < 2; ++config) {
            options.width = config == 0 ? 33 : 64;
            options.height = config == 0 ? 35 : 32;
            options.seed = config == 0 ? 42 : UINT32_MAX;
            options.frame = config == 0 ? 2 : 1;
            std::map<std::string, std::vector<Json>> previous;
            for(const auto* scenario : {"compute-reference", "compute-index-error", "compute-arithmetic-error"}) {
                options.scenario = scenario;
                for(unsigned repeat = 0; repeat < 2; ++repeat) {
                    const auto run = ngm::run_experiment(options);
                    Json record{{"directory", run.directory.string()},
                                {"scenario", scenario},
                                {"config", config},
                                {"repeat", repeat},
                                {"execution_status", run.report.at("status")},
                                {"frames", Json::array()}};
                    report["runs"].push_back(record);
                    save(root / "report.json", report);
                    if(run.report.at("status") == "unsupported") {
                        report["status"] = "unsupported";
                        save(root / "report.json", report);
                        return 3;
                    }
                    require(run.report.at("status") == "pass", "fixture execution failed: " + run.directory.string());
                    require(validation_clean(run.directory), "synchronization validation absent or reports errors");
                    std::vector<Json> rows;
                    for(uint32_t frame = 0; frame <= options.frame; ++frame) {
                        const auto path =
                            run.directory / "output" / ("compute-frame-" + std::to_string(frame) + ".json");
                        const auto row = Json::parse(ngm::read_regular_file(path, 1024 * 1024));
                        auto checked = oracle(row, options.width * options.height, options.seed, frame, scenario);
                        checked["readback_sha256"] = ngm::sha256_file(path);
                        report["runs"].back()["frames"].push_back(checked);
                        rows.push_back({{"input", row.at("input")}, {"output", row.at("output")}});
                    }
                    if(repeat == 0)
                        previous[scenario] = rows;
                    else
                        require(previous.at(scenario) == rows, "fresh-launch numerical repeat differs");
                    report["runs"].back()["status"] = "pass";
                    report["runs"].back()["synchronization_validation"] = "pass";
                    save(root / "report.json", report);
                }
            }
        }
        report["status"] = "pass";
        save(root / "report.json", report);
        std::cout << (root / "report.json").string() << '\n';
        return 0;
    } catch(const std::exception& error) {
        report["error"] = error.what();
        if(!root.empty() && fs::is_directory(root))
            save(root / "report.json", report);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
