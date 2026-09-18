#include "ngm/Experiment.hpp"

#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Image.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <stdexcept>
#include <sys/utsname.h>

namespace ngm {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;

void save(const fs::path& path, const Json& value) {
    const auto temporary = fs::path(path.string() + ".tmp");
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << value.dump(2) << '\n';
    output.close();
    if(!output) {
        throw std::runtime_error("Cannot write experiment record: " + path.string());
    }
    fs::rename(temporary, path);
}

fs::path allocate(const fs::path& root) {
    fs::create_directories(root);
    std::random_device random;
    constexpr char hex[] = "0123456789abcdef";
    for(int attempt = 0; attempt < 32; ++attempt) {
        std::string id = "run-";
        for(int word = 0; word < 4; ++word) {
            const auto value = random();
            for(int shift = 28; shift >= 0; shift -= 4) {
                id.push_back(hex[(value >> shift) & 15]);
            }
        }
        const auto directory = root / id;
        if(fs::create_directory(directory)) {
            return directory;
        }
    }
    throw std::runtime_error("Cannot allocate an isolated experiment directory");
}

std::map<std::string, std::string> environment(const fs::path& directory, bool validation) {
    std::map<std::string, std::string> result;
    for(const auto* key : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE",
                           "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP"}) {
        if(const auto* value = std::getenv(key); value && *value) {
            result.emplace(key, value);
        }
    }
    // Preserve only the existing desktop authentication location, never its
    // contents. HOME below is intentionally isolated from normal user config.
    if(!result.contains("XAUTHORITY")) {
        if(const auto* home = std::getenv("HOME")) {
            const auto authority = fs::path(home) / ".Xauthority";
            if(fs::is_regular_file(authority)) {
                result.emplace("XAUTHORITY", fs::absolute(authority).string());
            }
        }
    }
    result["PATH"] = "/usr/bin:/bin";
    result["LANG"] = "C";
    result["LC_ALL"] = "C";
    result["HOME"] = (directory / "config/home").string();
    for(const auto& [key, suffix] : std::array<std::pair<const char*, const char*>, 4>{{{"XDG_CONFIG_HOME", "config"},
                                                                                        {"XDG_DATA_HOME", "data"},
                                                                                        {"XDG_CACHE_HOME", "cache"},
                                                                                        {"XDG_STATE_HOME", "state"}}}) {
        result[key] = (directory / "config" / suffix).string();
    }
    result["TMPDIR"] = (directory / "config/tmp").string();
    // Documented Vulkan loader filter: skip implicit per-user overlays and
    // configuration layers in this fixture-only experiment. Nsight capture
    // adapters must construct their own environment and explicitly enable their
    // required injection path; this is not a generic capture environment.
    result["VK_LOADER_LAYERS_DISABLE"] = "~implicit~";
    if(validation) {
        result["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation";
        result["VK_LAYER_SETTINGS_PATH"] = (directory / "config").string();
        std::ofstream settings(directory / "config/vk_layer_settings.txt");
        settings << "khronos_validation.validate_sync = true\n"
                    "khronos_validation.report_flags = error,warn,info\n"
                    "khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG\n";
        settings.close();
        if(!settings) {
            throw std::runtime_error("Cannot write isolated validation layer settings");
        }
    }
    for(const auto* key : {"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "TMPDIR"}) {
        fs::create_directories(result.at(key));
    }
    return result;
}

Json read_result(const fs::path& path) {
    const auto input = read_regular_file(path, 1024 * 1024);
    if(input.find('\0') != std::string::npos) {
        throw std::runtime_error("NUL byte in fixture result JSON");
    }
    auto result = Json::parse(input, [](int depth, Json::parse_event_t, Json& value) {
        if(depth > 64) {
            throw std::runtime_error("Fixture result nesting exceeds limit");
        }
        if(value.is_string() && value.get_ref<const std::string&>().find('\0') != std::string::npos) {
            throw std::runtime_error("NUL byte in fixture evidence JSON string");
        }
        return true;
    });
    if(!result.is_object()) {
        throw std::runtime_error("Fixture result must be a JSON object");
    }
    return result;
}

void require(bool condition, const std::string& field) {
    if(!condition) {
        throw std::runtime_error("Invalid fixture evidence field: " + field);
    }
}

bool digest(const Json& value) {
    if(!value.is_string()) {
        return false;
    }
    const auto& text = value.get_ref<const std::string&>();
    return text.size() == 64 && std::all_of(text.begin(), text.end(),
                                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

void text_fields(const Json& object, std::initializer_list<const char*> fields, const std::string& prefix) {
    for(const auto* key : fields) {
        require(object.at(key).is_string() && !object.at(key).get_ref<const std::string&>().empty(), prefix + key);
    }
}

void validate_workload(const Json& result) {
    const auto scenario = result.at("inputs").at("scenario").get<std::string>();
    const bool combined = scenario.starts_with("combined-");
    const bool multipass = combined || scenario == "multipass-reference" || scenario == "pass-output-error";
    const bool bindless = combined || scenario == "bindless-reference" || scenario == "resource-selection-error";
    const bool indirect = combined || scenario == "indirect-reference" || scenario == "indirect-parameter-error";
    const auto& workload = result.at("workload");
    for(const auto& [key, expected] :
        std::array<std::pair<const char*, bool>, 4>{{{"offscreen_render_target", multipass},
                                                     {"post_processing", multipass},
                                                     {"bindless_storage_buffers", bindless},
                                                     {"indirect_draw", indirect}}}) {
        require(workload.at(key).is_boolean() && workload.at(key) == expected, std::string("workload.") + key);
    }
    for(const auto& [key, expected] :
        std::array<std::pair<const char*, int>, 4>{{{"render_pass_count", multipass ? 2 : 1},
                                                    {"descriptor_array_count", bindless ? 2 : 0},
                                                    {"indirect_draw_count", indirect ? 1 : 0},
                                                    {"indirect_first_instance", 0}}}) {
        require(workload.at(key).is_number_integer() && workload.at(key) == expected, std::string("workload.") + key);
    }
    const auto features =
        bindless ? Json({"runtimeDescriptorArray", "shaderStorageBufferArrayNonUniformIndexing"}) : Json::array();
    require(workload.at("required_api_features") == features, "workload.required_api_features");
    require(workload.at("minimum_api_version") == "1.3.0" &&
                workload.at("required_device_extensions") == Json({"VK_KHR_swapchain"}),
            "workload.required_api");
    require(workload.at("offscreen_format") == (multipass ? Json("R8G8B8A8_UNORM") : Json(nullptr)) &&
                workload.at("offscreen_format_features") ==
                    (multipass ? Json({"COLOR_ATTACHMENT", "SAMPLED_IMAGE"}) : Json::array()),
            "workload.offscreen_format");
    require(workload.at("indirect_command") == (indirect ? Json("vkCmdDrawIndirect") : Json(nullptr)),
            "workload.indirect_command");
    const auto& support = result.at("device_support");
    require(support.is_array() && !support.empty(), "device_support");
    const Json* selected = nullptr;
    for(const auto& candidate : support) {
        require(candidate.at("status") == "selected" || candidate.at("status") == "rejected", "device_support.status");
        require(candidate.at("missing_requirements").is_array(), "device_support.missing_requirements");
        if(candidate.at("status") == "selected") {
            require(selected == nullptr && candidate.at("missing_requirements").empty(), "device_support.selected");
            selected = &candidate;
        }
    }
    require(selected != nullptr, "device_support.selected");
    require(selected->at("name") == result.at("gpu").at("name") &&
                selected->at("api_version") == result.at("gpu").at("api_version"),
            "device_support.identity");
    for(const auto* key : {"runtimeDescriptorArray", "shaderStorageBufferArrayNonUniformIndexing"}) {
        const auto& enabled = result.at("rendering").at("enabled_api_features").at(key);
        const auto& available = selected->at("queried_api_features").at(key);
        require(enabled.is_boolean() && enabled == bindless && available.is_boolean() &&
                    (!bindless || available == true),
                std::string("rendering.enabled_api_features.") + key);
    }
    if(multipass) {
        require(selected->at("offscreen_rgba8_color_attachment_and_sampled_image") == true,
                "device_support.offscreen_rgba8_color_attachment_and_sampled_image");
    }
    if(bindless || indirect) {
        const auto& limits = selected->at("limits");
        for(const auto& [key, minimum] :
            std::array<std::pair<const char*, int>, 4>{{{"maxPerStageDescriptorStorageBuffers", bindless ? 2 : 0},
                                                        {"maxDescriptorSetStorageBuffers", bindless ? 2 : 0},
                                                        {"maxPushConstantsSize", bindless ? 8 : 0},
                                                        {"maxDrawIndirectCount", indirect ? 1 : 0}}}) {
            require(limits.at(key).is_number_integer() && limits.at(key) >= minimum,
                    std::string("device_support.limits.") + key);
        }
    }
}

void validate_metadata(const Json& result, const fs::path& directory) {
    const bool compute = result.at("inputs").at("scenario").get<std::string>().starts_with("compute-");
    if(!compute)
        validate_workload(result);
    const auto& gpu = result.at("gpu");
    text_fields(gpu, {"name", "api_version", "driver_name", "device_uuid", "driver_uuid"}, "gpu.");
    require(gpu.at("driver_info").is_string(), "gpu.driver_info");
    for(const auto* key : {"vendor_id", "device_id", "device_type", "driver_id", "driver_version_raw"}) {
        const auto& value = gpu.at(key);
        require(value.is_number_integer() && value >= 0 && value <= 0xffffffffULL, std::string("gpu.") + key);
    }
    const auto& desktop = result.at("desktop");
    require(desktop.at("backend") == (compute ? "none" : "xcb"), "desktop.backend");
    if(!compute)
        text_fields(desktop, {"display"}, "desktop.");
    require(desktop.at("session_type").is_string() && desktop.at("wayland_display").is_string(), "desktop.session");

    const auto& build = result.at("application").at("build");
    require(build.at("schema_version").is_number_integer() && build.at("schema_version") == 1,
            "application.build.schema_version");
    text_fields(build,
                {"project_version", "compiler", "compiler_id", "compiler_version", "build_type", "source_revision"},
                "application.build.");
    require(build.at("source_dirty").is_boolean() || build.at("source_dirty").is_null(),
            "application.build.source_dirty");
    require(result.at("project_version") == build.at("project_version"), "application.build.project_version");
    require(build.at("compile_commands").is_array() && !build.at("compile_commands").empty(),
            "application.build.compile_commands");
    for(const auto& command : build.at("compile_commands")) {
        text_fields(command, {"directory", "file", "command"}, "application.build.compile_commands.");
    }
    require(build.at("inputs").is_array() && !build.at("inputs").empty(), "application.build.inputs");
    for(const auto& input : build.at("inputs")) {
        text_fields(input, {"path"}, "application.build.inputs.");
        require(digest(input.at("sha256")), "application.build.inputs.sha256");
    }

    const auto& provenance = result.at("provenance");
    require(provenance.at("schema_version").is_number_integer() && provenance.at("schema_version") == 1 &&
                provenance.at("kind") == "shader_bundle",
            "provenance.kind/schema_version");
    text_fields(provenance, {"project_version", "source_revision"}, "provenance.");
    require(provenance.at("source_dirty").is_boolean() || provenance.at("source_dirty").is_null(),
            "provenance.source_dirty");
    const auto& shader_build = provenance.at("build");
    text_fields(shader_build, {"compiler_id", "compiler_version", "build_type"}, "provenance.build.");
    require(shader_build.at("cxx_flags").is_string(), "provenance.build.cxx_flags");
    auto retained_provenance = read_result(directory / "shaders/provenance.json");
    require(retained_provenance.at("schema_version").is_number_integer(),
            "provenance.retained_manifest.schema_version");
    retained_provenance["kind"] = "shader_bundle";
    require(retained_provenance == provenance, "provenance.retained_manifest");
    const auto& compiler = provenance.at("shader_compiler");
    text_fields(compiler, {"version"}, "provenance.shader_compiler.");
    require(digest(compiler.at("sha256")) && compiler.at("arguments").is_array() && !compiler.at("arguments").empty(),
            "provenance.shader_compiler.sha256/arguments");
    for(const auto& argument : compiler.at("arguments")) {
        require(argument.is_string(), "provenance.shader_compiler.arguments");
    }
    require(provenance.at("shaders").is_array() && provenance.at("shaders").size() == 10, "provenance.shaders");
    std::map<std::string, std::string> expected{{"scene.vert", "vertex"},
                                                {"scene.frag", "fragment"},
                                                {"shader-error.frag", "fragment"},
                                                {"indirect.vert", "vertex"},
                                                {"bindless.frag", "fragment"},
                                                {"post.vert", "vertex"},
                                                {"post.frag", "fragment"},
                                                {"compute-reference.comp", "compute"},
                                                {"compute-index-error.comp", "compute"},
                                                {"compute-arithmetic-error.comp", "compute"}};
    for(const auto& shader : provenance.at("shaders")) {
        const auto source = shader.at("source").get<std::string>();
        require(expected.contains(source) && shader.at("stage") == expected.at(source) &&
                    shader.at("spirv") == source + ".spv",
                "provenance.shaders.filename/stage");
        expected.erase(source);
        for(const auto* kind : {"source", "spirv"}) {
            const auto& hash = shader.at(std::string(kind) + "_sha256");
            require(digest(hash), "provenance.shaders.sha256");
            const auto content =
                read_regular_file(directory / "shaders" / shader.at(kind).get<std::string>(), 64 * 1024 * 1024);
            require(sha256(std::as_bytes(std::span(content))) == hash.get_ref<const std::string&>(),
                    "provenance.shaders.retained_content");
        }
    }
}
Json validate_compute_readback(const Json& result, const fs::path& directory) {
    const auto& inputs = result.at("inputs");
    const auto count = inputs.at("width").get<uint32_t>() * inputs.at("height").get<uint32_t>();
    require(count >= 1024 && count <= 16384, "compute.element_count");
    const auto scenario = inputs.at("scenario").get<std::string>();
    require(scenario == "compute-reference" || scenario == "compute-index-error" ||
                scenario == "compute-arithmetic-error",
            "compute.scenario");
    const auto frame = inputs.at("frame").get<uint32_t>();
    const auto name = "compute-frame-" + std::to_string(frame) + ".json";
    require(result.at("readback").at("path") == name, "readback.path");
    const auto& work = result.at("workload");
    require(work.at("kind") == "compute_affine_uint32" && work.at("presentation") == false &&
                work.at("element_count") == count && work.at("boundary") == "none" &&
                work.at("minimum_api_version") == "1.3.0" && work.at("required_device_extensions") == Json::array(),
            "compute.workload");
    const auto& compute = result.at("compute");
    require(compute.at("evidence_origin") == "application_observation" && compute.at("boundary_enabled") == false &&
                compute.at("entry_point") == "main" && compute.at("local_size") == Json({64, 1, 1}) &&
                compute.at("group_count") == Json({(count + 63) / 64, 1, 1}) &&
                compute.at("push_constant_count") == count,
            "compute.dispatch");
    require(compute.at("pipeline_label") == "compute.affine.pipeline" &&
                compute.at("shader_label") == "compute.affine.shader" &&
                compute.at("dispatch_label") == "compute.affine",
            "compute.labels");
    require(compute.at("descriptor_bindings") ==
                Json::array({{{"set", 0}, {"binding", 0}, {"label", "compute.input"}, {"bytes", count * 4}},
                             {{"set", 0}, {"binding", 1}, {"label", "compute.output"}, {"bytes", count * 4}}}),
            "compute.descriptors");
    for(const auto* kind : {"source", "spirv"}) {
        const auto path = "shaders/" + scenario + ".comp" + (std::string_view(kind) == "spirv" ? ".spv" : "");
        require(compute.at(std::string("shader_") + kind) == path &&
                    compute.at(std::string(kind) + "_sha256") == sha256_file(directory / path),
                "compute.shader_identity");
    }
    Json frames = Json::array();
    std::string hash;
    for(uint32_t current = 0; current <= frame; ++current) {
        const auto frame_name = "compute-frame-" + std::to_string(current) + ".json";
        const auto bytes = read_regular_file(directory / frame_name, 1024 * 1024);
        const auto row = read_result(directory / frame_name);
        hash = sha256(std::as_bytes(std::span(bytes)));
        if(current == frame)
            require(result.at("readback").at("sha256") == hash, "readback.sha256");
        require(row.at("schema_version").is_number_unsigned() && row.at("schema_version") == 1 &&
                    row.at("evidence_origin") == "application_readback" &&
                    row.at("phase") == "readback_before_frame_end" && row.at("frame").is_number_unsigned() &&
                    row.at("frame") == current && row.at("frame_boundary_id").is_null() && row.at("inputs") == inputs &&
                    row.at("compute") == compute &&
                    row.at("executable_sha256") == result.at("application").at("executable_sha256"),
                "readback.identity");
        for(const auto* key : {"input", "output"}) {
            const auto& values = row.at(key);
            require(values.is_array() && values.size() == count, std::string("readback.") + key);
            for(const auto& value : values)
                require(value.is_number_unsigned() && value.get<uint64_t>() <= UINT32_MAX, "readback.uint32");
        }
        // Validate input generation independently of the shader and without embedding
        // the expected defect or correct output in application-observation evidence.
        const auto seed = inputs.at("seed").get<uint32_t>();
        for(uint32_t i = 0; i < count; ++i)
            require(row.at("input")[i] == uint32_t((i * 13u ^ seed) + current * 7u), "readback.input_pattern");
        frames.push_back({{"path", "output/" + frame_name}, {"sha256", hash}, {"frame", current}});
    }
    return {{"path", "output/" + name}, {"sha256", hash},   {"element_count", count},
            {"format", "json_uint32"},  {"frames", frames}, {"correctness", "not_evaluated"}};
}

} // namespace

ExperimentResult run_experiment(const ExperimentOptions& options, std::stop_token cancellation) {
    if(options.fixture.empty() || options.output_root.empty() || options.scenario.empty() || options.width < 32 ||
       options.height < 32 || options.width > 4096 || options.height > 4096 || options.frame > 600 ||
       options.timeout.count() < 1 || options.timeout > std::chrono::minutes(10)) {
        throw std::invalid_argument("Experiment requires fixture, output root, scenario, dimensions 32..4096, "
                                    "frame 0..600 and timeout 1..600000 ms");
    }
    const auto fixture = fs::canonical(options.fixture);
    const auto root = fs::absolute(options.output_root);
    ExperimentResult result{allocate(root), Json::object()};
    auto& report = result.report;
    const auto started = std::chrono::system_clock::now();
    const auto steady_started = std::chrono::steady_clock::now();
    report = {
        {"schema_version", 1},
        {"project_version", project_version()},
        {"run_id", result.directory.filename().string()},
        {"status", "running"},
        {"evidence_origin", "application_readback"},
        {"started_unix_ms", std::chrono::duration_cast<std::chrono::milliseconds>(started.time_since_epoch()).count()},
        {"nsight", {{"status", "not_used"}}},
        {"sdk", {{"status", "not_used"}}},
        {"inputs",
         {{"scenario", options.scenario},
          {"seed", options.seed},
          {"width", options.width},
          {"height", options.height},
          {"frame", options.frame}}}};
    try {
        fs::create_directories(result.directory / "logs");
        fs::create_directories(result.directory / "config");
        const auto child_environment = environment(result.directory, options.validation);
        report["environment"] = child_environment;
        report["validation_requested"] = options.validation;
        struct utsname host{};
        if(uname(&host) == 0) {
            report["host"] = {{"system", host.sysname}, {"kernel", host.release}, {"architecture", host.machine}};
        }
        // Execute a private snapshot. A concurrent rebuild must not change the
        // executable between hashing it and the child opening its path.
        const auto snapshot = result.directory / "config/fixture-executable";
        fs::copy_file(fixture, snapshot);
        fs::permissions(snapshot, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
        report["fixture"] = {
            {"path", fixture.string()}, {"executed_path", snapshot.string()}, {"sha256", sha256_file(snapshot)}};
        report["runner"] = {{"sha256", sha256_file("/proc/self/exe")}};
        ProcessOptions process;
        process.executable = snapshot;
        process.arguments = {"--scenario", options.scenario,
                             "--seed",     std::to_string(options.seed),
                             "--width",    std::to_string(options.width),
                             "--height",   std::to_string(options.height),
                             "--frame",    std::to_string(options.frame),
                             "--output",   (result.directory / "output").string()};
        if(!options.shader_directory.empty()) {
            process.arguments.insert(process.arguments.end(),
                                     {"--shader-dir", fs::canonical(options.shader_directory).string()});
        }
        process.working_directory = result.directory;
        process.stdout_path = result.directory / "logs/stdout.log";
        process.stderr_path = result.directory / "logs/stderr.log";
        process.environment = child_environment;
        process.timeout = options.timeout;
        report["arguments"] = process.arguments;
        report["timeout_ms"] = process.timeout.count();
        save(result.directory / "config/run.json", report);
        save(result.directory / "report.json", report);
        const auto completion = run_process(process, cancellation);
        report["process"] = {{"timed_out", completion.timed_out},
                             {"cancelled", completion.cancelled},
                             {"cleanup_confirmed", completion.cleanup_confirmed},
                             {"error", completion.error}};
        report["process"]["exit_code"] = completion.exit_code ? Json(*completion.exit_code) : Json(nullptr);
        report["process"]["signal"] = completion.signal ? Json(*completion.signal) : Json(nullptr);
        report["status"] = "fail";
        if(completion.cancelled) {
            report["status"] = "cancelled";
        } else if(completion.timed_out) {
            report["status"] = "timeout";
        } else if(completion.exit_code == 3) {
            report["status"] = "unsupported";
        } else if(completion.exit_code == 0 && completion.error.empty() && completion.cleanup_confirmed) {
            const auto exported = read_result(result.directory / "output/result.json");
            if(!exported.at("schema_version").is_number_integer() || exported.at("schema_version") != 1 ||
               exported.at("status") != "pass" || exported.at("evidence_origin") != "application_readback" ||
               exported.at("inputs") != report.at("inputs")) {
                throw std::runtime_error("Fixture result does not match the requested run");
            }
            if(!exported.at("application").at("build").is_object() ||
               exported.at("application").at("executable_sha256") != report.at("fixture").at("sha256")) {
                throw std::runtime_error("Fixture executable identity does not match the retained executable snapshot");
            }
            validate_metadata(exported, result.directory / "output");
            report["result"] = exported;
            if(options.scenario.starts_with("compute-")) {
                report["readback"] = validate_compute_readback(exported, result.directory / "output");
            } else {
                const auto image_path = result.directory / "output/image.ppm";
                const auto image = read_ppm(image_path);
                if(image.width != options.width || image.height != options.height) {
                    throw std::runtime_error("Fixture image does not match requested dimensions");
                }
                const auto image_hash = sha256_file(image_path);
                if(!exported.at("gpu").is_object() || !exported.at("desktop").is_object() ||
                   !exported.at("provenance").is_object() || exported.at("image").at("path") != "image.ppm" ||
                   exported.at("image").at("format") != "P6_RGB8" || exported.at("image").at("width") != image.width ||
                   exported.at("image").at("height") != image.height ||
                   exported.at("image").at("sha256") != image_hash) {
                    throw std::runtime_error("Fixture metadata is missing or contradicts the retained image");
                }
                report["image"] = {{"path", "output/image.ppm"},
                                   {"sha256", image_hash},
                                   {"width", image.width},
                                   {"height", image.height},
                                   {"format", "P6_RGB8"}};
            }
            report["status"] = "pass";
        }
    } catch(const std::exception& error) {
        report["status"] = "fail";
        report["error"] = error.what();
    }
    report["elapsed_ms"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - steady_started)
            .count();
    save(result.directory / "report.json", report);
    return result;
}
} // namespace ngm
