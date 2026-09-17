#include "Check.hpp"
#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

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

// Private oracle: expected diagnoses and ideal image math stay in the harness,
// outside renderer output and MCP evidence. Compare interior pixels separately
// from rasterization edges, whose exact coverage may vary across GPUs.
nlohmann::json check_ideal(const ngm::Image& image, const ngm::ExperimentOptions& options) {
    const std::array<std::array<double, 3>, 3> colors{{{1, .125, .0625}, {.125, 1, .0625}, {.125, .0625, 1}}};
    const std::array<double, 3> background{.03125, .0625, .09375};
    std::array<double, 3> tint{};
    for(std::size_t i = 0; i < tint.size(); ++i) {
        tint[i] = (.65 + .35 * ((options.seed >> (i * 8)) & 255) / 255.0) * (.8 + .02 * (options.frame % 11));
    }
    if(options.scenario == "binding-error") {
        tint = {tint[2] * .25, tint[0], tint[1] * .5};
    }
    std::uint64_t checked = 0;
    std::uint64_t failed = 0;
    int maximum = 0;
    for(std::uint32_t y = 0; y < image.height; ++y) {
        for(std::uint32_t x = 0; x < image.width; ++x) {
            const double nx = 2.0 * (x + .5) / image.width - 1;
            const double ny = 2.0 * (y + .5) / image.height - 1;
            const double w2 = (ny + .75) / 1.5;
            const double w1 = (nx + .8 - .8 * w2) / 1.6;
            const double w0 = 1 - w1 - w2;
            const auto minimum = std::min({w0, w1, w2});
            if(std::abs(minimum) < 2.0 / std::min(image.width, image.height)) {
                continue;
            }
            auto expected = background;
            if(minimum > 0) {
                std::array<double, 3> interpolated{};
                for(std::size_t i = 0; i < 3; ++i) {
                    interpolated[i] = w0 * colors[0][i] + w1 * colors[1][i] + w2 * colors[2][i];
                }
                if(options.scenario == "shader-error") {
                    std::swap(interpolated[0], interpolated[2]);
                }
                for(std::size_t i = 0; i < 3; ++i) {
                    expected[i] = interpolated[i] * tint[i];
                }
                if(options.scenario == "pipeline-error") {
                    expected[0] = background[0];
                }
            }
            bool bad = false;
            for(std::size_t channel = 0; channel < 3; ++channel) {
                const int ideal = static_cast<int>(std::lround(std::clamp(expected[channel], 0.0, 1.0) * 255));
                const int actual = image.rgb[(static_cast<std::size_t>(y) * image.width + x) * 3 + channel];
                const int difference = std::abs(ideal - actual);
                maximum = std::max(maximum, difference);
                bad |= difference > 1;
            }
            ++checked;
            failed += bad;
        }
    }
    return {
        {"checked_pixels", checked},         {"failed_pixels", failed},
        {"max_channel_difference", maximum}, {"channel_tolerance", 1},
        {"units", "RGB8 channel steps"},     {"edge_exclusion_barycentric", 2.0 / std::min(image.width, image.height)}};
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 3 || (argc == 4 && std::string(argv[3]) == "--validation"),
                "expected real fixture and artifact root, optionally --validation");
        ngm::ExperimentOptions options;
        options.fixture = argv[1];
        options.output_root = argv[2];
        options.validation = argc == 4;
        nlohmann::json summary{{"schema_version", 1},
                               {"status", "running"},
                               {"validation_requested", options.validation},
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
            ngm::Image reference;
            for(const auto* scenario : {"reference", "shader-error", "binding-error", "pipeline-error"}) {
                options.scenario = scenario;
                const auto first = ngm::run_experiment(options);
                const auto second = ngm::run_experiment(options);
                nlohmann::json row{{"scenario", scenario},
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
                    row["ideal"] = check_ideal(image, options);
                    bool different_from_reference = true;
                    if(options.scenario == "reference") {
                        reference = image;
                        reference_run = first;
                    } else if(!reference.rgb.empty()) {
                        const auto delta = ngm::compare_images(reference, image, 1);
                        row["different_from_reference_pixels"] = delta.differing_pixels;
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
                }
                all_passed &= row["status"] == "pass";
                summary["runs"].push_back(row);
                save();
                std::cout << scenario << " " << options.width << 'x' << options.height << ": " << row["status"] << '\n';
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
