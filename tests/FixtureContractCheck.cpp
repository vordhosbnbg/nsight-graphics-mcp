#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Process.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;

void write_file(const fs::path& path, const std::string& contents) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.exceptions(std::ios::badbit | std::ios::failbit);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.close();
}

class Scratch {
public:
    explicit Scratch(const fs::path& root) {
        fs::create_directories(root);
        auto pattern = (fs::absolute(root) / "fixture contract XXXXXX").string();
        const auto created = mkdtemp(pattern.data());
        require(created != nullptr, "create isolated fixture contract scratch directory");
        path = created;
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
    fs::path path;
};

struct Case {
    ngm::ProcessOptions process;
    fs::path shaders;
    fs::path output;
};

Case make_case(const fs::path& executable, const fs::path& root, const std::string& name) {
    const auto directory = root / name;
    fs::create_directory(directory);
    fs::create_directory(directory / "home");
    Case test;
    test.shaders = directory / "shader bundle";
    test.output = directory / "application output";
    fs::create_directory(test.shaders);
    test.process.executable = executable;
    test.process.working_directory = directory;
    test.process.stdout_path = directory / "stdout.log";
    test.process.stderr_path = directory / "stderr.log";
    // This is the entire environment. No DISPLAY, WAYLAND_DISPLAY, inherited
    // Vulkan configuration, or parent HOME reaches any invocation. A valid
    // bundle must reach the missing-display check before loader initialization.
    test.process.environment = {{"HOME", (directory / "home").string()}, {"LANG", "C"}, {"LC_ALL", "C"}};
    test.process.arguments = {"--scenario",   "reference",
                              "--seed",       "42",
                              "--width",      "64",
                              "--height",     "48",
                              "--frame",      "2",
                              "--output",     test.output.string(),
                              "--shader-dir", test.shaders.string()};
    test.process.timeout = 3s;
    test.process.terminate_grace = 30ms;
    return test;
}

void copy_bundle(const fs::path& source, const fs::path& destination) {
    fs::copy_file(source / "provenance.json", destination / "provenance.json");
    for(const auto* name :
        {"scene.vert", "scene.frag", "shader-error.frag", "indirect.vert", "bindless.frag", "post.vert", "post.frag",
         "compute-reference.comp", "compute-index-error.comp", "compute-arithmetic-error.comp"}) {
        fs::copy_file(source / name, destination / name);
        fs::copy_file(source / (std::string(name) + ".spv"), destination / (std::string(name) + ".spv"));
    }
}

Json manifest(const Case& test) {
    return Json::parse(ngm::read_regular_file(test.shaders / "provenance.json", 1024 * 1024));
}

void set_argument(Case& test, std::string_view key, const std::string& value) {
    const auto position = std::find(test.process.arguments.begin(), test.process.arguments.end(), key);
    require(position != test.process.arguments.end() && position + 1 != test.process.arguments.end(),
            "test argument to replace exists");
    *(position + 1) = value;
}

void require_failure(const Case& test, int exit_code, std::string_view diagnostic, bool writes_result = true) {
    const auto started = std::chrono::steady_clock::now();
    const auto result = ngm::run_process(test.process);
    const auto errors = ngm::read_regular_file(test.process.stderr_path, 64 * 1024);
    const auto name = test.process.working_directory.filename().string();
    require(result.error.empty(), name + ": no supervisor error: " + result.error);
    require(result.exit_code == exit_code,
            name + ": expected exit " + std::to_string(exit_code) + ", stderr: " + errors);
    require(!result.signal && !result.timed_out && !result.cancelled && result.cleanup_confirmed,
            name + ": bounded clean application failure");
    require(std::chrono::steady_clock::now() - started < test.process.timeout,
            name + ": rejection did not depend on the process timeout");
    require(errors.find(diagnostic) != std::string::npos, name + ": actionable diagnostic: " + errors);
    require(ngm::read_regular_file(test.process.stdout_path, 64 * 1024).empty(), name + ": stdout stays empty");
    require(!fs::exists(test.output / "image.ppm"), name + ": failure creates no image evidence");
    if(writes_result) {
        const auto result_path = test.output / "result.json";
        const auto document = Json::parse(ngm::read_regular_file(result_path, 1024 * 1024));
        require(document.at("status") == (exit_code == 3 ? "unsupported" : "failed"),
                name + ": retained result cannot claim success");
        require(!document.contains("gpu") && !document.contains("image"),
                name + ": no GPU or image result before initialization");
        require(document.at("error").is_string(), name + ": retained failure explains the cause");
        require(!fs::exists(test.output / "result.json.tmp"), name + ": failure result publication completed");
    }
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 4, "usage: FixtureContractCheck <fixture> <built shader directory> <scratch root>");
        const auto executable = fs::absolute(argv[1]);
        const auto built_shaders = fs::absolute(argv[2]);
        Scratch scratch(argv[3]);

        for(const auto* scenario : {"compute-reference", "compute-index-error", "compute-arithmetic-error"}) {
            auto compute = make_case(executable, scratch.path, std::string("compute provenance ") + scenario);
            copy_bundle(built_shaders, compute.shaders);
            set_argument(compute, "--scenario", scenario);
            std::ofstream(compute.shaders / (std::string(scenario) + ".comp.spv")) << "tampered";
            require_failure(compute, 1, "SHA-256 mismatch");
        }
        auto too_many = make_case(executable, scratch.path, "compute element bound");
        set_argument(too_many, "--scenario", "compute-reference");
        set_argument(too_many, "--width", "4096");
        require_failure(too_many, 2, "element count", false);
        auto bad_boundary = make_case(executable, scratch.path, "graphics extension boundary");
        bad_boundary.process.arguments.insert(bad_boundary.process.arguments.end(),
                                              {"--compute-boundary", "vk_frame_boundary"});
        require_failure(bad_boundary, 2, "compute boundaries require", false);
        auto mixed_control = make_case(executable, scratch.path, "compute SDK boundary");
        set_argument(mixed_control, "--scenario", "compute-reference");
        mixed_control.process.arguments.insert(mixed_control.process.arguments.end(),
                                               {"--sdk-first-boundary-frame", "0"});
        require_failure(mixed_control, 2, "SDK control is graphics-only", false);
        auto display = make_case(executable, scratch.path, "missing display");
        copy_bundle(built_shaders, display.shaders);
        require_failure(display, 3, "DISPLAY is unset");
        const auto rejected = Json::parse(ngm::read_regular_file(display.output / "result.json", 1024 * 1024));
        require(rejected.at("application").at("executable_sha256") == ngm::sha256_file(executable),
                "standalone result identifies the running executable even when display prerequisites fail");
        require(rejected.at("application").at("build").at("compile_commands").size() == 7,
                "application identity contains actual fixture/core compiler commands");
        require(rejected.at("provenance").at("kind") == "shader_bundle",
                "shader provenance has a distinct identity kind");
        require(rejected.at("sdk_control").at("requested") == false &&
                    rejected.at("sdk_control").at("status") == "not_requested" &&
                    !fs::exists(display.output / "sdk-control.json"),
                "ordinary fixture launch performs no SDK initialization or boundary calls");

        auto sdk = make_case(executable, scratch.path, "sdk request without injection");
        copy_bundle(built_shaders, sdk.shaders);
        sdk.process.arguments.insert(sdk.process.arguments.end(), {"--sdk-first-boundary-frame", "0"});
        require_failure(sdk, 3, "SDK");
        const auto sdk_report = Json::parse(ngm::read_regular_file(sdk.output / "sdk-control.json", 64 * 1024));
        require(sdk_report.at("requested") == true && sdk_report.at("initialized_before_vulkan_instance") == false &&
                    sdk_report.at("boundaries_entered") == 0 && sdk_report.at("boundaries_completed") == 0 &&
                    (sdk_report.at("status") == "not_compiled" || sdk_report.at("status") == "unavailable"),
                "missing SDK build/injection fails before Vulkan and preserves explicit application evidence");
        require(sdk_report.at("application_context").at("application").at("executable_sha256") ==
                        ngm::sha256_file(executable) &&
                    sdk_report.at("application_context").at("application").at("build").at("nsight_sdk") ==
                        sdk_report.at("sdk_build") &&
                    sdk_report.at("application_context").at("inputs").at("scenario") == "reference" &&
                    sdk_report.at("application_context").at("shader_bundle").at("shaders").size() == 10,
                "SDK report preserves build/workload/shader context before initialization or capture termination");

        for(const auto* value : {"-1", "1", "600", "4294967296", "1x"}) {
            auto invalid_sdk = make_case(executable, scratch.path, std::string("invalid sdk frame ") + value);
            invalid_sdk.process.arguments.insert(invalid_sdk.process.arguments.end(),
                                                 {"--sdk-first-boundary-frame", value});
            require_failure(invalid_sdk, 2, "unsupported arguments", false);
            require(!fs::exists(invalid_sdk.output), "invalid SDK selection fails before creating output");
        }

        require(rejected.at("workload").at("required_api_features").empty(),
                "basic rendering never requires optional bindless features");
        for(const auto* scenario :
            {"multipass-reference", "pass-output-error", "bindless-reference", "resource-selection-error",
             "indirect-reference", "indirect-parameter-error", "combined-reference", "combined-pass-error",
             "combined-resource-error", "combined-indirect-error"}) {
            auto advanced = make_case(executable, scratch.path, std::string("advanced input ") + scenario);
            copy_bundle(built_shaders, advanced.shaders);
            set_argument(advanced, "--scenario", scenario);
            require_failure(advanced, 3, "DISPLAY is unset");
            const auto record = Json::parse(ngm::read_regular_file(advanced.output / "result.json", 1024 * 1024));
            const auto& requirements = record.at("workload");
            const std::string_view selected(scenario);
            const bool combined = selected.starts_with("combined-");
            const bool bindless =
                combined || selected == "bindless-reference" || selected == "resource-selection-error";
            const bool multipass = combined || selected == "multipass-reference" || selected == "pass-output-error";
            const bool indirect =
                combined || selected == "indirect-reference" || selected == "indirect-parameter-error";
            require(requirements.at("bindless_storage_buffers") == bindless &&
                        requirements.at("post_processing") == multipass &&
                        requirements.at("offscreen_render_target") == multipass &&
                        requirements.at("indirect_draw") == indirect,
                    "each advanced scenario preserves its requested workload even when display is unavailable");
            require(requirements.at("required_api_features") ==
                        (bindless ? Json({"runtimeDescriptorArray", "shaderStorageBufferArrayNonUniformIndexing"})
                                  : Json::array()),
                    "only bindless scenarios require the exact descriptor-indexing features");
            require(record.at("provenance").at("shaders").size() == 10,
                    "advanced artifacts retained before display check");
            require(record.at("device_support").empty(), "no device support claimed before a display was opened");
        }
        auto invalid_scenario = make_case(executable, scratch.path, "unsupported advanced scenario name");
        set_argument(invalid_scenario, "--scenario", "combined-unrecognized");
        require_failure(invalid_scenario, 2, "unknown scenario", false);

        auto malformed = make_case(executable, scratch.path, "malformed manifest");
        write_file(malformed.shaders / "provenance.json", "{ not JSON\n");
        require_failure(malformed, 1, "parse_error");

        // Syntactically valid metadata must still identify the retained shader
        // build. Exercise each metadata class before the display check, while
        // leaving shader bytes and their content hashes valid.
        const std::vector<std::pair<std::string, Json>> invalid_metadata{
            {"/project_version", nullptr},
            {"/source_revision", Json::array()},
            {"/project_version", ""},
            {"/source_dirty", "false"},
            {"/build", nullptr},
            {"/build/compiler_id", Json::array()},
            {"/build/compiler_version", ""},
            {"/build/build_type", true},
            {"/build/cxx_flags", Json::array()},
            {"/shader_compiler", nullptr},
            {"/shader_compiler/version", nullptr},
            {"/shader_compiler/sha256", "1234"},
            {"/shader_compiler/sha256", std::string(64, 'z')},
            {"/shader_compiler/arguments", nullptr},
            {"/shader_compiler/arguments", Json::array()},
            {"/shader_compiler/arguments", Json::array({"-V", 7})}};
        std::size_t metadata_case = 0;
        for(const auto& [pointer, value] : invalid_metadata) {
            auto invalid = make_case(executable, scratch.path, "malformed metadata " + std::to_string(metadata_case++));
            copy_bundle(built_shaders, invalid.shaders);
            auto document = manifest(invalid);
            document[Json::json_pointer(pointer)] = value;
            write_file(invalid.shaders / "provenance.json", document.dump());
            require_failure(invalid, 1, "invalid shader provenance metadata");
        }
        auto unknown_build = make_case(executable, scratch.path, "unknown checkout with empty compiler flags");
        copy_bundle(built_shaders, unknown_build.shaders);
        auto unknown_identity = manifest(unknown_build);
        unknown_identity["source_revision"] = "unknown";
        unknown_identity["source_dirty"] = nullptr;
        unknown_identity["build"]["cxx_flags"] = "";
        unknown_identity["build"]["compiler_id"] = "independent shader bundle compiler";
        write_file(unknown_build.shaders / "provenance.json", unknown_identity.dump());
        require_failure(unknown_build, 3, "DISPLAY is unset");

        auto deep = make_case(executable, scratch.path, "deep manifest");
        write_file(deep.shaders / "provenance.json", std::string(80, '[') + "0" + std::string(80, ']'));
        require_failure(deep, 1, "nesting exceeds limit");

        auto nul = make_case(executable, scratch.path, "literal NUL manifest");
        write_file(nul.shaders / "provenance.json", std::string("{}\0", 3));
        require_failure(nul, 1, "NUL byte");

        auto escaped_nul = make_case(executable, scratch.path, "escaped NUL manifest");
        write_file(escaped_nul.shaders / "provenance.json", "{\"value\":\"escaped\\u0000value\"}");
        require_failure(escaped_nul, 1, "NUL byte");

        auto oversized = make_case(executable, scratch.path, "oversized manifest");
        write_file(oversized.shaders / "provenance.json", std::string(1024 * 1024 + 1, ' '));
        require_failure(oversized, 1, "regular file within the size limit");

        auto fifo = make_case(executable, scratch.path, "FIFO manifest");
        require(mkfifo((fifo.shaders / "provenance.json").c_str(), 0600) == 0, "create manifest FIFO with no writer");
        require_failure(fifo, 1, "regular file within the size limit");

        for(const auto* artifact :
            {"scene.frag", "scene.vert.spv", "bindless.frag", "post.frag.spv", "indirect.vert.spv"}) {
            auto corrupted = make_case(executable, scratch.path, std::string("corrupted ") + artifact);
            copy_bundle(built_shaders, corrupted.shaders);
            auto bytes = ngm::read_regular_file(corrupted.shaders / artifact, 64 * 1024 * 1024);
            bytes.push_back('\n');
            write_file(corrupted.shaders / artifact, bytes);
            require_failure(corrupted, 1, std::string("SHA-256 mismatch: ") + artifact);
        }

        auto missing = make_case(executable, scratch.path, "missing shader artifact");
        copy_bundle(built_shaders, missing.shaders);
        require(fs::remove(missing.shaders / "scene.vert.spv"), "remove required SPIR-V artifact");
        require_failure(missing, 1, "scene.vert.spv");

        auto inventory = make_case(executable, scratch.path, "missing required inventory entry");
        copy_bundle(built_shaders, inventory.shaders);
        auto incomplete = manifest(inventory);
        incomplete["shaders"].erase(2);
        write_file(inventory.shaders / "provenance.json", incomplete.dump());
        require_failure(inventory, 1, "missing required artifact shader-error.frag");

        auto advanced_inventory = make_case(executable, scratch.path, "missing advanced inventory entry");
        copy_bundle(built_shaders, advanced_inventory.shaders);
        auto missing_advanced = manifest(advanced_inventory);
        missing_advanced["shaders"].erase(4);
        write_file(advanced_inventory.shaders / "provenance.json", missing_advanced.dump());
        require_failure(advanced_inventory, 1, "missing required artifact bindless.frag");

        auto invalid = make_case(executable, scratch.path, "invalid CLI range");
        set_argument(invalid, "--frame", "601");
        require_failure(invalid, 2, "frame must be in the range", false);
        require(!fs::exists(invalid.output), "invalid arguments never create the requested output");

        auto duplicate = make_case(executable, scratch.path, "duplicate CLI option");
        duplicate.process.arguments.insert(duplicate.process.arguments.end(), {"--seed", "7"});
        require_failure(duplicate, 2, "duplicate option --seed", false);
        require(!fs::exists(duplicate.output), "duplicate arguments never create the requested output");

        auto unknown = make_case(executable, scratch.path, "unknown CLI option");
        unknown.process.arguments.insert(unknown.process.arguments.end(), {"--invalid", "value"});
        require_failure(unknown, 2, "unknown option --invalid", false);
        require(!fs::exists(unknown.output), "unknown arguments never create the requested output");

        auto existing = make_case(executable, scratch.path, "existing output");
        fs::create_directory(existing.output);
        const std::string previous = "{\"previous_evidence\":true}\n";
        write_file(existing.output / "result.json", previous);
        require_failure(existing, 1, "output directory already exists", false);
        require(ngm::read_regular_file(existing.output / "result.json", 1024) == previous,
                "existing output evidence is preserved byte-for-byte");
    });
}
