#pragma once
#include "DrawStateFixture.hpp"

namespace ngm::check::push {
inline std::string layouts() {
    std::string result = "void CreateResourcesLayouts() {\n";
    for(const auto id : {37, 48, 49}) {
        const auto n = std::to_string(id);
        const std::string bytes = id == 49 ? "32u" : "16u";
        result += "{ static VkPushConstantRange VkPushConstantRange_p" + n +
                  "[1] = {VkPushConstantRange{VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),0u," + bytes +
                  "}};\n"
                  "static VkPipelineLayoutCreateInfo VkPipelineLayoutCreateInfo_p" +
                  n +
                  "[1] = "
                  "{VkPipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,nullptr,"
                  "VkPipelineLayoutCreateFlags(0),0u,nullptr,1u,VkPushConstantRange_p" +
                  n +
                  "}};\n"
                  "NV_VK_REPLAY(VulkanReplay_CreatePipelineLayout(VkDevice_uid_4,VkPipelineLayoutCreateInfo_p" +
                  n + ",nullptr,&VkPipelineLayout_uid_" + n + ")); }\n";
    }
    return result + "}\n";
}
inline std::string write(unsigned event, unsigned layout, unsigned offset, unsigned size, unsigned resource) {
    return "NV_VK_REPLAY(VulkanReplay_CmdPushConstants(cmdBuffer,VkPipelineLayout_uid_" + std::to_string(layout) +
           ",VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT)," + std::to_string(offset) + "u," + std::to_string(size) +
           "u,NV_GET_RESOURCE_STATIC(void*," + std::to_string(resource) + "," + std::to_string(size) +
           "ull))); // Event #" + std::to_string(event) + "\n";
}
template <typename Source>
std::vector<Source> sources(const std::string& statements = {}) {
    auto result = state::sources<Source>(
        false, statements.empty() ? state::begin + state::bind + write(4, 37, 0, 16, 18) + state::draw + state::end
                                  : statements);
    result[1].text += layouts();
    return result;
}
} // namespace ngm::check::push
