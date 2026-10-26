#include "ShaderTraceMcp.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/Image.hpp"
#include "ngm/Process.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

namespace {
using namespace ngm::check::shader_trace;
using ngm::check::require;
constexpr std::uint32_t width = 192, height = 128, seed = 42;
void oracle(const Json& page) {
    require(page.at("records_total") == 4 && page.at("attempted_records") == 4 && page.at("dropped_records") == 0 &&
                page.at("producer_claims_verified") == false && page.at("original_execution") == false,
            "four actual fixture probes retain their application-reported scope");
    const auto frame = page.at("records")[0].at("frame").get<std::uint32_t>();
    const auto px = static_cast<float>(width / 2) + 0.5f;
    const auto py = static_cast<float>(height / 2) + 0.5f;
    // Independent raster barycentrics from the fixture's declared geometry.
    const double c = (py - height * 0.125) / (height * 0.75);
    const double b = (px - width * 0.1 - c * width * 0.4) / (width * 0.8);
    const double a = 1 - b - c;
    const std::array<double, 3> color{a + (b + c) * 0.125, a * 0.125 + b + c * 0.0625, (a + b) * 0.0625 + c};
    const auto factor = 0.8f + 0.02f * static_cast<float>(frame % 11);
    std::array<float, 4> tint{0, 0, 0, 1};
    for(unsigned n = 0; n < 3; ++n)
        tint[n] = (0.65f + 0.35f * (static_cast<float>((seed >> (n * 8)) & 255u) / 255.0f)) * factor;
    std::set<std::uint32_t> seen;
    for(const auto& row : page.at("records")) {
        require(row.at("frame") == frame && row.at("draw") == 0 && row.at("pixel_x") == width / 2 &&
                    row.at("pixel_y") == height / 2 && row.at("sample") == 0 && row.at("primitive") == 0,
                "fixture selection matches one known non-helper triangle fragment");
        const auto probe = row.at("probe").get<std::uint32_t>();
        require(seen.insert(probe).second && probe < 4, "each requested probe was written once");
        const auto& bits = row.at("bits");
        if(probe == 0)
            require(bits == Json({std::bit_cast<std::uint32_t>(px), std::bit_cast<std::uint32_t>(py)}),
                    "GPU fragment-coordinate bits match exact pixel centers");
        else if(probe == 2) {
            for(unsigned n = 0; n < 4; ++n)
                require(bits[n] == std::bit_cast<std::uint32_t>(tint[n]),
                        "GPU palette-load bits match independently generated CPU inputs");
        } else {
            for(unsigned n = 0; n < bits.size(); ++n) {
                const auto actual = std::bit_cast<float>(bits[n].get<std::uint32_t>());
                const auto expected = n == 3 ? 1.0 : probe == 1 ? color[n] : color[n] * tint[n];
                require(std::isfinite(actual) && std::abs(actual - expected) <= 2e-6,
                        "GPU interpolated/output value matches independent raster oracle within 2e-6");
            }
        }
    }
}
void integration(const fs::path& server, const fs::path& fixture, const fs::path& shaders, const fs::path& nsight) {
    Scratch scratch;
    const auto store = scratch.path / "store";
    auto environment = ngm::capture_environment();
    std::vector<std::string> extra;
    for(const auto& [key, value] : environment)
        extra.push_back(key + '=' + value);
    Client client(server.string(), "/usr/bin:/bin",
                  {"--nsight-root", nsight.string(), "--artifact-root", store.string()}, false, extra,
                  ngm::check::mcp::ClientTimeouts{30s, 30s});
    initialize(client);
    const auto submission = call(client, "capture",
                                 {{"executable", fixture.string()},
                                  {"working_directory", scratch.path.string()},
                                  {"arguments",
                                   {"--scenario", "shader-trace", "--seed", "42", "--width", "192", "--height", "128",
                                    "--frame", "20", "--shader-dir", shaders.string()}},
                                  {"capture_frame", 3},
                                  {"application_output_option", "--output"},
                                  {"timeout_ms", 120000}});
    const auto status = completed(client, submission, 150s);
    require(status.at("state") == "succeeded", "real Nsight trace capture succeeds: " + status.at("error").dump());
    const auto page = call(client, "capture_shader_trace", {{"capture_id", submission.at("artifact_id")}});
    oracle(page);
    const auto frame = page.at("records")[0].at("frame").get<std::uint32_t>();
    const auto module = store / "bundles" / submission.at("artifact_id").get<std::string>() / "raw/application";
    const auto traced_image = module / ("shader-trace-image-" + std::to_string(frame) + ".ppm");
    ngm::ProcessOptions reference;
    reference.executable = fixture;
    reference.arguments = {"--scenario",   "reference",
                           "--seed",       "42",
                           "--width",      "192",
                           "--height",     "128",
                           "--frame",      std::to_string(frame),
                           "--shader-dir", shaders.string(),
                           "--output",     (scratch.path / "reference").string()};
    reference.environment = std::move(environment);
    reference.working_directory = scratch.path;
    reference.stdout_path = scratch.path / "reference.stdout";
    reference.stderr_path = scratch.path / "reference.stderr";
    reference.timeout = 30s;
    const auto outcome = ngm::run_process(reference);
    require(outcome.exit_code == 0 && outcome.cleanup_confirmed && !outcome.timed_out && !outcome.cancelled,
            "fresh ordinary reference finishes with confirmed cleanup");
    const auto difference =
        ngm::compare_images(ngm::read_ppm(traced_image), ngm::read_ppm(scratch.path / "reference/image.ppm"), 0);
    require(difference.differing_pixels == 0,
            "instrumented fixture image matches same-frame ordinary reference exactly");
    const auto selected = call(client, "capture_shader_trace",
                               {{"capture_id", submission.at("artifact_id")}, {"frame", frame}, {"probe", 2}});
    require(selected.at("matched_records") == 1 &&
                selected.at("records")[0].at("bits") == page.at("records")[2].at("bits"),
            "real MCP probe selection preserves GPU readback words");
    require(client.finish() == 0, "real MCP session exits after owned GPU cleanup");
    std::cout << Json{{"status", "pass"},
                      {"scope", "one fresh Nsight capture; fixture instrumented GPU values, not original registers"},
                      {"application_frame", frame},
                      {"records", page.at("records")},
                      {"reference_differing_pixels", difference.differing_pixels},
                      {"interpolation_absolute_tolerance", 2e-6}}
                     .dump(2)
              << '\n';
}
} // namespace
int main(int argc, char** argv) {
    if(argc != 5 || std::string_view(argv[4]).empty() || std::getenv("DISPLAY") == nullptr) {
        std::cerr << "unsupported: requires server, fixture, shader directory, explicit Nsight root and DISPLAY\n";
        return 77;
    }
    return ngm::check::run([&] {
        integration(fs::absolute(argv[1]), fs::absolute(argv[2]), fs::absolute(argv[3]), fs::absolute(argv[4]));
    });
}
