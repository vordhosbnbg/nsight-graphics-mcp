#include "Check.hpp"
#include "CppReplay.hpp"
#include "DrawStateFixture.hpp"
#include "Vulkan13Fixture.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ReplayTiming.hpp"

#include <algorithm>
#include <array>

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using namespace ngm::check::state;
std::string marker(const std::string& name, unsigned event) {
    const auto symbol = "VkDebugUtilsLabelEXT_marker_" + std::to_string(event);
    return "static VkDebugUtilsLabelEXT " + symbol +
           "[1] = {VkDebugUtilsLabelEXT{"
           "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,nullptr,\"" +
           name +
           "\",NV_TO_ARRAY(0.0f,0.0f,0.0f,0.0f)}};\n"
           "NV_VK_REPLAY(VulkanReplay_CmdBeginDebugUtilsLabelEXT(cmdBuffer," +
           symbol + ")); // Event #" + std::to_string(event) + "\n";
}
std::string close(unsigned event) {
    return "NV_VK_REPLAY(VulkanReplay_CmdEndDebugUtilsLabelEXT(cmdBuffer)); // Event #" + std::to_string(event) + "\n";
}
const std::string indexed =
    "NV_VK_REPLAY(VulkanReplay_CmdBindIndexBuffer(cmdBuffer,VkBuffer_uid_99,0ull,VK_INDEX_TYPE_UINT32)); // Event #24\n"
    "NV_VK_REPLAY(VulkanReplay_CmdDrawIndexed(cmdBuffer,6u,2u,3u,- 2,4u)); // Event #25\n";
const std::string initial_setup_source = R"cpp(void SetInitialFrameState00() {
BEGIN_DATA_SCOPE_FUNCTION();
NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));
{ static VkCommandBufferAllocateInfo VkCommandBufferAllocateInfo_initial[1] = {VkCommandBufferAllocateInfo{
VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,nullptr,VkCommandPool_uid_41,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1u}};
NV_VK_REPLAY(VulkanHelper_InitializeCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_42,VkCommandBufferAllocateInfo_initial,&Record_VkCommandBuffer_uid_42_r0)); }
NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1));
FreeCachedMemory();
})cpp";
std::string queue(const std::string& name, const std::string& statements = submit) {
    return "{ BEGIN_DATA_SCOPE();\n"
           "static VkDebugUtilsLabelEXT VkDebugUtilsLabelEXT_queue[1] = {VkDebugUtilsLabelEXT{"
           "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,nullptr,\"" +
           name +
           "\",NV_TO_ARRAY(0.0f,0.0f,0.0f,0.0f)}};\n"
           "NV_VK_REPLAY(VulkanReplay_QueueBeginDebugUtilsLabelEXT(VkQueue_uid_5,VkDebugUtilsLabelEXT_queue));\n" +
           statements + "NV_VK_REPLAY(VulkanReplay_QueueEndDebugUtilsLabelEXT(VkQueue_uid_5)); }\n";
}
auto inputs(const std::string& body = {}, const std::string& execution = {}) {
    return sources<ngm::CppSource>(false,
                                   body.empty() ? begin + bind + marker("leaf.direct", 20) + draw + close(21) +
                                                      marker("leaf.indexed", 22) + indexed + close(26) + end
                                                : body,
                                   execution.empty() ? frame(queue("queue.a") + queue("queue.b")) : execution);
}
std::string extra_recording(unsigned buffer, std::string body) {
    auto text = ngm::check::cpp::recording(body);
    while(text.find("VkCommandBuffer_uid_42_r1") != std::string::npos)
        text = replace(text, "VkCommandBuffer_uid_42_r1", "VkCommandBuffer_uid_" + std::to_string(buffer) + "_r1");
    return text;
}
std::string allocate_record(unsigned buffer) {
    const auto id = std::to_string(buffer);
    return "{ static VkCommandBufferAllocateInfo VkCommandBufferAllocateInfo_extra[1] = {VkCommandBufferAllocateInfo{"
           "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,nullptr,VkCommandPool_uid_40,VK_COMMAND_BUFFER_LEVEL_"
           "PRIMARY,1u}};"
           "NV_VK_REPLAY(VulkanReplay_AllocateCommandBuffers(VkDevice_uid_4,VkCommandBufferAllocateInfo_extra,NV_TO_"
           "OUTPUT_ARRAY(&VkCommandBuffer_uid_" +
           id +
           "))); }\n"
           "VulkanHelper_RecordCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_" +
           id + ",1u,&Record_VkCommandBuffer_uid_" + id + "_r1);\n";
}
std::string extra_submit(unsigned buffer) {
    return replace(submit, "VkCommandBuffer_uid_42", "VkCommandBuffer_uid_" + std::to_string(buffer));
}
ngm::ProfileTable event_table() {
    return ngm::parse_profile_table(
        "event_text\ttime_ms\nqueue.a\t9\n        leaf.direct\t1.25\n"
        "        leaf.indexed\t2.5\nqueue.b\t8\n        leaf.direct\t3.75\n        leaf.indexed\t4.5\n",
        ngm::ProfileTableKind::EventDurations);
}
ngm::ProfileTable regime_table() {
    return ngm::parse_profile_table(
        "flattened_event_name\tunknown_counter\nqueue.b/leaf.indexed\t0\n"
        "queue.a\t0\nqueue.b\t0\nqueue.a/leaf.indexed\t0\nqueue.a/leaf.direct\t0\nqueue.b/leaf.direct\t0\n",
        ngm::ProfileTableKind::RegimeMetrics);
}
void unavailable(const Json& result, std::size_t count) {
    require(result.at("draws").size() == count, "refusal preserves qualified literal draw selections");
    for(const auto& value : result.at("draws"))
        require(value.at("status") == "unavailable" && value.at("duration_ms").is_null() &&
                    value.at("trace").is_null() && value.at("original_capture_event_id").is_null(),
                "unproved joins expose neither timing nor invented original event IDs");
}
void refused(const std::vector<ngm::CppSource>& source) {
    try {
        const auto result = ngm::correlate_replay_draws(source, event_table(), regime_table());
        require(result.at("draws").empty() && !result.at("unsupported").empty(),
                "unsupported execution clears all earlier candidates");
    } catch(const ngm::InspectionError&) {
    }
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto v13 = ngm::correlate_replay_draws(
            ngm::check::vulkan13::sources<ngm::CppSource>(),
            ngm::parse_profile_table("event_text\ttime_ms\n", ngm::ProfileTableKind::EventDurations),
            ngm::parse_profile_table("flattened_event_name\tunknown_counter\n", ngm::ProfileTableKind::RegimeMetrics));
        require(v13.at("unsupported").empty(), "Vulkan 1.3 source coverage: " + v13.dump());
        unavailable(v13, 1);
        const auto events = event_table(), regimes = regime_table();
        const auto result = ngm::correlate_replay_draws(inputs(), events, regimes);
        require(result.at("unsupported").empty() && result.at("draws").size() == 4,
                "four submitted source calls: " + result.at("unsupported").dump());
        auto extra_regime = regimes;
        auto extra_row = regimes.rows.front();
        extra_row.label = "another.interval/leaf";
        extra_regime.rows.push_back(extra_row);
        extra_regime.rows.push_back(extra_row);
        require(ngm::correlate_replay_draws(inputs(), events, extra_regime)
                        .at("profile_paths")
                        .at("another.interval/leaf") == 2,
                "regime-only repeated paths remain visible to cross-interval guards");
        const std::array expected{1.25, 2.5, 3.75, 4.5};
        const std::array regime_positions{4, 3, 5, 0};
        for(std::size_t i = 0; i < 4; ++i) {
            const auto& draw = result.at("draws")[i];
            require(draw.at("status") == "available" && draw.at("duration_ms") == expected[i] &&
                        draw.at("trace").at("regime_row_index") == regime_positions[i] &&
                        draw.at("selection").at("submission_index") == i / 2 &&
                        draw.at("original_capture_event_id").is_null(),
                    "exact path joins shuffled tables without reusing numeric IDs or row positions");
        }
        require(result.at("draws")[0].at("selection").at("generated_event_index") ==
                    result.at("draws")[2].at("selection").at("generated_event_index"),
                "same recorded event has different submitted occurrences");
        const auto& ix = result.at("draws")[1];
        require(ix.at("parameters").at("index_count") == 6 && ix.at("parameters").at("instance_count") == 2 &&
                    ix.at("parameters").at("first_index") == 3 && ix.at("parameters").at("vertex_offset") == -2 &&
                    ix.at("parameters").at("first_instance") == 4 &&
                    ix.at("included_state_commands") == Json::array({"VulkanReplay_CmdBindIndexBuffer"}),
                "indexed signed parameters and included state packets remain explicit");
        auto two_columns = events;
        two_columns.columns.push_back("time_ms");
        for(auto& row : two_columns.rows)
            row.values.push_back(row.values[0]);
        unavailable(ngm::correlate_replay_draws(inputs(), two_columns, regimes), 4);
        auto missing = regimes;
        missing.rows.erase(missing.rows.begin());
        unavailable(ngm::correlate_replay_draws(inputs(), events, missing), 4);
        auto duplicate = regimes;
        duplicate.rows.push_back(regimes.rows[4]);
        auto ambiguous = ngm::correlate_replay_draws(inputs(), events, duplicate);
        require(ambiguous.at("draws")[0].at("duration_ms").is_null() &&
                    ambiguous.at("draws")[1].at("status") == "available",
                "only duplicated full paths lose association");
        auto source = inputs();
        source[2].text = replace(source[2].text, "queue.b", "queue.a");
        unavailable(ngm::correlate_replay_draws(source, events, regimes), 4);
        const auto pair = begin + bind + marker("leaf.direct", 20) + draw + indexed + close(26) + end;
        unavailable(ngm::correlate_replay_draws(inputs(pair, frame(queue("queue.a"))), events, regimes), 2);
        source = inputs();
        source[0].text =
            replace(source[0].text, draw,
                    "NV_VK_REPLAY(VulkanReplay_CmdExecuteCommands(cmdBuffer,1u,nullptr)); // Event #50\n" + draw);
        refused(source);
        source = inputs();
        source[1].text =
            replace(source[1].text, "VK_COMMAND_BUFFER_LEVEL_PRIMARY", "VK_COMMAND_BUFFER_LEVEL_SECONDARY");
        refused(source);
        source = inputs();
        source[0].text = replace(source[0].text, "VkCommandBufferUsageFlags(0)",
                                 "VkCommandBufferUsageFlags(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)");
        refused(source);
        source = inputs();
        source[0].text = replace(source[0].text, "VkCommandBufferUsageFlags(0)",
                                 "VkCommandBufferUsageFlags(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT)");
        require(ngm::correlate_replay_draws(source, events, regimes).at("draws")[2].at("status") == "available",
                "literal simultaneous-use recording keeps distinct submitted occurrences");
        source = inputs({}, frame(queue("queue.a") +
                                  "NV_VK_REPLAY(VulkanReplay_ResetCommandBuffer(VkCommandBuffer_uid_42,"
                                  "VkCommandBufferResetFlags(0)));\n" +
                                  queue("queue.b")));
        refused(source);
        const auto reset_other = "NV_VK_REPLAY(VulkanReplay_ResetCommandPool(VkDevice_uid_4,VkCommandPool_uid_99,"
                                 "VkCommandPoolResetFlags(0)));\n";
        const auto reset_own = replace(reset_other, "VkCommandPool_uid_99", "VkCommandPool_uid_41");
        const auto unaffected = ngm::correlate_replay_draws(
            inputs({}, frame(queue("queue.a") + reset_other + queue("queue.b"))), events, regimes);
        require(unaffected.at("draws").size() == 4 && unaffected.at("draws")[2].at("status") == "available",
                "an unrelated pool reset cannot discard a proven live recording");
        refused(inputs({}, frame(queue("queue.a") + reset_own + queue("queue.b"))));
        const auto open = allocate_record(44) + extra_submit(44), shut = allocate_record(45) + extra_submit(45);
        auto streaming = inputs({}, frame(open + shut + queue("queue.a") + queue("queue.b")));
        streaming[0].text += extra_recording(44, begin + marker("setup.span", 80) + end);
        streaming[0].text += extra_recording(45, begin + close(81) + end);
        const auto streamed = ngm::correlate_replay_draws(streaming, events, regimes);
        require(streamed.at("draws").size() == 4 && streamed.at("draws")[0].at("status") == "available" &&
                    streamed.at("source_paths").at("setup.span") == 1,
                "non-draw labels can open and close across primary buffer submissions on one queue");
        auto nested_stream = streaming;
        nested_stream[2].text = frame(open + submit + shut);
        const auto nested_events = ngm::parse_profile_table(
            "event_text\ttime_ms\nsetup.span\t20\n        leaf.direct\t1.25\n        leaf.indexed\t2.5\n",
            ngm::ProfileTableKind::EventDurations);
        const auto nested_regimes = ngm::parse_profile_table(
            "flattened_event_name\tunknown\nsetup.span\t0\nsetup.span/leaf.direct\t0\nsetup.span/leaf.indexed\t0\n",
            ngm::ProfileTableKind::RegimeMetrics);
        const auto nested_result = ngm::correlate_replay_draws(nested_stream, nested_events, nested_regimes);
        require(nested_result.at("draws").size() == 2 && nested_result.at("draws")[0].at("status") == "available" &&
                    nested_result.at("draws")[1].at("status") == "available",
                "a cross-buffer parent does not invalidate complete one-call child ranges");
        auto wrong_queue = streaming;
        wrong_queue[2].text =
            replace(wrong_queue[2].text, extra_submit(45), replace(extra_submit(45), "VkQueue_uid_5", "VkQueue_uid_6"));
        refused(wrong_queue);
        auto unclosed = streaming;
        unclosed[0].text = replace(unclosed[0].text, close(81), "");
        refused(unclosed);
        auto interleaved = streaming;
        interleaved[2].text = frame(open + queue("queue.a") + shut + queue("queue.b"));
        refused(interleaved);
        source = inputs();
        source[0].text = replace(source[0].text, "3u,1u,0u,0u", "0u,1u,0u,0u");
        const auto zero = ngm::correlate_replay_draws(source, events, regimes);
        require(zero.at("draws")[0].at("duration_ms").is_null() && zero.at("draws")[1].at("status") == "available",
                "zero-sized draw participation stays unqualified");
        const auto reused = inputs({}, frame(queue("queue.a") +
                                             "VulkanHelper_RecordCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_42,"
                                             "1u,&Record_VkCommandBuffer_uid_42_r1);\n" +
                                             queue("queue.b")));
        require(ngm::correlate_replay_draws(reused, events, regimes).at("draws").size() == 4,
                "explicit rerecording restores a new generation");
        source = inputs();
        const std::string invoke = "VulkanHelper_RecordCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_42,1u,&Record_"
                                   "VkCommandBuffer_uid_42_r1);";
        source[2].text = replace(source[2].text, invoke, "{ NV_THREAD_BEGIN(1); " + invoke + " NV_THREAD_END(1); }");
        const std::string output = R"cpp({ std::vector<uint8_t> std_vector_of_uint8_t_temp_1(16);
NV_VK_REPLAY(VulkanReplay_GetQueryPoolResults(VkDevice_uid_4,VkQueryPool_uid_99,0u,2u,16ull,std_vector_of_uint8_t_temp_1.data(),8ull,VkQueryResultFlags(VK_QUERY_RESULT_64_BIT))); }
)cpp";
        source[2].text.insert(source[2].text.rfind('}'), output);
        const auto annotated = ngm::correlate_replay_draws(source, events, regimes);
        require(annotated.at("draws").size() == 4 && annotated.at("draws")[0].at("status") == "available",
                "bounded blocking recording annotation and host-only query output do not change membership");
        require(ngm::detail::parse_cpp_replay(source).frame.size() > 0,
                "recorded-state grammar also qualifies bounded host reads and blocking recording annotations");
        auto invalid = source;
        invalid[2].text = replace(invalid[2].text, "NV_THREAD_BEGIN", "NV_THREAD_NON_BLOCKING_BEGIN");
        refused(invalid);
        invalid = source;
        invalid[2].text = replace(invalid[2].text, "NV_THREAD_END(1)", "NV_THREAD_END(2)");
        refused(invalid);
        invalid = source;
        invalid[2].text = replace(invalid[2].text, "temp_1(16)", "temp_1(8)");
        refused(invalid);
        invalid = source;
        invalid[2].text = replace(invalid[2].text, "temp_1.data()", "temp_2.data()");
        refused(invalid);
        auto initial = inputs(begin + end, frame(queue("queue.a")));
        initial[2].text = replace(initial[2].text, invoke, "");
        while(initial[0].text.find("VkCommandBuffer_uid_42_r1") != std::string::npos)
            initial[0].text = replace(initial[0].text, "VkCommandBuffer_uid_42_r1", "VkCommandBuffer_uid_42_r0");
        initial.push_back({"FrameSetup00.cpp", initial_setup_source});
        const auto metadata = ngm::correlate_replay_draws(initial, events, regimes);
        require(metadata.at("draws").empty() && metadata.at("unsupported").empty() &&
                    metadata.at("setup_sources").size() == 1,
                "explicit initializer links a non-draw setup recording without guessing from its r0 name");
        auto missing_setup = initial;
        missing_setup.pop_back();
        refused(missing_setup);
        auto changed_setup = initial;
        changed_setup.back().text = replace(initial_setup_source, "{ static", "if (true) { static");
        refused(changed_setup);
        changed_setup = initial;
        changed_setup.back().text =
            replace(initial_setup_source, "NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1));", "");
        refused(changed_setup);
        changed_setup = initial;
        changed_setup.back().text =
            replace(initial_setup_source, "FreeCachedMemory();", "VulkanHelper_UnknownSetup();");
        refused(changed_setup);
        auto initial_draw = inputs({}, frame(queue("queue.a")));
        initial_draw[2].text = replace(initial_draw[2].text, invoke, "");
        while(initial_draw[0].text.find("VkCommandBuffer_uid_42_r1") != std::string::npos)
            initial_draw[0].text =
                replace(initial_draw[0].text, "VkCommandBuffer_uid_42_r1", "VkCommandBuffer_uid_42_r0");
        initial_draw.push_back({"FrameSetup00.cpp", initial_setup_source});
        refused(initial_draw);
    });
}
