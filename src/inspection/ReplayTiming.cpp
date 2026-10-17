#include "ngm/ReplayTiming.hpp"
#include "CppReplay.hpp"
#include "ReplayValues.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ProfileMarkers.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>

namespace ngm {
namespace {
using Json = nlohmann::json;
using namespace detail;
constexpr std::size_t record_limit = 10000, byte_limit = 16U * 1024U * 1024U;
struct Label {
    std::string name;
    Json source;
};
std::string label_name(const ReplayValue& reference, const ReplayDeclarations& values) {
    const auto f = fields(reference, values, "VkDebugUtilsLabelEXT", 4);
    structure(f, "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT");
    require(f[2].text.starts_with('"') && f[2].text.size() <= 1538, "marker name is not a bounded literal");
    const auto parsed = Json::parse(f[2].text);
    require(parsed.is_string(), "marker name is not a string literal");
    const auto name = parsed.get<std::string>();
    require(!name.empty() && name.size() <= ProfileMarkerLimits::component_bytes && name.front() != ' ' &&
                name.back() != ' ' &&
                std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c > 126 || c == '/'; }),
            "marker names require bounded visible ASCII without slash or edge spaces");
    return name;
}
std::string flat_path(const std::vector<Label>& labels) {
    require(labels.size() <= ProfileMarkerLimits::depth, "combined marker depth exceeds bounds");
    std::string result;
    for(const auto& label : labels) {
        if(!result.empty())
            result += '/';
        result += label.name;
        require(result.size() <= ProfileMarkerLimits::path_bytes, "combined marker path exceeds bounds");
    }
    return result;
}
std::uint64_t u32(const ReplayValue& value) {
    const auto result = number(value);
    require(result <= UINT32_MAX, "draw argument exceeds uint32");
    return result;
}
std::int64_t i32(const ReplayValue& value) {
    const auto& v = unwrap(value, "int32_t");
    const auto negative = v.text.starts_with("- ");
    const auto result = number(negative ? v.text.substr(2) : v.text);
    require(result <= (negative ? 2147483648ULL : 2147483647ULL), "draw vertex offset exceeds int32");
    return negative ? -static_cast<std::int64_t>(result) : static_cast<std::int64_t>(result);
}
enum class CommandKind { Boundary, BeginLabel, EndLabel, Draw, State, Work };
struct Command {
    const ReplayCall* call = nullptr;
    CommandKind kind = CommandKind::Boundary;
    std::optional<Label> label;
    Json parameters;
};
struct Recording {
    bool one_time = false;
    std::size_t draw_count = 0;
    std::vector<Command> commands;
};
Recording recording(const ReplayRecording& input) {
    require(input.calls.size() >= 2 && input.calls.front().name == "VulkanReplay_BeginCommandBuffer" &&
                input.calls.back().name == "VulkanReplay_EndCommandBuffer" && input.calls.front().arguments.size() == 2,
            "recording needs qualified begin/end boundaries");
    const auto begin = fields(input.calls.front().arguments[1], input.values, "VkCommandBufferBeginInfo", 4);
    structure(begin, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO");
    require(begin[3].text == "nullptr", "primary recording inheritance must be null");
    const auto flags = unwrap(begin[2], "VkCommandBufferUsageFlags").text;
    require(flags == "0" || flags == "VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT" ||
                flags == "VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT",
            "recording usage flags are unqualified");
    Recording result;
    result.one_time = flags == "VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT";
    std::set<std::uint64_t> event_ids;
    const std::map<std::string, std::size_t> state{
        {"VulkanReplay_CmdBindVertexBuffers", 5},   {"VulkanReplay_CmdBindPipeline", 3},
        {"VulkanReplay_CmdBindIndexBuffer", 4},     {"VulkanReplay_CmdBindDescriptorSets", 8},
        {"VulkanReplay_CmdPushConstants", 6},       {"VulkanReplay_CmdSetViewport", 4},
        {"VulkanReplay_CmdSetScissor", 4},          {"VulkanReplay_CmdSetViewportWithCount", 3},
        {"VulkanReplay_CmdSetScissorWithCount", 3}, {"VulkanReplay_CmdSetDepthTestEnable", 2},
        {"VulkanReplay_CmdSetDepthWriteEnable", 2}, {"VulkanReplay_CmdSetDepthCompareOp", 2},
        {"VulkanReplay_CmdSetCullMode", 2},         {"VulkanReplay_CmdSetFrontFace", 2}};
    const std::map<std::string, std::size_t> work{
        {"VulkanReplay_CmdPipelineBarrier", 10}, {"VulkanReplay_CmdBeginRenderPass", 3},
        {"VulkanReplay_CmdEndRenderPass", 1},    {"VulkanReplay_CmdCopyBufferToImage", 6},
        {"VulkanReplay_CmdBlitImage", 8},        {"VulkanReplay_CmdResetQueryPool", 4},
        {"VulkanReplay_CmdWriteTimestamp", 4},   {"VulkanReplay_CmdPipelineBarrier2", 2},
        {"VulkanReplay_CmdBeginRendering", 2},   {"VulkanReplay_CmdEndRendering", 1},
        {"VulkanReplay_CmdWriteTimestamp2", 4}};
    for(std::size_t at = 0; at < input.calls.size(); ++at) {
        const auto& c = input.calls[at];
        const auto& a = c.arguments;
        require(!a.empty() && a[0].text == "cmdBuffer" && event_ids.insert(c.event).second,
                "command buffer differs or source event annotation repeats");
        Command command;
        command.call = &c;
        if(c.name == "VulkanReplay_BeginCommandBuffer")
            require(at == 0, "nested recording begin");
        else if(c.name == "VulkanReplay_EndCommandBuffer")
            require(a.size() == 1 && at + 1 == input.calls.size(), "unqualified end recording");
        else if(c.name == "VulkanReplay_CmdBeginDebugUtilsLabelEXT") {
            require(a.size() == 2, "invalid command marker begin");
            command.kind = CommandKind::BeginLabel;
            command.label = Label{label_name(a[1], input.values), c.source};
        } else if(c.name == "VulkanReplay_CmdEndDebugUtilsLabelEXT") {
            require(a.size() == 1, "invalid command marker end");
            command.kind = CommandKind::EndLabel;
        } else if(c.name == "VulkanReplay_CmdDraw" || c.name == "VulkanReplay_CmdDrawIndexed") {
            const bool indexed = c.name == "VulkanReplay_CmdDrawIndexed";
            require(a.size() == (indexed ? 6 : 5), "unqualified direct draw arity");
            command.kind = CommandKind::Draw;
            command.parameters = {{indexed ? "index_count" : "vertex_count", u32(a[1])},
                                  {"instance_count", u32(a[2])},
                                  {indexed ? "first_index" : "first_vertex", u32(a[3])},
                                  {"first_instance", u32(a[indexed ? 5 : 4])}};
            if(indexed)
                command.parameters["vertex_offset"] = i32(a[4]);
            ++result.draw_count;
        } else if(const auto found = state.find(c.name); found != state.end()) {
            require(a.size() == found->second, "unqualified state command arity");
            command.kind = CommandKind::State;
        } else if(const auto found = work.find(c.name); found != work.end()) {
            require(a.size() == found->second, "unqualified work command arity");
            command.kind = CommandKind::Work;
        } else
            throw Unqualified("unqualified recorded command prevents marker membership: " + c.name);
        result.commands.push_back(std::move(command));
    }
    return result;
}
struct Occurrence {
    std::size_t submission = 0, info = 0, buffer = 0;
    bool operator==(const Occurrence&) const = default;
};
struct Range {
    std::vector<Label> labels;
    std::size_t draws = 0, children = 0;
    bool other_work = false;
    std::set<std::string> state_commands;
    Occurrence begin;
    std::optional<Occurrence> end;
};
struct Candidate {
    Json value;
    std::string path, refusal;
    std::optional<std::size_t> range;
    Occurrence occurrence;
};
} // namespace
Json correlate_replay_draws(const std::vector<CppSource>& sources, const ProfileTable& events,
                            const ProfileTable& regimes) {
    Json result{{"draws", Json::array()},
                {"unsupported", Json::array()},
                {"setup_sources", Json::array()},
                {"source_paths", Json::object()},
                {"profile_paths", Json::object()},
                {"profile_paths_qualified", false},
                {"profile_hierarchy", {{"status", "unavailable"}, {"reason", "Source interpretation is incomplete"}}},
                {"scope", "Unique literal marker ranges containing one submitted direct/indexed source call, including "
                          "state packets and replay/instrumentation overhead; no isolated shader cost, original event "
                          "ID or live performance inference."}};
    try {
        const auto replay = detail::parse_cpp_replay(sources, detail::ReplayParseScope::Timing);
        std::map<std::string, Recording> recordings;
        std::map<std::string, std::string> active;
        std::map<std::string, std::size_t> generations;
        std::map<std::string, std::string> pools;
        std::set<std::pair<std::string, std::size_t>> one_time;
        std::map<std::string, std::vector<Label>> queues;
        std::map<std::string, std::vector<std::size_t>> command_stacks;
        std::map<std::string, std::size_t> paths;
        std::vector<Range> ranges;
        std::vector<Candidate> candidates;
        std::size_t submission = 0, path_bytes = 0, range_bytes = 0, candidate_bytes = 0;
        const auto require_primary = [&](const std::string& buffer) {
            if(pools.contains(buffer))
                return;
            const auto found = replay.objects.find(buffer);
            if(found == replay.objects.end())
                throw Unqualified("command buffer allocation is unavailable: " + buffer);
            require(found != replay.objects.end() &&
                        found->second.creation.name == "VulkanReplay_AllocateCommandBuffers" &&
                        found->second.creation.arguments.size() == 3,
                    "command buffer allocation is unavailable");
            const auto& object = found->second;
            const auto f = fields(object.creation.arguments[1], object.values, "VkCommandBufferAllocateInfo", 5);
            structure(f, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
            require(f[3].text == "VK_COMMAND_BUFFER_LEVEL_PRIMARY" && number(f[4]) == 1,
                    "secondary or batch allocation is unqualified");
            pools[buffer] = symbol(f[2], "VkCommandPool");
        };
        for(const auto& initialization : replay.initial_recordings) {
            const auto& a = initialization.arguments;
            require(a.size() == 4 && a[3].text.starts_with("& Record_"), "invalid initial recording metadata");
            const auto buffer = symbol(a[1], "VkCommandBuffer"), name = a[3].text.substr(2);
            require(!initialization.visible.contains(buffer) && name == "Record_" + buffer + "_r0" &&
                        replay.recordings.contains(name) && !active.contains(buffer),
                    "initial recording identity is absent or conflicting");
            const auto f = fields(a[2], initialization.visible, "VkCommandBufferAllocateInfo", 5);
            structure(f, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
            require(f[3].text == "VK_COMMAND_BUFFER_LEVEL_PRIMARY" && number(f[4]) == 1,
                    "initial recording requires one primary buffer");
            auto model = recording(replay.recordings.at(name));
            require(model.draw_count == 0, "draws in pre-frame initial recordings require separate qualification");
            recordings.emplace(name, std::move(model));
            active[buffer] = name;
            generations[buffer] = 1;
            pools[buffer] = symbol(f[2], "VkCommandPool");
            result["setup_sources"].push_back(initialization.source);
        }
        const auto retain_path = [&](const std::vector<Label>& labels) {
            auto path = flat_path(labels);
            if(!paths.contains(path)) {
                path_bytes += path.size();
                require(paths.size() < record_limit && path_bytes <= byte_limit, "source marker paths exceed bounds");
            }
            ++paths[path];
            return path;
        };
        const std::map<std::string, std::size_t> host{{"VulkanReplay_WaitForFences", 5},
                                                      {"VulkanReplay_ResetFences", 3},
                                                      {"VulkanReplay_GetFenceStatus", 2},
                                                      {"VulkanReplay_AcquireNextImageKHR", 6},
                                                      {"VulkanReplay_QueuePresentKHR", 2},
                                                      {"VulkanReplay_QueueWaitIdle", 1},
                                                      {"VulkanHelper_UpdateMappedMemory2", 8},
                                                      {"VulkanReplay_GetQueryPoolResults", 8},
                                                      {"VulkanHelper_UpdateDescriptorSetsFromDatabase", 6}};
        for(const auto& c : replay.frame) {
            const auto& a = c.arguments;
            if(c.name == "VulkanHelper_RecordCommandBuffer") {
                require(a.size() == 4 && number(a[2]) <= UINT32_MAX && a[3].text.starts_with("& Record_"),
                        "unqualified recording invocation");
                const auto buffer = symbol(a[1], "VkCommandBuffer"), name = a[3].text.substr(2);
                require(name == "Record_" + buffer + "_r" + std::to_string(number(a[2])) &&
                            replay.recordings.contains(name),
                        "recording identity is absent or inconsistent");
                if(!recordings.contains(name))
                    recordings.emplace(name, recording(replay.recordings.at(name)));
                active[buffer] = name;
                ++generations[buffer];
            } else if(c.name == "VulkanReplay_AllocateCommandBuffers") {
                require(a.size() == 3, "unqualified frame allocation");
                const auto f = fields(a[1], c.visible, "VkCommandBufferAllocateInfo", 5);
                structure(f, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
                require(f[3].text == "VK_COMMAND_BUFFER_LEVEL_PRIMARY" && number(f[4]) == 1 &&
                            a[2].constructor == "NV_TO_OUTPUT_ARRAY" && a[2].arguments.size() == 1 &&
                            a[2].arguments[0].text.starts_with("& "),
                        "frame allocation requires one primary output");
                auto output = a[2].arguments[0];
                output.text = output.text.substr(2);
                const auto buffer = symbol(output, "VkCommandBuffer");
                require(!pools.contains(buffer), "frame allocation reuses an already qualified handle");
                pools[buffer] = symbol(f[2], "VkCommandPool");
                active.erase(buffer);
            } else if(c.name == "VulkanReplay_ResetCommandPool") {
                require(a.size() == 3 && number(unwrap(a[2], "VkCommandPoolResetFlags")) == 0,
                        "unqualified pool reset");
                const auto pool = symbol(a[1], "VkCommandPool");
                for(auto at = active.begin(); at != active.end();) {
                    require_primary(at->first);
                    if(pools.at(at->first) == pool)
                        at = active.erase(at);
                    else
                        ++at;
                }
            } else if(c.name == "VulkanReplay_ResetCommandBuffer") {
                require(a.size() == 2 && number(unwrap(a[1], "VkCommandBufferResetFlags")) == 0,
                        "unqualified buffer reset");
                active.erase(symbol(a[0], "VkCommandBuffer"));
            } else if(c.name == "VulkanReplay_QueueBeginDebugUtilsLabelEXT") {
                require(a.size() == 2, "invalid queue marker begin");
                const auto queue = symbol(a[0], "VkQueue");
                require(command_stacks[queue].empty(),
                        "queue and command marker scopes interleave outside the qualified shape");
                auto& stack = queues[queue];
                stack.push_back({label_name(a[1], c.visible), c.source});
                (void)retain_path(stack);
            } else if(c.name == "VulkanReplay_QueueEndDebugUtilsLabelEXT") {
                require(a.size() == 1, "invalid queue marker end");
                const auto queue = symbol(a[0], "VkQueue");
                require(command_stacks[queue].empty(),
                        "queue and command marker scopes interleave outside the qualified shape");
                auto& stack = queues[queue];
                require(!stack.empty(), "unmatched queue marker end");
                stack.pop_back();
            } else if(c.name == "VulkanReplay_QueueSubmit" || c.name == "VulkanReplay_QueueSubmit2") {
                const bool sync2 = c.name.ends_with("2");
                require(a.size() == 4 && number(a[1]) > 0 && number(a[1]) <= 16, "unqualified queue submit");
                const auto queue = symbol(a[0], "VkQueue");
                const auto submits = array(a[2], c.visible, sync2 ? "VkSubmitInfo2" : "VkSubmitInfo", number(a[1]));
                for(std::size_t info_at = 0; info_at < submits.size(); ++info_at) {
                    const auto buffers = submitted_buffers(submits[info_at], c.visible, sync2);
                    for(std::size_t buffer_at = 0; buffer_at < buffers.size(); ++buffer_at) {
                        const auto buffer = symbol(buffers[buffer_at], "VkCommandBuffer");
                        require_primary(buffer);
                        require(active.contains(buffer),
                                "submitted command buffer lacks a current qualified recording");
                        const auto& name = active.at(buffer);
                        const auto& recorded = recordings.at(name);
                        if(recorded.one_time)
                            require(one_time.emplace(buffer, generations.at(buffer)).second,
                                    "one-time recording reused");
                        const Occurrence occurrence{submission, info_at, buffer_at};
                        auto& stack = command_stacks[queue];
                        for(const auto& command : recorded.commands) {
                            const auto& call = *command.call;
                            if(command.kind == CommandKind::BeginLabel) {
                                Range range;
                                range.begin = occurrence;
                                if(stack.empty())
                                    range.labels = queues[queue];
                                else {
                                    range.labels = ranges[stack.back()].labels;
                                    ++ranges[stack.back()].children;
                                }
                                range.labels.push_back(*command.label);
                                (void)retain_path(range.labels);
                                for(const auto& label : range.labels)
                                    range_bytes += label.name.size() + label.source.dump().size() + 64;
                                require(ranges.size() < record_limit && range_bytes <= byte_limit,
                                        "source range metadata exceeds bounds");
                                ranges.push_back(std::move(range));
                                stack.push_back(ranges.size() - 1);
                            } else if(command.kind == CommandKind::EndLabel) {
                                require(!stack.empty(), "unmatched command marker in submitted queue stream");
                                ranges[stack.back()].end = occurrence;
                                stack.pop_back();
                            } else if(command.kind == CommandKind::Draw) {
                                Candidate candidate;
                                candidate.occurrence = occurrence;
                                candidate.value = {{"selection",
                                                    {{"generated_frame", 0},
                                                     {"submission_index", submission},
                                                     {"submit_info_index", info_at},
                                                     {"command_buffer_index", buffer_at},
                                                     {"queue", queue},
                                                     {"command_buffer", buffer},
                                                     {"recording", name},
                                                     {"generated_event_index", call.event}}},
                                                   {"original_capture_event_id", nullptr},
                                                   {"function", call.name},
                                                   {"parameters", command.parameters},
                                                   {"draw_source", call.source},
                                                   {"submission_source", c.source}};
                                if(stack.empty())
                                    candidate.refusal = "Draw has no complete literal command marker range";
                                else {
                                    candidate.range = stack.back();
                                    candidate.path = flat_path(ranges[stack.back()].labels);
                                }
                                for(const auto range : stack)
                                    ++ranges[range].draws;
                                if(command.parameters.at("instance_count") == 0 ||
                                   command.parameters.at(call.name == "VulkanReplay_CmdDraw" ? "vertex_count"
                                                                                             : "index_count") == 0)
                                    candidate.refusal =
                                        "Zero-sized draw participation is outside the qualified timing scope";
                                candidate_bytes += candidate.value.dump().size() + candidate.path.size();
                                require(candidates.size() < record_limit && candidate_bytes <= byte_limit,
                                        "draw occurrence metadata exceeds bounds");
                                candidates.push_back(std::move(candidate));
                            } else if(command.kind == CommandKind::State) {
                                for(const auto range : stack)
                                    ranges[range].state_commands.insert(call.name);
                            } else if(command.kind == CommandKind::Work) {
                                for(const auto range : stack)
                                    ranges[range].other_work = true;
                            }
                        }
                    }
                }
                ++submission;
            } else if(const auto found = host.find(c.name); found != host.end())
                require(a.size() == found->second, "unqualified host call arity");
            else
                throw Unqualified("unqualified frame operation prevents occurrence association: " + c.name);
        }
        for(const auto& [queue, labels] : queues) {
            (void)queue;
            require(labels.empty(), "queue marker crosses the captured frame boundary");
        }
        for(const auto& [queue, stack] : command_stacks) {
            (void)queue;
            require(stack.empty(), "command marker crosses the captured frame boundary");
        }
        result["source_paths"] = paths;
        const auto markers = correlate_profile_markers(events, regimes);
        std::map<std::string, std::size_t> event_paths;
        std::map<std::string, std::size_t> profile_paths;
        for(std::size_t row = 0; row < markers.rows.size(); ++row) {
            event_paths.emplace(markers.rows[row].flat_path, row);
            ++profile_paths[markers.rows[row].flat_path];
        }
        // A regime-only path in another interval is also evidence of a repeated
        // path. Count each interval's two exports together, rather than treating
        // their matching event/regime pair as two occurrences.
        std::map<std::string, std::size_t> regime_paths;
        for(const auto& row : regimes.rows)
            ++regime_paths[row.label];
        for(const auto& [path, count] : regime_paths)
            profile_paths[path] = std::max(profile_paths[path], count);
        result["profile_paths"] = profile_paths;
        result["profile_paths_qualified"] = markers.status == ProfileMarkerStatus::Available;
        result["profile_hierarchy"] = {{"status", markers.status == ProfileMarkerStatus::Available     ? "available"
                                                  : markers.status == ProfileMarkerStatus::Unavailable ? "unavailable"
                                                                                                       : "unsupported"},
                                       {"reason", markers.reason}};
        std::size_t result_bytes = 0;
        for(auto& candidate : candidates) {
            auto& value = candidate.value;
            value["duration_ms"] = nullptr;
            value["trace"] = nullptr;
            value["marker_path"] = Json::array();
            value["marker_sources"] = Json::array();
            value["included_state_commands"] = Json::array();
            if(candidate.range) {
                const auto& range = ranges[*candidate.range];
                for(const auto& label : range.labels) {
                    value["marker_path"].push_back(label.name);
                    value["marker_sources"].push_back(label.source);
                }
                value["included_state_commands"] = range.state_commands;
                if(candidate.refusal.empty() && (range.draws != 1 || range.children != 0 || range.other_work))
                    candidate.refusal = "Innermost marker contains multiple draws, nested ranges or other GPU work";
                if(candidate.refusal.empty() &&
                   (!range.end || range.begin != *range.end || range.begin != candidate.occurrence))
                    candidate.refusal =
                        "Draw marker boundaries span submitted command buffer occurrences; timing scope is unqualified";
            }
            if(candidate.refusal.empty() && paths.at(candidate.path) != 1)
                candidate.refusal = "Literal source path identifies multiple marker occurrences";
            if(candidate.refusal.empty() && markers.status != ProfileMarkerStatus::Available)
                candidate.refusal = markers.reason;
            if(candidate.refusal.empty() && !event_paths.contains(candidate.path))
                candidate.refusal = "No exact same-input trace path for the literal source occurrence";
            if(candidate.refusal.empty()) {
                const auto row = event_paths.at(candidate.path);
                const auto& association = markers.rows[row];
                if(association.status != ProfileMarkerStatus::Available)
                    candidate.refusal = association.reason;
                else {
                    value["duration_ms"] = *events.rows[row].values[0].value;
                    value["trace"] = {{"event_row_index", row},
                                      {"regime_row_index", *association.regime_row},
                                      {"raw_duration", events.rows[row].values[0].text}};
                }
            }
            value["status"] = candidate.refusal.empty() ? "available" : "unavailable";
            value["reason"] = candidate.refusal.empty() ? "Unique same-input literal source/event/regime marker path "
                                                          "contains one submitted draw; state packets remain included"
                                                        : candidate.refusal;
            result_bytes += value.dump().size();
            require(result_bytes <= byte_limit, "timing result exceeds bounds");
            result["draws"].push_back(std::move(value));
        }
    } catch(const Unqualified& error) {
        result["draws"] = Json::array();
        result["source_paths"] = Json::object();
        result["unsupported"].push_back({{"reason", error.what()}, {"scope", "source_occurrence_association"}});
    } catch(const std::out_of_range&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Truncated replay timing input");
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Invalid replay timing value");
    }
    return result;
}
} // namespace ngm
