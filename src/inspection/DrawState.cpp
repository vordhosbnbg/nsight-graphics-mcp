#include "ngm/DrawState.hpp"
#include "CppReplay.hpp"
#include "DescriptorState.hpp"
#include "PushState.hpp"
#include "ReplayValues.hpp"
#include "ValueExecutionState.hpp"
#include "ngm/Inspection.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace ngm {
namespace {
using Json = nlohmann::json;
using namespace detail;
Json unavailable(const std::string& reason, const std::string& status = "unavailable") {
    return {{"status", status}, {"value", nullptr}, {"reason", reason}, {"evidence", Json::array()}};
}
Json available(Json value, Json evidence) {
    return {{"status", "qualified_recorded_state"},
            {"value", std::move(value)},
            {"reason", nullptr},
            {"evidence", std::move(evidence)}};
}
bool boolean(const ReplayValue& value) {
    if(value.text == "VK_TRUE")
        return true;
    if(value.text == "VK_FALSE")
        return false;
    const auto n = number(value);
    require(n <= 1, "non-boolean VkBool32");
    return n == 1;
}
std::string enumeration(const ReplayValue& value, const std::set<std::string>& permitted) {
    require(permitted.contains(value.text), "unknown enum value");
    return value.text;
}
const std::set<std::string> comparisons{"VK_COMPARE_OP_NEVER",
                                        "VK_COMPARE_OP_LESS",
                                        "VK_COMPARE_OP_EQUAL",
                                        "VK_COMPARE_OP_LESS_OR_EQUAL",
                                        "VK_COMPARE_OP_GREATER",
                                        "VK_COMPARE_OP_NOT_EQUAL",
                                        "VK_COMPARE_OP_GREATER_OR_EQUAL",
                                        "VK_COMPARE_OP_ALWAYS"};
std::string cull(const ReplayValue& value) {
    const auto& v = unwrap(value, "VkCullModeFlags");
    if(v.text == "0" || v.text == "VK_CULL_MODE_NONE")
        return "VK_CULL_MODE_NONE";
    if(v.text == "VK_CULL_MODE_FRONT_BIT | VK_CULL_MODE_BACK_BIT" ||
       v.text == "VK_CULL_MODE_BACK_BIT | VK_CULL_MODE_FRONT_BIT")
        return "VK_CULL_MODE_FRONT_AND_BACK";
    return enumeration(v, {"VK_CULL_MODE_FRONT_BIT", "VK_CULL_MODE_BACK_BIT", "VK_CULL_MODE_FRONT_AND_BACK"});
}
double floating(const ReplayValue& value) {
    if(value.constructor == "HexToFloat") {
        require(value.arguments.size() == 1, "invalid HexToFloat");
        const auto& text = value.arguments[0].text;
        require(text.starts_with("0x") && text.size() == 10, "noncanonical float bits");
        std::uint32_t bits = 0;
        const auto [last, error] = std::from_chars(text.data() + 2, text.data() + text.size(), bits, 16);
        require(error == std::errc{} && last == text.data() + text.size(), "invalid float bits");
        const auto result = std::bit_cast<float>(bits);
        require(std::isfinite(result), "nonfinite viewport value");
        return result;
    }
    std::string text;
    for(const auto c : value.text)
        if(c != ' ')
            text += c;
    if(text.ends_with('f'))
        text.pop_back();
    float result = 0;
    const auto [last, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    require(!text.empty() && error == std::errc{} && last == text.data() + text.size() && std::isfinite(result),
            "invalid literal float");
    return result;
}
Json viewport(const ReplayValue& value) {
    require(value.constructor == "VkViewport" && value.arguments.size() == 6, "invalid viewport");
    Json result = Json::array();
    for(const auto& field : value.arguments)
        result.push_back(floating(field));
    require(result[2].get<double>() > 0 && result[3].get<double>() != 0 && result[4].get<double>() >= 0 &&
                result[4].get<double>() <= 1 && result[5].get<double>() >= 0 && result[5].get<double>() <= 1,
            "viewport outside qualified range");
    return result;
}
Json scissor(const ReplayValue& value) {
    require(value.constructor == "VkRect2D" && value.arguments.size() == 2, "invalid scissor");
    const auto& offset = value.arguments[0];
    const auto& extent = value.arguments[1];
    require(offset.constructor == "VkOffset2D" && offset.arguments.size() == 2 && extent.constructor == "VkExtent2D" &&
                extent.arguments.size() == 2,
            "invalid scissor components");
    Json result = Json::array();
    for(const auto* component : {&offset, &extent})
        for(const auto& field : component->arguments) {
            const auto n = number(field);
            require(n <= 2147483647, "scissor exceeds qualified range");
            result.push_back(n);
        }
    require(result[0].get<std::uint64_t>() + result[2].get<std::uint64_t>() <= 2147483647 &&
                result[1].get<std::uint64_t>() + result[3].get<std::uint64_t>() <= 2147483647,
            "scissor extent overflow");
    return result;
}
struct Pipeline {
    std::string symbol, render_pass;
    Json identity, shaders;
    Json values = Json::object();
    std::set<std::string> dynamic;
    std::map<std::string, std::size_t> counts;
    Json source;
};
bool has_depth(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_CreateRenderPass",
            "render-pass creation unavailable");
    const auto& object = found->second;
    const auto f = fields(object.creation.arguments.at(1), object.values, "VkRenderPassCreateInfo", 9);
    structure(f, "VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO", "VkRenderPassCreateFlags");
    require(number(f[5]) == 1, "multiple subpasses unsupported");
    const auto subpass = fields(f[6], object.values, "VkSubpassDescription", 10);
    require(number(unwrap(subpass[0], "VkSubpassDescriptionFlags")) == 0 &&
                subpass[1].text == "VK_PIPELINE_BIND_POINT_GRAPHICS",
            "unsupported subpass flags or bind point");
    if(subpass[7].text == "nullptr")
        return false;
    const auto depth = fields(subpass[7], object.values, "VkAttachmentReference", 2);
    if(depth[0].text == "VK_ATTACHMENT_UNUSED" || number(depth[0]) == 4294967295)
        return false;
    const auto count = number(f[3]), index = number(depth[0]);
    require(count <= 16 && index < count, "depth attachment index/count unsupported");
    const auto attachments = array(f[4], object.values, "VkAttachmentDescription", count);
    const auto& attachment = attachments[index];
    require(attachment.constructor == "VkAttachmentDescription" && attachment.arguments.size() == 9,
            "invalid depth attachment");
    (void)enumeration(attachment.arguments[1],
                      {"VK_FORMAT_D16_UNORM", "VK_FORMAT_X8_D24_UNORM_PACK32", "VK_FORMAT_D32_SFLOAT",
                       "VK_FORMAT_D16_UNORM_S8_UINT", "VK_FORMAT_D24_UNORM_S8_UINT", "VK_FORMAT_D32_SFLOAT_S8_UINT"});
    return true;
}
Pipeline pipeline(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_CreateGraphicsPipelines",
            "pipeline creation unavailable or ambiguous");
    const auto& object = found->second;
    require(object.creation.arguments.size() == 6 && number(object.creation.arguments[2]) == 1,
            "pipeline batch unsupported");
    const auto f = fields(object.creation.arguments[3], object.values, "VkGraphicsPipelineCreateInfo", 19);
    structure(f, "VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO", "VkPipelineCreateFlags");
    require(number(f[16]) == 0 && (f[17].text == "VkPipeline ( VK_NULL_HANDLE )" || f[17].text == "VK_NULL_HANDLE") &&
                (f[18].text == "0" || f[18].text == "- 1"),
            "subpass or derived pipeline unsupported");
    Pipeline result;
    result.symbol = name;
    result.source = object.source;
    result.render_pass = symbol(f[15], "VkRenderPass");
    result.identity = available({{"symbol", name},
                                 {"layout", symbol(f[14], "VkPipelineLayout")},
                                 {"render_pass", result.render_pass},
                                 {"subpass", 0}},
                                Json::array({object.source}));
    result.shaders =
        object.shader_relationship.contains("stages")
            ? available(object.shader_relationship.at("stages"), Json::array({object.source}))
            : unavailable(object.shader_relationship.value("reason", "shader relationship unavailable"), "unsupported");
    try {
        const auto stages = array(f[4], object.values, "VkPipelineShaderStageCreateInfo", number(f[3]));
        for(const auto& stage : stages) {
            require(stage.constructor == "VkPipelineShaderStageCreateInfo" && stage.arguments.size() == 7 &&
                        number(unwrap(stage.arguments[2], "VkPipelineShaderStageCreateFlags")) == 0,
                    "shader stage flags unsupported");
            const auto module_symbol = symbol(stage.arguments[4], "VkShaderModule");
            require(replay.objects.contains(module_symbol), "shader module creation unavailable");
            const auto& module = replay.objects.at(module_symbol);
            const auto info = fields(module.creation.arguments.at(1), module.values, "VkShaderModuleCreateInfo", 5);
            require(number(unwrap(info[2], "VkShaderModuleCreateFlags")) == 0, "shader module flags unsupported");
        }
    } catch(const std::exception& error) {
        result.shaders = unavailable(error.what(), "unsupported");
    }
    if(f[13].text != "nullptr") {
        const auto dynamic = fields(f[13], object.values, "VkPipelineDynamicStateCreateInfo", 5);
        structure(dynamic, "VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO", "VkPipelineDynamicStateCreateFlags");
        for(const auto& value : array(dynamic[4], object.values, "VkDynamicState", number(dynamic[3]))) {
            const auto state = enumeration(
                value, {"VK_DYNAMIC_STATE_VIEWPORT", "VK_DYNAMIC_STATE_SCISSOR", "VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT",
                        "VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT", "VK_DYNAMIC_STATE_CULL_MODE",
                        "VK_DYNAMIC_STATE_FRONT_FACE", "VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE",
                        "VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE", "VK_DYNAMIC_STATE_DEPTH_COMPARE_OP"});
            require(result.dynamic.insert(state).second, "duplicate dynamic state");
        }
        require(!(result.dynamic.contains("VK_DYNAMIC_STATE_VIEWPORT") &&
                  result.dynamic.contains("VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT")) &&
                    !(result.dynamic.contains("VK_DYNAMIC_STATE_SCISSOR") &&
                      result.dynamic.contains("VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT")),
                "conflicting viewport/scissor dynamic forms");
    }
    try {
        const auto raster = fields(f[9], object.values, "VkPipelineRasterizationStateCreateInfo", 13);
        structure(raster, "VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO",
                  "VkPipelineRasterizationStateCreateFlags");
        require(!boolean(raster[4]), "rasterizer discard requires separate applicability qualification");
        result.values["cull_mode"] = available(cull(raster[6]), Json::array({object.source}));
        result.values["front_face"] =
            available(enumeration(raster[7], {"VK_FRONT_FACE_CLOCKWISE", "VK_FRONT_FACE_COUNTER_CLOCKWISE"}),
                      Json::array({object.source}));
    } catch(const Unqualified& error) {
        // Unknown raster extensions/discard applicability also affect whether
        // depth and viewport fields apply; refuse this pipeline as a whole.
        throw Unqualified(error.what());
    }
    try {
        const bool depth_attachment = has_depth(replay, result.render_pass);
        if(!depth_attachment) {
            for(const auto* key : {"depth_test", "depth_write", "depth_compare"})
                result.values[key] = unavailable("subpass has no depth attachment", "not_applicable");
        } else {
            const auto depth = fields(f[11], object.values, "VkPipelineDepthStencilStateCreateInfo", 12);
            structure(depth, "VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO",
                      "VkPipelineDepthStencilStateCreateFlags");
            result.values["depth_test"] = available(boolean(depth[3]), Json::array({object.source}));
            result.values["depth_write"] = available(boolean(depth[4]), Json::array({object.source}));
            result.values["depth_compare"] =
                available(enumeration(depth[5], comparisons), Json::array({object.source}));
        }
    } catch(const Unqualified& error) {
        for(const auto* key : {"depth_test", "depth_write", "depth_compare"})
            result.values[key] = unavailable(error.what(), "unsupported");
    }
    try {
        const auto view = fields(f[8], object.values, "VkPipelineViewportStateCreateInfo", 7);
        structure(view, "VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO", "VkPipelineViewportStateCreateFlags");
        for(const auto& [key, count_at, pointer_at, dynamic] :
            {std::tuple{"viewports", 3, 4, "VK_DYNAMIC_STATE_VIEWPORT"},
             std::tuple{"scissors", 5, 6, "VK_DYNAMIC_STATE_SCISSOR"}}) {
            const auto count = number(view[count_at]);
            require(count <= 16, "viewport/scissor count exceeds 16");
            result.counts[key] = count;
            if(result.dynamic.contains(std::string(dynamic) + "_WITH_COUNT")) {
                require(count == 0, "dynamic count pipeline has nonzero static count");
                continue;
            }
            require(count > 0, "zero static viewport/scissor count");
            if(!result.dynamic.contains(dynamic)) {
                Json values = Json::array();
                for(const auto& value : array(view[pointer_at], object.values,
                                              key == std::string("viewports") ? "VkViewport" : "VkRect2D", count))
                    values.push_back(key == std::string("viewports") ? viewport(value) : scissor(value));
                result.values[key] = available(std::move(values), Json::array({object.source}));
            }
        }
    } catch(const Unqualified& error) {
        for(const auto* key : {"viewports", "scissors"})
            result.values[key] = unavailable(error.what(), "unsupported");
    }
    return result;
}
const std::map<std::string, std::string> dynamic_names{{"depth_test", "VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE"},
                                                       {"depth_write", "VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE"},
                                                       {"depth_compare", "VK_DYNAMIC_STATE_DEPTH_COMPARE_OP"},
                                                       {"cull_mode", "VK_DYNAMIC_STATE_CULL_MODE"},
                                                       {"front_face", "VK_DYNAMIC_STATE_FRONT_FACE"},
                                                       {"viewports", "VK_DYNAMIC_STATE_VIEWPORT"},
                                                       {"scissors", "VK_DYNAMIC_STATE_SCISSOR"}};
struct DynamicArray {
    std::string method;
    std::size_t count = 0;
    std::map<std::size_t, Json> values, evidence;
};
Json recording_draws(const CppReplay& replay, const ReplayRecording& recording, bool value_selection = false) {
    const auto& calls = recording.calls;
    require(calls.size() >= 2 && calls.front().name == "VulkanReplay_BeginCommandBuffer" &&
                calls.back().name == "VulkanReplay_EndCommandBuffer",
            "recording requires explicit Begin/End");
    require(calls.front().arguments.size() == 2, "invalid begin-command-buffer arity");
    const auto begin = fields(calls.front().arguments[1], recording.values, "VkCommandBufferBeginInfo", 4);
    structure(begin, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO");
    require(begin[3].text == "nullptr", "inherited/secondary recording state unsupported");
    const auto usage = unwrap(begin[2], "VkCommandBufferUsageFlags").text;
    require(usage == "0" || usage == "VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT" ||
                usage == "VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT",
            "recording usage flags unsupported");
    Json draws = Json::array(), labels = Json::array();
    std::size_t result_bytes = 0;
    std::optional<Pipeline> current;
    Json binding;
    std::string pipeline_reason = "no graphics pipeline bind", pass;
    bool index_bound = false;
    std::map<std::string, Json> dynamic_values;
    std::map<std::string, DynamicArray> arrays;
    std::set<std::uint64_t> events;
    PushState push;
    DescriptorState descriptors;
    for(std::size_t at = 0; at < calls.size(); ++at) {
        const auto& c = calls[at];
        const auto& a = c.arguments;
        require(!a.empty() && a[0].text == "cmdBuffer" && events.insert(c.event).second,
                "different command buffer or duplicate event annotation");
        const auto arity = [&](std::size_t n) { require(a.size() == n, "unsupported command arity"); };
        const auto& name = c.name;
        if(name == "VulkanReplay_BeginCommandBuffer") {
            require(at == 0, "nested begin recording");
            continue;
        }
        if(name == "VulkanReplay_EndCommandBuffer") {
            arity(1);
            require(at + 1 == calls.size() && pass.empty() && labels.empty(),
                    "unclosed scope or unexpected end recording");
            continue;
        }
        if(name == "VulkanReplay_CmdBeginDebugUtilsLabelEXT") {
            arity(2);
            const auto label = fields(a[1], recording.values, "VkDebugUtilsLabelEXT", 4);
            structure(label, "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT");
            require(label[2].text.size() <= 258 && label[2].text.starts_with('"'), "invalid label literal");
            auto text = Json::parse(label[2].text);
            require(text.is_string() && labels.size() < 16, "invalid label/depth");
            labels.push_back(std::move(text));
            continue;
        }
        if(name == "VulkanReplay_CmdEndDebugUtilsLabelEXT") {
            arity(1);
            require(!labels.empty(), "unmatched label end");
            labels.erase(labels.size() - 1);
            continue;
        }
        if(name == "VulkanReplay_CmdPipelineBarrier") {
            arity(10);
            continue;
        }
        if(value_selection && (name == "VulkanReplay_CmdResetQueryPool" || name == "VulkanReplay_CmdWriteTimestamp")) {
            arity(4);
            const auto pool = symbol(a[name == "VulkanReplay_CmdResetQueryPool" ? 1 : 2], "VkQueryPool");
            (void)pool;
            require(number(a[3]) <= 65536, "value-selection query index/count exceeds bounds");
            if(name == "VulkanReplay_CmdResetQueryPool")
                require(number(a[2]) <= 65536, "value-selection first query exceeds bounds");
            continue;
        }
        if(value_selection && name == "VulkanReplay_CmdCopyImageToBuffer") {
            arity(6);
            require(pass.empty(), "image readback inside an active render pass");
            (void)symbol(a[1], "VkImage");
            (void)symbol(a[3], "VkBuffer");
            require(a[2].text == "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL" && number(a[4]) > 0 && number(a[4]) <= 64,
                    "value-selection image readback layout/count is unqualified");
            for(const auto& region : array(a[5], recording.values, "VkBufferImageCopy", number(a[4])))
                require(region.constructor == "VkBufferImageCopy" && region.arguments.size() == 6,
                        "value-selection image readback requires literal copy regions");
            // This branch preserves recorded binding banks only. Its target
            // remains a GPU write that the separate value model must invalidate.
            continue;
        }
        if(name == "VulkanReplay_CmdBeginRenderPass") {
            arity(3);
            require(pass.empty() && a[2].text == "VK_SUBPASS_CONTENTS_INLINE",
                    "nested or non-inline render pass unsupported");
            const auto info = fields(a[1], recording.values, "VkRenderPassBeginInfo", 7);
            structure(info, "VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO");
            pass = symbol(info[2], "VkRenderPass");
            continue;
        }
        if(name == "VulkanReplay_CmdEndRenderPass") {
            arity(1);
            require(!pass.empty(), "unmatched render pass end");
            pass.clear();
            continue;
        }
        if(name == "VulkanReplay_CmdBindPipeline") {
            arity(3);
            require(a[1].text == "VK_PIPELINE_BIND_POINT_GRAPHICS", "non-graphics bind unsupported");
            binding = {{"event_index", c.event}, {"source", c.source}};
            try {
                current = pipeline(replay, symbol(a[2], "VkPipeline"));
                for(const auto& [key, dynamic] : dynamic_names)
                    if(!current->dynamic.contains(dynamic) && !current->dynamic.contains(dynamic + "_WITH_COUNT")) {
                        dynamic_values.erase(key);
                        arrays.erase(key);
                    } else if((key == "viewports" || key == "scissors") && current->dynamic.contains(dynamic) &&
                              arrays.contains(key) && arrays.at(key).method.ends_with("WithCount")) {
                        // The regular form binds a static count, invalidating a
                        // previous WITH_COUNT value even though its array is dynamic.
                        arrays.erase(key);
                    }
            } catch(const Unqualified& error) {
                current.reset();
                dynamic_values.clear();
                arrays.clear();
                pipeline_reason = error.what();
            }
            continue;
        }
        if(name == "VulkanReplay_CmdBindDescriptorSets") {
            arity(8);
            descriptors.record(replay, c, recording.values);
            continue;
        }
        if(name == "VulkanReplay_CmdPushConstants") {
            arity(6);
            push.record(replay, c, recording.values);
            continue;
        }
        if(name == "VulkanReplay_CmdBindIndexBuffer") {
            arity(4);
            (void)symbol(a[1], "VkBuffer");
            (void)number(a[2]);
            (void)enumeration(a[3], {"VK_INDEX_TYPE_UINT16", "VK_INDEX_TYPE_UINT32"});
            index_bound = true;
            continue;
        }
        const std::map<std::string, std::string> setters{{"VulkanReplay_CmdSetDepthTestEnable", "depth_test"},
                                                         {"VulkanReplay_CmdSetDepthWriteEnable", "depth_write"},
                                                         {"VulkanReplay_CmdSetDepthCompareOp", "depth_compare"},
                                                         {"VulkanReplay_CmdSetCullMode", "cull_mode"},
                                                         {"VulkanReplay_CmdSetFrontFace", "front_face"}};
        if(const auto setter = setters.find(name); setter != setters.end()) {
            arity(2);
            Json value;
            if(setter->second == "depth_test" || setter->second == "depth_write")
                value = boolean(a[1]);
            else if(setter->second == "depth_compare")
                value = enumeration(a[1], comparisons);
            else if(setter->second == "cull_mode")
                value = cull(a[1]);
            else
                value = enumeration(a[1], {"VK_FRONT_FACE_CLOCKWISE", "VK_FRONT_FACE_COUNTER_CLOCKWISE"});
            dynamic_values[setter->second] =
                available(std::move(value), Json::array({{{"event_index", c.event}, {"source", c.source}}}));
            continue;
        }
        if(name == "VulkanReplay_CmdSetViewport" || name == "VulkanReplay_CmdSetScissor" ||
           name == "VulkanReplay_CmdSetViewportWithCount" || name == "VulkanReplay_CmdSetScissorWithCount") {
            const bool with_count = name.ends_with("WithCount"), view = name.find("Viewport") != std::string::npos;
            arity(with_count ? 3 : 4);
            const auto first = with_count ? 0 : number(a[1]), count = number(a[with_count ? 1 : 2]);
            require(count > 0 && count <= 16 && first + count <= 16, "dynamic array range exceeds qualified bounds");
            auto& state = arrays[view ? "viewports" : "scissors"];
            if(with_count || state.method != name)
                state = {};
            state.method = name;
            if(with_count)
                state.count = count;
            const auto values = array(a[with_count ? 2 : 3], recording.values, view ? "VkViewport" : "VkRect2D", count);
            for(std::size_t n = 0; n < count; ++n) {
                state.values[first + n] = view ? viewport(values[n]) : scissor(values[n]);
                state.evidence[first + n] = {{"event_index", c.event}, {"source", c.source}};
            }
            continue;
        }
        if(name == "VulkanReplay_CmdDraw" || name == "VulkanReplay_CmdDrawIndexed") {
            const bool indexed = name.ends_with("Indexed");
            arity(indexed ? 6 : 5);
            require(!pass.empty() && (!indexed || index_bound), "draw outside inline pass or missing index binding");
            for(std::size_t n = 1; n < a.size(); ++n) {
                auto text = a[n].text;
                const bool negative = indexed && n == 4 && text.starts_with("- ");
                if(negative)
                    text.erase(0, 2);
                const auto maximum = indexed && n == 4 ? (negative ? 2147483648ull : 2147483647ull) : 4294967295ull;
                require(number(text) <= maximum, "draw argument exceeds qualified bounds");
            }
            Json state = Json::object();
            for(const auto* key : {"pipeline", "shaders", "depth_test", "depth_write", "depth_compare", "cull_mode",
                                   "front_face", "viewports", "scissors"})
                state[key] = unavailable(pipeline_reason, "unsupported");
            if(current && current->render_pass == pass) {
                state["pipeline"] = current->identity;
                state["shaders"] = current->shaders;
                for(const auto& [key, dynamic] : dynamic_names) {
                    const bool with_count = current->dynamic.contains(dynamic + "_WITH_COUNT");
                    if(current->values.contains(key) && (current->values[key].at("status") == "not_applicable" ||
                                                         current->values[key].at("status") == "unsupported")) {
                        state[key] = current->values[key];
                        continue;
                    }
                    if(current->dynamic.contains(dynamic) || with_count) {
                        if(key == "viewports" || key == "scissors") {
                            state[key] = unavailable("missing or incomplete dynamic array after the last static bind");
                            if(const auto found = arrays.find(key); found != arrays.end()) {
                                const auto& value = found->second;
                                const bool method_matches = value.method.ends_with("WithCount") == with_count;
                                const auto count = with_count ? value.count : current->counts[key];
                                Json values = Json::array(), evidence = Json::array({current->source, binding});
                                if(method_matches && count > 0 && count <= 16) {
                                    for(std::size_t n = 0; n < count && value.values.contains(n); ++n) {
                                        values.push_back(value.values.at(n));
                                        evidence.push_back(value.evidence.at(n));
                                    }
                                    if(values.size() == count)
                                        state[key] = available(std::move(values), std::move(evidence));
                                }
                            }
                        } else if(dynamic_values.contains(key)) {
                            state[key] = dynamic_values.at(key);
                            state[key]["evidence"].push_back(current->source);
                            state[key]["evidence"].push_back(binding);
                        } else
                            state[key] = unavailable("missing dynamic setter after the last static bind");
                    } else if(dynamic_values.contains(key) || arrays.contains(key))
                        state[key] = unavailable(
                            "dynamic setter after a static pipeline bind makes this draw state invalid", "unsupported");
                    else if(current->values.contains(key)) {
                        state[key] = current->values.at(key);
                        state[key]["evidence"].push_back(binding);
                    }
                }
            } else if(current) {
                for(auto& value : state)
                    value = unavailable("bound pipeline and active render pass differ; compatibility unqualified",
                                        "unsupported");
            }
            state["uniform_material_values"] = unavailable(
                "descriptor/layout/write lifetime and execution-value correspondence are not yet qualified");
            state["descriptors"] =
                current ? descriptors.at_draw(replay, current->identity.at("value").at("layout").get<std::string>())
                        : unavailable("bound pipeline/layout unavailable");
            state["push_constants"] =
                current ? push.at_draw(replay, current->identity.at("value").at("layout").get<std::string>())
                        : unavailable("bound pipeline/layout unavailable");
            Json arguments = Json::array();
            for(const auto& value : a)
                arguments.push_back(value.text);
            draws.push_back({{"event_index", c.event},
                             {"function", name},
                             {"arguments", arguments},
                             {"labels", labels},
                             {"source", c.source},
                             {"state", state}});
            result_bytes += draws.back().dump().size();
            require(draws.size() <= 4096 && result_bytes <= 16U * 1024U * 1024U,
                    "recording draw results exceed bounds");
            continue;
        }
        throw Unqualified("unqualified command invalidates recording coverage: " + name);
    }
    return draws;
}
} // namespace

namespace detail {
Json value_recording_draws(const CppReplay& replay, const ReplayRecording& recording) {
    return recording_draws(replay, recording, true);
}
} // namespace detail

nlohmann::json inspect_draw_executions(const std::vector<CppSource>& sources) {
    Json result{{"executions", Json::array()},
                {"unsupported", Json::array()},
                {"selection_domain", "generated_cpp_recorded_submission"},
                {"evidence_origin", "nsight_generated_cpp_recorded_state"},
                {"captured_gpu_execution_correspondence", false},
                {"full_effective_state_coverage", false}};
    try {
        const auto replay = detail::parse_cpp_replay(sources);
        result["unsupported"] = replay.unsupported;
        std::map<std::string, Json> draws;
        for(const auto& [name, recording] : replay.recordings) {
            try {
                draws.emplace(name, recording_draws(replay, recording));
            } catch(const Unqualified& error) {
                result["unsupported"].push_back(
                    {{"recording", name}, {"source", recording.source}, {"reason", error.what()}});
            }
        }
        std::map<std::string, std::string> active;
        std::map<std::string, std::size_t> generations;
        std::set<std::pair<std::string, std::size_t>> one_time_submissions;
        std::set<std::string> submitted_recordings;
        std::size_t submission = 0;
        std::size_t result_bytes = result.at("unsupported").dump().size();
        for(const auto& call : replay.frame) {
            const auto& a = call.arguments;
            if(call.name == "VulkanHelper_RecordCommandBuffer") {
                require(a.size() == 4 && number(a[2]) > 0 && number(a[2]) <= 4294967295 &&
                            a[3].text.starts_with("& Record_"),
                        "unsupported recording invocation");
                const auto buffer = symbol(a[1], "VkCommandBuffer"), recording = a[3].text.substr(2);
                require(recording == "Record_" + buffer + "_r" + std::to_string(number(a[2])) &&
                            replay.recordings.contains(recording),
                        "recording/command-buffer identity mismatch or absent recording");
                active[buffer] = recording;
                ++generations[buffer];
            } else if(call.name == "VulkanReplay_ResetCommandBuffer") {
                require(a.size() == 2 && number(unwrap(a[1], "VkCommandBufferResetFlags")) == 0,
                        "unsupported command buffer reset");
                active.erase(symbol(a[0], "VkCommandBuffer"));
            } else if(call.name == "VulkanReplay_QueueSubmit") {
                require(a.size() == 4 && number(a[1]) > 0 && number(a[1]) <= 16,
                        "unsupported queue submission count/arity");
                const auto queue = symbol(a[0], "VkQueue");
                const auto submits = array(a[2], call.visible, "VkSubmitInfo", number(a[1]));
                for(std::size_t submit_at = 0; submit_at < submits.size(); ++submit_at) {
                    const auto& info = submits[submit_at];
                    require(info.constructor == "VkSubmitInfo" && info.arguments.size() == 9, "invalid submit info");
                    const auto& f = info.arguments;
                    structure(f, "VK_STRUCTURE_TYPE_SUBMIT_INFO");
                    const auto count = number(f[5]);
                    require(count > 0 && count <= 64, "empty or oversized command-buffer submission");
                    const auto buffers = array(f[6], call.visible, "VkCommandBuffer", count);
                    for(std::size_t buffer_at = 0; buffer_at < buffers.size(); ++buffer_at) {
                        const auto buffer = symbol(buffers[buffer_at], "VkCommandBuffer");
                        require(active.contains(buffer),
                                "submitted command buffer lacks a recording in the captured frame");
                        const auto& recording = active.at(buffer);
                        const auto& recorded = replay.recordings.at(recording);
                        require(!recorded.calls.empty() && recorded.calls.front().arguments.size() == 2,
                                "submitted recording has no qualified Begin info");
                        const auto begin =
                            fields(recorded.calls.front().arguments[1], recorded.values, "VkCommandBufferBeginInfo", 4);
                        if(unwrap(begin[2], "VkCommandBufferUsageFlags").text ==
                           "VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT")
                            require(one_time_submissions.emplace(buffer, generations[buffer]).second,
                                    "one-time command buffer submitted again without rerecording");
                        const auto allocation = replay.objects.find(buffer);
                        require(allocation != replay.objects.end(), "command-buffer allocation unavailable");
                        const auto alloc = fields(allocation->second.creation.arguments.at(1),
                                                  allocation->second.values, "VkCommandBufferAllocateInfo", 5);
                        structure(alloc, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
                        require(alloc[3].text == "VK_COMMAND_BUFFER_LEVEL_PRIMARY" && number(alloc[4]) == 1,
                                "secondary or batch command-buffer allocation unsupported");
                        submitted_recordings.insert(recording);
                        if(!draws.contains(recording))
                            continue;
                        for(const auto& draw : draws.at(recording)) {
                            auto execution = draw;
                            execution["selection"] = {{"frame", 0},
                                                      {"submission_index", submission},
                                                      {"submit_info_index", submit_at},
                                                      {"command_buffer_index", buffer_at},
                                                      {"queue", queue},
                                                      {"command_buffer", buffer},
                                                      {"recording", recording},
                                                      {"event_index", draw.at("event_index")},
                                                      {"indirect_subdraw", nullptr}};
                            execution["submission_source"] = call.source;
                            result_bytes += execution.dump().size();
                            result["executions"].push_back(std::move(execution));
                            require(result["executions"].size() <= 10000 && result_bytes <= 16U * 1024U * 1024U,
                                    "execution result exceeds bounds");
                        }
                    }
                }
                ++submission;
            } else if(call.name == "VulkanReplay_WaitForFences" || call.name == "VulkanReplay_ResetFences" ||
                      call.name == "VulkanReplay_AcquireNextImageKHR" || call.name == "VulkanReplay_QueuePresentKHR" ||
                      call.name == "VulkanHelper_UpdateMappedMemory2") {
                // These observed calls cannot alter the raster fields interpreted
                // here. Their data/descriptor visibility is deliberately absent.
                const std::map<std::string, std::size_t> counts{{"VulkanReplay_WaitForFences", 5},
                                                                {"VulkanReplay_ResetFences", 3},
                                                                {"VulkanReplay_AcquireNextImageKHR", 6},
                                                                {"VulkanReplay_QueuePresentKHR", 2},
                                                                {"VulkanHelper_UpdateMappedMemory2", 8}};
                require(a.size() == counts.at(call.name), "unqualified frame call arity");
            } else
                throw Unqualified("unqualified frame call prevents execution association: " + call.name);
        }
        for(const auto& [name, recording] : replay.recordings)
            if(!submitted_recordings.contains(name))
                result["unsupported"].push_back(
                    {{"recording", name},
                     {"source", recording.source},
                     {"reason", "recording is not submitted in the qualified captured frame"}});
    } catch(const Unqualified& error) {
        result["executions"] = Json::array();
        result["unsupported"].push_back({{"reason", error.what()}, {"scope", "frame_execution_association"}});
    } catch(const std::out_of_range&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Truncated state input or unresolved object");
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Invalid replay state value");
    }
    if(result.dump().size() > 16U * 1024U * 1024U)
        throw InspectionError(InspectionErrorCode::LimitExceeded, "Recorded-state result exceeds 16 MiB");
    return result;
}
} // namespace ngm
