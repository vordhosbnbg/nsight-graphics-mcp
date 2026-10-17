#pragma once

#include "DrawStateFixture.hpp"

// Hand-authored source syntax: no proprietary exporter implementation or data.
namespace ngm::check::vulkan13 {
inline const std::string rendering_pipeline = R"cpp(
static VkPipelineRenderingCreateInfo VkPipelineRenderingCreateInfo_rendering[1] = {VkPipelineRenderingCreateInfo{
VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,nullptr,0u,0u,nullptr,VK_FORMAT_D32_SFLOAT,VK_FORMAT_UNDEFINED}};
)cpp";
inline const std::string unrelated_setup = R"cpp(
{ static VkMemoryDedicatedAllocateInfo VkMemoryDedicatedAllocateInfo_memory[1] = {VkMemoryDedicatedAllocateInfo{
VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,nullptr,VkImage_uid_70,VkBuffer(VK_NULL_HANDLE)}}; }
{ static VkPhysicalDeviceFeatures VkPhysicalDeviceFeatures_features[1] = {VkPhysicalDeviceFeatures{0u}}; }
{ static VkWaylandSurfaceCreateInfoKHR VkWaylandSurfaceCreateInfoKHR_surface[1] = {VkWaylandSurfaceCreateInfoKHR{
VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,nullptr,VkWaylandSurfaceCreateFlagsKHR(0),
(*reinterpret_cast<wl_display**>(WindowSystemInstance().GetDisplayAddr())),
(*reinterpret_cast<wl_surface**>(WindowSystemInstance().GetWindowAddr()))}};
NV_VK_REPLAY(VulkanReplay_CreateWaylandSurfaceKHR(VkInstance_uid_1,VkWaylandSurfaceCreateInfoKHR_surface,nullptr,&VkSurfaceKHR_uid_3)); }
)cpp";
inline const std::string view = R"cpp(
void CreateResources02() { {
static VkImageViewCreateInfo VkImageViewCreateInfo_view[1] = {VkImageViewCreateInfo{
VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,nullptr,VkImageViewCreateFlags(0),VkImage_uid_70,VK_IMAGE_VIEW_TYPE_2D,
VK_FORMAT_D32_SFLOAT,VkComponentMapping{VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,
VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY},
VkImageSubresourceRange{VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT),0u,1u,0u,1u}}};
NV_VK_REPLAY(VulkanHelper_CreateImageView(VkDevice_uid_4,VkImageViewCreateInfo_view,0ull,nullptr,&VkImageView_uid_71));
NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_IMAGE_VIEW,uint64_t(VkImageView_uid_71),71ull));
} }
)cpp";
inline const std::string render = R"cpp(
static VkRenderingAttachmentInfo VkRenderingAttachmentInfo_depth[1] = {VkRenderingAttachmentInfo{
VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,nullptr,VkImageView_uid_71,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
VK_RESOLVE_MODE_NONE,VkImageView(VK_NULL_HANDLE),VK_IMAGE_LAYOUT_UNDEFINED,VK_ATTACHMENT_LOAD_OP_CLEAR,
VK_ATTACHMENT_STORE_OP_STORE,NV_TO_UNION(VkClearValue,0x00)}};
static VkRenderingInfo VkRenderingInfo_render[1] = {VkRenderingInfo{
VK_STRUCTURE_TYPE_RENDERING_INFO,nullptr,VkRenderingFlags(0),VkRect2D{VkOffset2D{0,0},VkExtent2D{192u,128u}},
1u,0u,0u,nullptr,VkRenderingAttachmentInfo_depth,nullptr}};
NV_VK_REPLAY(VulkanReplay_CmdBeginRendering(cmdBuffer,VkRenderingInfo_render)); // Event #2
)cpp";
inline const std::string submit = R"cpp(
{ static VkSemaphoreSubmitInfo VkSemaphoreSubmitInfo_wait[1] = {VkSemaphoreSubmitInfo{
VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,nullptr,VkSemaphore_uid_45,0ull,VkPipelineStageFlags2(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT),0u}};
static VkCommandBufferSubmitInfo VkCommandBufferSubmitInfo_buffer[1] = {VkCommandBufferSubmitInfo{
VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,nullptr,VkCommandBuffer_uid_42,0u}};
static VkSubmitInfo2 VkSubmitInfo2_submit[1] = {VkSubmitInfo2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2,nullptr,VkSubmitFlags(0),
1u,VkSemaphoreSubmitInfo_wait,1u,VkCommandBufferSubmitInfo_buffer,0u,nullptr}};
NV_VK_REPLAY(VulkanReplay_QueueSubmit2(VkQueue_uid_5,1u,VkSubmitInfo2_submit,VkFence_uid_43)); }
)cpp";
inline const std::string host = R"cpp(
{ NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4,1u,Serialization::DATABASE_HANDLE(0),88ull,0u,nullptr)); }
{ std::vector<uint8_t> std_vector_of_uint8_t_temp_1(128);
NV_VK_REPLAY(VulkanReplay_GetQueryPoolResults(VkDevice_uid_4,VkQueryPool_uid_80,8u,6u,128ull,
std_vector_of_uint8_t_temp_1.data(),16ull,VkQueryResultFlags(VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT))); }
)cpp";
inline const std::string commands = R"cpp(
static VkImageMemoryBarrier2 VkImageMemoryBarrier2_image[1] = {VkImageMemoryBarrier2{
VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,nullptr,VkPipelineStageFlags2(0),VkAccessFlags2(0),
VkPipelineStageFlags2(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT),VkAccessFlags2(0),VK_IMAGE_LAYOUT_UNDEFINED,
VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,4294967295u,4294967295u,VkImage_uid_70,
VkImageSubresourceRange{VkImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT),0u,1u,0u,1u}}};
static VkDependencyInfo VkDependencyInfo_barrier[1] = {VkDependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO,nullptr,
VkDependencyFlags(0),0u,nullptr,0u,nullptr,1u,VkImageMemoryBarrier2_image}};
NV_VK_REPLAY(VulkanReplay_CmdPipelineBarrier2(cmdBuffer,VkDependencyInfo_barrier)); // Event #20
NV_VK_REPLAY(VulkanReplay_CmdResetQueryPool(cmdBuffer,VkQueryPool_uid_80,0u,8u)); // Event #21
NV_VK_REPLAY(VulkanReplay_CmdWriteTimestamp2(cmdBuffer,VkPipelineStageFlags2(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT),VkQueryPool_uid_80,0u)); // Event #22
static uint64_t uint64_t_offsets[1] = {0ull};
NV_VK_REPLAY(VulkanReplay_CmdBindVertexBuffers(cmdBuffer,0u,1u,NV_TO_STACK_PTR(VkBuffer_uid_73),uint64_t_offsets)); // Event #23
NV_VK_REPLAY(VulkanReplay_CmdBindIndexBuffer(cmdBuffer,VkBuffer_uid_74,0ull,VK_INDEX_TYPE_UINT32)); // Event #24
)cpp";
template <class Source>
inline auto sources() {
    using state::replace;
    auto input = state::sources<Source>();
    auto begin = state::begin.substr(0, state::begin.find("static VkRenderPassBeginInfo"));
    auto draw = replace(state::draw, "CmdDraw(cmdBuffer,3u,1u,0u,0u)", "CmdDrawIndexed(cmdBuffer,3u,1u,0u,0,0u)");
    auto end = replace(state::end, "CmdEndRenderPass", "CmdEndRendering");
    input[0].text = cpp::recording(begin + commands + render + state::bind + draw + end);
    input[1].text = replace(input[1].text, "static VkGraphicsPipelineCreateInfo",
                            rendering_pipeline + "static VkGraphicsPipelineCreateInfo");
    input[1].text =
        replace(input[1].text, "VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr,",
                "VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, VkPipelineRenderingCreateInfo_rendering,");
    input[1].text = replace(input[1].text, "VkRenderPass_uid_19, 0u", "VkRenderPass(VK_NULL_HANDLE), 0u");
    input[1].text =
        replace(input[1].text, "void CreateResources00() {", "void CreateResources00() {" + unrelated_setup);
    const std::string specialization = R"cpp(
static VkSpecializationMapEntry VkSpecializationMapEntry_map[1] = {VkSpecializationMapEntry{0u,0u,4ull}};
static VkSpecializationInfo VkSpecializationInfo_info[1] = {VkSpecializationInfo{1u,VkSpecializationMapEntry_map,
4ull,NV_GET_RESOURCE_CHECKED(const void*,77,4ull)}};
)cpp";
    input[1].text = replace(input[1].text, "static VkPipelineShaderStageCreateInfo",
                            specialization + "static VkPipelineShaderStageCreateInfo");
    input[1].text = replace(input[1].text, "VkShaderModule_uid_36, \"main\", nullptr",
                            "VkShaderModule_uid_36, \"main\", VkSpecializationInfo_info");
    input[1].text += view;
    input[2].text = state::frame(host + submit);
    return input;
}
} // namespace ngm::check::vulkan13
