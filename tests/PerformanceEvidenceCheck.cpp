#include "Check.hpp"
#include "ngm/Experiment.hpp"
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 3, "stand-in and output root");
        ngm::ExperimentOptions options;
        options.fixture = argv[1];
        options.output_root = argv[2];
        options.width = 32;
        options.height = 32;
        options.frame = 2;
        options.seed = 42;
        options.warmup = 1;
        for(const auto* scenario : {"performance-reference", "performance-underfilled"}) {
            options.scenario = scenario;
            const auto run = ngm::run_experiment(options);
            require(run.report.at("status") == "pass", "synthetic performance record: " + run.report.dump());
            const auto& measured = run.report.at("result").at("performance").at("measurements");
            require(measured.size() == 2 && measured[0].at("frame") == 1 && measured[0].at("gpu_ns") == 20.0,
                    "warmup excluded and wrapped ticks converted");
        }
        for(uint32_t seed = 200; seed <= 209; ++seed) {
            options.seed = seed;
            const auto run = ngm::run_experiment(options);
            require(run.report.at("status") == "fail",
                    "contradictory intermediate timing rejected: " + std::to_string(seed));
        }
        for(const auto warmup : {std::optional<uint32_t>{}, std::optional<uint32_t>{3}, std::optional<uint32_t>{101}}) {
            options.warmup = warmup;
            bool refused = false;
            try {
                (void)ngm::run_experiment(options);
            } catch(const std::invalid_argument&) {
                refused = true;
            }
            require(refused, "invalid performance policy rejected before allocation");
        }
    });
}
