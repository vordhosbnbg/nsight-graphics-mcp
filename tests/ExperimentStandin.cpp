#include "ngm/Hash.hpp"
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <thread>

// CPU-only executable contract stand-in. It never initializes Vulkan or Nsight.
int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> values;
        for(int i = 1; i + 1 < argc; i += 2) {
            values[argv[i]] = argv[i + 1];
        }
        const auto& scenario = values.at("--scenario");
        std::cout << "fixture stand-in stdout\n";
        std::cerr << "fixture stand-in stderr\n";
        if(scenario == "failure") {
            return 1;
        }
        if(scenario == "unsupported") {
            return 3;
        }
        if(scenario == "hang") {
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }
        const auto directory = std::filesystem::path(values.at("--output"));
        std::filesystem::create_directory(directory);
        if(scenario == "malformed-result") {
            std::ofstream(directory / "result.json") << "{not json";
            return 0;
        }
        const auto width = std::stoul(values.at("--width"));
        const auto height = std::stoul(values.at("--height"));
        nlohmann::json result{
            {"schema_version", 1},
            {"status", "pass"},
            {"evidence_origin", "application_readback"},
            {"inputs",
             {{"scenario", scenario},
              {"seed", std::stoul(values.at("--seed"))},
              {"width", width},
              {"height", height},
              {"frame", std::stoul(values.at("--frame"))}}},
            {"gpu", {{"status", "test_standin"}}},
            {"desktop", {{"status", "test_standin"}}},
            {"provenance", {{"status", "test_standin"}}},
            {"image", {{"path", "image.ppm"}, {"width", width}, {"height", height}, {"format", "P6_RGB8"}}},
            {"test_environment",
             {{"home", std::getenv("HOME") ? std::getenv("HOME") : ""},
              {"unrelated_variable_inherited", std::getenv("NGM_PARENT_ONLY") != nullptr}}}};
        result["project_version"] = "test-only";
        result["gpu"] = {{"name", "CPU stand-in; not a GPU result"},
                         {"api_version", "test-only"},
                         {"driver_name", "test-only"},
                         {"driver_info", ""},
                         {"device_uuid", "test-only"},
                         {"driver_uuid", "test-only"},
                         {"vendor_id", 0},
                         {"device_id", 0},
                         {"device_type", 0},
                         {"driver_id", 0},
                         {"driver_version_raw", 0}};
        result["desktop"] = {
            {"backend", "xcb"}, {"display", "test-only"}, {"session_type", "test-only"}, {"wayland_display", ""}};
        const bool combined = scenario.starts_with("combined-");
        const bool multipass = combined || scenario == "multipass-reference" || scenario == "pass-output-error";
        const bool bindless = combined || scenario == "bindless-reference" || scenario == "resource-selection-error";
        const bool indirect = combined || scenario == "indirect-reference" || scenario == "indirect-parameter-error";
        using Json = nlohmann::json;
        result["workload"] = {
            {"offscreen_render_target", multipass},
            {"post_processing", multipass},
            {"bindless_storage_buffers", bindless},
            {"indirect_draw", indirect},
            {"render_pass_count", multipass ? 2 : 1},
            {"minimum_api_version", "1.3.0"},
            {"required_device_extensions", {"VK_KHR_swapchain"}},
            {"required_api_features",
             bindless ? Json({"runtimeDescriptorArray", "shaderStorageBufferArrayNonUniformIndexing"}) : Json::array()},
            {"offscreen_format", multipass ? Json("R8G8B8A8_UNORM") : Json(nullptr)},
            {"offscreen_format_features", multipass ? Json({"COLOR_ATTACHMENT", "SAMPLED_IMAGE"}) : Json::array()},
            {"descriptor_array_count", bindless ? 2 : 0},
            {"indirect_command", indirect ? Json("vkCmdDrawIndirect") : Json(nullptr)},
            {"indirect_draw_count", indirect ? 1 : 0},
            {"indirect_first_instance", 0}};
        result["rendering"] = {
            {"enabled_api_features",
             {{"runtimeDescriptorArray", bindless}, {"shaderStorageBufferArrayNonUniformIndexing", bindless}}}};
        result["device_support"] = {
            {{"name", "CPU stand-in; not a GPU result"},
             {"api_version", "test-only"},
             {"status", "selected"},
             {"missing_requirements", Json::array()},
             {"queried_api_features",
              {{"runtimeDescriptorArray", true}, {"shaderStorageBufferArrayNonUniformIndexing", true}}},
             {"offscreen_rgba8_color_attachment_and_sampled_image", true},
             {"limits",
              {{"maxPerStageDescriptorStorageBuffers", 2},
               {"maxDescriptorSetStorageBuffers", 2},
               {"maxPushConstantsSize", 128},
               {"maxDrawIndirectCount", 1}}}}};
        if(scenario == "missing-workload") {
            result.erase("workload");
        } else if(scenario == "contradictory-workload") {
            result["workload"]["indirect_draw"] = true;
        } else if(scenario == "floating-workload-count") {
            result["workload"]["render_pass_count"] = 1.0;
        } else if(scenario == "missing-device-support") {
            result["device_support"] = Json::array();
        } else if(scenario == "contradictory-feature-enablement") {
            result["rendering"]["enabled_api_features"]["runtimeDescriptorArray"] = true;
        } else if(scenario == "combined-unsupported-feature") {
            result["device_support"][0]["queried_api_features"]["runtimeDescriptorArray"] = false;
        } else if(scenario == "combined-unsupported-format") {
            result["device_support"][0]["offscreen_rgba8_color_attachment_and_sampled_image"] = false;
        } else if(scenario == "combined-unsupported-limit") {
            result["device_support"][0]["limits"]["maxPerStageDescriptorStorageBuffers"] = 1;
        }
        const auto test_digest = ngm::sha256(std::as_bytes(std::span("test", 4)));
        const nlohmann::json build{
            {"schema_version", 1},
            {"project_version", "test-only"},
            {"compiler", "test-only"},
            {"compiler_id", "test-only"},
            {"compiler_version", "test-only"},
            {"build_type", "test-only"},
            {"source_revision", "unknown"},
            {"source_dirty", nullptr},
            {"compile_commands", {{{"directory", "test-only"}, {"file", "test-only"}, {"command", "test-only"}}}},
            {"inputs", {{{"path", "test-only"}, {"sha256", test_digest}}}}};
        result["provenance"] = {
            {"schema_version", 1},
            {"kind", "shader_bundle"},
            {"project_version", "test-only"},
            {"source_revision", "unknown"},
            {"source_dirty", nullptr},
            {"build",
             {{"compiler_id", "test-only"},
              {"compiler_version", "test-only"},
              {"build_type", "test-only"},
              {"cxx_flags", ""}}},
            {"shader_compiler", {{"version", "test-only"}, {"sha256", test_digest}, {"arguments", {"test-only"}}}},
            {"shaders", nlohmann::json::array()}};
        std::filesystem::create_directory(directory / "shaders");
        for(const auto* name :
            {"scene.vert", "scene.frag", "shader-error.frag", "indirect.vert", "bindless.frag", "post.vert",
             "post.frag", "compute-reference.comp", "compute-index-error.comp", "compute-arithmetic-error.comp",
             "performance-reference.comp", "performance-underfilled.comp"}) {
            const std::string source = name;
            std::ofstream(directory / "shaders" / source) << "test";
            std::ofstream(directory / "shaders" / (source + ".spv")) << "test";
            result["provenance"]["shaders"].push_back({{"source", source},
                                                       {"spirv", source + ".spv"},
                                                       {"stage", source.ends_with(".vert")   ? "vertex"
                                                                 : source.ends_with(".comp") ? "compute"
                                                                                             : "fragment"},
                                                       {"source_sha256", test_digest},
                                                       {"spirv_sha256", test_digest}});
            result["provenance"]["shaders"].back()["compilation_profile"] = "diagnostic";
            result["provenance"]["shaders"].back()["compiler_arguments"] = {"-V", "--target-env", "vulkan1.3", "-g",
                                                                            "-Od"};
            if(source.starts_with("performance-")) {
                result["provenance"]["shaders"].back()["compilation_profile"] = "performance";
                result["provenance"]["shaders"].back()["compiler_arguments"] = {"-V", "--target-env", "vulkan1.3",
                                                                                "-g0"};
            }
        }
        if(scenario == "wrong-inputs") {
            result["inputs"]["seed"] = 99;
        }
        if(scenario != "missing-image") {
            std::ofstream image(directory / "image.ppm", std::ios::binary);
            image << "P6\n" << width << ' ' << height << "\n255\n";
            image << std::string(width * height * 3, 'x');
        }
        if(scenario != "missing-image") {
            result["image"]["sha256"] = ngm::sha256_file(directory / "image.ppm");
        }
        result["application"] = {{"executable_sha256", ngm::sha256_file("/proc/self/exe")}, {"build", build}};
        if(scenario == "inconsistent-metadata") {
            result["image"]["width"] = 999;
            result["image"]["sha256"] = "wrong";
        } else if(scenario == "missing-provenance") {
            result.erase("provenance");
        } else if(scenario == "empty-metadata") {
            for(const auto* key : {"gpu", "desktop", "provenance"}) {
                result[key] = nlohmann::json::object();
            }
            result["application"]["build"] = nlohmann::json::object();
        } else if(scenario == "missing-driver") {
            result["gpu"].erase("driver_version_raw");
        } else if(scenario == "tampered-shader") {
            std::ofstream(directory / "shaders/scene.vert") << "altered";
        } else if(scenario == "wrong-executable") {
            result["application"]["executable_sha256"] = test_digest;
        } else if(scenario == "missing-shader-build") {
            result["provenance"].erase("build");
        } else if(scenario == "malformed-shader-build") {
            result["provenance"]["build"]["cxx_flags"] = nlohmann::json::array();
        } else if(scenario == "fifo-image") {
            std::filesystem::remove(directory / "image.ppm");
            if(mkfifo((directory / "image.ppm").c_str(), 0600) != 0) {
                return 2;
            }
        }
        if(result.contains("provenance") && scenario != "missing-shader-manifest") {
            auto manifest = result.at("provenance");
            manifest.erase("kind");
            if(scenario == "contradictory-shader-manifest") {
                manifest["project_version"] = "another shader bundle";
            } else if(scenario == "floating-shader-schema") {
                manifest["schema_version"] = 1.0;
            }
            if(scenario == "fifo-shader-manifest") {
                if(mkfifo((directory / "shaders/provenance.json").c_str(), 0600) != 0) {
                    return 2;
                }
            } else {
                std::ofstream(directory / "shaders/provenance.json") << manifest.dump();
            }
        }
        const bool performance = scenario.starts_with("performance-");
        if(performance)
            result["inputs"]["warmup"] = std::stoul(values.at("--warmup"));
        if(scenario.starts_with("compute-") || performance) {
            const uint32_t count = width * height;
            const uint32_t seed = result["inputs"]["seed"];
            const uint32_t frame = result["inputs"]["frame"];
            result.erase("image");
            result["desktop"]["backend"] = "none";
            result["workload"] = {{"kind", performance ? "compute_xorshift_uint32" : "compute_affine_uint32"},
                                  {"presentation", false},
                                  {"element_count", count},
                                  {"boundary", "none"},
                                  {"minimum_api_version", "1.3.0"},
                                  {"required_device_extensions", nlohmann::json::array()}};
            result["compute"] = {
                {"evidence_origin", "application_observation"},
                {"boundary_enabled", false},
                {"entry_point", "main"},
                {"local_size", {64, 1, 1}},
                {"group_count", {(count + 63) / 64, 1, 1}},
                {"push_constant_count", count},
                {"pipeline_label", performance ? "performance.xorshift.pipeline" : "compute.affine.pipeline"},
                {"shader_label", performance ? "performance.xorshift.shader" : "compute.affine.shader"},
                {"dispatch_label", performance ? "performance.xorshift" : "compute.affine"},
                {"shader_source", "shaders/" + scenario + ".comp"},
                {"shader_spirv", "shaders/" + scenario + ".comp.spv"},
                {"source_sha256", test_digest},
                {"spirv_sha256", test_digest},
                {"descriptor_bindings",
                 nlohmann::json::array(
                     {{{"set", 0}, {"binding", 0}, {"label", "compute.input"}, {"bytes", count * 4}},
                      {{"set", 0}, {"binding", 1}, {"label", "compute.output"}, {"bytes", count * 4}}})}};
            const uint32_t warmup = performance ? result["inputs"]["warmup"].get<uint32_t>() : 0;
            if(performance)
                result["performance"] = {{"evidence_origin", "application_timestamps"},
                                         {"warmup_submits", warmup},
                                         {"measured_submits", frame + 1 - warmup},
                                         {"iterations", 2048},
                                         {"input_policy", "identical input on every submit"},
                                         {"timestamp_valid_bits", 36},
                                         {"timestamp_period_ns", 2.0},
                                         {"unit", "ns"},
                                         {"scope", "CPU synthetic timestamps"},
                                         {"clock_control", "not_requested"},
                                         {"replay", "none"},
                                         {"measurements", Json::array()}};
            auto row = nlohmann::json{{"schema_version", 1},
                                      {"evidence_origin", "application_readback"},
                                      {"phase", "readback_before_frame_end"},
                                      {"frame", frame},
                                      {"frame_boundary_id", nullptr},
                                      {"inputs", result["inputs"]},
                                      {"compute", result["compute"]},
                                      {"executable_sha256", result["application"]["executable_sha256"]},
                                      {"input", nlohmann::json::array()},
                                      {"output", nlohmann::json::array()}};
            for(uint32_t i = 0; i < count; ++i) {
                row["input"].push_back(uint32_t((i * 13u ^ seed) + frame * 7u));
                row["output"].push_back(0u); // Execution evidence alone makes no correctness claim.
            }
            // Corruption occurs in the intermediate frame, while the final
            // frame remains valid, to exercise validation of the entire run.
            const auto name = "compute-frame-" + std::to_string(frame) + ".json";
            for(uint32_t current = 0; current <= frame; ++current) {
                auto observed = row;
                observed["frame"] = current;
                for(uint32_t i = 0; i < count; ++i)
                    observed["input"][i] = uint32_t((i * 13u ^ seed) + (performance ? 0u : current * 7u));
                if(performance) {
                    observed["timing"] = {
                        {"frame", current},          {"warmup", current < warmup}, {"timestamp_start", 68719476730ULL},
                        {"timestamp_end", 4u},       {"elapsed_ticks", 10u},       {"gpu_ns", 20.0},
                        {"host_submission_ns", 400u}};
                    if(current >= warmup)
                        result["performance"]["measurements"].push_back(observed["timing"]);
                    if(current == 0) {
                        if(seed == 200)
                            observed["timing"]["elapsed_ticks"] = 11u;
                        if(seed == 201)
                            observed["timing"]["warmup"] = false;
                        if(seed == 202)
                            observed["timing"]["timestamp_start"] = 68719476730.0;
                        if(seed == 203)
                            observed["timing"]["gpu_ns"] = 19.0;
                        if(seed == 204)
                            result["performance"]["timestamp_valid_bits"] = 0u;
                        if(seed == 205)
                            observed["timing"]["host_submission_ns"] = 137438953472ULL;
                        if(seed == 206)
                            result["performance"]["iterations"] = 1;
                        if(seed == 207)
                            result["performance"]["timestamp_period_ns"] = -2.0;
                        if(seed == 209)
                            observed["timing"]["timestamp_start"] = 137438953466ULL;
                        if(seed == 208)
                            result["performance"]["replay"] = "unknown";
                    }
                }
                if(current == 0) {
                    if(seed == 100)
                        observed["output"].erase(observed["output"].begin());
                    if(seed == 101)
                        observed["output"][0] = -1;
                    if(seed == 102)
                        observed["output"][0] = 4294967296ULL;
                    if(seed == 103)
                        observed["frame"] = current + 1;
                    if(seed == 104)
                        observed["executable_sha256"] = test_digest;
                    if(seed == 105)
                        observed["input"][0] = 0;
                    if(seed == 106)
                        observed["compute"]["group_count"][0] = 1;
                    if(seed == 107)
                        observed["compute"]["shader_spirv"] = "../other.spv";
                    if(seed == 110)
                        observed["output"][0] = 0.5;
                    if(seed == 111)
                        observed["output"].push_back(0u);
                    if(seed == 112)
                        observed["evidence_origin"] = "nsight_export";
                    if(seed == 113)
                        continue;
                }
                const auto path = directory / ("compute-frame-" + std::to_string(current) + ".json");
                if(current == 0 && seed == 114)
                    std::ofstream(path) << "{invalid";
                else
                    std::ofstream(path) << observed.dump();
            }
            result["readback"] = {{"path", name}, {"sha256", ngm::sha256_file(directory / name)}};
            if(seed == 108)
                result["readback"]["sha256"] = test_digest;
            if(seed == 109)
                result["readback"]["path"] = "../outside.json";
        }
        std::ofstream(directory / "result.json") << result.dump();
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
