#pragma once
#include "DrawStateFixture.hpp"

namespace ngm::check::descriptor {
inline std::string setup() {
    std::string result = "void CreateResourcesDescriptors() {\n";
    for(const auto id : {90, 91, 92}) {
        const auto n = std::to_string(id);
        result +=
            "{ static VkDescriptorSetLayoutBinding VkDescriptorSetLayoutBinding_b" + n +
            "[2] = {VkDescriptorSetLayoutBinding{9u,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,1u,"
            "VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),nullptr},"
            "VkDescriptorSetLayoutBinding{2u,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,2u,VkShaderStageFlags(" +
            std::string(id == 92 ? "VK_SHADER_STAGE_VERTEX_BIT" : "VK_SHADER_STAGE_FRAGMENT_BIT") +
            "),nullptr}};\n"
            "static VkDescriptorSetLayoutCreateInfo VkDescriptorSetLayoutCreateInfo_l" +
            n +
            "[1] = {VkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,nullptr,"
            "VkDescriptorSetLayoutCreateFlags(0),2u,VkDescriptorSetLayoutBinding_b" +
            n +
            "}};\n"
            "NV_VK_REPLAY(VulkanReplay_CreateDescriptorSetLayout(VkDevice_uid_4,VkDescriptorSetLayoutCreateInfo_l" +
            n + ",nullptr,&VkDescriptorSetLayout_uid_" + n + ")); }\n";
    }
    for(const auto id : {37, 48, 49, 50, 51}) {
        const auto n = std::to_string(id);
        result +=
            "{ static VkDescriptorSetLayout VkDescriptorSetLayout_s" + n + "[3] = {VkDescriptorSetLayout_uid_" +
            std::string(id == 49   ? "92"
                        : id == 48 ? "91"
                                   : "90") +
            ",VkDescriptorSetLayout_uid_" +
            std::string(id == 50   ? "92"
                        : id == 48 ? "91"
                                   : "90") +
            ",VkDescriptorSetLayout_uid_90};\n"
            "static VkPipelineLayoutCreateInfo VkPipelineLayoutCreateInfo_l" +
            n +
            "[1] = {VkPipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,nullptr,"
            "VkPipelineLayoutCreateFlags(0),3u,VkDescriptorSetLayout_s" +
            n +
            (id == 51
                 ? ",1u,NV_TO_STACK_PTR(VkPushConstantRange{VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),0u,4u})"
                 : ",0u,nullptr") +
            "}};\n"
            "NV_VK_REPLAY(VulkanReplay_CreatePipelineLayout(VkDevice_uid_4,VkPipelineLayoutCreateInfo_l" +
            n + ",nullptr,&VkPipelineLayout_uid_" + n + ")); }\n";
    }
    for(const auto id : {100, 101, 102, 103, 104}) {
        const auto n = std::to_string(id), layout = std::to_string(id == 103 ? 91 : id == 104 ? 92 : 90);
        result +=
            "{ static VkDescriptorSetAllocateInfo VkDescriptorSetAllocateInfo_a" + n +
            "[1] = {VkDescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,nullptr,"
            "VkDescriptorPool_uid_89,1u,NV_TO_STACK_PTR(VkDescriptorSetLayout_uid_" +
            layout +
            ")}};\n"
            "NV_VK_REPLAY(VulkanReplay_AllocateDescriptorSets(VkDevice_uid_4,VkDescriptorSetAllocateInfo_a" +
            n + ",NV_TO_OUTPUT_ARRAY(&VkDescriptorSet_uid_" + n +
            ")));\n"
            "NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_DESCRIPTOR_SET,uint64_t(VkDescriptorSet_uid_" +
            n + ")," + n + "ull)); }\n";
    }
    return result + "}\n";
}
inline std::string bind(unsigned event, unsigned layout, unsigned first, const std::vector<unsigned>& sets,
                        const std::vector<unsigned>& offsets) {
    const auto join = [](const auto& values, const std::string& prefix, const std::string& suffix) {
        std::string result;
        for(const auto value : values)
            result += (result.empty() ? "" : ",") + prefix + std::to_string(value) + suffix;
        return result;
    };
    return "NV_VK_REPLAY(VulkanReplay_CmdBindDescriptorSets(cmdBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,"
           "VkPipelineLayout_uid_" +
           std::to_string(layout) + "," + std::to_string(first) + "u," + std::to_string(sets.size()) +
           "u,NV_TO_ARRAY(" + join(sets, "VkDescriptorSet_uid_", "") + ")," + std::to_string(offsets.size()) + "u," +
           (offsets.empty() ? "nullptr" : "NV_TO_ARRAY(" + join(offsets, "", "u") + ")") + ")); // Event #" +
           std::to_string(event) + "\n";
}
inline const std::string initial = bind(4, 37, 0, {100, 101, 102}, {20, 21, 90, 120, 121, 190, 220, 221, 290});
template <typename Source>
std::vector<Source> sources(const std::string& statements = {}) {
    auto result = state::sources<Source>(
        false, statements.empty() ? state::begin + state::bind + initial + state::draw + state::end : statements);
    result[1].text += setup();
    return result;
}
} // namespace ngm::check::descriptor
