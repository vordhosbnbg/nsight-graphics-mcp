#include "Check.hpp"
#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>

namespace {
bool validation_logs_clean(const std::filesystem::path& run) {
    std::string messages;
    for(const auto* stream : {"stdout.log", "stderr.log"}) {
        messages += ngm::read_regular_file(run / "logs" / stream, 16 * 1024 * 1024);
    }
    const bool active = messages.find("CURRENT-VALIDATION-ENABLED") != std::string::npos &&
                        messages.find("  - Synchronization") != std::string::npos;
    const bool errors = messages.find("Validation Error") != std::string::npos ||
                        messages.find("SYNC-HAZARD") != std::string::npos ||
                        messages.find("VUID-") != std::string::npos;
    return active && !errors;
}

// Kept only in the private harness: selected workload, reference pairing,
// and intended fault. Renderer/MCP results contain no oracle or diagnosis.
struct Scenario {
    const char* name;
    const char* reference;
    bool multipass = false;
    bool bindless = false;
    bool indirect = false;
    bool post_swap = false;
    std::uint32_t resource_xor = 0;
    std::uint32_t instances = 1;
};
constexpr std::array<Scenario, 14> scenarios{
    {{"reference", "reference"},
     {"shader-error", "reference"},
     {"binding-error", "reference"},
     {"pipeline-error", "reference"},
     {"multipass-reference", "multipass-reference", true},
     {"pass-output-error", "multipass-reference", true, false, false, true},
     {"bindless-reference", "bindless-reference", false, true},
     {"resource-selection-error", "bindless-reference", false, true, false, false, 1},
     {"indirect-reference", "indirect-reference", false, false, true, false, 0, 2},
     {"indirect-parameter-error", "indirect-reference", false, false, true},
     {"combined-reference", "combined-reference", true, true, true, false, 0, 2},
     {"combined-pass-error", "combined-reference", true, true, true, true, 0, 2},
     {"combined-resource-error", "combined-reference", true, true, true, false, 1, 2},
     {"combined-indirect-error", "combined-reference", true, true, true}}};

// Private oracle: expected diagnoses and ideal image math stay in the harness,
// outside renderer output and MCP evidence. Compare interior pixels separately
// from rasterization edges, whose exact coverage may vary across GPUs.
nlohmann::json check_ideal(const ngm::Image& image, const ngm::ExperimentOptions& options, const Scenario& scenario) {
    const std::array<std::array<double, 3>, 3> colors{{{1, .125, .0625}, {.125, 1, .0625}, {.125, .0625, 1}}};
    const std::array<double, 3> background{.03125, .0625, .09375};
    std::array<double, 3> primary{};
    for(std::size_t i = 0; i < primary.size(); ++i) {
        primary[i] = (.65 + .35 * ((options.seed >> (i * 8)) & 255) / 255.0) * (.8 + .02 * (options.frame % 11));
    }
    const std::array<double, 3> secondary{primary[2] * .25, primary[0], primary[1] * .5};
    std::uint64_t checked = 0;
    std::uint64_t failed = 0;
    int maximum = 0;
    const int tolerance = scenario.multipass ? 2 : 1;
    const double edge_band = 2.0 / std::min(image.width, image.height);
    const auto quantize = [](double value) { return std::lround(std::clamp(value, 0.0, 1.0) * 255); };
    for(std::uint32_t y = 0; y < image.height; ++y) {
        for(std::uint32_t x = 0; x < image.width; ++x) {
            const double window_x = 2.0 * (x + .5) / image.width - 1;
            const double ny = 2.0 * (y + .5) / image.height - 1;
            auto expected = background;
            bool edge = false;
            for(std::uint32_t instance = 0; instance < scenario.instances; ++instance) {
                const double nx = scenario.indirect ? (window_x - (-.5 + instance)) * 2 : window_x;
                const double w2 = (ny + .75) / 1.5;
                const double w1 = (nx + .8 - .8 * w2) / 1.6;
                const double w0 = 1 - w1 - w2;
                const auto minimum = std::min({w0, w1, w2});
                edge |= std::abs(minimum) < edge_band;
                if(minimum > 0) {
                    std::array<double, 3> interpolated{};
                    for(std::size_t i = 0; i < 3; ++i) {
                        interpolated[i] = w0 * colors[0][i] + w1 * colors[1][i] + w2 * colors[2][i];
                    }
                    if(options.scenario == "shader-error") {
                        std::swap(interpolated[0], interpolated[2]);
                    }
                    const bool alternate =
                        options.scenario == "binding-error" ||
                        (scenario.bindless &&
                         (((x / 8) ^ ((options.seed ^ options.frame) & 1u) ^ scenario.resource_xor) & 1u));
                    const auto& tint = alternate ? secondary : primary;
                    for(std::size_t i = 0; i < 3; ++i) {
                        expected[i] = interpolated[i] * tint[i];
                    }
                    if(options.scenario == "pipeline-error") {
                        expected[0] = background[0];
                    }
                }
            }
            if(edge) {
                continue;
            }
            if(scenario.multipass) {
                // Scene storage is RGBA8_UNORM, then texelFetch reads exactly
                // this pixel for an independent final RGB8 conversion.
                for(auto& channel : expected) {
                    channel = static_cast<double>(quantize(channel)) / 255.0;
                }
                if(scenario.post_swap) {
                    std::swap(expected[0], expected[2]);
                }
                const std::array<double, 3> scale{.75, .875, .5};
                const std::array<double, 3> bias{.03125, .015625, .0625};
                for(std::size_t channel = 0; channel < 3; ++channel) {
                    expected[channel] = expected[channel] * scale[channel] + bias[channel];
                }
            }
            bool bad = false;
            for(std::size_t channel = 0; channel < 3; ++channel) {
                const int ideal = static_cast<int>(quantize(expected[channel]));
                const int actual = image.rgb[(static_cast<std::size_t>(y) * image.width + x) * 3 + channel];
                const int difference = std::abs(ideal - actual);
                maximum = std::max(maximum, difference);
                bad |= difference > tolerance;
            }
            ++checked;
            failed += bad;
        }
    }
    return {{"checked_pixels", checked},
            {"failed_pixels", failed},
            {"max_channel_difference", maximum},
            {"channel_tolerance", tolerance},
            {"units", "RGB8 channel steps"},
            {"edge_exclusion_barycentric", edge_band},
            {"intermediate_unorm_conversions", scenario.multipass ? 1 : 0}};
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc >= 3,
                "expected real fixture and artifact root, optionally --validation and --suite basic|advanced|all");
        ngm::ExperimentOptions options;
        options.fixture = argv[1];
        options.output_root = argv[2];
        std::string suite = "basic";
        bool suite_selected = false;
        for(int i = 3; i < argc; ++i) {
            const std::string_view argument(argv[i]);
            if(argument == "--validation" && !options.validation) {
                options.validation = true;
            } else if(argument == "--suite" && !suite_selected && i + 1 < argc) {
                suite = argv[++i];
                suite_selected = true;
            } else {
                require(false, "unknown or repeated harness argument");
            }
        }
        require(suite == "basic" || suite == "advanced" || suite == "all", "suite must be basic, advanced, or all");
        nlohmann::json summary{{"schema_version", 1},
                               {"status", "running"},
                               {"validation_requested", options.validation},
                               {"suite", suite},
                               {"evidence_origin", "application_readback"},
                               {"runs", nlohmann::json::array()}};
        const auto summary_directory = std::filesystem::path(argv[2]) / "summaries";
        std::filesystem::create_directories(summary_directory);
        const auto summary_path =
            summary_directory /
            ("matrix-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json");
        const auto save = [&] {
            std::ofstream output(summary_path);
            output << summary.dump(2) << '\n';
            require(output.good(), "write GPU matrix summary");
        };
        bool all_passed = true;
        ngm::ExperimentResult reference_run;
        for(int configuration = 0; configuration < 2; ++configuration) {
            options.width = configuration == 0 ? 192 : 257;
            options.height = configuration == 0 ? 128 : 193;
            options.seed = configuration == 0 ? 42 : 0x87654321;
            options.frame = configuration == 0 ? 2 : 5;
            std::map<std::string, ngm::Image> references;
            std::map<std::string, std::filesystem::path> reference_reports;
            for(std::size_t index = 0; index < scenarios.size(); ++index) {
                if((suite == "basic" && index >= 4) || (suite == "advanced" && index < 4)) {
                    continue;
                }
                const auto& scenario = scenarios[index];
                options.scenario = scenario.name;
                const auto first = ngm::run_experiment(options);
                const auto second = ngm::run_experiment(options);
                nlohmann::json row{{"scenario", scenario.name},
                                   {"reference_scenario", scenario.reference},
                                   {"inputs", first.report.at("inputs")},
                                   {"first_report", (first.directory / "report.json").string()},
                                   {"second_report", (second.directory / "report.json").string()},
                                   {"status", "fail"}};
                bool validation_clean = true;
                if(options.validation) {
                    for(const auto& run : {first.directory, second.directory}) {
                        // Keep full diagnostics in the retained run. A layer may
                        // log on either stream depending on loader settings.
                        validation_clean &= validation_logs_clean(run);
                    }
                    row["synchronization_validation_active_and_clean"] = validation_clean;
                }
                if(first.report.at("status") == "pass" && second.report.at("status") == "pass") {
                    const auto image = ngm::read_ppm(first.directory / "output/image.ppm");
                    const auto repeated = ngm::read_ppm(second.directory / "output/image.ppm");
                    const auto difference = ngm::compare_images(image, repeated, 0);
                    row["repeated_differing_pixels"] = difference.differing_pixels;
                    row["ideal"] = check_ideal(image, options, scenario);
                    bool different_from_reference = true;
                    if(options.scenario == scenario.reference) {
                        references[scenario.reference] = image;
                        reference_reports[scenario.reference] = first.directory / "report.json";
                        if(options.scenario == "reference") {
                            reference_run = first;
                        }
                    } else if(references.contains(scenario.reference)) {
                        const auto delta = ngm::compare_images(references.at(scenario.reference), image, 1);
                        row["different_from_reference_pixels"] = delta.differing_pixels;
                        row["reference_report"] = reference_reports.at(scenario.reference).string();
                        different_from_reference = delta.differing_pixels > image.width * image.height / 10;
                    } else {
                        different_from_reference = false;
                    }
                    if(difference.differing_pixels == 0 && row["ideal"]["failed_pixels"] == 0 &&
                       different_from_reference && validation_clean) {
                        row["status"] = "pass";
                    }
                } else {
                    row["first_status"] = first.report.at("status");
                    row["second_status"] = second.report.at("status");
                    if(first.report.at("status") == "unsupported" && second.report.at("status") == "unsupported") {
                        row["status"] = "unsupported";
                    }
                }
                all_passed &= row["status"] == "pass";
                summary["runs"].push_back(row);
                save();
                std::cout << scenario.name << " " << options.width << 'x' << options.height << ": " << row["status"]
                          << '\n';
            }
        }
        if(!reference_run.directory.empty()) {
            const auto override_directory = summary_directory / (summary_path.stem().string() + "-shader-override");
            std::filesystem::copy(reference_run.directory / "output/shaders", override_directory,
                                  std::filesystem::copy_options::recursive);
            auto bundle =
                nlohmann::json::parse(ngm::read_regular_file(override_directory / "provenance.json", 1024 * 1024));
            bundle["build"]["compiler_id"] = "foreign shader bundle";
            bundle["project_version"] = "shader-bundle-only";
            {
                std::ofstream output(override_directory / "provenance.json");
                output << bundle.dump(2) << '\n';
                require(output.good(), "write alternate shader bundle metadata");
            }
            options.scenario = "reference";
            options.shader_directory = override_directory;
            const auto overridden = ngm::run_experiment(options);
            bool separated = false;
            if(overridden.report.at("status") == "pass") {
                const auto& actual = overridden.report.at("result");
                separated = actual.at("application") == reference_run.report.at("result").at("application") &&
                            actual.at("provenance").at("build").at("compiler_id") == "foreign shader bundle" &&
                            actual.at("provenance").at("project_version") == "shader-bundle-only" &&
                            overridden.report.at("image").at("sha256") == reference_run.report.at("image").at("sha256");
            }
            summary["shader_override"] = {{"status", separated ? "pass" : "fail"},
                                          {"report", (overridden.directory / "report.json").string()}};
            if(options.validation) {
                const bool clean = validation_logs_clean(overridden.directory);
                summary["shader_override"]["synchronization_validation_active_and_clean"] = clean;
                separated &= clean;
                summary["shader_override"]["status"] = separated ? "pass" : "fail";
            }
            all_passed &= separated;
        }
        summary["status"] = all_passed ? "pass" : "fail";
        save();
        std::cout << "GPU matrix: " << summary_path << '\n';
        require(all_passed, "GPU fixture matrix; inspect retained reports and private analytic oracle results");
    });
}
