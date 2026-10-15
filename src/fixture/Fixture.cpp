#include "Fixture.hpp"
#include "FixtureBuildIdentity.hpp"
#include "SdkControl.hpp"

#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <nlohmann/json.hpp>
#include <volk.h>
#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ngm::fixture {
namespace {
using Json = nlohmann::json;
constexpr std::uint64_t gpu_timeout_ns = 10'000'000'000;
constexpr std::uint32_t timing_draw_count = 6, timing_max_queries = 2 + 2 * timing_draw_count;
constexpr std::array<std::string_view, 32> scenarios{"reference",
                                                     "shader-error",
                                                     "binding-error",
                                                     "pipeline-error",
                                                     "multipass-reference",
                                                     "pass-output-error",
                                                     "bindless-reference",
                                                     "resource-selection-error",
                                                     "indirect-reference",
                                                     "indirect-parameter-error",
                                                     "combined-reference",
                                                     "combined-pass-error",
                                                     "combined-resource-error",
                                                     "combined-indirect-error",
                                                     "compute-reference",
                                                     "compute-index-error",
                                                     "compute-arithmetic-error",
                                                     "performance-reference",
                                                     "performance-underfilled",
                                                     "state-static",
                                                     "state-dynamic",
                                                     "state-indexed",
                                                     "state-count",
                                                     "state-constants",
                                                     "state-constants-reuse",
                                                     "state-descriptors",
                                                     "state-descriptors-reuse",
                                                     "timing-direct",
                                                     "timing-indexed",
                                                     "timing-reuse",
                                                     "timing-values",
                                                     "timing-values-error"};
constexpr std::array<std::string_view, 13> shader_sources{"scene.vert",
                                                          "scene.frag",
                                                          "shader-error.frag",
                                                          "indirect.vert",
                                                          "bindless.frag",
                                                          "post.vert",
                                                          "post.frag",
                                                          "compute-reference.comp",
                                                          "compute-index-error.comp",
                                                          "compute-arithmetic-error.comp",
                                                          "performance-reference.comp",
                                                          "performance-underfilled.comp",
                                                          "state-constants.frag"};

struct Workload {
    bool multipass = false;
    bool bindless = false;
    bool indirect = false;
    bool state = false;
    bool dynamic_state = false;
    bool indexed = false;
    bool dynamic_count = false;
    bool constants = false;
    bool descriptor_state = false;
    bool reuse_commands = false;
    bool timing = false;
    bool value_history = false;
    bool value_error = false;
    std::uint32_t channel_order = 0;
    std::uint32_t resource_xor = 0;
    std::uint32_t instance_count = 1;
};

Workload workload(std::string_view scenario) {
    const bool combined = scenario.starts_with("combined-");
    Workload result;
    result.multipass = combined || scenario == "multipass-reference" || scenario == "pass-output-error";
    result.bindless = combined || scenario == "bindless-reference" || scenario == "resource-selection-error";
    result.indirect = combined || scenario == "indirect-reference" || scenario == "indirect-parameter-error";
    result.state = scenario.starts_with("state-");
    result.dynamic_state = scenario == "state-dynamic" || scenario == "state-count";
    result.timing = scenario.starts_with("timing-");
    result.value_history = scenario == "timing-values" || scenario == "timing-values-error";
    result.value_error = scenario == "timing-values-error";
    result.indexed = scenario == "state-indexed" || scenario == "timing-indexed" || scenario == "timing-reuse";
    result.dynamic_count = scenario == "state-count";
    result.constants = scenario.starts_with("state-constants");
    result.descriptor_state = scenario.starts_with("state-descriptors");
    result.reuse_commands =
        scenario == "state-constants-reuse" || scenario == "state-descriptors-reuse" || scenario == "timing-reuse";
    result.channel_order = scenario == "pass-output-error" || scenario == "combined-pass-error" ? 1u : 0u;
    result.resource_xor = scenario == "resource-selection-error" || scenario == "combined-resource-error" ? 1u : 0u;
    result.instance_count =
        result.indirect && scenario != "indirect-parameter-error" && scenario != "combined-indirect-error" ? 2u : 1u;
    return result;
}

Json workload_metadata(std::string_view scenario) {
    const auto selected = workload(scenario);
    Json required = Json::array();
    if(selected.bindless) {
        required.push_back("runtimeDescriptorArray");
        required.push_back("shaderStorageBufferArrayNonUniformIndexing");
    }
    return {
        {"offscreen_render_target", selected.multipass},
        {"post_processing", selected.multipass},
        {"bindless_storage_buffers", selected.bindless},
        {"indirect_draw", selected.indirect},
        {"indexed_draw", selected.indexed},
        {"state_validation", selected.state},
        {"dynamic_raster_depth_state", selected.dynamic_state},
        {"dynamic_viewport_scissor_count", selected.dynamic_count},
        {"partial_push_constants", selected.constants},
        {"descriptor_state_validation", selected.descriptor_state},
        {"timing_validation", selected.timing},
        {"uniform_value_history_validation", selected.value_history},
        {"submissions_per_frame", selected.value_history    ? 3
                                  : selected.reuse_commands ? 2
                                                            : 1},
        {"draws_per_submission", selected.timing             ? 6
                                 : selected.constants        ? 3
                                 : selected.descriptor_state ? 5
                                                             : 1},
        {"render_pass_count", selected.multipass ? 2 : 1},
        {"minimum_api_version", "1.3.0"},
        {"required_device_extensions", {"VK_KHR_swapchain"}},
        {"required_api_features", required},
        {"offscreen_format", selected.multipass ? Json("R8G8B8A8_UNORM") : Json(nullptr)},
        {"offscreen_format_features", selected.multipass ? Json({"COLOR_ATTACHMENT", "SAMPLED_IMAGE"}) : Json::array()},
        {"descriptor_array_count", selected.bindless ? 2 : 0},
        {"indirect_command", selected.indirect ? Json("vkCmdDrawIndirect") : Json(nullptr)},
        {"indirect_draw_count", selected.indirect ? 1 : 0},
        {"indirect_first_instance", 0}};
}

template <typename T>
T vk_struct(VkStructureType type) {
    T value{};
    value.sType = type;
    return value;
}
template <typename Handle>
std::uint64_t handle_bits(Handle handle) {
    if constexpr(std::is_pointer_v<Handle>)
        return reinterpret_cast<std::uint64_t>(handle);
    else
        return static_cast<std::uint64_t>(handle);
}

void check(VkResult result, std::string_view operation) {
    if(result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
    }
}

std::string environment(const char* name) {
    const auto* value = std::getenv(name);
    return value ? value : "";
}

std::string api_version(std::uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." + std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

std::string uuid_string(const std::uint8_t* bytes) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for(std::size_t i = 0; i < VK_UUID_SIZE; ++i) {
        if(i == 4 || i == 6 || i == 8 || i == 10) {
            output << '-';
        }
        output << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return output.str();
}

std::uint32_t parse_uint(std::string_view text, std::string_view option) {
    std::uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if(error != std::errc{} || end != text.data() + text.size() || text.empty()) {
        throw std::invalid_argument(std::string(option) + " requires an unsigned 32-bit decimal integer");
    }
    return value;
}

void write_json(const std::filesystem::path& path, const Json& value) {
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
    output.close();
    std::filesystem::rename(temporary, path);
}

std::string checked_filename(const Json& record, std::string_view key) {
    const auto value = record.at(std::string(key)).get<std::string>();
    const std::filesystem::path path(value);
    if(value.empty() || path.is_absolute() || path.has_parent_path() || value == "." || value == ".." ||
       value.find('\\') != std::string::npos) {
        throw std::runtime_error("shader provenance contains an unsafe filename");
    }
    return value;
}

void require_metadata(bool valid, const std::string& field) {
    if(!valid) {
        throw std::runtime_error("invalid shader provenance metadata: " + field);
    }
}

void require_metadata_text(const Json& object, const char* key, const std::string& prefix, bool allow_empty = false) {
    require_metadata(object.is_object() && object.contains(key), prefix + key);
    const auto& value = object.at(key);
    require_metadata(value.is_string() && (allow_empty || !value.get_ref<const std::string&>().empty()), prefix + key);
}

void validate_bundle_metadata(const Json& manifest) {
    for(const auto* key : {"project_version", "source_revision"}) {
        require_metadata_text(manifest, key, "");
    }
    require_metadata(manifest.contains("source_dirty") &&
                         (manifest.at("source_dirty").is_boolean() || manifest.at("source_dirty").is_null()),
                     "source_dirty must be boolean or null");
    require_metadata(manifest.contains("build") && manifest.at("build").is_object(), "build must be an object");
    const auto& build = manifest.at("build");
    for(const auto* key : {"compiler_id", "compiler_version", "build_type"}) {
        require_metadata_text(build, key, "build.");
    }
    require_metadata_text(build, "cxx_flags", "build.", true);

    require_metadata(manifest.contains("shader_compiler") && manifest.at("shader_compiler").is_object(),
                     "shader_compiler must be an object");
    const auto& compiler = manifest.at("shader_compiler");
    require_metadata_text(compiler, "version", "shader_compiler.");
    require_metadata_text(compiler, "sha256", "shader_compiler.");
    const auto& digest = compiler.at("sha256").get_ref<const std::string&>();
    require_metadata(digest.size() == 64 && std::all_of(digest.begin(), digest.end(),
                                                        [](char character) {
                                                            return (character >= '0' && character <= '9') ||
                                                                   (character >= 'a' && character <= 'f');
                                                        }),
                     "shader_compiler.sha256 must contain 64 lowercase hexadecimal characters");
    require_metadata(compiler.contains("arguments") && compiler.at("arguments").is_array() &&
                         !compiler.at("arguments").empty(),
                     "shader_compiler.arguments must be a nonempty string array");
    for(const auto& argument : compiler.at("arguments")) {
        require_metadata(argument.is_string(), "shader_compiler.arguments must contain only strings");
    }
}

Json retain_shaders(const Options& options) {
    const auto manifest_path = options.shader_directory / "provenance.json";
    const auto input = ngm::read_regular_file(manifest_path, 1024 * 1024);
    if(input.find('\0') != std::string::npos) {
        throw std::runtime_error("NUL byte in shader provenance JSON");
    }
    const auto manifest = Json::parse(input, [](int depth, Json::parse_event_t, Json& value) {
        if(depth > 64) {
            throw std::runtime_error("Shader provenance JSON nesting exceeds limit");
        }
        if(value.is_string() && value.get_ref<const std::string&>().find('\0') != std::string::npos) {
            throw std::runtime_error("NUL byte in shader provenance JSON string");
        }
        return true;
    });
    if(!manifest.at("schema_version").is_number_integer() || manifest.at("schema_version") != 1 ||
       !manifest.at("shaders").is_array() || manifest.at("shaders").empty() || manifest.at("shaders").size() > 16) {
        throw std::runtime_error("unsupported shader provenance schema or shader inventory");
    }
    // An override directory is subject to the same provenance contract as the
    // built-in shader directory. Render the retained, verified copies below.
    validate_bundle_metadata(manifest);
    const auto retained = options.output / "shaders";
    std::filesystem::create_directory(retained);
    std::set<std::string> filenames;
    for(const auto& shader : manifest.at("shaders")) {
        const auto stage = shader.at("stage").get<std::string>();
        const auto source = checked_filename(shader, "source");
        const auto spirv = checked_filename(shader, "spirv");
        const bool recognized = std::find(shader_sources.begin(), shader_sources.end(), source) != shader_sources.end();
        const bool vertex = recognized && std::string_view(source).ends_with(".vert");
        const bool fragment = recognized && std::string_view(source).ends_with(".frag");
        const bool compute = recognized && std::string_view(source).ends_with(".comp");
        if((!vertex && !fragment && !compute) || spirv != source + ".spv" ||
           stage != (vertex    ? "vertex"
                     : compute ? "compute"
                               : "fragment")) {
            throw std::runtime_error(
                "unexpected shader filename, stage, or source/SPIR-V pairing in fixture provenance");
        }
        const bool performance_shader = source.starts_with("performance-");
        const Json expected_flags = performance_shader ? Json({"-V", "--target-env", "vulkan1.3", "-g0"})
                                                       : Json({"-V", "--target-env", "vulkan1.3", "-g", "-Od"});
        if(shader.value("compilation_profile", "") != (performance_shader ? "performance" : "diagnostic") ||
           shader.value("compiler_arguments", Json()) != expected_flags)
            throw std::runtime_error("shader requires explicit matching compilation profile and arguments");
        for(const auto* kind : {"source", "spirv"}) {
            const auto name = checked_filename(shader, kind);
            if(!filenames.insert(name).second) {
                throw std::runtime_error("duplicate filename in shader provenance: " + name);
            }
            const auto original = options.shader_directory / name;
            const auto maximum_size = std::string_view(kind) == "spirv" ? 64u * 1024u * 1024u : 1024u * 1024u;
            const auto bytes = ngm::read_regular_file(original, maximum_size);
            const auto destination = retained / name;
            std::ofstream copy(destination, std::ios::binary);
            copy.exceptions(std::ios::badbit | std::ios::failbit);
            copy.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            copy.close();
            if(ngm::sha256_file(destination) != shader.at(std::string(kind) + "_sha256").get<std::string>()) {
                throw std::runtime_error("shader provenance SHA-256 mismatch: " + name +
                                         "; rebuild the shader artifacts");
            }
        }
    }
    for(const auto source : shader_sources) {
        for(const auto& required : {std::string(source), std::string(source) + ".spv"}) {
            if(!filenames.contains(required)) {
                throw std::runtime_error("shader provenance is missing required artifact " + required);
            }
        }
    }
    write_json(retained / "provenance.json", manifest);
    return manifest;
}

struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize allocation_size = 0;
    bool coherent = false;
    void* mapped = nullptr;
    std::uint32_t memory_type_index = 0;
    VkMemoryPropertyFlags memory_flags = 0;
};

class Renderer {
public:
    Renderer(const Options& options, SdkControl& sdk) :
        options_(options), sdk_(sdk), workload_(workload(options.scenario)) {}
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer() {
        cleanup();
    }

    void initialize() {
        create_window();
        create_instance();
        select_device();
        create_device();
        create_swapchain();
        create_render_pass();
        if(workload_.multipass) {
            create_offscreen();
        }
        create_buffers_and_descriptors();
        create_pipeline();
        create_commands();
    }

    Json gpu_metadata() const {
        return {{"name", properties_.deviceName},
                {"vendor_id", properties_.vendorID},
                {"device_id", properties_.deviceID},
                {"device_type", static_cast<std::uint32_t>(properties_.deviceType)},
                {"api_version", api_version(properties_.apiVersion)},
                {"driver_version_raw", properties_.driverVersion},
                {"driver_id", static_cast<std::uint32_t>(driver_.driverID)},
                {"driver_name", driver_.driverName},
                {"driver_info", driver_.driverInfo},
                {"device_uuid", uuid_string(ids_.deviceUUID)},
                {"driver_uuid", uuid_string(ids_.driverUUID)}};
    }

    const Json& device_support() const {
        return device_support_;
    }

    Json rendering_metadata() const {
        Json metadata{
            {"requested_api_version", "1.3.0"},
            {"surface_format", format_ == VK_FORMAT_R8G8B8A8_UNORM ? "R8G8B8A8_UNORM" : "B8G8R8A8_UNORM"},
            {"surface_color_space", "SRGB_NONLINEAR_KHR"},
            {"present_mode", "FIFO_KHR"},
            {"swapchain_images", images_.size()},
            {"debug_labels_enabled", debug_utils_},
            {"graphics_queue_family", graphics_family_},
            {"present_queue_family", present_family_},
            {"rendered_frames", options_.frame + 1},
            {"shader_configuration", "diagnostic"},
            {"readback_before_presentation", true},
            {"presentation_completion", maintenance_extension_.empty() ? "device_wait_idle" : maintenance_extension_}};
        metadata["selected_shader_artifacts"] = {
            workload_.indirect ? "shaders/indirect.vert.spv" : "shaders/scene.vert.spv",
            workload_.constants ? "shaders/state-constants.frag.spv"
            : workload_.bindless
                ? "shaders/bindless.frag.spv"
                : (options_.scenario == "shader-error" ? "shaders/shader-error.frag.spv" : "shaders/scene.frag.spv")};
        if(workload_.multipass) {
            metadata["selected_shader_artifacts"].push_back("shaders/post.vert.spv");
            metadata["selected_shader_artifacts"].push_back("shaders/post.frag.spv");
        }
        metadata["enabled_api_features"] = {{"runtimeDescriptorArray", workload_.bindless},
                                            {"shaderStorageBufferArrayNonUniformIndexing", workload_.bindless}};
        if(maintenance_extension_.empty()) {
            metadata["presentation_teardown_limit"] = "Without swapchain_maintenance1, device idle does not formally "
                                                      "prove completion of presentation-engine access";
        }
        return metadata;
    }

    void render() {
        for(std::uint32_t frame = 0; frame <= options_.frame; ++frame) {
            poll_window();
            check(vkWaitForFences(device_, 1, &submit_fence_, VK_TRUE, gpu_timeout_ns), "waiting for frame completion");
            wait_for_present();
            sdk_.before_frame(frame, graphics_queue_);
            if(!workload_.value_history)
                update_palettes(frame);
            std::uint32_t image_index = 0;
            const auto acquire =
                vkAcquireNextImageKHR(device_, swapchain_, gpu_timeout_ns, acquired_, VK_NULL_HANDLE, &image_index);
            if(acquire == VK_ERROR_OUT_OF_DATE_KHR) {
                throw std::runtime_error(
                    "the desktop changed the swapchain; rerun without resizing the fixture window");
            }
            if(acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
                check(acquire, "acquiring a desktop swapchain image (check that the window is visible)");
            }
            check(vkResetFences(device_, 1, &submit_fence_), "resetting frame fence");
            check(vkResetCommandBuffer(command_, 0), "resetting frame commands");
            if(workload_.value_history)
                update_values(frame, 0);
            record(image_index, frame);
            const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            auto submit = vk_struct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
            const auto repetitions = workload_.value_history ? 3u : workload_.reuse_commands ? 2u : 1u;
            for(std::uint32_t occurrence = 0; occurrence < repetitions; ++occurrence) {
                if(occurrence != 0) {
                    check(vkWaitForFences(device_, 1, &submit_fence_, VK_TRUE, gpu_timeout_ns),
                          "waiting before command-buffer reuse");
                    if(workload_.value_history)
                        check(vkQueueWaitIdle(graphics_queue_), "completing the prior value submission");
                    check(vkResetFences(device_, 1, &submit_fence_), "resetting reused submission fence");
                    if(workload_.value_history) {
                        check(vkResetCommandBuffer(command_, 0), "resetting value-history commands");
                        update_values(frame, occurrence);
                        record(image_index, frame);
                    }
                }
                submit.waitSemaphoreCount = occurrence == 0 ? 1u : 0u;
                submit.pWaitSemaphores = &acquired_;
                submit.pWaitDstStageMask = &wait_stage;
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &command_;
                submit.signalSemaphoreCount = occurrence + 1 == repetitions ? 1u : 0u;
                submit.pSignalSemaphores = &present_ready_[image_index];
                std::string queue_label;
                if(options_.timing_draw_witness) {
                    queue_label = "ngm.fixture.queue." + std::to_string(graphics_family_) + ".frame." +
                                  std::to_string(frame) + ".submission." + std::to_string(occurrence);
                    auto label = vk_struct<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
                    label.pLabelName = queue_label.c_str();
                    vkQueueBeginDebugUtilsLabelEXT(graphics_queue_, &label);
                }
                const auto submitted_at = std::chrono::steady_clock::now();
                check(vkQueueSubmit(graphics_queue_, 1, &submit, submit_fence_), "submitting the scene");
                if(options_.timing_draw_witness)
                    vkQueueEndDebugUtilsLabelEXT(graphics_queue_);
                if(workload_.value_history && occurrence + 1 == repetitions)
                    check(vkQueueWaitIdle(graphics_queue_), "completing the final value submission");
                if(timing_queries_)
                    collect_timing(frame, occurrence, submitted_at, queue_label);
                if(!options_.state_witness.empty()) {
                    state_witness_["submissions"].push_back({{"frame", frame},
                                                             {"label", "frame." + std::to_string(frame)},
                                                             {"queue_family", graphics_family_},
                                                             {"command_buffer_count", 1},
                                                             {"submission_occurrence", occurrence},
                                                             {"constant_draws", constant_draws_},
                                                             {"descriptor_binds", descriptor_binds_},
                                                             {"descriptor_draws", descriptor_draws_},
                                                             {"draw", draw_witness_}});
                    write_json(options_.state_witness, state_witness_);
                }
            }
            auto present = vk_struct<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
            present.waitSemaphoreCount = 1;
            present.pWaitSemaphores = &present_ready_[image_index];
            present.swapchainCount = 1;
            present.pSwapchains = &swapchain_;
            present.pImageIndices = &image_index;
            auto present_fence =
                vk_struct<VkSwapchainPresentFenceInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR);
            if(present_fence_) {
                check(vkResetFences(device_, 1, &present_fence_), "resetting the presentation fence");
                present_fence.swapchainCount = 1;
                present_fence.pFences = &present_fence_;
                present.pNext = &present_fence;
            }
            const auto result = vkQueuePresentKHR(present_queue_, &present);
            // Out-of-date/surface-lost requests still enqueue their waits.
            // Allocation failures leave synchronization primitives untouched.
            present_pending_ =
                present_fence_ && result != VK_ERROR_OUT_OF_HOST_MEMORY && result != VK_ERROR_OUT_OF_DEVICE_MEMORY;
            if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                check(result, "presenting the scene (the fixture window must remain available)");
            }
        }
        check(vkWaitForFences(device_, 1, &submit_fence_, VK_TRUE, gpu_timeout_ns),
              "waiting for selected-frame readback");
        wait_for_present();
        if(!readback_.coherent) {
            auto range = vk_struct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
            range.memory = readback_.memory;
            range.size = VK_WHOLE_SIZE;
            check(vkInvalidateMappedMemoryRanges(device_, 1, &range), "invalidating image readback memory");
        }
        write_image();
        check(vkDeviceWaitIdle(device_), "finishing presentation");
    }

private:
    void wait_for_present() {
        if(present_pending_) {
            check(vkWaitForFences(device_, 1, &present_fence_, VK_TRUE, gpu_timeout_ns),
                  "waiting for presentation resource release");
            present_pending_ = false;
        }
    }

    void check_xcb(xcb_void_cookie_t cookie, std::string_view operation) {
        if(auto* error = xcb_request_check(connection_, cookie)) {
            const auto code = error->error_code;
            std::free(error);
            throw std::runtime_error(std::string(operation) + " failed (X11 error " + std::to_string(code) + ")");
        }
    }

    void create_window() {
        if(environment("DISPLAY").empty()) {
            throw Unsupported("DISPLAY is unset. Run in an existing X11 desktop or a Wayland desktop with Xwayland, "
                              "and preserve its DISPLAY/XAUTHORITY environment. Headless operation is not supported.");
        }
        int screen_index = 0;
        connection_ = xcb_connect(nullptr, &screen_index);
        if(!connection_ || xcb_connection_has_error(connection_)) {
            throw Unsupported("cannot connect to the X11/Xwayland desktop at DISPLAY=" + environment("DISPLAY") +
                              "; check the running desktop and its DISPLAY/XAUTHORITY permissions");
        }
        auto screen = xcb_setup_roots_iterator(xcb_get_setup(connection_));
        for(int i = 0; i < screen_index && screen.rem; ++i) {
            xcb_screen_next(&screen);
        }
        if(!screen.rem) {
            throw Unsupported("DISPLAY selected an unavailable X11 screen");
        }
        window_ = xcb_generate_id(connection_);
        const std::array<std::uint32_t, 2> values{screen.data->black_pixel,
                                                  XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_EXPOSURE};
        check_xcb(xcb_create_window_checked(connection_, XCB_COPY_FROM_PARENT, window_, screen.data->root, 0, 0,
                                            static_cast<std::uint16_t>(options_.width),
                                            static_cast<std::uint16_t>(options_.height), 0,
                                            XCB_WINDOW_CLASS_INPUT_OUTPUT, screen.data->root_visual,
                                            XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values.data()),
                  "creating the fixture window");
        constexpr std::string_view title = "Nsight Graphics MCP deterministic fixture";
        check_xcb(xcb_change_property_checked(connection_, XCB_PROP_MODE_REPLACE, window_, XCB_ATOM_WM_NAME,
                                              XCB_ATOM_STRING, 8, static_cast<std::uint32_t>(title.size()),
                                              title.data()),
                  "naming the fixture window");
        check_xcb(xcb_map_window_checked(connection_, window_), "mapping the fixture window");
        if(xcb_flush(connection_) <= 0) {
            throw std::runtime_error("the X11 desktop connection closed while mapping the fixture window");
        }
    }

    void poll_window() {
        while(auto* event = xcb_poll_for_event(connection_)) {
            const auto type = event->response_type & ~0x80u;
            const bool destroyed = type == XCB_DESTROY_NOTIFY;
            bool resized = false;
            if(type == XCB_CONFIGURE_NOTIFY) {
                const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
                resized = configure->width != options_.width || configure->height != options_.height;
            }
            const bool error = type == 0;
            std::free(event);
            if(destroyed || resized || error) {
                throw std::runtime_error("fixture window was closed, resized, or rejected by the X11 server; "
                                         "rerun with the requested window available");
            }
        }
        if(xcb_connection_has_error(connection_)) {
            throw std::runtime_error("the X11/Xwayland desktop connection closed during rendering");
        }
    }

    void create_instance() {
        if(volkInitialize() != VK_SUCCESS) {
            throw Unsupported(
                "the Vulkan loader could not initialize; install a Vulkan loader and a compatible GPU driver");
        }
        if(volkGetInstanceVersion() < VK_API_VERSION_1_3) {
            throw Unsupported("the fixture's SPIR-V 1.6 shaders require a Vulkan 1.3 loader and device");
        }
        std::uint32_t count = 0;
        check(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr),
              "enumerating Vulkan instance extensions");
        std::vector<VkExtensionProperties> available(count);
        check(vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data()),
              "reading Vulkan instance extensions");
        const auto has = [&](const char* name) {
            return std::any_of(available.begin(), available.end(),
                               [&](const auto& value) { return std::strcmp(value.extensionName, name) == 0; });
        };
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME};
        for(const auto* required : extensions) {
            if(!has(required)) {
                throw Unsupported(std::string("the Vulkan loader lacks required desktop extension ") + required);
            }
        }
        debug_utils_ = has(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if(debug_utils_) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        if(has(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME)) {
            surface_maintenance_khr_ = has(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            surface_maintenance_ext_ = has(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            if(surface_maintenance_khr_ || surface_maintenance_ext_) {
                extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            }
            if(surface_maintenance_khr_) {
                extensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            }
            if(surface_maintenance_ext_) {
                extensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
            }
        }
        auto application = vk_struct<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        application.pApplicationName = "ngm-vulkan-fixture";
        application.pEngineName = "ngm deterministic raster fixture";
        application.apiVersion = VK_API_VERSION_1_3;
        auto create = vk_struct<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        create.pApplicationInfo = &application;
        create.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        create.ppEnabledExtensionNames = extensions.data();
        check(vkCreateInstance(&create, nullptr, &instance_), "creating Vulkan 1.3 instance");
        volkLoadInstance(instance_);
        // volk is built without platform macros. Load the XCB entry point
        // explicitly rather than assuming its static library defines a global.
        const auto create_surface =
            reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(vkGetInstanceProcAddr(instance_, "vkCreateXcbSurfaceKHR"));
        if(!create_surface) {
            throw Unsupported("the Vulkan loader did not provide vkCreateXcbSurfaceKHR");
        }
        auto surface = vk_struct<VkXcbSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR);
        surface.connection = connection_;
        surface.window = window_;
        check(create_surface(instance_, &surface, nullptr, &surface_), "creating an XCB Vulkan surface");
    }

    void select_device() {
        std::uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "enumerating Vulkan devices");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "reading Vulkan devices");
        for(const auto candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            Json evaluation{{"name", properties.deviceName},
                            {"api_version", api_version(properties.apiVersion)},
                            {"status", "rejected"},
                            {"missing_requirements", Json::array()}};
            if(properties.apiVersion < VK_API_VERSION_1_3) {
                evaluation["missing_requirements"].push_back("Vulkan 1.3");
                device_support_.push_back(evaluation);
                continue;
            }
            auto indexing =
                vk_struct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
            auto features = vk_struct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            features.pNext = &indexing;
            vkGetPhysicalDeviceFeatures2(candidate, &features);
            evaluation["queried_api_features"] = {
                {"runtimeDescriptorArray", indexing.runtimeDescriptorArray == VK_TRUE},
                {"shaderStorageBufferArrayNonUniformIndexing",
                 indexing.shaderStorageBufferArrayNonUniformIndexing == VK_TRUE}};
            evaluation["limits"] = {
                {"maxPerStageDescriptorStorageBuffers", properties.limits.maxPerStageDescriptorStorageBuffers},
                {"maxDescriptorSetStorageBuffers", properties.limits.maxDescriptorSetStorageBuffers},
                {"maxDrawIndirectCount", properties.limits.maxDrawIndirectCount},
                {"maxPushConstantsSize", properties.limits.maxPushConstantsSize}};
            if(workload_.bindless) {
                if(!indexing.runtimeDescriptorArray) {
                    evaluation["missing_requirements"].push_back("runtimeDescriptorArray");
                }
                if(!indexing.shaderStorageBufferArrayNonUniformIndexing) {
                    evaluation["missing_requirements"].push_back("shaderStorageBufferArrayNonUniformIndexing");
                }
                if(properties.limits.maxPerStageDescriptorStorageBuffers < 2 ||
                   properties.limits.maxDescriptorSetStorageBuffers < 2 || properties.limits.maxPushConstantsSize < 8) {
                    evaluation["missing_requirements"].push_back(
                        "two storage-buffer descriptors and eight push-constant bytes");
                }
            }
            if(workload_.indirect && properties.limits.maxDrawIndirectCount < 1) {
                evaluation["missing_requirements"].push_back("one indirect draw command");
            }
            if(workload_.multipass) {
                VkFormatProperties format_properties{};
                vkGetPhysicalDeviceFormatProperties(candidate, VK_FORMAT_R8G8B8A8_UNORM, &format_properties);
                constexpr auto required_format =
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
                VkImageFormatProperties image_properties{};
                const auto image_support = vkGetPhysicalDeviceImageFormatProperties(
                    candidate, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &image_properties);
                const bool supported = image_support == VK_SUCCESS &&
                                       (format_properties.optimalTilingFeatures & required_format) == required_format &&
                                       image_properties.maxExtent.width >= options_.width &&
                                       image_properties.maxExtent.height >= options_.height &&
                                       (image_properties.sampleCounts & VK_SAMPLE_COUNT_1_BIT) != 0;
                evaluation["offscreen_rgba8_color_attachment_and_sampled_image"] = supported;
                if(!supported) {
                    evaluation["missing_requirements"].push_back(
                        "RGBA8_UNORM optimal color attachment and sampled image at requested dimensions");
                }
            }
            if(!evaluation.at("missing_requirements").empty()) {
                device_support_.push_back(evaluation);
                continue;
            }
            std::uint32_t extension_count = 0;
            check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr),
                  "enumerating device extensions");
            std::vector<VkExtensionProperties> extensions(extension_count);
            check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, extensions.data()),
                  "reading device extensions");
            if(std::none_of(extensions.begin(), extensions.end(), [](const auto& value) {
                   return std::strcmp(value.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
               })) {
                evaluation["missing_requirements"].push_back("VK_KHR_swapchain");
                device_support_.push_back(evaluation);
                continue;
            }
            std::uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            std::optional<std::uint32_t> graphics;
            std::optional<std::uint32_t> present;
            for(std::uint32_t i = 0; i < family_count; ++i) {
                VkBool32 supports_present = VK_FALSE;
                check(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface_, &supports_present),
                      "querying desktop presentation support");
                const bool supports_graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                if(supports_graphics && !graphics) {
                    graphics = i;
                }
                if(supports_present && !present) {
                    present = i;
                }
                if(supports_graphics && supports_present) {
                    graphics = i;
                    present = i;
                    break;
                }
            }
            if(graphics && present) {
                evaluation["status"] = "selected";
                device_support_.push_back(evaluation);
                physical_ = candidate;
                graphics_family_ = *graphics;
                present_family_ = *present;
                timestamp_bits_ = families[*graphics].timestampValidBits;
                const auto has = [&](const char* extension) {
                    return std::any_of(extensions.begin(), extensions.end(), [&](const auto& available) {
                        return std::strcmp(available.extensionName, extension) == 0;
                    });
                };
                if(surface_maintenance_khr_ && has(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
                    maintenance_extension_ = VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
                } else if(surface_maintenance_ext_ && has(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
                    maintenance_extension_ = VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME;
                }
                if(!maintenance_extension_.empty()) {
                    auto maintenance = vk_struct<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
                    auto features = vk_struct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
                    features.pNext = &maintenance;
                    vkGetPhysicalDeviceFeatures2(candidate, &features);
                    if(!maintenance.swapchainMaintenance1) {
                        maintenance_extension_.clear();
                    }
                }
                break;
            }
            evaluation["missing_requirements"].push_back(
                "graphics and presentation to the selected X11/Xwayland desktop");
            device_support_.push_back(evaluation);
        }
        if(physical_ == VK_NULL_HANDLE) {
            throw Unsupported(
                "no GPU satisfies the requested scenario's Vulkan 1.3, presentation, feature, and format requirements; "
                "see device_support in result.json: " +
                device_support_.dump());
        }
        ids_ = vk_struct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
        driver_ = vk_struct<VkPhysicalDeviceDriverProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
        ids_.pNext = &driver_;
        auto properties = vk_struct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
        properties.pNext = &ids_;
        vkGetPhysicalDeviceProperties2(physical_, &properties);
        properties_ = properties.properties;
        vkGetPhysicalDeviceMemoryProperties(physical_, &memory_properties_);
    }

    void create_device() {
        const float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queues;
        for(const auto family : std::set<std::uint32_t>{graphics_family_, present_family_}) {
            auto queue = vk_struct<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            queues.push_back(queue);
        }
        std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        auto maintenance = vk_struct<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
        if(!maintenance_extension_.empty()) {
            extensions.push_back(maintenance_extension_.c_str());
            maintenance.swapchainMaintenance1 = VK_TRUE;
        }
        auto create = vk_struct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        create.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
        create.pQueueCreateInfos = queues.data();
        create.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
        create.ppEnabledExtensionNames = extensions.data();
        auto enabled =
            vk_struct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
        enabled.runtimeDescriptorArray = workload_.bindless ? VK_TRUE : VK_FALSE;
        enabled.shaderStorageBufferArrayNonUniformIndexing = workload_.bindless ? VK_TRUE : VK_FALSE;
        enabled.pNext = maintenance_extension_.empty() ? nullptr : &maintenance;
        create.pNext = &enabled;
        check(vkCreateDevice(physical_, &create, nullptr, &device_), "creating the graphics device");
        volkLoadDevice(device_);
        if(workload_.dynamic_state &&
           (!vkCmdSetCullMode || !vkCmdSetFrontFace || !vkCmdSetDepthTestEnable || !vkCmdSetDepthWriteEnable ||
            !vkCmdSetDepthCompareOp || !vkCmdSetViewportWithCount || !vkCmdSetScissorWithCount))
            throw Unsupported("state fixture requires the core Vulkan 1.3 dynamic-state entry points");
        vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, present_family_, 0, &present_queue_);
        name(VK_OBJECT_TYPE_QUEUE, graphics_queue_, "fixture.graphics_queue");
    }

    template <typename Handle>
    void name(VkObjectType type, Handle handle, const std::string& text) {
        if(debug_utils_ && vkSetDebugUtilsObjectNameEXT) {
            auto object = vk_struct<VkDebugUtilsObjectNameInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
            object.objectType = type;
            if constexpr(std::is_pointer_v<Handle>) {
                object.objectHandle = reinterpret_cast<std::uint64_t>(handle);
            } else {
                object.objectHandle = static_cast<std::uint64_t>(handle);
            }
            object.pObjectName = text.c_str();
            check(vkSetDebugUtilsObjectNameEXT(device_, &object), "naming a Vulkan resource");
        }
    }

    void create_swapchain() {
        VkSurfaceCapabilitiesKHR capabilities{};
        check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &capabilities),
              "querying desktop surface capabilities");
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if((capabilities.supportedUsageFlags & usage) != usage) {
            throw Unsupported("the desktop surface does not support color rendering plus transfer-source readback");
        }
        extent_ = {options_.width, options_.height};
        if(capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
            if(capabilities.currentExtent.width != extent_.width ||
               capabilities.currentExtent.height != extent_.height) {
                throw Unsupported(
                    "the desktop resized the requested fixture window; use a resolution that fits the desktop");
            }
        }
        if(extent_.width < capabilities.minImageExtent.width || extent_.width > capabilities.maxImageExtent.width ||
           extent_.height < capabilities.minImageExtent.height || extent_.height > capabilities.maxImageExtent.height) {
            throw Unsupported("the requested dimensions are outside the Vulkan desktop surface limits");
        }
        std::uint32_t count = 0;
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, nullptr), "querying surface formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, formats.data()),
              "reading surface formats");
        std::optional<VkSurfaceFormatKHR> selected;
        for(const auto preferred : {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}) {
            for(const auto& candidate : formats) {
                if((candidate.format == preferred || candidate.format == VK_FORMAT_UNDEFINED) &&
                   candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    selected = VkSurfaceFormatKHR{preferred, candidate.colorSpace};
                    break;
                }
            }
            if(selected) {
                break;
            }
        }
        if(!selected) {
            throw Unsupported("the fixture requires an RGBA8/BGRA8 UNORM surface with SRGB_NONLINEAR color space");
        }
        format_ = selected->format;
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for(const auto candidate : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
            if(capabilities.supportedCompositeAlpha & candidate) {
                alpha = candidate;
                break;
            }
        }
        auto image_count = capabilities.minImageCount + 1;
        if(capabilities.maxImageCount > 0) {
            image_count = std::min(image_count, capabilities.maxImageCount);
        }
        const std::array families{graphics_family_, present_family_};
        auto create = vk_struct<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        create.surface = surface_;
        create.minImageCount = image_count;
        create.imageFormat = format_;
        create.imageColorSpace = selected->colorSpace;
        create.imageExtent = extent_;
        create.imageArrayLayers = 1;
        create.imageUsage = usage;
        create.imageSharingMode =
            graphics_family_ == present_family_ ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
        if(graphics_family_ != present_family_) {
            create.queueFamilyIndexCount = static_cast<std::uint32_t>(families.size());
            create.pQueueFamilyIndices = families.data();
        }
        create.preTransform = capabilities.currentTransform;
        create.compositeAlpha = alpha;
        create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        create.clipped = VK_FALSE;
        check(vkCreateSwapchainKHR(device_, &create, nullptr, &swapchain_), "creating the window swapchain");
        check(vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr), "querying swapchain images");
        images_.resize(count);
        check(vkGetSwapchainImagesKHR(device_, swapchain_, &count, images_.data()), "reading swapchain images");
        image_views_.resize(count, VK_NULL_HANDLE);
        present_ready_.resize(count, VK_NULL_HANDLE);
        for(std::uint32_t i = 0; i < count; ++i) {
            name(VK_OBJECT_TYPE_IMAGE, images_[i], "fixture.window_color." + std::to_string(i));
            auto view = vk_struct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
            view.image = images_[i];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = format_;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            check(vkCreateImageView(device_, &view, nullptr, &image_views_[i]), "creating a window image view");
            const auto semaphore = vk_struct<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
            check(vkCreateSemaphore(device_, &semaphore, nullptr, &present_ready_[i]),
                  "creating a presentation semaphore");
        }
    }

    VkRenderPass make_render_pass(VkFormat format, const std::string& label,
                                  VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED) {
        VkAttachmentDescription color{};
        color.format = format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = initial_layout;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &reference;
        VkAttachmentDescription depth = color;
        depth.format = VK_FORMAT_D32_SFLOAT;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference depth_reference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        if(workload_.state)
            subpass.pDepthStencilAttachment = &depth_reference;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if(workload_.state) {
            dependency.srcStageMask |=
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask |=
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask |=
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        auto create = vk_struct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
        const std::array<VkAttachmentDescription, 2> attachments{color, depth};
        create.attachmentCount = workload_.state ? 2u : 1u;
        create.pAttachments = attachments.data();
        create.subpassCount = 1;
        create.pSubpasses = &subpass;
        create.dependencyCount = 1;
        create.pDependencies = &dependency;
        VkRenderPass result = VK_NULL_HANDLE;
        check(vkCreateRenderPass(device_, &create, nullptr, &result), "creating " + label);
        name(VK_OBJECT_TYPE_RENDER_PASS, result, label);
        return result;
    }

    void create_render_pass() {
        if(workload_.state)
            create_depth();
        render_pass_ = make_render_pass(format_, workload_.multipass ? "fixture.post.present" : "fixture.scene.raster");
        framebuffers_.resize(image_views_.size(), VK_NULL_HANDLE);
        for(std::size_t i = 0; i < framebuffers_.size(); ++i) {
            auto framebuffer = vk_struct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
            framebuffer.renderPass = render_pass_;
            const std::array<VkImageView, 2> attachments{image_views_[i], depth_view_};
            framebuffer.attachmentCount = workload_.state ? 2u : 1u;
            framebuffer.pAttachments = attachments.data();
            framebuffer.width = extent_.width;
            framebuffer.height = extent_.height;
            framebuffer.layers = 1;
            check(vkCreateFramebuffer(device_, &framebuffer, nullptr, &framebuffers_[i]), "creating a frame buffer");
        }
    }

    void create_depth() {
        VkImageFormatProperties supported{};
        if(vkGetPhysicalDeviceImageFormatProperties(
               physical_, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &supported) != VK_SUCCESS ||
           supported.maxExtent.width < extent_.width || supported.maxExtent.height < extent_.height ||
           !(supported.sampleCounts & VK_SAMPLE_COUNT_1_BIT))
            throw Unsupported("state fixture requires a D32_SFLOAT depth attachment at the selected dimensions");
        auto create = vk_struct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        create.imageType = VK_IMAGE_TYPE_2D;
        create.format = VK_FORMAT_D32_SFLOAT;
        create.extent = {extent_.width, extent_.height, 1};
        create.mipLevels = create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT;
        create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        check(vkCreateImage(device_, &create, nullptr, &depth_image_), "creating the state depth image");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, depth_image_, &requirements);
        std::optional<std::uint32_t> selected;
        for(std::uint32_t i = 0; i < memory_properties_.memoryTypeCount; ++i)
            if(requirements.memoryTypeBits & (1u << i)) {
                selected = i;
                if(memory_properties_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    break;
            }
        if(!selected)
            throw Unsupported("no memory type supports the state depth image");
        auto allocate = vk_struct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = *selected;
        check(vkAllocateMemory(device_, &allocate, nullptr, &depth_memory_), "allocating state depth memory");
        check(vkBindImageMemory(device_, depth_image_, depth_memory_, 0), "binding state depth memory");
        auto view = vk_struct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = depth_image_;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = create.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(device_, &view, nullptr, &depth_view_), "creating the state depth view");
        name(VK_OBJECT_TYPE_IMAGE, depth_image_, "fixture.state.depth");
    }

    void create_offscreen() {
        offscreen_pass_ = make_render_pass(VK_FORMAT_R8G8B8A8_UNORM, "fixture.scene.offscreen",
                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto create = vk_struct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        create.imageType = VK_IMAGE_TYPE_2D;
        create.format = VK_FORMAT_R8G8B8A8_UNORM;
        create.extent = {extent_.width, extent_.height, 1};
        create.mipLevels = 1;
        create.arrayLayers = 1;
        create.samples = VK_SAMPLE_COUNT_1_BIT;
        create.tiling = VK_IMAGE_TILING_OPTIMAL;
        create.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(vkCreateImage(device_, &create, nullptr, &offscreen_image_), "creating the offscreen scene image");
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device_, offscreen_image_, &requirements);
        std::optional<std::uint32_t> memory_type;
        for(std::uint32_t i = 0; i < memory_properties_.memoryTypeCount; ++i) {
            if(requirements.memoryTypeBits & (1u << i)) {
                memory_type = i;
                if(memory_properties_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                    break;
                }
            }
        }
        if(!memory_type) {
            throw Unsupported("no memory type can back the required offscreen scene image");
        }
        auto allocate = vk_struct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = *memory_type;
        check(vkAllocateMemory(device_, &allocate, nullptr, &offscreen_memory_), "allocating offscreen image memory");
        check(vkBindImageMemory(device_, offscreen_image_, offscreen_memory_, 0), "binding offscreen image memory");
        auto view = vk_struct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = offscreen_image_;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = create.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(vkCreateImageView(device_, &view, nullptr, &offscreen_view_), "creating the offscreen image view");
        auto framebuffer = vk_struct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
        framebuffer.renderPass = offscreen_pass_;
        framebuffer.attachmentCount = 1;
        framebuffer.pAttachments = &offscreen_view_;
        framebuffer.width = extent_.width;
        framebuffer.height = extent_.height;
        framebuffer.layers = 1;
        check(vkCreateFramebuffer(device_, &framebuffer, nullptr, &offscreen_framebuffer_),
              "creating the offscreen framebuffer");
        auto sampler = vk_struct<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        sampler.magFilter = VK_FILTER_NEAREST;
        sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod = 0;
        check(vkCreateSampler(device_, &sampler, nullptr, &post_sampler_), "creating the post-processing sampler");
        name(VK_OBJECT_TYPE_IMAGE, offscreen_image_, "fixture.scene.color");
        name(VK_OBJECT_TYPE_IMAGE_VIEW, offscreen_view_, "fixture.scene.color_view");
        name(VK_OBJECT_TYPE_FRAMEBUFFER, offscreen_framebuffer_, "fixture.scene.offscreen_framebuffer");
        name(VK_OBJECT_TYPE_SAMPLER, post_sampler_, "fixture.post.nearest_sampler");
    }

    void create_buffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage, const std::string& label) {
        auto create = vk_struct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        create.size = size;
        create.usage = usage;
        create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device_, &create, nullptr, &buffer.handle), "creating " + label);
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device_, buffer.handle, &requirements);
        std::optional<std::uint32_t> selected;
        for(std::uint32_t i = 0; i < memory_properties_.memoryTypeCount; ++i) {
            const auto flags = memory_properties_.memoryTypes[i].propertyFlags;
            if((requirements.memoryTypeBits & (1u << i)) && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                selected = i;
                if(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
                    break;
                }
            }
        }
        if(!selected) {
            throw Unsupported("the GPU has no host-visible memory for " + label);
        }
        buffer.coherent =
            (memory_properties_.memoryTypes[*selected].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        buffer.allocation_size = requirements.size;
        buffer.memory_type_index = *selected;
        buffer.memory_flags = memory_properties_.memoryTypes[*selected].propertyFlags;
        auto allocate = vk_struct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = *selected;
        check(vkAllocateMemory(device_, &allocate, nullptr, &buffer.memory), "allocating " + label);
        check(vkBindBufferMemory(device_, buffer.handle, buffer.memory, 0), "binding " + label);
        check(vkMapMemory(device_, buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer.mapped), "mapping " + label);
        name(VK_OBJECT_TYPE_BUFFER, buffer.handle, label);
    }

    void flush_buffer(const Buffer& buffer, std::string_view label) {
        if(!buffer.coherent) {
            auto range = vk_struct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
            range.memory = buffer.memory;
            range.size = VK_WHOLE_SIZE;
            check(vkFlushMappedMemoryRanges(device_, 1, &range), label);
        }
    }

    void create_buffers_and_descriptors() {
        if(workload_.indexed) {
            const std::array<std::uint32_t, 3> indices{0, 1, 2};
            create_buffer(indices_, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "fixture.state.indices");
            std::memcpy(indices_.mapped, indices.data(), sizeof(indices));
            flush_buffer(indices_, "flushing state indices");
        }
        create_buffer(readback_, static_cast<VkDeviceSize>(extent_.width) * extent_.height * 4,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT, "fixture.application_readback");
        if(workload_.descriptor_state) {
            create_descriptor_fixture();
            return;
        }
        const auto palette_usage =
            workload_.bindless ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        create_buffer(palettes_[0], sizeof(float) * 4, palette_usage, "fixture.palette.primary");
        create_buffer(palettes_[1], sizeof(float) * 4, palette_usage, "fixture.palette.secondary");
        const auto descriptor_type =
            workload_.bindless ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = descriptor_type;
        binding.descriptorCount = workload_.bindless ? 2u : 1u;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        auto layout = vk_struct<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        layout.bindingCount = 1;
        layout.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &descriptor_layout_),
              "creating the palette layout");
        if(workload_.multipass) {
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            check(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &post_descriptor_layout_),
                  "creating the post-processing layout");
        }
        std::vector<VkDescriptorPoolSize> sizes{{descriptor_type, workload_.value_history ? 3u : 2u}};
        if(workload_.multipass) {
            sizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1});
        }
        auto pool = vk_struct<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets = 3;
        pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptor_pool_),
              "creating the fixture descriptor pool");
        const std::array layouts{descriptor_layout_, descriptor_layout_};
        auto allocation = vk_struct<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        allocation.descriptorPool = descriptor_pool_;
        allocation.descriptorSetCount = workload_.bindless ? 1u : 2u;
        allocation.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocation, descriptors_.data()), "allocating palette descriptors");
        std::array<VkDescriptorBufferInfo, 2> buffers{};
        for(std::size_t i = 0; i < buffers.size(); ++i) {
            buffers[i] = {palettes_[i].handle, 0, sizeof(float) * 4};
        }
        for(std::uint32_t i = 0; i < allocation.descriptorSetCount; ++i) {
            auto write = vk_struct<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            write.dstSet = descriptors_[i];
            write.dstBinding = 0;
            write.descriptorCount = workload_.bindless ? 2u : 1u;
            write.descriptorType = descriptor_type;
            write.pBufferInfo = &buffers[i];
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET, descriptors_[i], "fixture.palette_set." + std::to_string(i));
        }
        if(workload_.value_history) {
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &descriptor_layout_;
            check(vkAllocateDescriptorSets(device_, &allocation, &value_descriptor_),
                  "allocating the value-history destination");
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET, value_descriptor_, "fixture.values.destination");
        }
        if(workload_.multipass) {
            allocation.descriptorSetCount = 1;
            allocation.pSetLayouts = &post_descriptor_layout_;
            check(vkAllocateDescriptorSets(device_, &allocation, &post_descriptor_), "allocating the post descriptor");
            const VkDescriptorImageInfo image{post_sampler_, offscreen_view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            auto write = vk_struct<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            write.dstSet = post_descriptor_;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &image;
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET, post_descriptor_, "fixture.post.scene_color_set");
        }
        if(workload_.indirect) {
            create_buffer(indirect_, sizeof(VkDrawIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                          "fixture.scene.indirect_parameters");
            const VkDrawIndirectCommand draw{3, workload_.instance_count, 0, 0};
            std::memcpy(indirect_.mapped, &draw, sizeof(draw));
            flush_buffer(indirect_, "flushing indirect draw memory");
        }
    }

    VkShaderModule load_shader(const std::string& filename) {
        const auto path = options_.output / "shaders" / filename;
        const auto size = std::filesystem::file_size(path);
        if(size < 20 || size % sizeof(std::uint32_t) != 0 || size > 64u * 1024u * 1024u) {
            throw std::runtime_error("invalid SPIR-V byte length: " + path.string());
        }
        std::vector<std::uint32_t> code(size / sizeof(std::uint32_t));
        std::ifstream input(path, std::ios::binary);
        input.exceptions(std::ios::badbit | std::ios::failbit);
        input.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
        if(code[0] != 0x07230203u) {
            throw std::runtime_error("invalid SPIR-V magic: " + path.string());
        }
        auto create = vk_struct<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        create.codeSize = static_cast<std::size_t>(size);
        create.pCode = code.data();
        VkShaderModule shader = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device_, &create, nullptr, &shader), "creating shader " + filename);
        if(workload_.state)
            shader_witness_[std::to_string(handle_bits(shader))] = {
                {"module_handle", handle_bits(shader)},
                {"filename", filename},
                {"bytes", create.codeSize},
                {"sha256", ngm::sha256(std::as_bytes(std::span(code)))}};
        shaders_.push_back(shader);
        name(VK_OBJECT_TYPE_SHADER_MODULE, shader, "fixture." + filename);
        return shader;
    }

    void create_descriptor_fixture() {
        const auto& limits = properties_.limits;
        if(limits.maxBoundDescriptorSets < 2 || limits.maxDescriptorSetUniformBuffersDynamic < 6 ||
           limits.maxDescriptorSetStorageBuffersDynamic < 2)
            throw Unsupported(
                "descriptor-state fixture requires two sets, six dynamic uniform and two dynamic storage descriptors");
        descriptor_stride_ = std::max(
            {VkDeviceSize{16}, limits.minUniformBufferOffsetAlignment, limits.minStorageBufferOffsetAlignment});
        if(descriptor_stride_ > 1024U * 1024U)
            throw Unsupported("descriptor-state alignment exceeds fixture bounds");
        for(std::size_t i = 0; i < palettes_.size(); ++i)
            create_buffer(palettes_[i], descriptor_stride_ * 16,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                          "fixture.descriptor.palette." + std::to_string(i));
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{
            {{9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
             {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
             {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
        const std::array layouts{&descriptor_layout_, &compatible_descriptor_layout_, &incompatible_descriptor_layout_};
        for(std::size_t n = 0; n < layouts.size(); ++n) {
            if(n == 2)
                bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            auto info = vk_struct<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
            info.bindingCount = static_cast<std::uint32_t>(bindings.size());
            info.pBindings = bindings.data();
            check(vkCreateDescriptorSetLayout(device_, &info, nullptr, layouts[n]), "creating descriptor-state layout");
            if(!options_.state_witness.empty()) {
                Json entries = Json::array();
                for(const auto& b : bindings)
                    entries.push_back({{"binding", b.binding},
                                       {"descriptor_type", b.descriptorType},
                                       {"descriptor_count", b.descriptorCount},
                                       {"stage_mask", b.stageFlags}});
                state_witness_["descriptor_layouts"].push_back(
                    {{"handle", handle_bits(*layouts[n])}, {"bindings", entries}});
            }
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, *layouts[n], "fixture.descriptors.layout." + std::to_string(n));
        }
        const std::array<VkDescriptorPoolSize, 2> sizes{
            {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 12}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 4}}};
        auto pool = vk_struct<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets = 4;
        pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptor_pool_), "creating descriptor-state pool");
        const std::array set_layouts{descriptor_layout_, descriptor_layout_, compatible_descriptor_layout_,
                                     incompatible_descriptor_layout_};
        auto allocation = vk_struct<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        allocation.descriptorPool = descriptor_pool_;
        allocation.descriptorSetCount = static_cast<std::uint32_t>(set_layouts.size());
        allocation.pSetLayouts = set_layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocation, state_descriptors_.data()),
              "allocating descriptor-state sets");
        for(std::size_t n = 0; n < state_descriptors_.size(); ++n) {
            const VkDescriptorBufferInfo buffer{palettes_[n % 2].handle, 0, sizeof(float) * 4};
            const std::array infos{buffer, buffer};
            std::array<VkWriteDescriptorSet, 3> writes{};
            const std::array<std::uint32_t, 3> binding_numbers{0, 2, 9};
            for(std::size_t i = 0; i < writes.size(); ++i) {
                writes[i] = vk_struct<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                writes[i].dstSet = state_descriptors_[n];
                writes[i].dstBinding = binding_numbers[i];
                writes[i].descriptorCount = i == 1 ? 2u : 1u;
                writes[i].descriptorType =
                    i == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                writes[i].pBufferInfo = infos.data();
            }
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
            if(!options_.state_witness.empty())
                state_witness_["descriptor_allocations"].push_back({{"handle", handle_bits(state_descriptors_[n])},
                                                                    {"layout_handle", handle_bits(set_layouts[n])},
                                                                    {"buffer_handle", handle_bits(buffer.buffer)}});
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET, state_descriptors_[n], "fixture.descriptors.set." + std::to_string(n));
        }
        // Change just array element one. It is retained as private API evidence;
        // the product must continue to report descriptor contents unavailable.
        const VkDescriptorBufferInfo replacement{palettes_[1].handle, 0, sizeof(float) * 4};
        auto partial = vk_struct<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        partial.dstSet = state_descriptors_[0];
        partial.dstBinding = 2;
        partial.dstArrayElement = 1;
        partial.descriptorCount = 1;
        partial.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        partial.pBufferInfo = &replacement;
        vkUpdateDescriptorSets(device_, 1, &partial, 0, nullptr);
        if(!options_.state_witness.empty()) {
            state_witness_["partial_descriptor_update"] = {{"set_handle", handle_bits(partial.dstSet)},
                                                           {"binding", partial.dstBinding},
                                                           {"array_element", partial.dstArrayElement},
                                                           {"descriptor_count", partial.descriptorCount},
                                                           {"buffer_handle", handle_bits(replacement.buffer)}};
            state_witness_["descriptor_stride"] = descriptor_stride_;
        }
    }

    void make_pipeline(const std::string& vertex_file, const std::string& fragment_file,
                       VkDescriptorSetLayout descriptor_layout, std::uint32_t push_size, VkRenderPass render_pass,
                       VkPipelineLayout& pipeline_layout, VkPipeline& pipeline, const std::string& label) {
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = vk_struct<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = load_shader(vertex_file);
        stages[0].pName = "main";
        stages[1] = vk_struct<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = load_shader(fragment_file);
        stages[1].pName = "main";
        auto layout = vk_struct<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        const std::array state_layouts{descriptor_layout, descriptor_layout == incompatible_descriptor_layout_
                                                              ? descriptor_layout_
                                                              : descriptor_layout};
        layout.setLayoutCount = workload_.descriptor_state ? 2u : 1u;
        layout.pSetLayouts = workload_.descriptor_state ? state_layouts.data() : &descriptor_layout;
        const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, push_size};
        if(push_size != 0) {
            layout.pushConstantRangeCount = 1;
            layout.pPushConstantRanges = &push;
        }
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &pipeline_layout), "creating " + label + " layout");
        const auto vertex =
            vk_struct<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
        auto assembly = vk_struct<VkPipelineInputAssemblyStateCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const VkViewport viewport =
            workload_.dynamic_state
                ? VkViewport{4, 6, 16, 16, 0.125f, 0.875f}
                : VkViewport{0, 0, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0, 1};
        const VkRect2D scissor = workload_.dynamic_state ? VkRect2D{{2, 3}, {8, 8}} : VkRect2D{{0, 0}, extent_};
        auto viewport_state =
            vk_struct<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;
        if(workload_.dynamic_count) {
            viewport_state.viewportCount = viewport_state.scissorCount = 0;
            viewport_state.pViewports = nullptr;
            viewport_state.pScissors = nullptr;
        }
        auto raster = vk_struct<VkPipelineRasterizationStateCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        if(workload_.dynamic_state) {
            raster.cullMode = VK_CULL_MODE_FRONT_BIT;
            raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        }
        auto depth = vk_struct<VkPipelineDepthStencilStateCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
        depth.depthTestEnable = depth.depthWriteEnable = workload_.dynamic_state ? VK_FALSE : VK_TRUE;
        if(workload_.constants || workload_.descriptor_state)
            depth.depthWriteEnable = VK_FALSE;
        depth.depthCompareOp = workload_.dynamic_state ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
        depth.maxDepthBounds = 1;
        const std::array<VkDynamicState, 7> dynamic_states{
            workload_.dynamic_count ? VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT : VK_DYNAMIC_STATE_VIEWPORT,
            workload_.dynamic_count ? VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT : VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_CULL_MODE,
            VK_DYNAMIC_STATE_FRONT_FACE,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP};
        auto dynamic =
            vk_struct<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
        dynamic.pDynamicStates = dynamic_states.data();
        auto multisample =
            vk_struct<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if(options_.scenario == "pipeline-error") {
            attachment.colorWriteMask &= ~VK_COLOR_COMPONENT_R_BIT;
        }
        auto blend =
            vk_struct<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        auto create = vk_struct<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
        create.stageCount = static_cast<std::uint32_t>(stages.size());
        create.pStages = stages.data();
        create.pVertexInputState = &vertex;
        create.pInputAssemblyState = &assembly;
        create.pViewportState = &viewport_state;
        create.pRasterizationState = &raster;
        create.pMultisampleState = &multisample;
        create.pDepthStencilState = workload_.state ? &depth : nullptr;
        create.pColorBlendState = &blend;
        create.pDynamicState = workload_.dynamic_state ? &dynamic : nullptr;
        create.layout = pipeline_layout;
        create.renderPass = render_pass;
        check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &create, nullptr, &pipeline), "creating " + label);
        if(workload_.state) {
            Json inputs = {
                {"pipeline_handle", handle_bits(pipeline)},
                {"layout_handle", handle_bits(create.layout)},
                {"render_pass_handle", handle_bits(create.renderPass)},
                {"subpass", create.subpass},
                {"depthTestEnable", depth.depthTestEnable},
                {"depthWriteEnable", depth.depthWriteEnable},
                {"depthCompareOp", depth.depthCompareOp},
                {"cullMode", raster.cullMode},
                {"frontFace", raster.frontFace},
                {"viewport",
                 {viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth}},
                {"scissor", {scissor.offset.x, scissor.offset.y, scissor.extent.width, scissor.extent.height}},
                {"dynamic_states", workload_.dynamic_state ? Json(dynamic_states) : Json::array()}};
            inputs["push_size"] = push_size;
            inputs["label"] = label;
            inputs["descriptor_set_layouts"] = Json::array();
            for(std::uint32_t n = 0; n < layout.setLayoutCount; ++n)
                inputs["descriptor_set_layouts"].push_back(handle_bits(layout.pSetLayouts[n]));
            auto& witness_stages = inputs["stages"] = Json::array();
            for(const auto& stage : stages) {
                auto module = shader_witness_.at(std::to_string(handle_bits(stage.module)));
                module["stage"] = stage.stage;
                module["flags"] = stage.flags;
                module["entry_point"] = stage.pName;
                witness_stages.push_back(std::move(module));
            }
            if(label == "fixture.scene.graphics")
                state_witness_["pipeline_api_inputs"] = inputs;
            state_witness_["pipelines"].push_back(std::move(inputs));
        }
        name(VK_OBJECT_TYPE_PIPELINE, pipeline, label);
    }

    void create_pipeline() {
        make_pipeline(workload_.indirect ? "indirect.vert.spv" : "scene.vert.spv",
                      workload_.constants ? "state-constants.frag.spv"
                      : workload_.bindless
                          ? "bindless.frag.spv"
                          : (options_.scenario == "shader-error" ? "shader-error.frag.spv" : "scene.frag.spv"),
                      descriptor_layout_,
                      workload_.constants  ? 16u
                      : workload_.bindless ? 8u
                                           : 0u,
                      workload_.multipass ? offscreen_pass_ : render_pass_, pipeline_layout_, pipeline_,
                      "fixture.scene.graphics");
        if(workload_.constants) {
            make_pipeline("scene.vert.spv", "state-constants.frag.spv", descriptor_layout_, 16, render_pass_,
                          compatible_layout_, compatible_pipeline_, "fixture.constants.compatible");
            make_pipeline("scene.vert.spv", "state-constants.frag.spv", descriptor_layout_, 32, render_pass_,
                          incompatible_layout_, incompatible_pipeline_, "fixture.constants.incompatible");
        }
        if(workload_.descriptor_state) {
            make_pipeline("scene.vert.spv", "scene.frag.spv", compatible_descriptor_layout_, 0, render_pass_,
                          compatible_layout_, compatible_pipeline_, "fixture.descriptors.compatible");
            make_pipeline("scene.vert.spv", "scene.frag.spv", incompatible_descriptor_layout_, 0, render_pass_,
                          incompatible_layout_, incompatible_pipeline_, "fixture.descriptors.incompatible");
        }
        if(workload_.multipass) {
            make_pipeline("post.vert.spv", "post.frag.spv", post_descriptor_layout_, 4, render_pass_,
                          post_pipeline_layout_, post_pipeline_, "fixture.post.graphics");
        }
    }

    std::uint32_t timing_query_count() const {
        return options_.timing_draw_witness ? timing_max_queries : 2u;
    }

    void create_commands() {
        if(workload_.timing && (!debug_utils_ || !vkCmdBeginDebugUtilsLabelEXT || !vkCmdEndDebugUtilsLabelEXT))
            throw Unsupported("Timing workloads require VK_EXT_debug_utils command labels");
        if(options_.timing_draw_witness && (!vkQueueBeginDebugUtilsLabelEXT || !vkQueueEndDebugUtilsLabelEXT))
            throw Unsupported("Per-draw timing witness requires VK_EXT_debug_utils queue labels");
        if(!options_.timing_witness.empty()) {
            if(timestamp_bits_ < 36 || timestamp_bits_ > 64 || !std::isfinite(properties_.limits.timestampPeriod) ||
               properties_.limits.timestampPeriod <= 0)
                throw Unsupported("Timing witness requires a graphics queue with valid Vulkan timestamps");
            auto queries = vk_struct<VkQueryPoolCreateInfo>(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
            queries.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queries.queryCount = timing_query_count();
            check(vkCreateQueryPool(device_, &queries, nullptr, &timing_queries_), "creating timing witness queries");
            timing_witness_ = {
                {"schema_version", 1},
                {"evidence_origin", "private_fixture_timestamps"},
                {"project_version", ngm::project_version()},
                {"executable_sha256", ngm::sha256_file("/proc/self/exe")},
                {"shader_manifest_sha256", ngm::sha256_file(options_.shader_directory / "provenance.json")},
                {"gpu", gpu_metadata()},
                {"scenario", options_.scenario},
                {"timestamp_valid_bits", timestamp_bits_},
                {"timestamp_period_ns", properties_.limits.timestampPeriod},
                {"stage", "BOTTOM_OF_PIPE at both scene boundaries"},
                {"scope", "Incremental whole graphics-pass envelope, including clear/pipeline work. "
                          "Not an isolated draw duration or Nsight evidence. Host fence/read/write "
                          "cadence and timestamp commands are part of this validation workload."},
                {"submissions", Json::array()}};
            if(options_.timing_draw_witness) {
                timing_witness_["schema_version"] = 2;
                timing_witness_["query_count"] = timing_query_count();
                timing_witness_["stage"] = "BOTTOM_OF_PIPE before/after each labelled call and scene";
                timing_witness_["scope"] =
                    "Independent application timestamps around each one-call label, including indexed-bind state, "
                    "plus the whole scene. Queue labels distinguish frame/submission occurrences. These are "
                    "instrumented application envelopes, not Nsight evidence or isolated shader costs. Saved "
                    "replay execution is a separate run; it does not execute this application's witness I/O.";
                timing_witness_["inputs"] = {{"seed", options_.seed},
                                             {"width", options_.width},
                                             {"height", options_.height},
                                             {"frame", options_.frame}};
            }
            write_json(options_.timing_witness, timing_witness_);
        }
        auto pool = vk_struct<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = graphics_family_;
        check(vkCreateCommandPool(device_, &pool, nullptr, &command_pool_), "creating the command pool");
        auto allocation = vk_struct<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        allocation.commandPool = command_pool_;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device_, &allocation, &command_), "allocating frame commands");
        auto fence = vk_struct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        check(vkCreateFence(device_, &fence, nullptr, &submit_fence_), "creating the frame fence");
        const auto semaphore = vk_struct<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        check(vkCreateSemaphore(device_, &semaphore, nullptr, &acquired_), "creating the image acquisition semaphore");
        if(!maintenance_extension_.empty()) {
            fence.flags = 0;
            check(vkCreateFence(device_, &fence, nullptr, &present_fence_), "creating the presentation fence");
        } else {
            std::cerr << "ngm-vulkan-fixture: swapchain_maintenance1 presentation fences are unavailable; "
                         "device-idle teardown cannot formally confirm presentation-engine completion.\n";
        }
    }

    void update_palettes(std::uint32_t frame) {
        std::array<float, 4> primary{0, 0, 0, 1};
        const auto factor = 0.8f + 0.02f * static_cast<float>(frame % 11);
        for(std::uint32_t i = 0; i < 3; ++i) {
            const auto component = (options_.seed >> (i * 8)) & 255u;
            primary[i] = (0.65f + 0.35f * (static_cast<float>(component) / 255.0f)) * factor;
        }
        const std::array<float, 4> secondary{primary[2] * 0.25f, primary[0], primary[1] * 0.5f, 1.0f};
        const std::array values{primary, secondary};
        for(std::size_t i = 0; i < palettes_.size(); ++i) {
            for(std::size_t n = 0; n < (workload_.descriptor_state ? 16u : 1u); ++n)
                std::memcpy(static_cast<std::byte*>(palettes_[i].mapped) + n * descriptor_stride_, values[i].data(),
                            sizeof(values[i]));
            flush_buffer(palettes_[i], "flushing palette memory");
        }
    }

    void update_values(std::uint32_t frame, std::uint32_t occurrence) {
        if(occurrence > 2)
            throw std::logic_error("value-history submission exceeds its fixture contract");
        const auto phase = static_cast<std::uint32_t>((std::uint64_t{frame} * 3 + occurrence) % 11);
        update_palettes(phase);
        const auto source = workload_.value_error && occurrence == 2 ? 1u : occurrence == 1 ? 1u : 0u;
        auto copy = vk_struct<VkCopyDescriptorSet>(VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET);
        copy.srcSet = descriptors_[source];
        copy.srcBinding = 0;
        copy.dstSet = value_descriptor_;
        copy.dstBinding = 0;
        copy.descriptorCount = 1;
        vkUpdateDescriptorSets(device_, 0, nullptr, 1, &copy);
        if(!options_.timing_witness.empty()) {
            value_witness_ = {{"evidence_origin", "private_fixture_host_api_and_mapped_bytes"},
                              {"frame", frame},
                              {"submission_occurrence", occurrence},
                              {"phase", phase},
                              {"preceding_queue_idle", occurrence != 0},
                              {"bound_set_handle", handle_bits(value_descriptor_)},
                              {"copy",
                               {{"src_set", handle_bits(copy.srcSet)},
                                {"src_binding", copy.srcBinding},
                                {"src_array_element", copy.srcArrayElement},
                                {"dst_set", handle_bits(copy.dstSet)},
                                {"dst_binding", copy.dstBinding},
                                {"dst_array_element", copy.dstArrayElement},
                                {"descriptor_count", copy.descriptorCount}}},
                              {"buffers", Json::array()}};
            for(std::size_t n = 0; n < palettes_.size(); ++n) {
                const auto& buffer = palettes_[n];
                std::array<std::uint32_t, 4> bits{};
                std::memcpy(bits.data(), buffer.mapped, sizeof(bits));
                value_witness_["buffers"].push_back({{"source_set", handle_bits(descriptors_[n])},
                                                     {"buffer_handle", handle_bits(buffer.handle)},
                                                     {"memory_handle", handle_bits(buffer.memory)},
                                                     {"buffer_bytes", sizeof(bits)},
                                                     {"binding_offset", 0},
                                                     {"allocation_bytes", buffer.allocation_size},
                                                     {"map_offset", 0},
                                                     {"map_bytes", buffer.allocation_size},
                                                     {"upload_offset", 0},
                                                     {"upload_bytes", sizeof(bits)},
                                                     {"memory_type_index", buffer.memory_type_index},
                                                     {"memory_property_flags", buffer.memory_flags},
                                                     {"descriptor_offset", 0},
                                                     {"descriptor_range", sizeof(bits)},
                                                     {"float32_bits", bits}});
            }
        }
    }

    void begin_label(const std::string& text) {
        if(!options_.timing_witness.empty())
            timing_path_.push_back(text);
        if(debug_utils_ && vkCmdBeginDebugUtilsLabelEXT) {
            auto label = vk_struct<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
            label.pLabelName = text.c_str();
            label.color[0] = 0.2f;
            label.color[1] = 0.6f;
            label.color[2] = 1.0f;
            label.color[3] = 1.0f;
            vkCmdBeginDebugUtilsLabelEXT(command_, &label);
            if(timing_queries_) {
                const auto ordinal = timing_markers_.size();
                timing_markers_.push_back(
                    {{"ordinal", ordinal},
                     {"name", text},
                     {"parent_ordinal", timing_stack_.empty() ? Json(nullptr) : Json(timing_stack_.back())},
                     {"path", timing_path_}});
                timing_stack_.push_back(ordinal);
            }
        }
    }

    void end_label() {
        if(debug_utils_ && vkCmdEndDebugUtilsLabelEXT) {
            vkCmdEndDebugUtilsLabelEXT(command_);
        }
        if(!options_.timing_witness.empty()) {
            if(timing_path_.empty())
                throw std::logic_error("unbalanced private timing label witness");
            timing_path_.pop_back();
            if(timing_stack_.empty())
                throw std::logic_error("unbalanced private timing marker occurrence");
            timing_stack_.pop_back();
        }
    }

    void record(std::uint32_t image_index, std::uint32_t frame) {
        auto begin = vk_struct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = workload_.reuse_commands ? VkCommandBufferUsageFlags{0}
                                               : VkCommandBufferUsageFlags{VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        constant_draws_ = Json::array();
        descriptor_binds_ = descriptor_draws_ = Json::array();
        timing_draws_ = Json::array();
        timing_markers_ = Json::array();
        timing_path_.clear();
        timing_stack_.clear();
        check(vkBeginCommandBuffer(command_, &begin), "beginning frame commands");
        if(timing_queries_)
            vkCmdResetQueryPool(command_, timing_queries_, 0, timing_query_count());
        begin_label("frame." + std::to_string(frame));
        if(workload_.multipass) {
            auto image = vk_struct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
            image.srcAccessMask = frame == 0 ? VkAccessFlags{0} : VkAccessFlags{VK_ACCESS_SHADER_READ_BIT};
            image.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            image.oldLayout = frame == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            image.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            image.image = offscreen_image_;
            image.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(command_,
                                 frame == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &image);
        }
        begin_label(workload_.multipass ? "scene.offscreen" : "scene.raster");
        if(timing_queries_)
            vkCmdWriteTimestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timing_queries_, 0);
        VkClearValue clear{};
        clear.color.float32[0] = 0.03125f;
        clear.color.float32[1] = 0.0625f;
        clear.color.float32[2] = 0.09375f;
        clear.color.float32[3] = 1;
        auto pass = vk_struct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
        pass.renderPass = workload_.multipass ? offscreen_pass_ : render_pass_;
        pass.framebuffer = workload_.multipass ? offscreen_framebuffer_ : framebuffers_[image_index];
        pass.renderArea = {{0, 0}, extent_};
        VkClearValue depth_clear{};
        depth_clear.depthStencil.depth = 1;
        const std::array<VkClearValue, 2> clears{clear, depth_clear};
        pass.clearValueCount = workload_.state ? 2u : 1u;
        pass.pClearValues = clears.data();
        vkCmdBeginRenderPass(command_, &pass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        if(workload_.state)
            draw_witness_ = {{"command", workload_.indexed ? "vkCmdDrawIndexed" : "vkCmdDraw"},
                             {"bound_pipeline_handle", handle_bits(pipeline_)},
                             {"active_render_pass_handle", handle_bits(pass.renderPass)},
                             {"count", 3},
                             {"instanceCount", 1},
                             {"setters", Json::object()}};
        if(workload_.dynamic_state) {
            const VkViewport viewport{0, 0, static_cast<float>(extent_.width), static_cast<float>(extent_.height),
                                      0, 1};
            const VkRect2D scissor{{0, 0}, extent_};
            if(workload_.dynamic_count) {
                vkCmdSetViewportWithCount(command_, 1, &viewport);
                vkCmdSetScissorWithCount(command_, 1, &scissor);
            } else {
                vkCmdSetViewport(command_, 0, 1, &viewport);
                vkCmdSetScissor(command_, 0, 1, &scissor);
            }
            const VkBool32 enabled = VK_TRUE;
            const VkCompareOp compare = VK_COMPARE_OP_LESS;
            const VkCullModeFlags cull = VK_CULL_MODE_NONE;
            const VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            vkCmdSetDepthTestEnable(command_, enabled);
            vkCmdSetDepthWriteEnable(command_, enabled);
            vkCmdSetDepthCompareOp(command_, compare);
            vkCmdSetCullMode(command_, cull);
            vkCmdSetFrontFace(command_, front);
            draw_witness_["setters"] = {
                {"depthTestEnable", enabled},
                {"depthWriteEnable", enabled},
                {"depthCompareOp", compare},
                {"cullMode", cull},
                {"frontFace", front},
                {"viewport",
                 {viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth}},
                {"scissor", {scissor.offset.x, scissor.offset.y, scissor.extent.width, scissor.extent.height}}};
        }
        const auto descriptor =
            workload_.value_history ? value_descriptor_ : descriptors_[options_.scenario == "binding-error" ? 1 : 0];
        if(!workload_.descriptor_state)
            vkCmdBindDescriptorSets(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor, 0,
                                    nullptr);
        if(workload_.bindless) {
            const std::array<std::uint32_t, 2> selection{workload_.resource_xor, (options_.seed ^ frame) & 1u};
            vkCmdPushConstants(command_, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               static_cast<std::uint32_t>(sizeof(selection)), selection.data());
        }
        if(workload_.timing) {
            record_timing();
        } else if(workload_.descriptor_state) {
            record_descriptors();
        } else if(workload_.constants) {
            record_constants();
        } else if(workload_.indirect) {
            vkCmdDrawIndirect(command_, indirect_.handle, 0, 1, sizeof(VkDrawIndirectCommand));
        } else if(workload_.indexed) {
            vkCmdBindIndexBuffer(command_, indices_.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(command_, 3, 1, 0, 0, 0);
        } else {
            vkCmdDraw(command_, 3, 1, 0, 0);
        }
        vkCmdEndRenderPass(command_);
        if(timing_queries_)
            vkCmdWriteTimestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timing_queries_, 1);
        end_label();
        if(workload_.multipass) {
            auto sample = vk_struct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
            sample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sample.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            sample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            sample.image = offscreen_image_;
            sample.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(command_, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &sample);
            begin_label("post.present");
            pass.renderPass = render_pass_;
            pass.framebuffer = framebuffers_[image_index];
            vkCmdBeginRenderPass(command_, &pass, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline_);
            vkCmdBindDescriptorSets(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline_layout_, 0, 1,
                                    &post_descriptor_, 0, nullptr);
            vkCmdPushConstants(command_, post_pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(workload_.channel_order), &workload_.channel_order);
            vkCmdDraw(command_, 3, 1, 0, 0);
            vkCmdEndRenderPass(command_);
            end_label();
        }
        auto barrier = vk_struct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images_[image_index];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if(frame == options_.frame || (workload_.value_history && timing_queries_)) {
            begin_label("application.readback");
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            vkCmdPipelineBarrier(command_, source_stage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                 &barrier);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {extent_.width, extent_.height, 1};
            vkCmdCopyImageToBuffer(command_, images_[image_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   readback_.handle, 1, &copy);
            auto host = vk_struct<VkBufferMemoryBarrier>(VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
            host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            host.buffer = readback_.handle;
            host.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(command_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                                 &host, 0, nullptr);
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            end_label();
        }
        barrier.dstAccessMask = 0;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(command_, source_stage, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);
        end_label();
        check(vkEndCommandBuffer(command_), "ending frame commands");
    }

    void record_timing() {
        unsigned ordinal = 0;
        const auto draw = [&](std::uint32_t instances) {
            if(ordinal >= timing_draw_count)
                throw std::logic_error("timing workload exceeds its query allocation");
            if(options_.timing_draw_witness) {
                vkCmdWriteTimestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timing_queries_, 2 + ordinal * 2);
                begin_label("ngm.fixture.draw." + std::to_string(ordinal));
            }
            const bool indexed = workload_.indexed && (!workload_.reuse_commands || ordinal % 2 == 1);
            if(indexed) {
                vkCmdBindIndexBuffer(command_, indices_.handle, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(command_, 3, instances, 0, 0, 0);
            } else
                vkCmdDraw(command_, 3, instances, 0, 0);
            if(timing_queries_)
                timing_draws_.push_back({{"ordinal", ordinal},
                                         {"command", indexed ? "vkCmdDrawIndexed" : "vkCmdDraw"},
                                         {"count", 3},
                                         {"instance_count", instances},
                                         {"marker_ordinal", timing_stack_.back()},
                                         {"marker_path", timing_path_}});
            if(options_.timing_draw_witness) {
                end_label();
                vkCmdWriteTimestamp(command_, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timing_queries_, 3 + ordinal * 2);
            }
            ++ordinal;
        };
        begin_label("timing.batch");
        begin_label("branch.a");
        begin_label("unique.first");
        draw(8);
        end_label();
        for(const auto instances : {16u, 32u}) {
            begin_label("repeat");
            draw(instances);
            end_label();
        }
        end_label();
        begin_label("branch.b");
        begin_label("repeat");
        draw(64);
        end_label();
        begin_label("unique.pair");
        draw(128);
        draw(128);
        end_label();
        end_label();
        end_label();
        if(ordinal != timing_draw_count)
            throw std::logic_error("timing workload did not fill its query allocation");
    }

    void collect_timing(std::uint32_t frame, std::uint32_t occurrence,
                        std::chrono::steady_clock::time_point submitted_at, const std::string& queue_label) {
        check(vkWaitForFences(device_, 1, &submit_fence_, VK_TRUE, gpu_timeout_ns), "waiting for timing witness");
        const auto host_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - submitted_at)
                .count();
        const auto period = properties_.limits.timestampPeriod;
        if(static_cast<long double>(host_ns) >= std::ldexp(static_cast<long double>(period), timestamp_bits_))
            throw std::runtime_error("Timing witness may contain multiple timestamp wraps");
        std::array<std::uint64_t, timing_max_queries> timestamps{};
        const auto count = timing_query_count();
        check(vkGetQueryPoolResults(device_, timing_queries_, 0, count, count * sizeof(std::uint64_t),
                                    timestamps.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT),
              "reading timing witness");
        const auto mask = timestamp_bits_ == 64 ? UINT64_MAX : (std::uint64_t{1} << timestamp_bits_) - 1;
        for(std::size_t i = 0; i < count; ++i)
            if(timestamps[i] > mask)
                throw std::runtime_error("Timing witness has bits outside the queue's valid range");
        const auto ticks = (timestamps[1] - timestamps[0]) & mask;
        const auto ns = static_cast<double>(ticks) * period;
        if(!std::isfinite(ns) || ns <= 0)
            throw std::runtime_error("Timing witness has no positive finite scene duration");
        auto draws = timing_draws_;
        if(options_.timing_draw_witness) {
            if(draws.size() != timing_draw_count)
                throw std::logic_error("timing witness lost a recorded draw");
            std::uint64_t previous_end = 0;
            for(std::size_t i = 0; i < draws.size(); ++i) {
                const auto begin = timestamps[2 + 2 * i], end = timestamps[3 + 2 * i];
                const auto from_scene = (begin - timestamps[0]) & mask;
                const auto until_scene = (end - timestamps[0]) & mask;
                if(from_scene < previous_end || until_scene < from_scene || until_scene > ticks)
                    throw std::runtime_error("per-draw timestamp order escapes the scene envelope");
                const auto elapsed = (end - begin) & mask;
                const auto duration = static_cast<double>(elapsed) * period;
                if(!std::isfinite(duration))
                    throw std::runtime_error("per-draw timestamp duration is not finite");
                auto& draw = draws[i];
                draw["query_start"] = 2 + 2 * i;
                draw["query_end"] = 3 + 2 * i;
                draw["timestamp_start"] = begin;
                draw["timestamp_end"] = end;
                draw["elapsed_ticks"] = elapsed;
                draw["duration_ns"] = duration;
                previous_end = until_scene;
            }
        }
        Json submission{{"frame", frame},
                        {"submission_occurrence", occurrence},
                        {"queue_family", graphics_family_},
                        {"command_buffer_handle", handle_bits(command_)},
                        {"timestamp_start", timestamps[0]},
                        {"timestamp_end", timestamps[1]},
                        {"elapsed_ticks", ticks},
                        {"duration_ns", ns},
                        {"host_submit_wait_ns", host_ns},
                        {"draws", std::move(draws)},
                        {"markers", timing_markers_}};
        if(options_.timing_draw_witness)
            submission["queue_label"] = queue_label;
        if(workload_.value_history) {
            submission["value_history"] = value_witness_;
            submission["value_history"]["queue_idle_after_submit"] = occurrence == 2;
            if(!readback_.coherent) {
                auto range = vk_struct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
                range.memory = readback_.memory;
                range.size = VK_WHOLE_SIZE;
                check(vkInvalidateMappedMemoryRanges(device_, 1, &range), "invalidating value-history image readback");
            }
            const auto image =
                "value-frame-" + std::to_string(frame) + "-submission-" + std::to_string(occurrence) + ".ppm";
            write_image(options_.output / image);
            submission["value_history"]["gpu_readback"] = {{"evidence_origin", "application_gpu_image_readback"},
                                                           {"path", image},
                                                           {"sha256", ngm::sha256_file(options_.output / image)},
                                                           {"width", extent_.width},
                                                           {"height", extent_.height}};
        }
        timing_witness_["submissions"].push_back(std::move(submission));
        write_json(options_.timing_witness, timing_witness_);
    }

    void record_descriptors() {
        const auto bind = [&](VkPipelineLayout layout, std::uint32_t first, std::vector<unsigned> indices,
                              std::vector<unsigned> slots) {
            std::vector<VkDescriptorSet> sets;
            std::vector<std::uint32_t> offsets;
            for(const auto index : indices)
                sets.push_back(state_descriptors_.at(index));
            for(const auto slot : slots)
                offsets.push_back(static_cast<std::uint32_t>(slot * descriptor_stride_));
            vkCmdBindDescriptorSets(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, first,
                                    static_cast<std::uint32_t>(sets.size()), sets.data(),
                                    static_cast<std::uint32_t>(offsets.size()), offsets.data());
            Json handles = Json::array();
            for(const auto set : sets)
                handles.push_back(handle_bits(set));
            descriptor_binds_.push_back({{"layout_handle", handle_bits(layout)},
                                         {"first_set", first},
                                         {"descriptor_handles", handles},
                                         {"dynamic_offsets", offsets}});
        };
        const auto observe = [&](const char* label, VkPipeline pipeline, VkPipelineLayout layout,
                                 std::array<int, 2> bind_indices) {
            begin_label(label);
            vkCmdDraw(command_, 3, 1, 0, 0);
            Json expected = Json::array();
            for(std::size_t n = 0; n < bind_indices.size(); ++n) {
                Json item{{"set", n}, {"available", bind_indices[n] >= 0}};
                if(bind_indices[n] >= 0) {
                    const auto& actual = descriptor_binds_.at(static_cast<std::size_t>(bind_indices[n]));
                    const auto index = n - actual.at("first_set").get<std::size_t>();
                    Json offsets = Json::array();
                    for(std::size_t i = 0; i < 4; ++i)
                        offsets.push_back(actual.at("dynamic_offsets").at(index * 4 + i));
                    item["descriptor_handle"] = actual.at("descriptor_handles").at(index);
                    item["binding_layout_handle"] = actual.at("layout_handle");
                    item["dynamic_offsets"] = offsets;
                    item["bind_index"] = bind_indices[n];
                }
                expected.push_back(std::move(item));
            }
            descriptor_draws_.push_back({{"label", label},
                                         {"pipeline_handle", handle_bits(pipeline)},
                                         {"layout_handle", handle_bits(layout)},
                                         {"expected_sets", expected}});
            draw_witness_["bound_pipeline_handle"] = handle_bits(pipeline);
            end_label();
        };
        bind(pipeline_layout_, 0, {0, 1}, {1, 2, 3, 4, 5, 6, 7, 8});
        observe("descriptors.initial", pipeline_, pipeline_layout_, {0, 0});
        bind(compatible_layout_, 1, {2}, {9, 10, 11, 12});
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, compatible_pipeline_);
        observe("descriptors.partial", compatible_pipeline_, compatible_layout_, {0, 1});
        bind(incompatible_layout_, 0, {3}, {2, 3, 4, 5});
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, incompatible_pipeline_);
        observe("descriptors.disturbed", incompatible_pipeline_, incompatible_layout_, {2, -1});
        bind(compatible_layout_, 0, {0}, {3, 4, 5, 6});
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, compatible_pipeline_);
        observe("descriptors.prefix-restored", compatible_pipeline_, compatible_layout_, {3, -1});
        bind(compatible_layout_, 1, {2}, {4, 5, 6, 7});
        observe("descriptors.restored", compatible_pipeline_, compatible_layout_, {3, 4});
    }

    void record_constants() {
        std::array<std::uint32_t, 4> bank{1, 1, 0, 0x12345678};
        const auto observe = [&](const char* label, VkPipeline pipeline, VkPipelineLayout layout) {
            begin_label(label);
            vkCmdDraw(command_, 3, 1, 0, 0);
            std::string data;
            constexpr char digits[] = "0123456789abcdef";
            for(const auto value : std::as_bytes(std::span(bank))) {
                const auto b = std::to_integer<unsigned>(value);
                data += digits[b >> 4];
                data += digits[b & 15];
            }
            constant_draws_.push_back({{"label", label},
                                       {"pipeline_handle", handle_bits(pipeline)},
                                       {"layout_handle", handle_bits(layout)},
                                       {"offset", 0},
                                       {"length", sizeof(bank)},
                                       {"data", data},
                                       {"stage", VK_SHADER_STAGE_FRAGMENT_BIT}});
            draw_witness_["bound_pipeline_handle"] = handle_bits(pipeline);
            end_label();
        };
        vkCmdPushConstants(command_, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(bank), bank.data());
        observe("constants.initial", pipeline_, pipeline_layout_);
        const std::uint32_t blue = 1;
        vkCmdPushConstants(command_, compatible_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 8, sizeof(blue), &blue);
        std::memcpy(reinterpret_cast<std::byte*>(bank.data()) + 8, &blue, sizeof(blue));
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, compatible_pipeline_);
        vkCmdBindDescriptorSets(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, compatible_layout_, 0, 1, &descriptors_[0],
                                0, nullptr);
        observe("constants.partial", compatible_pipeline_, compatible_layout_);
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, incompatible_pipeline_);
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, compatible_pipeline_);
        observe("constants.restored", compatible_pipeline_, compatible_layout_);
    }

    void write_image(const std::filesystem::path& destination = {}) {
        const auto path = destination.empty() ? options_.output / "image.ppm" : destination;
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << "P6\n" << extent_.width << ' ' << extent_.height << "\n255\n";
        const auto* pixels = static_cast<const std::uint8_t*>(readback_.mapped);
        std::vector<char> row(static_cast<std::size_t>(extent_.width) * 3);
        const bool bgra = format_ == VK_FORMAT_B8G8R8A8_UNORM;
        for(std::uint32_t y = 0; y < extent_.height; ++y) {
            for(std::uint32_t x = 0; x < extent_.width; ++x) {
                const auto* pixel = pixels + (static_cast<std::size_t>(y) * extent_.width + x) * 4;
                row[x * 3] = static_cast<char>(pixel[bgra ? 2 : 0]);
                row[x * 3 + 1] = static_cast<char>(pixel[1]);
                row[x * 3 + 2] = static_cast<char>(pixel[bgra ? 0 : 2]);
            }
            output.write(row.data(), static_cast<std::streamsize>(row.size()));
        }
        output.close();
    }

    void destroy_buffer(Buffer& buffer) noexcept {
        if(buffer.mapped) {
            vkUnmapMemory(device_, buffer.memory);
        }
        if(buffer.handle) {
            vkDestroyBuffer(device_, buffer.handle, nullptr);
        }
        if(buffer.memory) {
            vkFreeMemory(device_, buffer.memory, nullptr);
        }
    }

    void cleanup() noexcept {
        if(device_) {
            // Maintenance1 establishes when the presentation engine releases
            // semaphore/swapchain handles, including enqueued error requests.
            // Do not destroy those handles after a failed fence wait; this
            // executable will release the Vulkan resources at process exit.
            if(present_pending_ &&
               vkWaitForFences(device_, 1, &present_fence_, VK_TRUE, gpu_timeout_ns) != VK_SUCCESS) {
                std::cerr << "ngm-vulkan-fixture: presentation cleanup did not complete; "
                             "leaving Vulkan resources for process teardown.\n";
                if(connection_) {
                    xcb_disconnect(connection_);
                }
                return;
            }
            // Without maintenance1, per-image semaphore reuse remains valid,
            // but WaitIdle alone cannot formally prove safe WSI teardown.
            vkDeviceWaitIdle(device_);
            if(timing_queries_)
                vkDestroyQueryPool(device_, timing_queries_, nullptr);
            if(command_pool_) {
                vkDestroyCommandPool(device_, command_pool_, nullptr);
            }
            if(submit_fence_) {
                vkDestroyFence(device_, submit_fence_, nullptr);
            }
            if(acquired_) {
                vkDestroySemaphore(device_, acquired_, nullptr);
            }
            if(present_fence_) {
                vkDestroyFence(device_, present_fence_, nullptr);
            }
            for(const auto semaphore : present_ready_) {
                if(semaphore) {
                    vkDestroySemaphore(device_, semaphore, nullptr);
                }
            }
            if(pipeline_) {
                vkDestroyPipeline(device_, pipeline_, nullptr);
            }
            if(post_pipeline_) {
                vkDestroyPipeline(device_, post_pipeline_, nullptr);
            }
            if(compatible_pipeline_)
                vkDestroyPipeline(device_, compatible_pipeline_, nullptr);
            if(incompatible_pipeline_)
                vkDestroyPipeline(device_, incompatible_pipeline_, nullptr);
            for(const auto shader : shaders_) {
                vkDestroyShaderModule(device_, shader, nullptr);
            }
            if(pipeline_layout_) {
                vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
            }
            if(post_pipeline_layout_) {
                vkDestroyPipelineLayout(device_, post_pipeline_layout_, nullptr);
            }
            if(compatible_layout_)
                vkDestroyPipelineLayout(device_, compatible_layout_, nullptr);
            if(incompatible_layout_)
                vkDestroyPipelineLayout(device_, incompatible_layout_, nullptr);
            if(descriptor_pool_) {
                vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
            }
            if(descriptor_layout_) {
                vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
            }
            if(compatible_descriptor_layout_)
                vkDestroyDescriptorSetLayout(device_, compatible_descriptor_layout_, nullptr);
            if(incompatible_descriptor_layout_)
                vkDestroyDescriptorSetLayout(device_, incompatible_descriptor_layout_, nullptr);
            if(post_descriptor_layout_) {
                vkDestroyDescriptorSetLayout(device_, post_descriptor_layout_, nullptr);
            }
            if(post_sampler_) {
                vkDestroySampler(device_, post_sampler_, nullptr);
            }
            for(auto& palette : palettes_) {
                destroy_buffer(palette);
            }
            destroy_buffer(readback_);
            destroy_buffer(indirect_);
            destroy_buffer(indices_);
            if(offscreen_framebuffer_) {
                vkDestroyFramebuffer(device_, offscreen_framebuffer_, nullptr);
            }
            if(offscreen_pass_) {
                vkDestroyRenderPass(device_, offscreen_pass_, nullptr);
            }
            if(offscreen_view_) {
                vkDestroyImageView(device_, offscreen_view_, nullptr);
            }
            if(offscreen_image_) {
                vkDestroyImage(device_, offscreen_image_, nullptr);
            }
            if(offscreen_memory_) {
                vkFreeMemory(device_, offscreen_memory_, nullptr);
            }
            for(const auto framebuffer : framebuffers_) {
                if(framebuffer) {
                    vkDestroyFramebuffer(device_, framebuffer, nullptr);
                }
            }
            if(render_pass_) {
                vkDestroyRenderPass(device_, render_pass_, nullptr);
            }
            if(depth_view_)
                vkDestroyImageView(device_, depth_view_, nullptr);
            if(depth_image_)
                vkDestroyImage(device_, depth_image_, nullptr);
            if(depth_memory_)
                vkFreeMemory(device_, depth_memory_, nullptr);
            for(const auto view : image_views_) {
                if(view) {
                    vkDestroyImageView(device_, view, nullptr);
                }
            }
            if(swapchain_) {
                vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            }
            vkDestroyDevice(device_, nullptr);
        }
        if(surface_) {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
        }
        if(instance_) {
            vkDestroyInstance(instance_, nullptr);
        }
        if(connection_) {
            if(window_) {
                xcb_destroy_window(connection_, window_);
            }
            xcb_disconnect(connection_);
        }
    }

    const Options& options_;
    SdkControl& sdk_;
    const Workload workload_;
    Json device_support_ = Json::array();
    Json state_witness_{{"schema_version", 1},
                        {"evidence_origin", "private_fixture_api_witness"},
                        {"scope", "API inputs and successful queue submission, not GPU state observation"},
                        {"submissions", Json::array()}};
    Json draw_witness_;
    Json shader_witness_ = Json::object();
    Json constant_draws_ = Json::array();
    Json descriptor_binds_ = Json::array(), descriptor_draws_ = Json::array();
    Json timing_witness_, timing_draws_, timing_markers_;
    std::vector<std::string> timing_path_;
    std::vector<std::size_t> timing_stack_;
    VkQueryPool timing_queries_ = VK_NULL_HANDLE;
    std::uint32_t timestamp_bits_ = 0;
    VkDeviceSize descriptor_stride_ = 0;
    VkDescriptorSetLayout compatible_descriptor_layout_ = VK_NULL_HANDLE,
                          incompatible_descriptor_layout_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 4> state_descriptors_{};
    xcb_connection_t* connection_ = nullptr;
    xcb_window_t window_ = XCB_WINDOW_NONE;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties_{};
    VkPhysicalDeviceIDProperties ids_{};
    VkPhysicalDeviceDriverProperties driver_{};
    VkPhysicalDeviceMemoryProperties memory_properties_{};
    VkDevice device_ = VK_NULL_HANDLE;
    std::uint32_t graphics_family_ = 0;
    std::uint32_t present_family_ = 0;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    bool debug_utils_ = false;
    bool surface_maintenance_khr_ = false;
    bool surface_maintenance_ext_ = false;
    std::string maintenance_extension_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;
    std::vector<VkSemaphore> present_ready_;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    VkImage depth_image_ = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory_ = VK_NULL_HANDLE;
    VkImageView depth_view_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkImage offscreen_image_ = VK_NULL_HANDLE;
    VkDeviceMemory offscreen_memory_ = VK_NULL_HANDLE;
    VkImageView offscreen_view_ = VK_NULL_HANDLE;
    VkRenderPass offscreen_pass_ = VK_NULL_HANDLE;
    VkFramebuffer offscreen_framebuffer_ = VK_NULL_HANDLE;
    VkSampler post_sampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout post_descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet post_descriptor_ = VK_NULL_HANDLE;
    VkPipelineLayout post_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline post_pipeline_ = VK_NULL_HANDLE;
    Buffer indirect_;
    Buffer indices_;
    Buffer readback_;
    std::array<Buffer, 2> palettes_{};
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> descriptors_{};
    VkDescriptorSet value_descriptor_ = VK_NULL_HANDLE;
    Json value_witness_;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    std::vector<VkShaderModule> shaders_;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout compatible_layout_ = VK_NULL_HANDLE, incompatible_layout_ = VK_NULL_HANDLE;
    VkPipeline compatible_pipeline_ = VK_NULL_HANDLE, incompatible_pipeline_ = VK_NULL_HANDLE;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    VkCommandBuffer command_ = VK_NULL_HANDLE;
    VkFence submit_fence_ = VK_NULL_HANDLE;
    VkSemaphore acquired_ = VK_NULL_HANDLE;
    VkFence present_fence_ = VK_NULL_HANDLE;
    bool present_pending_ = false;
};
} // namespace

Options parse_arguments(std::span<const char* const> arguments) {
    Options options;
    options.shader_directory = NGM_FIXTURE_SHADER_DIR;
    std::set<std::string_view> seen;
    for(std::size_t i = 0; i < arguments.size(); i += 2) {
        const std::string_view option(arguments[i]);
        if(i + 1 == arguments.size()) {
            throw std::invalid_argument("missing value for " + std::string(option));
        }
        if(!seen.insert(option).second) {
            throw std::invalid_argument("duplicate option " + std::string(option));
        }
        const std::string_view value(arguments[i + 1]);
        if(option == "--scenario") {
            options.scenario = value;
        } else if(option == "--seed") {
            options.seed = parse_uint(value, option);
        } else if(option == "--width") {
            options.width = parse_uint(value, option);
        } else if(option == "--height") {
            options.height = parse_uint(value, option);
        } else if(option == "--frame") {
            options.frame = parse_uint(value, option);
        } else if(option == "--sdk-first-boundary-frame") {
            options.sdk_first_boundary_frame = parse_uint(value, option);
        } else if(option == "--warmup") {
            options.warmup = parse_uint(value, option);
        } else if(option == "--compute-boundary") {
            if(value != "none" && value != "vk_frame_boundary")
                throw std::invalid_argument("compute boundary must be none or vk_frame_boundary");
            options.compute_frame_boundary = value == "vk_frame_boundary";
        } else if(option == "--output") {
            options.output = value;
        } else if(option == "--shader-dir") {
            options.shader_directory = value;
        } else if(option == "--state-witness") {
            options.state_witness = value;
        } else if(option == "--timing-witness") {
            options.timing_witness = value;
        } else if(option == "--timing-scope") {
            if(value != "scene" && value != "draw")
                throw std::invalid_argument("timing scope must be scene or draw");
            options.timing_draw_witness = value == "draw";
        } else {
            throw std::invalid_argument("unknown option " + std::string(option));
        }
    }
    for(const auto* required : {"--scenario", "--seed", "--width", "--height", "--frame", "--output"}) {
        if(!seen.contains(required)) {
            throw std::invalid_argument(std::string("required option ") + required + " was not supplied");
        }
    }
    if(std::find(scenarios.begin(), scenarios.end(), options.scenario) == scenarios.end()) {
        throw std::invalid_argument("unknown scenario " + options.scenario);
    }
    if(options.width < 32 || options.width > 4096 || options.height < 32 || options.height > 4096) {
        throw std::invalid_argument("width and height must each be in the range 32..4096");
    }
    if(options.frame > 600) {
        throw std::invalid_argument("frame must be in the range 0..600");
    }
    if(options.output.empty() || options.shader_directory.empty()) {
        throw std::invalid_argument("output and shader directory paths must not be empty");
    }
    options.output = std::filesystem::absolute(options.output);
    if(seen.contains("--state-witness") && options.state_witness.empty())
        throw std::invalid_argument("state witness path must not be empty");
    if(!options.state_witness.empty()) {
        options.state_witness = std::filesystem::absolute(options.state_witness).lexically_normal();
        const auto parent = std::filesystem::canonical(options.state_witness.parent_path());
        if(!options.scenario.starts_with("state-") || std::filesystem::exists(options.state_witness) ||
           std::filesystem::exists(options.state_witness.string() + ".tmp") ||
           options.state_witness == options.output.lexically_normal() ||
           (parent / options.state_witness.filename())
               .string()
               .starts_with(options.output.lexically_normal().string() + "/"))
            throw std::invalid_argument(
                "state witness requires a state scenario, a fresh file and a destination outside application output");
    }
    if(seen.contains("--timing-witness") && options.timing_witness.empty())
        throw std::invalid_argument("timing witness path must not be empty");
    if(seen.contains("--timing-scope") && options.timing_witness.empty())
        throw std::invalid_argument("timing scope requires --timing-witness");
    if(!options.timing_witness.empty()) {
        options.timing_witness = std::filesystem::absolute(options.timing_witness).lexically_normal();
        const auto parent = std::filesystem::canonical(options.timing_witness.parent_path());
        if(!options.scenario.starts_with("timing-") || std::filesystem::exists(options.timing_witness) ||
           std::filesystem::exists(options.timing_witness.string() + ".tmp") ||
           options.timing_witness == options.output.lexically_normal() ||
           (parent / options.timing_witness.filename())
               .string()
               .starts_with(options.output.lexically_normal().string() + "/"))
            throw std::invalid_argument(
                "timing witness requires a timing scenario, a fresh file and a destination outside application output");
    }
    if(options.sdk_first_boundary_frame &&
       (options.frame < 2 || *options.sdk_first_boundary_frame > options.frame - 2)) {
        throw std::invalid_argument("SDK first boundary must leave at least two subsequent application frames");
    }
    const bool performance = options.scenario.starts_with("performance-");
    const bool compute = options.scenario.starts_with("compute-") || performance;
    if(performance != options.warmup.has_value() ||
       (options.warmup && (*options.warmup > 100 || *options.warmup > options.frame)) ||
       (performance && options.compute_frame_boundary))
        throw std::invalid_argument("performance scenarios require --warmup 0..100, at least one measured frame and no "
                                    "frame-boundary extension");
    if((seen.contains("--compute-boundary") && !compute) || (compute && options.sdk_first_boundary_frame))
        throw std::invalid_argument("compute boundaries require a compute scenario; SDK control is graphics-only");
    if(compute && options.width * options.height > 16384)
        throw std::invalid_argument("compute element count (width * height) must not exceed 16384");
    options.shader_directory = std::filesystem::absolute(options.shader_directory);
    return options;
}

void run(const Options& options) {
    if(!std::filesystem::create_directory(options.output)) {
        throw std::runtime_error("output directory already exists: " + options.output.string() +
                                 "; each launch requires a new directory");
    }
    Json result{{"schema_version", 1},
                {"status", "failed"},
                {"project_version", ngm::project_version()},
                {"evidence_origin", "application_readback"},
                {"workload", workload_metadata(options.scenario)},
                {"inputs",
                 {{"scenario", options.scenario},
                  {"seed", options.seed},
                  {"width", options.width},
                  {"height", options.height},
                  {"frame", options.frame}}},
                {"desktop",
                 {{"backend", "xcb"},
                  {"display", environment("DISPLAY")},
                  {"session_type", environment("XDG_SESSION_TYPE")},
                  {"wayland_display", environment("WAYLAND_DISPLAY")}}}};
    if(!options.timing_witness.empty())
        result["inputs"]["timing_witness_scope"] = options.timing_draw_witness ? "draw" : "scene";
    SdkControl sdk(options.sdk_first_boundary_frame, options.output / "sdk-control.json");
    Renderer renderer(options, sdk);
    try {
        result["application"] = {{"executable_sha256", ngm::sha256_file("/proc/self/exe")},
                                 {"build", Json::parse(build_identity_json)}};
        result["provenance"] = retain_shaders(options);
        result["provenance"]["kind"] = "shader_bundle";
        if(options.warmup)
            result["inputs"]["warmup"] = *options.warmup;
        if(options.scenario.starts_with("compute-") || options.scenario.starts_with("performance-")) {
            run_compute(options, result);
            return;
        }
        sdk.initialize_before_vulkan({{"application", result.at("application")},
                                      {"inputs", result.at("inputs")},
                                      {"workload", result.at("workload")},
                                      {"shader_bundle", result.at("provenance")},
                                      {"desktop", result.at("desktop")}});
        result["sdk_control"] = sdk.report();
        renderer.initialize();
        result["device_support"] = renderer.device_support();
        result["gpu"] = renderer.gpu_metadata();
        result["rendering"] = renderer.rendering_metadata();
        renderer.render();
        result["sdk_control"] = sdk.report();
        result["image"] = {{"path", "image.ppm"},
                           {"width", options.width},
                           {"height", options.height},
                           {"format", "P6_RGB8"},
                           {"sha256", ngm::sha256_file(options.output / "image.ppm")}};
        result["status"] = "pass";
        write_json(options.output / "result.json", result);
    } catch(const std::exception& error) {
        result["sdk_control"] = sdk.report();
        result["device_support"] = renderer.device_support();
        result["status"] = dynamic_cast<const Unsupported*>(&error) ? "unsupported" : "failed";
        result["error"] = error.what();
        try {
            write_json(options.output / "result.json", result);
        } catch(const std::exception& write_error) {
            std::cerr << "ngm-vulkan-fixture: could not preserve failure result: " << write_error.what() << '\n';
        }
        throw;
    }
}
} // namespace ngm::fixture
