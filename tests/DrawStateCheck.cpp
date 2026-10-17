#include "Check.hpp"
#include "DrawStateFixture.hpp"
#include "ngm/DrawState.hpp"
#include "ngm/Inspection.hpp"

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using namespace ngm::check::state;
Json state(const std::vector<ngm::CppSource>& input) {
    const auto result = ngm::inspect_draw_executions(input);
    require(result.at("executions").size() == 1,
            "one independently selected synthetic submission: " + result.at("unsupported").dump());
    return result.at("executions")[0].at("state");
}
void refused(const std::vector<ngm::CppSource>& input) {
    try {
        const auto parsed = ngm::inspect_draw_executions(input);
        require(parsed.at("executions").empty() && !parsed.at("unsupported").empty(),
                "unqualified source cannot leave earlier draws available");
    } catch(const ngm::InspectionError&) {
    }
}
} // namespace
int main() {
    return ngm::check::run([] {
        auto input = sources<ngm::CppSource>();
        const auto fixed = state(input);
        require(fixed.at("depth_test").at("value") == true &&
                    fixed.at("depth_compare").at("value") == "VK_COMPARE_OP_GREATER" &&
                    fixed.at("cull_mode").at("value") == "VK_CULL_MODE_BACK_BIT",
                "static API fields");
        require(fixed.at("viewports").at("value") == Json::array({{0.0, 0.0, 192.0, 128.0, 0.0, 1.0}}),
                "static viewport");
        const auto dynamic = state(sources<ngm::CppSource>(true));
        require(dynamic.at("depth_test").at("value") == false && dynamic.at("depth_write").at("value") == false &&
                    dynamic.at("depth_compare").at("value") == "VK_COMPARE_OP_LESS" &&
                    dynamic.at("cull_mode").at("value") == "VK_CULL_MODE_NONE" &&
                    dynamic.at("front_face").at("value") == "VK_FRONT_FACE_COUNTER_CLOCKWISE",
                "dynamic setters beat disagreeing creation values");
        const auto expected_viewports = Json::array({{4.0, 6.0, 16.0, 16.0, 0.25, 0.75}});
        const auto expected_scissors = Json::array({{2, 3, 8, 8}});
        require(dynamic.at("viewports").at("value") == expected_viewports &&
                    dynamic.at("scissors").at("value") == expected_scissors,
                "dynamic arrays");
        auto missing = sources<ngm::CppSource>(true, begin + bind + draw + end);
        const auto absent = state(missing);
        for(const auto* field :
            {"depth_test", "depth_write", "depth_compare", "cull_mode", "front_face", "viewports", "scissors"})
            require(absent.at(field).at("status") == "unavailable", "missing setters never use static defaults");
        auto partial = sources<ngm::CppSource>(true);
        partial[0].text = replace(partial[0].text, "CmdSetViewport(cmdBuffer,0u", "CmdSetViewport(cmdBuffer,1u");
        require(state(partial).at("viewports").at("status") == "unavailable", "unwritten viewport zero is unavailable");
        auto invalid_static = state(sources<ngm::CppSource>(false, begin + bind + setters + draw + end));
        require(invalid_static.at("depth_test").at("status") == "unsupported",
                "setter after static bind cannot produce a valid static draw");
        auto with_static_pipeline = sources<ngm::CppSource>(true);
        auto second = objects(false);
        second.resize(second.find("void CreateResources01"));
        second = replace(second, "void CreateResources00", "void CreateResources02");
        for(const auto& [from, to] : {std::pair{"VkPipeline_uid_38", "VkPipeline_uid_39"},
                                      std::pair{"VkShaderModule_uid_36", "VkShaderModule_uid_46"}})
            while(second.find(from) != std::string::npos)
                second = replace(second, from, to);
        with_static_pipeline[1].text += second;
        const auto static_bind =
            replace(replace(bind, "VkPipeline_uid_38", "VkPipeline_uid_39"), "Event #3", "Event #30");
        with_static_pipeline[0].text = ngm::check::cpp::recording(begin + setters + static_bind + bind + draw + end);
        const auto invalidated = state(with_static_pipeline);
        require(invalidated.at("depth_test").at("status") == "unavailable" &&
                    invalidated.at("viewports").at("status") == "unavailable",
                "intervening static bind invalidates earlier dynamic values");
        with_static_pipeline[0].text = ngm::check::cpp::recording(begin + static_bind + setters + bind + draw + end);
        require(state(with_static_pipeline).at("depth_test").at("value") == false,
                "setters after last static bind remain valid before dynamic bind");
        with_static_pipeline[0].text =
            ngm::check::cpp::recording(begin + bind + setters + replace(bind, "Event #3", "Event #30") + draw + end);
        require(state(with_static_pipeline).at("depth_compare").at("value") == "VK_COMPARE_OP_LESS",
                "dynamic rebind preserves dynamic values");
        const auto duplicate_submit =
            ngm::inspect_draw_executions(sources<ngm::CppSource>(false, {}, frame(submit + submit)));
        require(duplicate_submit.at("executions").size() == 2 &&
                    duplicate_submit.at("executions")[0].at("selection").at("submission_index") == 0 &&
                    duplicate_submit.at("executions")[1].at("selection").at("submission_index") == 1,
                "reused recording has distinct submitted occurrences");
        const std::string queue_begin = R"cpp(
{ BEGIN_DATA_SCOPE();
static VkDebugUtilsLabelEXT VkDebugUtilsLabelEXT_queue[1] = {VkDebugUtilsLabelEXT{
VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,nullptr,"Frame submit",NV_TO_ARRAY(0.0f,0.0f,0.0f,0.0f)}};
NV_VK_REPLAY(VulkanReplay_QueueBeginDebugUtilsLabelEXT(VkQueue_uid_5,VkDebugUtilsLabelEXT_queue));
}
)cpp";
        const std::string queue_end = "NV_VK_REPLAY(VulkanReplay_QueueEndDebugUtilsLabelEXT(VkQueue_uid_5));\n";
        const auto labelled =
            ngm::inspect_draw_executions(sources<ngm::CppSource>(false, {}, frame(queue_begin + submit + queue_end)));
        require(labelled.at("executions").size() == 1 && labelled.at("unsupported").empty(),
                "balanced queue labels preserve recorded draw occurrences: " + labelled.at("unsupported").dump());
        const auto& queue_labels = labelled.at("executions")[0].at("queue_labels");
        require(queue_labels.size() == 1 && queue_labels[0].at("name") == "Frame submit" &&
                    queue_labels[0].at("source").at("path") == "Frame0Part00.cpp" &&
                    labelled.at("executions")[0].at("state") == duplicate_submit.at("executions")[0].at("state"),
                "queue metadata retains source provenance without changing raster state");
        const auto nested_begin = replace(queue_begin, "\"Frame submit\"", "\"Nested\"");
        const auto nested = ngm::inspect_draw_executions(
            sources<ngm::CppSource>(false, {}, frame(queue_begin + nested_begin + submit + queue_end + queue_end)));
        require(nested.at("executions").size() == 1 && nested.at("unsupported").empty() &&
                    nested.at("executions")[0].at("queue_labels")[1].at("name") == "Nested",
                "nested queue scopes preserve their recorded order");
        const auto other_begin =
            replace(replace(queue_begin, "VkQueue_uid_5", "VkQueue_uid_6"), "\"Frame submit\"", "\"Other queue\"");
        const auto other_end = replace(queue_end, "VkQueue_uid_5", "VkQueue_uid_6");
        const auto other_submit = replace(submit, "VkQueue_uid_5", "VkQueue_uid_6");
        const auto multiple_queues = ngm::inspect_draw_executions(sources<ngm::CppSource>(
            false, {}, frame(queue_begin + other_begin + submit + queue_end + other_submit + other_end)));
        require(multiple_queues.at("executions").size() == 2 && multiple_queues.at("unsupported").empty() &&
                    multiple_queues.at("executions")[0].at("queue_labels")[0].at("name") == "Frame submit" &&
                    multiple_queues.at("executions")[1].at("queue_labels")[0].at("name") == "Other queue",
                "independent queues do not share label stacks");
        const auto reused = ngm::inspect_draw_executions(
            sources<ngm::CppSource>(false, {}, frame(queue_begin + submit + queue_end + submit)));
        require(reused.at("executions").size() == 2 && reused.at("unsupported").empty() &&
                    reused.at("executions")[0].at("queue_labels").size() == 1 &&
                    reused.at("executions")[1].at("queue_labels").empty(),
                "recording reuse receives submission-local queue labels");
        for(const auto& statements :
            {queue_end + submit, queue_begin + submit, queue_begin + submit + other_end,
             replace(queue_begin, "\"Frame submit\"", "0u") + submit + queue_end,
             replace(queue_begin, "VkQueue_uid_5,VkDebugUtilsLabelEXT_queue", "VkQueue_uid_5") + submit + queue_end,
             queue_begin + submit + replace(queue_end, "VkQueue_uid_5)", "VkQueue_uid_5,0u)"),
             replace(queue_begin, "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT", "VK_UNKNOWN") + submit + queue_end})
            refused(sources<ngm::CppSource>(false, {}, frame(statements)));
        std::string too_deep;
        for(std::size_t i = 0; i < 17; ++i)
            too_deep += queue_begin;
        too_deep += submit;
        for(std::size_t i = 0; i < 17; ++i)
            too_deep += queue_end;
        refused(sources<ngm::CppSource>(false, {}, frame(too_deep)));
        refused(sources<ngm::CppSource>(
            false, {}, frame(replace(queue_begin, "Frame submit", std::string(257, 'x')) + submit + queue_end)));
        std::string too_many_queues;
        for(std::size_t i = 0; i < 65; ++i) {
            const auto queue = "VkQueue_uid_" + std::to_string(i + 100);
            too_many_queues +=
                replace(queue_begin, "VkQueue_uid_5", queue) + replace(queue_end, "VkQueue_uid_5", queue);
        }
        refused(sources<ngm::CppSource>(false, {}, frame(too_many_queues + submit)));
        input = sources<ngm::CppSource>(false, {}, frame(submit + submit));
        input[0].text = replace(input[0].text, "VkCommandBufferUsageFlags(0)",
                                "VkCommandBufferUsageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)");
        refused(input);
        for(const auto& command :
            {std::string("NV_VK_REPLAY(VulkanReplay_CmdSetCullModeEXT(cmdBuffer,VkCullModeFlags(0))); // Event #14\n"),
             std::string(
                 "NV_VK_REPLAY(VulkanReplay_CmdDrawIndirect(cmdBuffer,VkBuffer_uid_1,0ull,1u,16u)); // Event #14\n"),
             std::string("NV_VK_REPLAY(VulkanReplay_CmdExecuteCommands(cmdBuffer,1u,nullptr)); // Event #14\n")})
            refused(sources<ngm::CppSource>(false, begin + bind + draw + command + end));
        input = sources<ngm::CppSource>();
        input[0].text = replace(input[0].text, "// Event #11", "// Event #3");
        refused(input);
        input = sources<ngm::CppSource>();
        input[0].text += "void Record_VkCommandBuffer_uid_42_r1_Part01(VkCommandBuffer cmdBuffer) {}\n";
        refused(input);
        input = sources<ngm::CppSource>();
        input[1].text = replace(input[1].text, "VK_COMMAND_BUFFER_LEVEL_PRIMARY", "VK_COMMAND_BUFFER_LEVEL_SECONDARY");
        refused(input);
        input = sources<ngm::CppSource>();
        input[2].text =
            replace(input[2].text, "&Record_VkCommandBuffer_uid_42_r1", "&Record_VkCommandBuffer_uid_99_r1");
        refused(input);
        input = sources<ngm::CppSource>(
            false, {},
            frame(
                "NV_VK_REPLAY(VulkanReplay_ResetCommandBuffer(VkCommandBuffer_uid_42,VkCommandBufferResetFlags(0)));" +
                submit));
        refused(input);
        input = sources<ngm::CppSource>(false, {}, "void RunFrame0Part00(uint64_t frameIndex) {}\n");
        const auto unsubmitted = ngm::inspect_draw_executions(input);
        require(unsubmitted.at("executions").empty() && !unsubmitted.at("unsupported").empty(),
                "unsubmitted recording does not become an execution");
        input = sources<ngm::CppSource>();
        input[1].text = replace(input[1].text, "nullptr, VkPipelineCreateFlags(0)",
                                "VkUnknown_extension, VkPipelineCreateFlags(0)");
        require(state(input).at("pipeline").at("status") == "unsupported",
                "unknown pipeline extensions invalidate association");
        input = sources<ngm::CppSource>();
        input[1].text = replace(input[1].text, "nullptr,VkAttachmentReference_depth,0u", "nullptr,nullptr,0u");
        require(state(input).at("depth_test").at("status") == "not_applicable",
                "no depth attachment is distinct from disabled depth testing");
        input = sources<ngm::CppSource>(true);
        input[0].text = replace(input[0].text, "4.0f,6.0f", "HexToFloat(0x7FC00000),6.0f");
        refused(input);
        require(ngm::inspect_draw_executions(sources<ngm::CppSource>()).at("full_effective_state_coverage") == false,
                "partial recorded-state coverage never claims full GPU state");
    });
}
