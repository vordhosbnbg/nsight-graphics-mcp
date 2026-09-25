#include "ngm/Experiment.hpp"
#include "ngm/Version.hpp"

#include <charconv>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
std::uint32_t number(const std::string& text) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if(error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("Expected an unsigned decimal integer: " + text);
    }
    return value;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if(argc == 2 && std::string_view(argv[1]) == "--version") {
            std::cout << "ngm-experiment " << ngm::project_version() << '\n';
            return 0;
        }
        if(argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage: ngm-experiment --fixture PATH --output-root DIR --scenario NAME --seed N "
                         "--width N --height N --frame N [--shader-dir DIR] [--timeout-ms N] [--validation true|false] "
                         "[--warmup N]\n"
                         "Runs a fresh graphics or compute fixture in isolated configuration and retains a report.\n";
            return 0;
        }
        std::map<std::string, std::string> values;
        for(int index = 1; index < argc; index += 2) {
            const std::string key = argv[index];
            if(key != "--fixture" && key != "--output-root" && key != "--scenario" && key != "--seed" &&
               key != "--width" && key != "--height" && key != "--frame" && key != "--shader-dir" &&
               key != "--timeout-ms" && key != "--validation" && key != "--warmup") {
                throw std::invalid_argument("Unsupported argument: " + key);
            }
            if(index + 1 >= argc || !values.emplace(key, argv[index + 1]).second) {
                throw std::invalid_argument("Missing value or duplicate argument: " + key);
            }
        }
        for(const auto* required :
            {"--fixture", "--output-root", "--scenario", "--seed", "--width", "--height", "--frame"}) {
            if(!values.contains(required)) {
                throw std::invalid_argument(std::string("Missing required option: ") + required + "; use --help");
            }
        }
        ngm::ExperimentOptions options;
        options.fixture = values.at("--fixture");
        options.output_root = values.at("--output-root");
        options.scenario = values.at("--scenario");
        options.seed = number(values.at("--seed"));
        options.width = number(values.at("--width"));
        options.height = number(values.at("--height"));
        options.frame = number(values.at("--frame"));
        if(values.contains("--warmup"))
            options.warmup = number(values.at("--warmup"));
        if(values.contains("--shader-dir")) {
            options.shader_directory = values.at("--shader-dir");
        }
        if(values.contains("--timeout-ms")) {
            options.timeout = std::chrono::milliseconds(number(values.at("--timeout-ms")));
        }
        if(values.contains("--validation")) {
            const auto& value = values.at("--validation");
            if(value != "true" && value != "false") {
                throw std::invalid_argument("--validation must be true or false");
            }
            options.validation = value == "true";
        }
        const auto result = ngm::run_experiment(options);
        std::cout << nlohmann::json{{"report", (result.directory / "report.json").string()},
                                    {"status", result.report.at("status")}}
                         .dump()
                  << '\n';
        return result.report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << "ngm-experiment: " << error.what() << '\n';
        return 2;
    }
}
