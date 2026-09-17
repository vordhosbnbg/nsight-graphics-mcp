#include "Check.hpp"
#include "ngm/Experiment.hpp"

#include <cstdlib>
#include <fstream>

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 3, "expected stand-in executable and scratch root");
        ngm::ExperimentOptions options;
        options.fixture = argv[1];
        options.output_root = argv[2];
        options.scenario = "reference";
        options.seed = 42;
        options.width = 64;
        options.height = 48;
        options.frame = 2;
        require(setenv("NGM_PARENT_ONLY", "not inherited", 1) == 0, "set isolation test input");
        const auto first = ngm::run_experiment(options);
        const auto second = ngm::run_experiment(options);
        require(first.report.at("status") == "pass" && second.report.at("status") == "pass",
                "valid fixture results pass");
        require(first.directory != second.directory, "fresh directories per run");
        require(first.report.at("result").at("test_environment").at("home") ==
                    (first.directory / "config/home").string(),
                "isolated HOME reaches child");
        require(first.report.at("result").at("test_environment").at("unrelated_variable_inherited") == false,
                "unrelated parent configuration does not reach child");
        require(first.report.at("image").at("sha256") == second.report.at("image").at("sha256"),
                "repeat image identities");
        require(std::filesystem::exists(first.directory / "logs/stdout.log") &&
                    std::filesystem::exists(first.directory / "logs/stderr.log"),
                "separate logs retained");
        for(const auto* scenario :
            {"malformed-result", "wrong-inputs", "missing-image", "failure", "unsupported", "inconsistent-metadata",
             "missing-provenance", "fifo-image", "empty-metadata", "missing-driver", "tampered-shader",
             "wrong-executable", "missing-shader-build", "malformed-shader-build", "missing-shader-manifest",
             "contradictory-shader-manifest", "fifo-shader-manifest", "floating-shader-schema"}) {
            options.scenario = scenario;
            const auto result = ngm::run_experiment(options);
            const auto status = options.scenario == "unsupported" ? "unsupported" : "fail";
            require(result.report.at("status") == status, "failed or unsupported run not reported as success");
            std::ifstream saved(result.directory / "report.json");
            require(nlohmann::json::parse(saved) == result.report, "failure retained on disk");
            require(!std::filesystem::exists(result.directory / "report.json.tmp"),
                    "report published with no temporary left");
        }
        options.scenario = "hang";
        options.timeout = std::chrono::milliseconds(50);
        const auto timeout = ngm::run_experiment(options);
        require(timeout.report.at("status") == "timeout" &&
                    timeout.report.at("process").at("cleanup_confirmed") == true,
                "timeout cleans owned process and retains report");
    });
}
