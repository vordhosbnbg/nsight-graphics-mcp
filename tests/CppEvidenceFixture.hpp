#pragma once
#include <string>

// Synthetic generated-source grammar; never evidence of Nsight compatibility.
namespace ngm::check::cpp {
inline const std::string resources = R"cpp(
#include "Resources.h"
void CreateResources00() {
    {
        BEGIN_DATA_SCOPE();
        static VkShaderModuleCreateInfo VkShaderModuleCreateInfo_temp_1[1] = { VkShaderModuleCreateInfo{
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, VkShaderModuleCreateFlags(0),
            128ull, NV_GET_RESOURCE_CHECKED(const uint32_t*, 14, 128ull)} };
        NV_VK_REPLAY(VulkanReplay_CreateShaderModule(VkDevice_uid_4, VkShaderModuleCreateInfo_temp_1,
            nullptr, &VkShaderModule_uid_36));
    }
    {
        static VkPipelineShaderStageCreateInfo VkPipelineShaderStageCreateInfo_stages[1] = {
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr,
            VkPipelineShaderStageCreateFlags(0), VK_SHADER_STAGE_FRAGMENT_BIT, VkShaderModule_uid_36, "main", nullptr}};
        static VkGraphicsPipelineCreateInfo VkGraphicsPipelineCreateInfo_temp_1[1] = { VkGraphicsPipelineCreateInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, VkPipelineCreateFlags(0),
            1u, VkPipelineShaderStageCreateInfo_stages, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, VkPipelineLayout_uid_37, VkRenderPass_uid_19, 0u, VkPipeline(VK_NULL_HANDLE), 0} };
        NV_VK_REPLAY(VulkanReplay_CreateGraphicsPipelines(VkDevice_uid_4, VkPipelineCache(VK_NULL_HANDLE), 1u,
            VkGraphicsPipelineCreateInfo_temp_1, nullptr, NV_TO_OUTPUT_ARRAY(&VkPipeline_uid_38)));
    }
}
)cpp";
inline const std::string begin = R"cpp(
NV_VK_REPLAY(VulkanReplay_BeginCommandBuffer(cmdBuffer, nullptr)); // Event #12
NV_VK_REPLAY(VulkanReplay_CmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VkPipeline_uid_38)); // Event #16
)cpp";
inline const std::string draw = "NV_VK_REPLAY(VulkanReplay_CmdDraw(cmdBuffer, 3u, 1u, 0u, 0u)); // Event #18\n";
inline const std::string end = "NV_VK_REPLAY(VulkanReplay_EndCommandBuffer(cmdBuffer)); // Event #23\n";
inline std::string recording(const std::string& statements) {
    return "void Record_VkCommandBuffer_uid_42_r1_Part00(VkCommandBuffer cmdBuffer) {\n"
           "BEGIN_DATA_SCOPE_FUNCTION();\n" +
           statements +
           "}\n"
           "void Record_VkCommandBuffer_uid_42_r1(VkCommandBuffer cmdBuffer) {\n"
           "BEGIN_DATA_SCOPE_FUNCTION();\nRecord_VkCommandBuffer_uid_42_r1_Part00(cmdBuffer);\n}\n";
}
} // namespace ngm::check::cpp
