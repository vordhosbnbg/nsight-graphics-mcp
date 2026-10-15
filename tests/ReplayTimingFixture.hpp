#pragma once

#include "DrawStateFixture.hpp"

// Test-owned source/table grammar. These CPU values never qualify GPU timings.
namespace ngm::check::timing {
inline std::string marker(const std::string& name, unsigned event) {
    const auto symbol = "VkDebugUtilsLabelEXT_marker_" + std::to_string(event);
    return "static VkDebugUtilsLabelEXT " + symbol +
           "[1] = {VkDebugUtilsLabelEXT{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,nullptr,\"" + name +
           "\",NV_TO_ARRAY(0.0f,0.0f,0.0f,0.0f)}};\n"
           "NV_VK_REPLAY(VulkanReplay_CmdBeginDebugUtilsLabelEXT(cmdBuffer," +
           symbol + ")); // Event #" + std::to_string(event) + "\n";
}
inline std::string close(unsigned event) {
    return "NV_VK_REPLAY(VulkanReplay_CmdEndDebugUtilsLabelEXT(cmdBuffer)); // Event #" + std::to_string(event) + "\n";
}
inline std::string queue_name(std::size_t interval, char occurrence) {
    return "interval." + std::to_string(interval) + '.' + occurrence;
}
inline std::string queue(const std::string& name) {
    return "{ BEGIN_DATA_SCOPE();\n"
           "static VkDebugUtilsLabelEXT VkDebugUtilsLabelEXT_queue[1] = {VkDebugUtilsLabelEXT{"
           "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,nullptr,\"" +
           name +
           "\",NV_TO_ARRAY(0.0f,0.0f,0.0f,0.0f)}};\n"
           "NV_VK_REPLAY(VulkanReplay_QueueBeginDebugUtilsLabelEXT(VkQueue_uid_5,VkDebugUtilsLabelEXT_queue));\n" +
           state::submit + "NV_VK_REPLAY(VulkanReplay_QueueEndDebugUtilsLabelEXT(VkQueue_uid_5)); }\n";
}
template <typename Source>
inline std::vector<Source> sources(std::size_t interval) {
    auto result = state::sources<Source>(
        false,
        state::begin + state::bind + marker("leaf.direct", 20) + state::draw + close(21) + marker("leaf.indexed", 22) +
            "NV_VK_REPLAY(VulkanReplay_CmdDrawIndexed(cmdBuffer,6u,2u,3u,- 2,4u)); // Event #25\n" + close(26) +
            state::end,
        state::frame(queue(queue_name(interval, 'a')) + queue(queue_name(interval, 'b'))));
    result.push_back({"FrameSetup00.cpp",
                      "void SetInitialFrameState00() { BEGIN_DATA_SCOPE_FUNCTION();\n"
                      "NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));\n"
                      "NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1));\nFreeCachedMemory(); }\n"});
    return result;
}
inline std::string event_table(std::size_t interval) {
    return "event_text\ttime_ms\n" + queue_name(interval, 'a') +
           "\t9\n        leaf.direct\t1.25\n        leaf.indexed\t2.5\n" + queue_name(interval, 'b') +
           "\t8\n        leaf.direct\t3.75\n        leaf.indexed\t4.5\n";
}
inline std::string regime_table(std::size_t interval) {
    const auto a = queue_name(interval, 'a'), b = queue_name(interval, 'b');
    return "flattened_event_name\tsynthetic.metric\n" + b + "/leaf.indexed\t0\n" + a + "\t0\n" + b + "\t0\n" + a +
           "/leaf.indexed\t0\n" + a + "/leaf.direct\t0\n" + b + "/leaf.direct\t0\n";
}
} // namespace ngm::check::timing
