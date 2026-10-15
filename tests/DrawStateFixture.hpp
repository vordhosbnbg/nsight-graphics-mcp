#pragma once

#include "Check.hpp"
#include "CppEvidenceFixture.hpp"
#include <vector>

// Hand-written bounded source grammar, never hardware/producer qualification.
namespace ngm::check::state {
inline std::string replace(std::string text, const std::string& from, const std::string& to) {
    const auto found = text.find(from);
    require(found != std::string::npos, "synthetic state anchor");
    text.replace(found, from.size(), to);
    return text;
}
inline const std::string state_declarations = R"cpp(
static VkViewport VkViewport_values[1] = { VkViewport{0.0f, 0.0f, 192.0f, 128.0f, 0.0f, 1.0f} };
static VkRect2D VkRect2D_values[1] = { VkRect2D{VkOffset2D{0,0}, VkExtent2D{192u,128u}} };
static VkPipelineViewportStateCreateInfo VkPipelineViewportStateCreateInfo_view[1] = {
VkPipelineViewportStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr,
VkPipelineViewportStateCreateFlags(0), 1u, VkViewport_values, 1u, VkRect2D_values}};
static VkPipelineRasterizationStateCreateInfo VkPipelineRasterizationStateCreateInfo_raster[1] = {
VkPipelineRasterizationStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr,
VkPipelineRasterizationStateCreateFlags(0), 0u, 0u, VK_POLYGON_MODE_FILL, VkCullModeFlags(VK_CULL_MODE_BACK_BIT),
VK_FRONT_FACE_CLOCKWISE, 0u, 0.0f, 0.0f, 0.0f, 1.0f}};
static VkPipelineDepthStencilStateCreateInfo VkPipelineDepthStencilStateCreateInfo_depth[1] = {
VkPipelineDepthStencilStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr,
VkPipelineDepthStencilStateCreateFlags(0), 1u, 1u, VK_COMPARE_OP_GREATER, 0u, 0u,
VkStencilOpState{VK_STENCIL_OP_KEEP,VK_STENCIL_OP_KEEP,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_NEVER,0u,0u,0u},
VkStencilOpState{VK_STENCIL_OP_KEEP,VK_STENCIL_OP_KEEP,VK_STENCIL_OP_KEEP,VK_COMPARE_OP_NEVER,0u,0u,0u},0.0f,1.0f}};
)cpp";
inline const std::string setup = R"cpp(
void CreateResources01() {
 {
 static VkAttachmentDescription VkAttachmentDescription_values[1] = {
 VkAttachmentDescription{VkAttachmentDescriptionFlags(0), VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
 VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
 static VkAttachmentReference VkAttachmentReference_depth[1] = {VkAttachmentReference{0u,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
 static VkSubpassDescription VkSubpassDescription_values[1] = {
 VkSubpassDescription{VkSubpassDescriptionFlags(0),VK_PIPELINE_BIND_POINT_GRAPHICS,0u,nullptr,0u,nullptr,nullptr,VkAttachmentReference_depth,0u,nullptr}};
 static VkRenderPassCreateInfo VkRenderPassCreateInfo_info[1] = {VkRenderPassCreateInfo{
 VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,nullptr,VkRenderPassCreateFlags(0),1u,VkAttachmentDescription_values,
 1u,VkSubpassDescription_values,0u,nullptr}};
 NV_VK_REPLAY(VulkanReplay_CreateRenderPass(VkDevice_uid_4,VkRenderPassCreateInfo_info,nullptr,&VkRenderPass_uid_19));
 }
 {
 static VkCommandBufferAllocateInfo VkCommandBufferAllocateInfo_info[1] = {VkCommandBufferAllocateInfo{
 VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,nullptr,VkCommandPool_uid_41,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1u}};
 NV_VK_REPLAY(VulkanReplay_AllocateCommandBuffers(VkDevice_uid_4,VkCommandBufferAllocateInfo_info,NV_TO_OUTPUT_ARRAY(&VkCommandBuffer_uid_42)));
 }
}
)cpp";
inline std::string objects(bool dynamic = false) {
    auto result = cpp::resources;
    auto declarations = state_declarations;
    if(dynamic)
        declarations += R"cpp(
static VkDynamicState VkDynamicState_values[7] = {VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,VK_DYNAMIC_STATE_CULL_MODE,VK_DYNAMIC_STATE_FRONT_FACE,VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
static VkPipelineDynamicStateCreateInfo VkPipelineDynamicStateCreateInfo_dynamic[1] = {VkPipelineDynamicStateCreateInfo{
VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,nullptr,VkPipelineDynamicStateCreateFlags(0),7u,VkDynamicState_values}};
)cpp";
    result =
        replace(result, "static VkGraphicsPipelineCreateInfo", declarations + "\nstatic VkGraphicsPipelineCreateInfo");
    result = replace(result,
                     "1u, VkPipelineShaderStageCreateInfo_stages, nullptr, nullptr, nullptr, nullptr, nullptr, "
                     "nullptr,\n            nullptr, nullptr, nullptr,",
                     "1u, VkPipelineShaderStageCreateInfo_stages, nullptr, nullptr, nullptr, "
                     "VkPipelineViewportStateCreateInfo_view, VkPipelineRasterizationStateCreateInfo_raster, "
                     "nullptr,\n            VkPipelineDepthStencilStateCreateInfo_depth, nullptr, " +
                         std::string(dynamic ? "VkPipelineDynamicStateCreateInfo_dynamic," : "nullptr,"));
    return result + setup;
}
inline const std::string begin = R"cpp(
static VkCommandBufferBeginInfo VkCommandBufferBeginInfo_info[1] = {VkCommandBufferBeginInfo{
VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,nullptr,VkCommandBufferUsageFlags(0),nullptr}};
NV_VK_REPLAY(VulkanReplay_BeginCommandBuffer(cmdBuffer,VkCommandBufferBeginInfo_info)); // Event #1
static VkRenderPassBeginInfo VkRenderPassBeginInfo_info[1] = {VkRenderPassBeginInfo{
VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,nullptr,VkRenderPass_uid_19,VkFramebuffer_uid_21,
VkRect2D{VkOffset2D{0,0},VkExtent2D{192u,128u}},0u,nullptr}};
NV_VK_REPLAY(VulkanReplay_CmdBeginRenderPass(cmdBuffer,VkRenderPassBeginInfo_info,VK_SUBPASS_CONTENTS_INLINE)); // Event #2
)cpp";
inline const std::string bind = "NV_VK_REPLAY(VulkanReplay_CmdBindPipeline(cmdBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,"
                                "VkPipeline_uid_38)); // Event #3\n";
inline const std::string setters = R"cpp(
NV_VK_REPLAY(VulkanReplay_CmdSetDepthTestEnable(cmdBuffer,0u)); // Event #4
NV_VK_REPLAY(VulkanReplay_CmdSetDepthWriteEnable(cmdBuffer,0u)); // Event #5
NV_VK_REPLAY(VulkanReplay_CmdSetDepthCompareOp(cmdBuffer,VK_COMPARE_OP_LESS)); // Event #6
NV_VK_REPLAY(VulkanReplay_CmdSetCullMode(cmdBuffer,VkCullModeFlags(0))); // Event #7
NV_VK_REPLAY(VulkanReplay_CmdSetFrontFace(cmdBuffer,VK_FRONT_FACE_COUNTER_CLOCKWISE)); // Event #8
static VkViewport VkViewport_dynamic[1] = {VkViewport{4.0f,6.0f,16.0f,16.0f,0.25f,0.75f}};
static VkRect2D VkRect2D_dynamic[1] = {VkRect2D{VkOffset2D{2,3},VkExtent2D{8u,8u}}};
NV_VK_REPLAY(VulkanReplay_CmdSetViewport(cmdBuffer,0u,1u,VkViewport_dynamic)); // Event #9
NV_VK_REPLAY(VulkanReplay_CmdSetScissor(cmdBuffer,0u,1u,VkRect2D_dynamic)); // Event #10
)cpp";
inline const std::string draw = "NV_VK_REPLAY(VulkanReplay_CmdDraw(cmdBuffer,3u,1u,0u,0u)); // Event #11\n";
inline const std::string end = R"cpp(
NV_VK_REPLAY(VulkanReplay_CmdEndRenderPass(cmdBuffer)); // Event #12
NV_VK_REPLAY(VulkanReplay_EndCommandBuffer(cmdBuffer)); // Event #13
)cpp";
inline const std::string submit = R"cpp(
{ BEGIN_DATA_SCOPE();
static VkSubmitInfo VkSubmitInfo_info[1] = {VkSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO,nullptr,0u,nullptr,nullptr,
1u,NV_TO_ARRAY(VkCommandBuffer_uid_42),0u,nullptr}};
NV_VK_REPLAY(VulkanReplay_QueueSubmit(VkQueue_uid_5,1u,VkSubmitInfo_info,VkFence_uid_43)); }
)cpp";
inline std::string frame(const std::string& submissions = submit) {
    return "void RunFrame0Part00(uint64_t frameIndex) { BEGIN_DATA_SCOPE_FUNCTION();\n"
           "VulkanHelper_RecordCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_42,1u,&Record_VkCommandBuffer_uid_42_"
           "r1);\n" +
           submissions + "}\n";
}
template <typename Source>
inline std::vector<Source> sources(bool dynamic = false, const std::string& statements = {},
                                   const std::string& execution = {}) {
    return {{"CommandList00.cpp",
             cpp::recording(statements.empty() ? begin + bind + (dynamic ? setters : "") + draw + end : statements)},
            {"Resources00.cpp", objects(dynamic)},
            {"Frame0Part00.cpp", execution.empty() ? frame() : execution}};
}
} // namespace ngm::check::state
