#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
void require(bool condition, const std::string& reason) {
    if(!condition)
        throw std::runtime_error(reason);
}
void save(const fs::path& path, const Json& value) {
    std::ofstream f(path.string() + ".tmp");
    f.exceptions(std::ios::badbit | std::ios::failbit);
    f << value.dump(2) << '\n';
    f.close();
    fs::rename(path.string() + ".tmp", path);
}
// Independent wide-arithmetic harness oracle, separate from application and
// Nsight observations. Fixed inputs allow reuse across frames and both variants.
std::vector<uint32_t> oracle(uint32_t count, uint32_t seed) {
    std::vector<uint32_t> output;
    constexpr uint64_t mask = 0xffffffffULL;
    for(uint64_t i = 0; i < count; ++i) {
        uint64_t v = ((i * 13) & mask) ^ seed;
        for(uint64_t iteration = 0; iteration < 2048; ++iteration) {
            v = (v ^ ((v << 13) & mask)) & mask;
            v = (v ^ (v >> 17)) & mask;
            v = (v ^ ((v << 5) & mask)) & mask;
            v = (v + iteration) & mask;
        }
        output.push_back(static_cast<uint32_t>(v));
    }
    return output;
}
double median(std::vector<double> values) {
    require(!values.empty(), "nonempty measurements");
    std::sort(values.begin(), values.end());
    const auto n = values.size();
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2;
}
Json statistics(const std::vector<double>& values) {
    const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
    const auto mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    return {{"unit", "ns"}, {"count", values.size()}, {"min", *lo},
            {"max", *hi},   {"mean", mean},           {"median", median(values)}};
}
bool validation_clean(const fs::path& run) {
    const auto log = ngm::read_regular_file(run / "logs/stdout.log", 16 * 1024 * 1024) +
                     ngm::read_regular_file(run / "logs/stderr.log", 16 * 1024 * 1024);
    return log.find("CURRENT-VALIDATION-ENABLED") != std::string::npos &&
           log.find("  - Synchronization") != std::string::npos && log.find("Validation Error") == std::string::npos &&
           log.find("SYNC-HAZARD") == std::string::npos && log.find("VUID-") == std::string::npos;
}
} // namespace
int main(int argc, char** argv) {
    fs::path root;
    Json report{{"status", "failed"},
                {"evidence_origin", "private_performance_validation_harness"},
                {"runs", Json::array()},
                {"policy",
                 {{"fresh_processes", true},
                  {"warmup_submits", 30},
                  {"measured_submits_per_launch", 40},
                  {"repeats_per_variant", 4},
                  {"order", "alternating"},
                  {"unit", "ns"},
                  {"clock_control", "not_requested"},
                  {"replay", "none"},
                  {"cadence", "Each dispatch is fence-waited, read back, serialized to JSON and hashed before the next "
                              "submit; this is not continuous throughput"}}}};
    try {
        require(argc == 3, "FIXTURE OUTPUT_ROOT");
        const auto base = fs::absolute(argv[2]);
        fs::create_directories(base);
        auto candidate = base / ("run-" + std::to_string(getpid()) + "-" +
                                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(fs::create_directory(candidate), "new owned output");
        root = candidate;
        ngm::ExperimentOptions options;
        options.fixture = fs::absolute(argv[1]);
        options.output_root = root / "runs";
        options.timeout = std::chrono::seconds(60);
        std::map<std::string, std::vector<double>> medians;
        Json hardware, executable;
        std::map<std::string, Json> shader_identities;
        for(unsigned phase = 0; phase < 2; ++phase) {
            options.validation = phase == 0;
            options.width = phase == 0 ? 33 : 128;
            options.height = phase == 0 ? 35 : 128;
            options.seed = phase == 0 ? UINT32_MAX : 42;
            options.warmup = phase == 0 ? 1 : 30;
            options.frame = phase == 0 ? 2 : 69;
            const auto expected = oracle(options.width * options.height, options.seed);
            const auto repeats = phase == 0 ? 1u : 4u;
            for(unsigned repeat = 0; repeat < repeats; ++repeat)
                for(unsigned order = 0; order < 2; ++order) {
                    options.scenario =
                        (order ^ (repeat % 2)) == 0 ? "performance-underfilled" : "performance-reference";
                    const auto run = ngm::run_experiment(options);
                    report["runs"].push_back({{"directory", run.directory.string()},
                                              {"scenario", options.scenario},
                                              {"phase", phase == 0 ? "validation" : "measurement"},
                                              {"repeat", repeat},
                                              {"order", order},
                                              {"execution_status", run.report.at("status")}});
                    save(root / "report.json", report);
                    if(run.report.at("status") == "unsupported") {
                        report["status"] = "unsupported";
                        save(root / "report.json", report);
                        return 3;
                    }
                    require(run.report.at("status") == "pass", "fixture failed: " + run.directory.string());
                    const auto& result = run.report.at("result");
                    if(hardware.is_null()) {
                        hardware = result.at("gpu");
                        executable = result.at("application").at("executable_sha256");
                    }
                    require(result.at("gpu") == hardware &&
                                result.at("application").at("executable_sha256") == executable,
                            "same observed hardware and executable across group");
                    const Json shader_identity{{"source_sha256", result.at("compute").at("source_sha256")},
                                               {"spirv_sha256", result.at("compute").at("spirv_sha256")},
                                               {"local_size", result.at("compute").at("local_size")}};
                    const auto [identity, inserted] = shader_identities.emplace(options.scenario, shader_identity);
                    require(inserted || identity->second == shader_identity,
                            "stable shader identity across fresh launches");
                    require(result.at("compute").at("local_size") ==
                                Json({options.scenario == "performance-underfilled" ? 1 : 64, 1, 1}),
                            "declared fixture workgroup variant");
                    if(options.validation)
                        require(validation_clean(run.directory), "synchronization validation missing or errors");
                    for(uint32_t frame = 0; frame <= options.frame; ++frame) {
                        const auto row = Json::parse(ngm::read_regular_file(
                            run.directory / "output" / ("compute-frame-" + std::to_string(frame) + ".json"),
                            1024 * 1024));
                        for(uint32_t i = 0; i < expected.size(); ++i) {
                            const auto input = (uint64_t(i) * 13) ^ options.seed;
                            require(row.at("input")[i] == (input & 0xffffffffULL), "independent input oracle");
                            require(row.at("output")[i] == expected[i], "independent output oracle");
                        }
                    }
                    std::vector<double> times;
                    for(const auto& timing : result.at("performance").at("measurements"))
                        times.push_back(timing.at("gpu_ns"));
                    auto& record = report["runs"].back();
                    record["status"] = "pass";
                    record["absolute_tolerance"] = 0;
                    record["validated_frames"] = options.frame + 1;
                    record["elements_per_frame"] = expected.size();
                    record["statistics"] = statistics(times);
                    record["shader_sha256"] = result.at("compute").at("spirv_sha256");
                    record["local_size"] = result.at("compute").at("local_size");
                    record["run_report_sha256"] = ngm::sha256_file(run.directory / "report.json");
                    if(phase == 1) {
                        medians[options.scenario].push_back(median(times));
                        const auto half = times.begin() + times.size() / 2;
                        record["first_half_median_ns"] = median({times.begin(), half});
                        record["second_half_median_ns"] = median({half, times.end()});
                    }
                    save(root / "report.json", report);
                }
        }
        for(const auto& [scenario, values] : medians)
            report["launch_medians"][scenario] = statistics(values);
        const auto& slow = medians.at("performance-underfilled");
        const auto& fast = medians.at("performance-reference");
        report["paired_median_ratios"] = Json::array();
        for(std::size_t i = 0; i < slow.size(); ++i)
            report["paired_median_ratios"].push_back(slow[i] / fast[i]);
        report["separated_launch_ranges"] =
            *std::min_element(slow.begin(), slow.end()) > *std::max_element(fast.begin(), fast.end());
        report["comparison_status"] =
            report.at("separated_launch_ranges") == true ? "repeatable_change_observed" : "inconclusive";
        report["shader_identities"] = shader_identities;
        report["gpu"] = hardware;
        report["fixture_sha256"] = executable;
        report["scope"] = "Known variant measurement and all-frame numerical validation; not an actual source repair "
                          "or Nsight comparison";
        report["status"] = "pass";
        save(root / "report.json", report);
        std::cout << (root / "report.json").string() << '\n';
        return 0;
    } catch(const std::exception& e) {
        report["error"] = e.what();
        if(!root.empty())
            try {
                save(root / "report.json", report);
            } catch(const std::exception& w) {
                std::cerr << w.what() << '\n';
            }
        std::cerr << e.what() << '\n';
        return 1;
    }
}
