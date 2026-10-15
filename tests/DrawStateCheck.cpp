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
        require(dynamic.at("viewports").at("value") == Json::array({{4.0, 6.0, 16.0, 16.0, 0.25, 0.75}}) &&
                    dynamic.at("scissors").at("value") == Json::array({{2, 3, 8, 8}}),
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
