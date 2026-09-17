#include "Fixture.hpp"
#include "FixtureBuildIdentity.hpp"

#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <nlohmann/json.hpp>
#include <volk.h>
#include <xcb/xcb.h>

#include <algorithm>
#include <array>
#include <charconv>
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
constexpr std::array<std::string_view, 4> scenarios{"reference", "shader-error", "binding-error", "pipeline-error"};

template <typename T>
T vk_struct(VkStructureType type) {
    T value{};
    value.sType = type;
    return value;
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
        const bool vertex = source == "scene.vert";
        const bool fragment = source == "scene.frag" || source == "shader-error.frag";
        if((!vertex && !fragment) || spirv != source + ".spv" || stage != (vertex ? "vertex" : "fragment")) {
            throw std::runtime_error(
                "unexpected shader filename, stage, or source/SPIR-V pairing in fixture provenance");
        }
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
    for(const auto* required : {"scene.vert", "scene.vert.spv", "scene.frag", "scene.frag.spv", "shader-error.frag",
                                "shader-error.frag.spv"}) {
        if(!filenames.contains(required)) {
            throw std::runtime_error(std::string("shader provenance is missing required artifact ") + required);
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
};

class Renderer {
public:
    explicit Renderer(const Options& options) : options_(options) {}
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
            {"selected_shader_artifacts",
             {"shaders/scene.vert.spv",
              options_.scenario == "shader-error" ? "shaders/shader-error.frag.spv" : "shaders/scene.frag.spv"}},
            {"readback_before_presentation", true},
            {"presentation_completion", maintenance_extension_.empty() ? "device_wait_idle" : maintenance_extension_}};
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
            record(image_index, frame);
            const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            auto submit = vk_struct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &acquired_;
            submit.pWaitDstStageMask = &wait_stage;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command_;
            submit.signalSemaphoreCount = 1;
            submit.pSignalSemaphores = &present_ready_[image_index];
            check(vkQueueSubmit(graphics_queue_, 1, &submit, submit_fence_), "submitting the scene");
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
            if(properties.apiVersion < VK_API_VERSION_1_3) {
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
                physical_ = candidate;
                graphics_family_ = *graphics;
                present_family_ = *present;
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
        }
        if(physical_ == VK_NULL_HANDLE) {
            throw Unsupported("no Vulkan 1.3 GPU supports graphics and presentation to this X11/Xwayland desktop");
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
        create.pNext = maintenance_extension_.empty() ? nullptr : &maintenance;
        check(vkCreateDevice(physical_, &create, nullptr, &device_), "creating the graphics device");
        volkLoadDevice(device_);
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

    void create_render_pass() {
        VkAttachmentDescription color{};
        color.format = format_;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &reference;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        auto create = vk_struct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
        create.attachmentCount = 1;
        create.pAttachments = &color;
        create.subpassCount = 1;
        create.pSubpasses = &subpass;
        create.dependencyCount = 1;
        create.pDependencies = &dependency;
        check(vkCreateRenderPass(device_, &create, nullptr, &render_pass_), "creating the raster render pass");
        name(VK_OBJECT_TYPE_RENDER_PASS, render_pass_, "fixture.scene.raster");
        framebuffers_.resize(image_views_.size(), VK_NULL_HANDLE);
        for(std::size_t i = 0; i < framebuffers_.size(); ++i) {
            auto framebuffer = vk_struct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
            framebuffer.renderPass = render_pass_;
            framebuffer.attachmentCount = 1;
            framebuffer.pAttachments = &image_views_[i];
            framebuffer.width = extent_.width;
            framebuffer.height = extent_.height;
            framebuffer.layers = 1;
            check(vkCreateFramebuffer(device_, &framebuffer, nullptr, &framebuffers_[i]), "creating a frame buffer");
        }
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
        auto allocate = vk_struct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = *selected;
        check(vkAllocateMemory(device_, &allocate, nullptr, &buffer.memory), "allocating " + label);
        check(vkBindBufferMemory(device_, buffer.handle, buffer.memory, 0), "binding " + label);
        check(vkMapMemory(device_, buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer.mapped), "mapping " + label);
        name(VK_OBJECT_TYPE_BUFFER, buffer.handle, label);
    }

    void create_buffers_and_descriptors() {
        create_buffer(readback_, static_cast<VkDeviceSize>(extent_.width) * extent_.height * 4,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT, "fixture.application_readback");
        create_buffer(palettes_[0], sizeof(float) * 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "fixture.palette.primary");
        create_buffer(palettes_[1], sizeof(float) * 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "fixture.palette.secondary");
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        auto layout = vk_struct<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        layout.bindingCount = 1;
        layout.pBindings = &binding;
        check(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &descriptor_layout_),
              "creating the palette layout");
        const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2};
        auto pool = vk_struct<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets = 2;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &size;
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptor_pool_),
              "creating the palette descriptor pool");
        const std::array layouts{descriptor_layout_, descriptor_layout_};
        auto allocation = vk_struct<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        allocation.descriptorPool = descriptor_pool_;
        allocation.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
        allocation.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocation, descriptors_.data()), "allocating palette descriptors");
        for(std::size_t i = 0; i < descriptors_.size(); ++i) {
            const VkDescriptorBufferInfo buffer{palettes_[i].handle, 0, sizeof(float) * 4};
            auto write = vk_struct<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            write.dstSet = descriptors_[i];
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &buffer;
            vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
            name(VK_OBJECT_TYPE_DESCRIPTOR_SET, descriptors_[i], "fixture.palette_set." + std::to_string(i));
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
        shaders_.push_back(shader);
        name(VK_OBJECT_TYPE_SHADER_MODULE, shader, "fixture." + filename);
        return shader;
    }

    void create_pipeline() {
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = vk_struct<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = load_shader("scene.vert.spv");
        stages[0].pName = "main";
        stages[1] = vk_struct<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module =
            load_shader(options_.scenario == "shader-error" ? "shader-error.frag.spv" : "scene.frag.spv");
        stages[1].pName = "main";
        auto layout = vk_struct<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptor_layout_;
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &pipeline_layout_),
              "creating the scene pipeline layout");
        const auto vertex =
            vk_struct<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
        auto assembly = vk_struct<VkPipelineInputAssemblyStateCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const VkViewport viewport{0, 0, static_cast<float>(extent_.width), static_cast<float>(extent_.height), 0, 1};
        const VkRect2D scissor{{0, 0}, extent_};
        auto viewport_state =
            vk_struct<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;
        auto raster = vk_struct<VkPipelineRasterizationStateCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
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
        create.pColorBlendState = &blend;
        create.layout = pipeline_layout_;
        create.renderPass = render_pass_;
        check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &create, nullptr, &pipeline_),
              "creating the scene graphics pipeline");
        name(VK_OBJECT_TYPE_PIPELINE, pipeline_, "fixture.scene.graphics");
    }

    void create_commands() {
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
            std::memcpy(palettes_[i].mapped, values[i].data(), sizeof(values[i]));
            if(!palettes_[i].coherent) {
                auto range = vk_struct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
                range.memory = palettes_[i].memory;
                range.size = VK_WHOLE_SIZE;
                check(vkFlushMappedMemoryRanges(device_, 1, &range), "flushing palette memory");
            }
        }
    }

    void begin_label(const std::string& text) {
        if(debug_utils_ && vkCmdBeginDebugUtilsLabelEXT) {
            auto label = vk_struct<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
            label.pLabelName = text.c_str();
            label.color[0] = 0.2f;
            label.color[1] = 0.6f;
            label.color[2] = 1.0f;
            label.color[3] = 1.0f;
            vkCmdBeginDebugUtilsLabelEXT(command_, &label);
        }
    }

    void end_label() {
        if(debug_utils_ && vkCmdEndDebugUtilsLabelEXT) {
            vkCmdEndDebugUtilsLabelEXT(command_);
        }
    }

    void record(std::uint32_t image_index, std::uint32_t frame) {
        auto begin = vk_struct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command_, &begin), "beginning frame commands");
        begin_label("frame." + std::to_string(frame));
        begin_label("scene.raster");
        VkClearValue clear{};
        clear.color.float32[0] = 0.03125f;
        clear.color.float32[1] = 0.0625f;
        clear.color.float32[2] = 0.09375f;
        clear.color.float32[3] = 1;
        auto pass = vk_struct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
        pass.renderPass = render_pass_;
        pass.framebuffer = framebuffers_[image_index];
        pass.renderArea = {{0, 0}, extent_};
        pass.clearValueCount = 1;
        pass.pClearValues = &clear;
        vkCmdBeginRenderPass(command_, &pass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        const auto descriptor = descriptors_[options_.scenario == "binding-error" ? 1 : 0];
        vkCmdBindDescriptorSets(command_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1, &descriptor, 0,
                                nullptr);
        vkCmdDraw(command_, 3, 1, 0, 0);
        vkCmdEndRenderPass(command_);
        end_label();
        auto barrier = vk_struct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images_[image_index];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        auto source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if(frame == options_.frame) {
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

    void write_image() {
        const auto path = options_.output / "image.ppm";
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
            for(const auto shader : shaders_) {
                vkDestroyShaderModule(device_, shader, nullptr);
            }
            if(pipeline_layout_) {
                vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
            }
            if(descriptor_pool_) {
                vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
            }
            if(descriptor_layout_) {
                vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
            }
            for(auto& palette : palettes_) {
                destroy_buffer(palette);
            }
            destroy_buffer(readback_);
            for(const auto framebuffer : framebuffers_) {
                if(framebuffer) {
                    vkDestroyFramebuffer(device_, framebuffer, nullptr);
                }
            }
            if(render_pass_) {
                vkDestroyRenderPass(device_, render_pass_, nullptr);
            }
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
    std::vector<VkFramebuffer> framebuffers_;
    Buffer readback_;
    std::array<Buffer, 2> palettes_{};
    VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> descriptors_{};
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    std::vector<VkShaderModule> shaders_;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
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
        } else if(option == "--output") {
            options.output = value;
        } else if(option == "--shader-dir") {
            options.shader_directory = value;
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
    try {
        result["application"] = {{"executable_sha256", ngm::sha256_file("/proc/self/exe")},
                                 {"build", Json::parse(build_identity_json)}};
        result["provenance"] = retain_shaders(options);
        result["provenance"]["kind"] = "shader_bundle";
        Renderer renderer(options);
        renderer.initialize();
        result["gpu"] = renderer.gpu_metadata();
        result["rendering"] = renderer.rendering_metadata();
        renderer.render();
        result["image"] = {{"path", "image.ppm"},
                           {"width", options.width},
                           {"height", options.height},
                           {"format", "P6_RGB8"},
                           {"sha256", ngm::sha256_file(options.output / "image.ppm")}};
        result["status"] = "pass";
        write_json(options.output / "result.json", result);
    } catch(const std::exception& error) {
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
