#include "Fixture.hpp"
#include "ngm/Hash.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <volk.h>

namespace ngm::fixture {
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
void check(VkResult r, const char* text) {
    if(r != VK_SUCCESS)
        throw std::runtime_error(std::string(text) + ": " + std::to_string(r));
}
template <typename T>
T info(VkStructureType type) {
    T v{};
    v.sType = type;
    return v;
}
void save(const fs::path& path, const Json& value) {
    std::ofstream output(path.string() + ".tmp");
    output.exceptions(std::ios::failbit | std::ios::badbit);
    output << value.dump(2) << '\n';
    output.close();
    fs::rename(path.string() + ".tmp", path);
}
struct Compute {
    explicit Compute(uint32_t elements, uint32_t input_seed) : count(elements), seed(input_seed) {}
    Compute(const Compute&) = delete;
    Compute& operator=(const Compute&) = delete;
    uint32_t count;
    uint32_t seed;
    VkInstance instance{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    uint32_t family{};
    bool boundary{};
    bool outstanding{};
    VkPhysicalDeviceProperties properties{};
    struct Buffer {
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        uint32_t* mapped{};
    };
    Buffer input, output;
    VkDescriptorSetLayout set_layout{};
    VkDescriptorPool descriptor_pool{};
    VkDescriptorSet descriptor{};
    VkPipelineLayout layout{};
    VkShaderModule module{};
    VkPipeline pipeline{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    VkFence fence{};
    ~Compute() {
        // Failed waits must reach the caller's evidence writer. Do not issue
        // unbounded Vulkan teardown on a queue that did not finish; the owned
        // process exits and the supervisor bounds any driver-side exit stall.
        if(outstanding)
            return;
        if(device) {
            if(fence)
                vkDestroyFence(device, fence, nullptr);
            if(pool)
                vkDestroyCommandPool(device, pool, nullptr);
            if(pipeline)
                vkDestroyPipeline(device, pipeline, nullptr);
            if(module)
                vkDestroyShaderModule(device, module, nullptr);
            if(layout)
                vkDestroyPipelineLayout(device, layout, nullptr);
            if(descriptor_pool)
                vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
            if(set_layout)
                vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
            for(auto* b : {&input, &output}) {
                if(b->mapped)
                    vkUnmapMemory(device, b->memory);
                if(b->buffer)
                    vkDestroyBuffer(device, b->buffer, nullptr);
                if(b->memory)
                    vkFreeMemory(device, b->memory, nullptr);
            }
            vkDestroyDevice(device, nullptr);
        }
        if(instance)
            vkDestroyInstance(instance, nullptr);
    }
    void name(VkObjectType type, uint64_t handle, const char* value) {
        if(vkSetDebugUtilsObjectNameEXT) {
            auto n = info<VkDebugUtilsObjectNameInfoEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT);
            n.objectType = type;
            n.objectHandle = handle;
            n.pObjectName = value;
            check(vkSetDebugUtilsObjectNameEXT(device, &n), "name");
        }
    }
    void make_buffer(Buffer& b, const char* label) {
        auto create = info<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        create.size = count * sizeof(uint32_t);
        create.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &create, nullptr, &b.buffer), "buffer");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, b.buffer, &requirements);
        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        uint32_t index = memory.memoryTypeCount;
        for(uint32_t i = 0; i < memory.memoryTypeCount; ++i)
            if((requirements.memoryTypeBits & (1u << i)) &&
               (memory.memoryTypes[i].propertyFlags &
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                   (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                index = i;
                break;
            }
        if(index == memory.memoryTypeCount)
            throw Unsupported("No coherent host-visible storage memory");
        auto alloc = info<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = requirements.size;
        alloc.memoryTypeIndex = index;
        check(vkAllocateMemory(device, &alloc, nullptr, &b.memory), "allocate");
        check(vkBindBufferMemory(device, b.buffer, b.memory, 0), "bind");
        void* mapped{};
        check(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &mapped), "map");
        b.mapped = static_cast<uint32_t*>(mapped);
        name(VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(b.buffer), label);
    }
    void initialize(const fs::path& shader, bool require_boundary) {
        check(volkInitialize(), "volk");
        auto app = info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        app.pApplicationName = "ngm-vulkan-fixture-compute";
        app.apiVersion = VK_API_VERSION_1_3;
        uint32_t n{};
        check(vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr), "instance extensions");
        std::vector<VkExtensionProperties> ie(n);
        check(vkEnumerateInstanceExtensionProperties(nullptr, &n, ie.data()), "instance extensions");
        const char* debug = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        bool debug_available =
            std::any_of(ie.begin(), ie.end(), [&](auto& e) { return std::strcmp(e.extensionName, debug) == 0; });
        auto ci = info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        ci.pApplicationInfo = &app;
        ci.enabledExtensionCount = debug_available ? 1 : 0;
        ci.ppEnabledExtensionNames = debug_available ? &debug : nullptr;
        check(vkCreateInstance(&ci, nullptr, &instance), "instance");
        volkLoadInstance(instance);
        check(vkEnumeratePhysicalDevices(instance, &n, nullptr), "devices");
        std::vector<VkPhysicalDevice> devices(n);
        check(vkEnumeratePhysicalDevices(instance, &n, devices.data()), "devices");
        for(auto candidate : devices) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(candidate, &p);
            if(p.apiVersion < VK_API_VERSION_1_3)
                continue;
            if(require_boundary) {
                uint32_t en{};
                check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &en, nullptr), "candidate extensions");
                std::vector<VkExtensionProperties> available(en);
                check(vkEnumerateDeviceExtensionProperties(candidate, nullptr, &en, available.data()),
                      "candidate extensions");
                if(std::none_of(available.begin(), available.end(), [](const auto& e) {
                       return std::strcmp(e.extensionName, VK_EXT_FRAME_BOUNDARY_EXTENSION_NAME) == 0;
                   }))
                    continue;
                auto fb = info<VkPhysicalDeviceFrameBoundaryFeaturesEXT>(
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT);
                auto f = info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
                f.pNext = &fb;
                vkGetPhysicalDeviceFeatures2(candidate, &f);
                if(!fb.frameBoundary)
                    continue;
            }
            uint32_t qn{};
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qn, nullptr);
            std::vector<VkQueueFamilyProperties> qs(qn);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qn, qs.data());
            for(uint32_t i = 0; i < qn; ++i)
                if(qs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                    physical = candidate;
                    family = i;
                    properties = p;
                    break;
                }
            if(physical)
                break;
        }
        if(!physical)
            throw Unsupported(require_boundary
                                  ? "No Vulkan 1.3 compute queue with VK_EXT_frame_boundary extension/feature"
                                  : "No Vulkan 1.3 compute queue");
        check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, nullptr), "device extensions");
        std::vector<VkExtensionProperties> extensions(n);
        check(vkEnumerateDeviceExtensionProperties(physical, nullptr, &n, extensions.data()), "device extensions");
        const char* ext = VK_EXT_FRAME_BOUNDARY_EXTENSION_NAME;
        boundary = std::any_of(extensions.begin(), extensions.end(),
                               [&](auto& e) { return std::strcmp(e.extensionName, ext) == 0; });
        auto feature = info<VkPhysicalDeviceFrameBoundaryFeaturesEXT>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAME_BOUNDARY_FEATURES_EXT);
        if(boundary) {
            auto f = info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            f.pNext = &feature;
            vkGetPhysicalDeviceFeatures2(physical, &f);
            boundary = feature.frameBoundary;
        }
        if(require_boundary && !boundary)
            throw Unsupported("VK_EXT_frame_boundary extension/feature unavailable");
        boundary = boundary && require_boundary;
        float priority = 1;
        auto qc = info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        qc.queueFamilyIndex = family;
        qc.queueCount = 1;
        qc.pQueuePriorities = &priority;
        auto dc = info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        dc.queueCreateInfoCount = 1;
        dc.pQueueCreateInfos = &qc;
        dc.enabledExtensionCount = boundary ? 1 : 0;
        dc.ppEnabledExtensionNames = boundary ? &ext : nullptr;
        dc.pNext = boundary ? &feature : nullptr;
        check(vkCreateDevice(physical, &dc, nullptr, &device), "device");
        volkLoadDevice(device);
        vkGetDeviceQueue(device, family, 0, &queue);
        make_buffer(input, "compute.input");
        make_buffer(output, "compute.output");
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for(uint32_t i = 0; i < 2; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        auto sl = info<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        sl.bindingCount = 2;
        sl.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(device, &sl, nullptr, &set_layout), "set layout");
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
        auto dp = info<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        dp.maxSets = 1;
        dp.poolSizeCount = 1;
        dp.pPoolSizes = &size;
        check(vkCreateDescriptorPool(device, &dp, nullptr, &descriptor_pool), "pool");
        auto da = info<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        da.descriptorPool = descriptor_pool;
        da.descriptorSetCount = 1;
        da.pSetLayouts = &set_layout;
        check(vkAllocateDescriptorSets(device, &da, &descriptor), "descriptor");
        std::array<VkDescriptorBufferInfo, 2> buffers{{{input.buffer, 0, count * 4}, {output.buffer, 0, count * 4}}};
        std::array<VkWriteDescriptorSet, 2> writes{};
        for(uint32_t i = 0; i < 2; ++i) {
            writes[i] = info<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            writes[i].dstSet = descriptor;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &buffers[i];
        }
        vkUpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, 4};
        auto pl = info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &set_layout;
        pl.pushConstantRangeCount = 1;
        pl.pPushConstantRanges = &range;
        check(vkCreatePipelineLayout(device, &pl, nullptr, &layout), "pipeline layout");
        const auto bytes = fs::file_size(shader);
        if(!bytes || bytes % 4 || bytes > 1024 * 1024)
            throw std::runtime_error("bad shader size");
        std::vector<uint32_t> code(bytes / 4);
        std::ifstream in(shader, std::ios::binary);
        in.exceptions(std::ios::failbit | std::ios::badbit);
        in.read(reinterpret_cast<char*>(code.data()), bytes);
        auto sm = info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        sm.codeSize = bytes;
        sm.pCode = code.data();
        check(vkCreateShaderModule(device, &sm, nullptr, &module), "module");
        name(VK_OBJECT_TYPE_SHADER_MODULE, reinterpret_cast<uint64_t>(module), "compute.affine.shader");
        auto pc = info<VkComputePipelineCreateInfo>(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
        pc.layout = layout;
        pc.stage = info<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        pc.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.stage.module = module;
        pc.stage.pName = "main";
        check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pc, nullptr, &pipeline), "pipeline");
        name(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(pipeline), "compute.affine.pipeline");
        auto cp = info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        cp.queueFamilyIndex = family;
        check(vkCreateCommandPool(device, &cp, nullptr, &pool), "command pool");
        auto ca = info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        ca.commandPool = pool;
        ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ca.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device, &ca, &command), "command buffer");
        auto fc = info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        check(vkCreateFence(device, &fc, nullptr, &fence), "fence");
    }
    void finish_frame(uint32_t frame) {
        if(!boundary)
            return;
        auto fb = info<VkFrameBoundaryEXT>(VK_STRUCTURE_TYPE_FRAME_BOUNDARY_EXT);
        fb.flags = VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT;
        fb.frameID = frame;
        fb.bufferCount = 1;
        fb.pBuffers = &output.buffer;
        auto submit = info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.pNext = &fb;
        check(vkResetFences(device, 1, &fence), "reset boundary fence");
        outstanding = true;
        check(vkQueueSubmit(queue, 1, &submit, fence), "frame end");
        check(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ull), "wait frame end");
        outstanding = false;
    }
    void frame(uint32_t frame) {
        for(uint32_t i = 0; i < count; ++i) {
            input.mapped[i] = (i * 13u ^ seed) + frame * 7u;
            output.mapped[i] = 0xdeadbeefu;
        }
        check(vkResetCommandPool(device, pool, 0), "reset pool");
        auto begin = info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command, &begin), "begin");
        if(vkCmdBeginDebugUtilsLabelEXT) {
            auto label = info<VkDebugUtilsLabelEXT>(VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT);
            label.pLabelName = "compute.affine";
            vkCmdBeginDebugUtilsLabelEXT(command, &label);
        }
        auto host = info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        host.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        host.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &host, 0,
                             nullptr, 0, nullptr);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &descriptor, 0, nullptr);
        vkCmdPushConstants(command, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4, &count);
        vkCmdDispatch(command, (count + 63) / 64, 1, 1);
        auto read = info<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        read.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        read.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &read, 0,
                             nullptr, 0, nullptr);
        if(vkCmdEndDebugUtilsLabelEXT)
            vkCmdEndDebugUtilsLabelEXT(command);
        check(vkEndCommandBuffer(command), "end");
        // Only the separate end submission carries a boundary. The measured
        // Nsight delimiter also counts a non-END annotation, which would split
        // dispatch and readback into different capture intervals.
        auto submit = info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        check(vkResetFences(device, 1, &fence), "reset fence");
        outstanding = true;
        check(vkQueueSubmit(queue, 1, &submit, fence), "submit");
        check(vkWaitForFences(device, 1, &fence, VK_TRUE, 5000000000ull), "wait");
        outstanding = false;
    }
};
std::string version(uint32_t value) {
    return std::to_string(VK_API_VERSION_MAJOR(value)) + "." + std::to_string(VK_API_VERSION_MINOR(value)) + "." +
           std::to_string(VK_API_VERSION_PATCH(value));
}
std::string uuid(const uint8_t* bytes) {
    const char* hex = "0123456789abcdef";
    std::string result;
    for(unsigned i = 0; i < VK_UUID_SIZE; ++i) {
        result += hex[bytes[i] >> 4];
        result += hex[bytes[i] & 15];
    }
    return result;
}
} // namespace

void run_compute(const Options& options, nlohmann::json& result) {
    const auto count = options.width * options.height;
    const auto source = options.scenario + ".comp";
    const auto shader_path = options.output / "shaders" / (source + ".spv");
    result["desktop"]["backend"] = "none";
    result["workload"] = {{"kind", "compute_affine_uint32"},
                          {"presentation", false},
                          {"element_count", count},
                          {"minimum_api_version", "1.3.0"},
                          {"boundary", options.compute_frame_boundary ? "vk_frame_boundary" : "none"},
                          {"required_device_extensions",
                           options.compute_frame_boundary ? Json({"VK_EXT_frame_boundary"}) : Json::array()}};
    Compute compute(count, options.seed);
    compute.initialize(shader_path, options.compute_frame_boundary);
    auto driver = info<VkPhysicalDeviceDriverProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    auto ids = info<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    auto properties = info<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    properties.pNext = &driver;
    driver.pNext = &ids;
    vkGetPhysicalDeviceProperties2(compute.physical, &properties);
    const auto& p = properties.properties;
    result["gpu"] = {{"name", p.deviceName},
                     {"vendor_id", p.vendorID},
                     {"device_id", p.deviceID},
                     {"device_type", static_cast<uint32_t>(p.deviceType)},
                     {"api_version", version(p.apiVersion)},
                     {"driver_version_raw", p.driverVersion},
                     {"driver_id", static_cast<uint32_t>(driver.driverID)},
                     {"driver_name", driver.driverName},
                     {"driver_info", driver.driverInfo},
                     {"device_uuid", uuid(ids.deviceUUID)},
                     {"driver_uuid", uuid(ids.driverUUID)}};
    result["compute"] = {
        {"evidence_origin", "application_observation"},
        {"entry_point", "main"},
        {"shader_source", "shaders/" + source},
        {"shader_spirv", "shaders/" + source + ".spv"},
        {"source_sha256", ngm::sha256_file(options.output / "shaders" / source)},
        {"spirv_sha256", ngm::sha256_file(shader_path)},
        {"pipeline_label", "compute.affine.pipeline"},
        {"shader_label", "compute.affine.shader"},
        {"dispatch_label", "compute.affine"},
        {"local_size", {64, 1, 1}},
        {"group_count", {(count + 63) / 64, 1, 1}},
        {"push_constant_count", count},
        {"descriptor_bindings",
         Json::array({{{"set", 0}, {"binding", 0}, {"label", "compute.input"}, {"bytes", count * 4}},
                      {{"set", 0}, {"binding", 1}, {"label", "compute.output"}, {"bytes", count * 4}}})},
        {"boundary_enabled", compute.boundary}};
    // This setup can survive capture-driven target termination. It records
    // application observations, never an inferred join to Nsight event IDs.
    result["status"] = "running";
    save(options.output / "compute-setup.json", result);
    for(uint32_t frame = 0; frame <= options.frame; ++frame) {
        compute.frame(frame);
        const auto name = "compute-frame-" + std::to_string(frame) + ".json";
        Json row{{"schema_version", 1},
                 {"evidence_origin", "application_readback"},
                 {"phase", "readback_before_frame_end"},
                 {"frame", frame},
                 {"frame_boundary_id", compute.boundary ? Json(frame) : Json(nullptr)},
                 {"inputs", result.at("inputs")},
                 {"compute", result.at("compute")},
                 {"executable_sha256", result.at("application").at("executable_sha256")},
                 {"input", std::vector<uint32_t>(compute.input.mapped, compute.input.mapped + count)},
                 {"output", std::vector<uint32_t>(compute.output.mapped, compute.output.mapped + count)}};
        save(options.output / name, row);
        result["readback"] = {{"path", name}, {"sha256", ngm::sha256_file(options.output / name)}};
        compute.finish_frame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // Pass means execution completed; correctness is evaluated by an independent
    // harness oracle. Deliberately faulty variants also complete execution.
    result["status"] = "pass";
    save(options.output / "result.json", result);
}
} // namespace ngm::fixture
