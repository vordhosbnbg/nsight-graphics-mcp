#include "Check.hpp"
#include "CppReplay.hpp"
#include "DrawStateFixture.hpp"
#include "ReplayValues.hpp"
#include "ValueExecutionState.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"

namespace {
using ngm::check::require;
using namespace ngm::detail;
const std::string memory = R"cpp(
void CreateResourcesMemory() {
  BEGIN_DATA_SCOPE_FUNCTION();
  {
    static VkQueryPoolCreateInfo VkQueryPoolCreateInfo_info[1] = {VkQueryPoolCreateInfo{
      VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,nullptr,VkQueryPoolCreateFlags(0),VK_QUERY_TYPE_TIMESTAMP,4u,VkQueryPipelineStatisticFlags(0)}};
    NV_VK_REPLAY(VulkanReplay_CreateQueryPool(VkDevice_uid_4,VkQueryPoolCreateInfo_info,nullptr,&VkQueryPool_uid_10));
  }
  {
    static VkBufferCreateInfo VkBufferCreateInfo_info[1] = {VkBufferCreateInfo{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,nullptr,VkBufferCreateFlags(0),64ull,
      VkBufferUsageFlags(19),VK_SHARING_MODE_EXCLUSIVE,0u,nullptr}};
    static VkMemoryRequirements VkMemoryRequirements_info[1] = {VkMemoryRequirements{128ull,64ull,7u}};
    NV_VK_REPLAY(VulkanHelper_CreateBuffer3(VkDevice_uid_4,VkBufferCreateInfo_info,VkMemoryRequirements_info,
      VkDeviceAddress(0),VkDeviceAddress(0),0ull,nullptr,&VkBuffer_uid_7));
    NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_BUFFER,uint64_t(VkBuffer_uid_7),900ull));
  }
  {
    static VkMemoryAllocateInfo VkMemoryAllocateInfo_info[1] = {VkMemoryAllocateInfo{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,nullptr,128ull,3u}};
    static VkBindBufferMemoryInfo VkBindBufferMemoryInfo_info[1] = {VkBindBufferMemoryInfo{
      VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,nullptr,VkBuffer_uid_7,VkDeviceMemory_uid_8,16ull}};
    static VulkanHelper_InitializeDeviceMemoryInfo VulkanHelper_InitializeDeviceMemoryInfo_info[1] = {
      VulkanHelper_InitializeDeviceMemoryInfo{2u,0ull,1u,VkBindBufferMemoryInfo_info,0u,nullptr,0u,nullptr,0u,nullptr,nullptr,VK_FALSE}};
    NV_VK_REPLAY(VulkanHelper_AllocateAndInitializeMemory3(VkDevice_uid_4,VkMemoryAllocateInfo_info,
      VulkanHelper_InitializeDeviceMemoryInfo_info,&VkDeviceMemory_uid_8,nullptr));
  }
}
)cpp";
const std::string initialization = R"cpp(
void SetInitialFrameState00() {
  BEGIN_DATA_SCOPE_FUNCTION();
  NV_MESSAGE_VERBOSE("Initializing resources...(%d/%d)",1,g_ResourceInitFunctionCount);
  NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));
  static VkImageLayout VkImageLayout_initial[1] = {VK_IMAGE_LAYOUT_UNDEFINED};
  {
    BEGIN_DATA_SCOPE();
    NV_VK_REPLAY(VulkanHelper_PopulateBuffer3(VkDevice_uid_4,0u,VkBuffer_uid_7,0ull,64ull,
      NV_GET_RESOURCE_CHECKED(const uint32_t*,5,64ull)));
  }
  NV_VK_REPLAY(VulkanReplay_MapMemory(VkDevice_uid_4,VkDeviceMemory_uid_8,16ull,64ull,0u,&VkDeviceMemory_uid_8_MappedPointer));
  NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4,1u,Serialization::DATABASE_HANDLE(11),88ull,0,nullptr));
  VulkanHelper_InitializeQueryPool(VkDevice_uid_4,VkQueryPool_uid_10);
  FreeCachedMemory();
  NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1));
}
)cpp";
const std::string update = R"cpp(
static VkDescriptorBufferInfo VkDescriptorBufferInfo_update[1] = {VkDescriptorBufferInfo{VkBuffer_uid_7,0ull,64ull}};
NV_VK_REPLAY(VulkanHelper_UpdateMappedMemory2(VkDevice_uid_4,VkDeviceMemory_uid_8,VkDeviceMemory_uid_8_MappedPointer,
  16ull,64ull,NV_GET_RESOURCE(const uint8_t*,23),1ull,VkDescriptorBufferInfo_update));
)cpp";
std::vector<ngm::CppSource> sources() {
    auto result = ngm::check::state::sources<ngm::CppSource>();
    result.push_back({"ResourcesMemory.cpp", memory});
    result.push_back({"FrameSetup00.cpp", initialization});
    result[2].text =
        ngm::check::state::replace(result[2].text, ngm::check::state::submit, update + ngm::check::state::submit);
    return result;
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto input = sources();
        const auto parsed = parse_cpp_replay(input, ReplayParseScope::Values);
        require(parsed.recordings.size() == 1 &&
                    value_recording_draws(parsed, parsed.recordings.begin()->second).size() == 1,
                "Values recording selection preserves existing bounded graphics-state interpretation");
        auto readback = input;
        readback[0].text = ngm::check::state::replace(readback[0].text, "static VkRenderPassBeginInfo", R"cpp(
NV_VK_REPLAY(VulkanReplay_CmdResetQueryPool(cmdBuffer,VkQueryPool_uid_10,0u,4u)); // Event #51
NV_VK_REPLAY(VulkanReplay_CmdWriteTimestamp(cmdBuffer,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,VkQueryPool_uid_10,0u)); // Event #52
static VkRenderPassBeginInfo)cpp");
        const std::string readback_copy = R"cpp(
static VkBufferImageCopy VkBufferImageCopy_readback[1] = {VkBufferImageCopy{0ull,0u,0u,
VkImageSubresourceLayers{VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT),0u,0u,1u},
VkOffset3D{0,0,0},VkExtent3D{4u,4u,1u}}};
NV_VK_REPLAY(VulkanReplay_CmdCopyImageToBuffer(cmdBuffer,VkImage_uid_9,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
VkBuffer_uid_7,1u,VkBufferImageCopy_readback)); // Event #53
)cpp";
        const std::string end_recording = "NV_VK_REPLAY(VulkanReplay_EndCommandBuffer(cmdBuffer));";
        readback[0].text = ngm::check::state::replace(readback[0].text, end_recording, readback_copy + end_recording);
        const auto with_readback = parse_cpp_replay(readback, ReplayParseScope::Values);
        const auto selected = value_recording_draws(with_readback, with_readback.recordings.begin()->second);
        require(selected.size() == 1 && selected[0].at("event_index") == 11 &&
                    selected[0].at("state").at("pipeline").at("value").at("symbol") == "VkPipeline_uid_38",
                "query/readback commands preserve the preceding recorded graphics binding banks only");
        for(const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
                {"VulkanReplay_CmdCopyImageToBuffer", "VulkanReplay_CmdDispatch"},
                {"VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL", "VK_IMAGE_LAYOUT_GENERAL"},
                {"VkBuffer_uid_7,1u,VkBufferImageCopy_readback", "VkBuffer_uid_7,65u,VkBufferImageCopy_readback"},
                {"// Event #53", "// Event #11"}}) {
            auto changed = readback;
            changed[0].text = ngm::check::state::replace(changed[0].text, from, to);
            bool refused = false;
            try {
                const auto candidate = parse_cpp_replay(changed, ReplayParseScope::Values);
                (void)value_recording_draws(candidate, candidate.recordings.begin()->second);
            } catch(const Unqualified&) {
                refused = true;
            }
            require(refused, "unknown work, unqualified copy and ambiguous event annotations refuse selection");
        }
        require(parsed.resource_memory.size() == 3 && parsed.initial_state.size() == 7 && parsed.frame.size() == 3,
                "separate resource definitions, ordered setup and frame operations retained");
        require(parsed.resource_memory[0].name == "VulkanHelper_CreateBuffer3" &&
                    parsed.resource_memory[1].name == "VulkanHelper_RegisterObject" &&
                    parsed.resource_memory[2].name == "VulkanHelper_AllocateAndInitializeMemory3",
                "resource helper effects preserved without executing them");
        require(parsed.initial_state[3].name == "VulkanHelper_UpdateDescriptorSetsFromDatabase" &&
                    parsed.initial_state[3].arguments[2].constructor == "Serialization::DATABASE_HANDLE" &&
                    number(parsed.initial_state[3].arguments[2].arguments[0]) == 11,
                "only the literal database constructor is normalized");
        const auto& upload = parsed.frame[1];
        require(upload.name == "VulkanHelper_UpdateMappedMemory2" && parsed.frame[2].name == "VulkanReplay_QueueSubmit",
                "host upload remains before the recorded submission");
        const auto fields = ngm::detail::fields(upload.arguments[7], upload.visible, "VkDescriptorBufferInfo", 3);
        require(symbol(fields[0], "VkBuffer") == "VkBuffer_uid_7" && number(fields[2]) == 64,
                "typed buffer declaration is visible at the exact upload statement");
        const auto source = upload.source;
        require(source.at("sha256") ==
                    ngm::sha256(std::as_bytes(std::span(input[2].text.data(), input[2].text.size()))),
                "full source hash participates in value-call provenance");
        require(input[2].text.substr(source.at("byte_offset"), source.at("byte_count")).starts_with("NV_VK_REPLAY("),
                "source byte span points at the actual wrapped effect");
        require(parsed.resource_memory[1].arguments[2].text == "900ull" &&
                    parsed.resource_memory[1].arguments[1].arguments[0].text == "VkBuffer_uid_7",
                "typed registration ID stays distinct from the UID suffix");
        const auto bind = ngm::detail::fields(parsed.resource_memory[2].arguments[2], parsed.resource_memory[2].visible,
                                              "VulkanHelper_InitializeDeviceMemoryInfo", 12);
        require(bind[0].text == "2u" && number(bind[2]) == 1,
                "memory-binding helper version and count remain source syntax");
        auto conditional_properties = input;
        conditional_properties[3].text =
            ngm::check::state::replace(conditional_properties[3].text, "  BEGIN_DATA_SCOPE_FUNCTION();", R"cpp(
BEGIN_DATA_SCOPE_FUNCTION();
{
#if HEADER_PROPERTIES
static VkMemoryHeap VkMemoryHeap_properties[1] = {VkMemoryHeap{128ull,VkMemoryHeapFlags(0)}};
static VkPhysicalDeviceVulkan13Features VkPhysicalDeviceVulkan13Features_features[1] = {
VkPhysicalDeviceVulkan13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,nullptr}};
static VkDevicePrivateDataCreateInfo VkDevicePrivateDataCreateInfo_private[1] = {
VkDevicePrivateDataCreateInfo{VK_STRUCTURE_TYPE_DEVICE_PRIVATE_DATA_CREATE_INFO,nullptr,1u}};
static VkPhysicalDeviceBufferDeviceAddressFeatures VkPhysicalDeviceBufferDeviceAddressFeatures_address[1] = {
VkPhysicalDeviceBufferDeviceAddressFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,nullptr,VK_TRUE,VK_TRUE,VK_FALSE}};
static VkMemoryOpaqueCaptureAddressAllocateInfo VkMemoryOpaqueCaptureAddressAllocateInfo_capture[1] = {
VkMemoryOpaqueCaptureAddressAllocateInfo{VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,nullptr,123ull}};
static VkMemoryAllocateFlagsInfo VkMemoryAllocateFlagsInfo_address[1] = {
VkMemoryAllocateFlagsInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,VkMemoryOpaqueCaptureAddressAllocateInfo_capture,
VkMemoryAllocateFlags(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT),0u}};
#endif
}
)cpp");
        require(parse_cpp_replay(conditional_properties, ReplayParseScope::Values).resource_memory.size() == 3,
                "unrelated typed header properties do not imply conditional buffer effects");
        auto device_memory = input;
        device_memory[3].text += R"cpp(
void CreateResourcesDeviceIdentity() {
 {
 NV_VK_REPLAY(VulkanHelper_GetCompatiblePhysicalDevice(VkInstance_uid_1,0u,VkPhysicalDeviceProperties{0u},
 1u,NV_TO_ARRAY(VkQueueFamilyProperties{VkQueueFlags(VK_QUEUE_GRAPHICS_BIT),1u,64u,VkExtent3D{1u,1u,1u}}),
 VkPhysicalDeviceMemoryProperties{1u,NV_TO_ARRAY(VkMemoryType{VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),0u}),
 1u,NV_TO_ARRAY(VkMemoryHeap{4096ull,VkMemoryHeapFlags(0)})},&VkPhysicalDevice_uid_2));
 }
 { NV_VK_REPLAY(VulkanReplay_CreateDevice(VkPhysicalDevice_uid_2,VkDeviceCreateInfo_info,nullptr,&VkDevice_uid_4)); }
}
)cpp";
        const auto metadata = parse_cpp_replay(device_memory, ReplayParseScope::Values).resource_memory;
        require(metadata.size() == 5 && metadata[3].name == "VulkanHelper_GetCompatiblePhysicalDevice" &&
                    metadata[3].arguments[2].constructor == "captured_physical_properties" &&
                    metadata[3].arguments[5].constructor == "VkPhysicalDeviceMemoryProperties" &&
                    metadata[3].arguments[5].arguments[1].arguments[0].constructor == "VkMemoryType" &&
                    metadata[4].name == "VulkanReplay_CreateDevice",
                "captured memory-type flags and physical/device link survive without evaluating unrelated limits");
        auto with_queue = device_memory;
        with_queue[3].text += R"cpp(
void CreateResourcesQueueIdentity() {
 { NV_VK_REPLAY(VulkanReplay_GetDeviceQueue(VkDevice_uid_4,0u,1u,&VkQueue_uid_30)); }
}
)cpp";
        const auto queued = parse_cpp_replay(with_queue, ReplayParseScope::Values);
        require(queued.resource_memory.size() == 5 && queued.resource_queues.size() == 1 &&
                    queued.resource_queues[0].arguments.size() == 4 &&
                    queued.resource_queues[0].arguments[0].text == "VkDevice_uid_4" &&
                    queued.resource_queues[0].arguments[2].text == "1u" &&
                    queued.resource_queues[0].arguments[3].text == "& VkQueue_uid_30",
                "separate queue/device source identity does not infer queue ownership from numeric suffixes");

        auto copied = input;
        copied[2].text = ngm::check::state::replace(copied[2].text, update, R"cpp(
static VkCopyDescriptorSet VkCopyDescriptorSet_copy[1] = {VkCopyDescriptorSet{
VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,nullptr,VkDescriptorSet_uid_100,0u,0u,VkDescriptorSet_uid_101,0u,1u,1u}};
NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4,1u,Serialization::DATABASE_HANDLE(13),88ull,1u,VkCopyDescriptorSet_copy));
)cpp");
        const auto copy = parse_cpp_replay(copied, ReplayParseScope::Values);
        require(copy.frame[1].arguments[4].text == "1u" &&
                    ngm::detail::fields(copy.frame[1].arguments[5], copy.frame[1].visible, "VkCopyDescriptorSet", 9)[7]
                            .text == "1u",
                "literal copy fields remain available for a separate lifetime interpreter");
        for(const auto scope : {ReplayParseScope::RecordedState, ReplayParseScope::Timing}) {
            bool refused = false;
            try {
                (void)parse_cpp_replay(copied, scope);
            } catch(const ngm::InspectionError&) {
                refused = true;
            }
            require(refused, "existing parser policies do not inherit the value-only database/copy grammar");
        }
        const auto negative = [&](std::size_t source_index, const std::string& from, const std::string& to) {
            auto altered = input;
            altered[source_index].text = ngm::check::state::replace(altered[source_index].text, from, to);
            bool refused = false;
            try {
                (void)parse_cpp_replay(altered, ReplayParseScope::Values);
            } catch(const ngm::InspectionError&) {
                refused = true;
            }
            require(refused, "unknown conditional, control, helper, database or memory side effect refuses");
        };
        negative(4, "DATABASE_HANDLE(11)", "DATABASE_HANDLE(2147483648)");
        negative(4, "DATABASE_HANDLE(11)", "DATABASE_HANDLE(-2)");
        negative(4, "g_ResourceInitFunctionCount", "mutate()");
        negative(4, "DATABASE_HANDLE(11)", "DATABASE_HANDLE(handle)");
        negative(4, "Serialization::DATABASE_HANDLE", "Other::DATABASE_HANDLE");
        negative(4, "  FreeCachedMemory();", "  mutate();");
        negative(4, "  FreeCachedMemory();", "  if(flag) FreeCachedMemory();");
        negative(4, "  FreeCachedMemory();", "  #if UNKNOWN\n  FreeCachedMemory();\n#endif");
        negative(3, "  BEGIN_DATA_SCOPE_FUNCTION();", "  BEGIN_DATA_SCOPE_FUNCTION();\nmutate();");
        negative(3, "VulkanHelper_CreateBuffer3", "VulkanHelper_UnknownBufferEffect");
        negative(3, "900ull", "900ull) + mutate(");
        auto duplicate = input;
        duplicate.push_back({"other/FrameSetup00.cpp", initialization});
        bool refused = false;
        try {
            (void)parse_cpp_replay(duplicate, ReplayParseScope::Values);
        } catch(const ngm::InspectionError&) {
            refused = true;
        }
        require(refused, "multiple setup roots cannot imply an execution order");
        auto conditional_memory = input;
        conditional_memory[3].text = "#if BUFFER_MAY_EXIST\n" + conditional_memory[3].text + "#endif\n";
        refused = false;
        try {
            (void)parse_cpp_replay(conditional_memory, ReplayParseScope::Values);
        } catch(const ngm::InspectionError&) {
            refused = true;
        }
        require(refused, "conditional memory definitions remain unqualified");
    });
}
