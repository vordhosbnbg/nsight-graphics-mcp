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
        for(const auto* name : {"scene.vert", "scene.frag", "shader-error.frag"}) {
            const std::string source = name;
            std::ofstream(directory / "shaders" / source) << "test";
            std::ofstream(directory / "shaders" / (source + ".spv")) << "test";
            result["provenance"]["shaders"].push_back({{"source", source},
                                                       {"spirv", source + ".spv"},
                                                       {"stage", source == "scene.vert" ? "vertex" : "fragment"},
                                                       {"source_sha256", test_digest},
                                                       {"spirv_sha256", test_digest}});
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
        std::ofstream(directory / "result.json") << result.dump();
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
