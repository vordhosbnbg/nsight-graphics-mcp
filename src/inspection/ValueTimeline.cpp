#include "ValueTimeline.hpp"
#include "DescriptorState.hpp"
#include "ReplayValues.hpp"
#include "ValueDescriptors.hpp"
#include "ValueExecutionState.hpp"
#include "ValueMemory.hpp"
#include "ValueUploads.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/SpirvUniforms.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <tuple>

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
std::string output(const ReplayValue& value, const char* type) {
    require(value.text.starts_with("& "), "value timeline needs one literal output handle");
    auto id = value;
    id.text = id.text.substr(2);
    return symbol(id, type);
}
std::string fence(const ReplayValue& value) {
    return value.text == "VK_NULL_HANDLE" || value.text == "VkFence ( VK_NULL_HANDLE )" ? "" : symbol(value, "VkFence");
}
struct ModuleProof {
    Json reflection, effects, read;
};
struct Pending {
    std::string queue, device, fence, recording;
};
struct Queue {
    std::string device;
    std::uint64_t family;
};
struct Pool {
    std::string device;
    std::uint64_t family;
    bool reset;
};
struct Fence {
    std::string device;
    bool signaled, pending = false;
};
std::uint64_t literal_flags(const ReplayValue& value, const char* type,
                            const std::map<std::string, std::uint64_t>& names) {
    const auto& text = unwrap(value, type).text;
    if(!text.empty() && std::isdigit(static_cast<unsigned char>(text.front())))
        return number(text);
    std::uint64_t bits = 0;
    for(std::size_t at = 0; at < text.size();) {
        const auto end = text.find(" | ", at);
        const auto token = text.substr(at, end == std::string::npos ? end : end - at);
        require(names.contains(token), "unqualified captured command-pool or fence flags");
        bits |= names.at(token);
        if(end == std::string::npos)
            return bits;
        at = end + 3;
    }
    throw Unqualified("empty captured command-pool or fence flags");
}
void initial_helper(const ReplayCall& call) {
    const auto& a = call.arguments;
    if(call.name == "VulkanHelper_InitBegin" || call.name == "VulkanHelper_InitEnd") {
        require(a.size() == 1, "unqualified value initialization envelope");
        (void)symbol(a[0], "VkInstance");
    } else if(call.name == "VulkanHelper_InitializeSwapchain") {
        require(a.size() == 3 && number(a[2]) == 0, "unqualified initial swapchain helper");
        (void)symbol(a[0], "VkDevice");
        (void)symbol(a[1], "VkSwapchainKHR");
    } else if(call.name == "VulkanHelper_PopulateSwapchainImage") {
        require(a.size() == 10 && number(a[1]) == 0 && number(unwrap(a[3], "VulkanHelper_PopulateImageFlags")) == 0 &&
                    number(a[8]) > 0 && number(a[8]) <= 16U * 1024U * 1024U && a[9].constructor == "NV_GET_RESOURCE" &&
                    a[9].arguments.size() == 2 && a[9].arguments[0].text == "const uint32_t *" &&
                    number(a[9].arguments[1]) <= INT32_MAX,
                "unqualified initial swapchain population helper");
        (void)symbol(a[0], "VkDevice");
        (void)symbol(a[2], "VkImage");
    } else if(call.name == "VulkanHelper_InitializeQueryPool") {
        require(a.size() == 2, "unqualified initial query helper");
        (void)symbol(a[0], "VkDevice");
        (void)symbol(a[1], "VkQueryPool");
    } else if(call.name == "VulkanHelper_RegisterObject")
        require(a.size() == 3, "unqualified serialization registration helper");
    else if(call.name == "VulkanReplay_SetDebugUtilsObjectNameEXT") {
        require(a.size() == 2, "unqualified debug-name helper");
        const auto f = fields(a[1], call.visible, "VkDebugUtilsObjectNameInfoEXT", 5);
        structure(f, "VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT");
    } else
        require(call.name == "FreeCachedMemory" && a.empty(), "unmodeled initial helper prevents source values");
}
void passive_frame(const ReplayCall& call) {
    const auto& a = call.arguments;
    if(call.name == "VulkanReplay_AcquireNextImageKHR") {
        require(a.size() == 6, "unqualified acquire call");
        const auto found = call.visible.find(a[5].text);
        require(found != call.visible.end() && found->second.type == "uint32_t" && found->second.elements.size() == 1 &&
                    found->second.end < call.position,
                "acquire output must be a prior local uint32 object, never a mapped buffer pointer");
    } else if(call.name == "VulkanReplay_GetQueryPoolResults")
        require(a.size() == 8 && a[5].constructor == "timing_host_byte_vector_data" && a[5].arguments.size() == 1,
                "host query output must be the parser-qualified owned byte vector");
    else if(call.name == "VulkanReplay_QueuePresentKHR") {
        require(a.size() == 2, "unqualified present call");
        const auto f = fields(a[1], call.visible, "VkPresentInfoKHR", 8);
        require(f[0].text == "VK_STRUCTURE_TYPE_PRESENT_INFO_KHR" && f[7].text == "nullptr",
                "present results must not write an unmodeled host or mapped buffer pointer");
        if(f[1].text != "nullptr") {
            const auto next = fields(f[1], call.visible, "VkSwapchainPresentFenceInfoKHR", 4);
            structure(next, "VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR");
            require(number(next[2]) == number(f[4]), "present fence chain/count is unqualified");
        }
    } else if(call.name == "VulkanReplay_SetDebugUtilsObjectNameEXT")
        initial_helper(call);
    else
        throw Unqualified("unmodeled host or GPU effect prevents selected source values");
}
bool infinite_all(const ReplayCall& call, const Pending& pending) {
    if(call.name != "VulkanReplay_WaitForFences")
        return false;
    const auto& a = call.arguments;
    require(a.size() == 5, "unqualified value fence wait");
    if(symbol(a[0], "VkDevice") != pending.device || number(a[4]) != UINT64_MAX ||
       (a[3].text != "VK_TRUE" && a[3].text != "1u" && a[3].text != "1"))
        return false;
    const auto fences = array(a[2], call.visible, "VkFence", number(a[1]));
    return std::any_of(fences.begin(), fences.end(), [&](const auto& f) { return fence(f) == pending.fence; });
}
void barrier(const ReplayCall& call, const ReplayRecording& recording) {
    const auto& a = call.arguments;
    require(a.size() == 10, "unqualified value barrier arity");
    for(const auto [count_at, pointer_at, fields_count, type, tag] :
        {std::tuple{4u, 5u, 4u, "VkMemoryBarrier", "VK_STRUCTURE_TYPE_MEMORY_BARRIER"},
         std::tuple{6u, 7u, 9u, "VkBufferMemoryBarrier", "VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER"},
         std::tuple{8u, 9u, 10u, "VkImageMemoryBarrier", "VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER"}}) {
        const auto count = number(a[count_at]);
        require(count <= 64 && (count || a[pointer_at].text == "nullptr"),
                "value barrier count/pointer exceeds bounds");
        if(!count)
            continue;
        for(const auto& value : array(a[pointer_at], recording.values, type, count)) {
            require(value.constructor == type && value.arguments.size() == fields_count,
                    "value barrier lacks its literal structure");
            const auto& f = value.arguments;
            structure(f, tag);
            if(count_at != 4) {
                const auto index = count_at == 6 ? 4 : 6;
                const auto ignored = [](const auto& field) {
                    return field.text == "VK_QUEUE_FAMILY_IGNORED" || field.text == "4294967295u" ||
                           field.text == "4294967295";
                };
                require(ignored(f[index]) && ignored(f[index + 1]),
                        "queue-family ownership transfers need separate value visibility proof");
            }
        }
    }
}
} // namespace

Json value_execution_candidates(const CppReplay& replay) {
    Json result{{"executions", Json::array()},
                {"unsupported", replay.unsupported},
                {"selection_domain", "generated_cpp_captured_submission_candidates"},
                {"captured_gpu_execution_correspondence", false}};
    try {
        std::map<std::string, Json> recordings;
        std::map<std::string, std::string> active;
        std::size_t submission = 0, bytes = 0;
        for(const auto& call : replay.frame) {
            const auto& a = call.arguments;
            if(call.name == "VulkanHelper_RecordCommandBuffer") {
                require(a.size() == 4 && number(a[2]) > 0 && number(a[2]) <= UINT32_MAX &&
                            a[3].text.starts_with("& Record_"),
                        "candidate recording invocation is unqualified");
                const auto buffer = symbol(a[1], "VkCommandBuffer"), name = a[3].text.substr(2);
                require(name == "Record_" + buffer + "_r" + std::to_string(number(a[2])) &&
                            replay.recordings.contains(name),
                        "candidate recording/callback identity is absent or mismatched");
                if(!recordings.contains(name)) {
                    try {
                        recordings[name] = value_recording_draws(replay, replay.recordings.at(name));
                    } catch(const Unqualified& error) {
                        recordings[name] = nullptr;
                        result["unsupported"].push_back(
                            {{"recording", name}, {"source", call.source}, {"reason", error.what()}});
                    }
                }
                active[buffer] = name;
            } else if(call.name == "VulkanReplay_ResetCommandBuffer") {
                require(a.size() == 2, "candidate command-buffer reset arity is unqualified");
                active.erase(symbol(a[0], "VkCommandBuffer"));
            } else if(call.name == "VulkanReplay_QueueSubmit") {
                require(a.size() == 4 && number(a[1]) > 0 && number(a[1]) <= 16,
                        "candidate submission count/arity is unqualified");
                const auto queue = symbol(a[0], "VkQueue");
                const auto submits = array(a[2], call.visible, "VkSubmitInfo", number(a[1]));
                for(std::size_t si = 0; si < submits.size(); ++si) {
                    require(submits[si].constructor == "VkSubmitInfo" && submits[si].arguments.size() == 9,
                            "candidate submit info is unqualified");
                    const auto& f = submits[si].arguments;
                    structure(f, "VK_STRUCTURE_TYPE_SUBMIT_INFO");
                    require(number(f[5]) > 0 && number(f[5]) <= 64, "candidate command-buffer count exceeds bounds");
                    const auto buffers = array(f[6], call.visible, "VkCommandBuffer", number(f[5]));
                    for(std::size_t bi = 0; bi < buffers.size(); ++bi) {
                        const auto buffer = symbol(buffers[bi], "VkCommandBuffer");
                        require(active.contains(buffer), "candidate submission has no captured-frame recording");
                        const auto& name = active.at(buffer);
                        if(recordings.at(name).is_null())
                            continue;
                        for(const auto& draw : recordings.at(name)) {
                            Json row{{"selection",
                                      {{"frame", 0},
                                       {"submission_index", submission},
                                       {"submit_info_index", si},
                                       {"command_buffer_index", bi},
                                       {"command_buffer", buffer},
                                       {"recording", name},
                                       {"event_index", draw.at("event_index")},
                                       {"queue", queue}}},
                                     {"draw_source", draw.at("source")},
                                     {"submission_source", call.source},
                                     {"labels", draw.at("labels")},
                                     {"function", draw.at("function")},
                                     {"arguments", draw.at("arguments")},
                                     {"value_status", "not_read"}};
                            bytes += row.dump().size();
                            require(result.at("executions").size() < 256 && bytes <= 16U * 1024U * 1024U,
                                    "candidate execution count/bytes exceed bounds");
                            result["executions"].push_back(std::move(row));
                        }
                    }
                }
                ++submission;
            }
        }
    } catch(const Unqualified& error) {
        result["executions"] = Json::array();
        result["unsupported"].push_back({{"scope", "candidate_submission_association"}, {"reason", error.what()}});
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Malformed value-candidate source fields");
    }
    return result;
}

Json value_draw_plans(const CppReplay& replay, const std::string& capture_id, const Json& captured_inputs,
                      const std::vector<Json>& reads, const std::function<ValueShader(const Json&)>& load_shader,
                      std::chrono::steady_clock::time_point deadline) {
    Json result{{"status", "unavailable"},
                {"reason", nullptr},
                {"executions", Json::array()},
                {"captured_gpu_execution_correspondence", false}};
    try {
        const auto checkpoint = [&] {
            require(std::chrono::steady_clock::now() < deadline, "value composition exceeded its shared deadline");
        };
        checkpoint();
        require(replay.unsupported.empty(), "value source contains unsupported creations or recordings");
        require(reads.size() <= 64 && replay.frame.size() <= 10000, "value input counts exceed bounds");
        require(!capture_id.empty() && captured_inputs.is_object() && captured_inputs.size() == 2 &&
                    captured_inputs.contains("data.bin") && captured_inputs.contains("data.bin.rec"),
                "value composition requires the captured database identities");
        const auto same_inputs = [&](const Json& read) {
            require(read.at("capture_id") == capture_id && read.at("inputs") == captured_inputs,
                    "value evidence belongs to a different capture or database snapshot");
        };
        for(const auto& read : reads)
            same_inputs(read);
        const auto memory = inspect_value_memory_bindings(replay);
        ValueUploads uploads(memory);
        ValueDescriptors descriptors(value_descriptor_allocations(replay));
        checkpoint();
        std::map<std::string, Queue> queues;
        for(const auto& call : replay.resource_queues) {
            const auto& a = call.arguments;
            require(call.name == "VulkanReplay_GetDeviceQueue" && a.size() == 4 && number(a[1]) <= UINT32_MAX &&
                        number(a[2]) <= UINT32_MAX &&
                        queues.emplace(output(a[3], "VkQueue"), Queue{symbol(a[0], "VkDevice"), number(a[1])}).second,
                    "queue/device source identity is absent, duplicated or unqualified");
        }
        require(!queues.empty(), "value timeline has no captured queue/device relationship");
        std::map<std::string, Pool> pools;
        std::map<std::string, Fence> fences;
        for(const auto& [id, object] : replay.objects) {
            checkpoint();
            const auto& call = object.creation;
            const auto& a = call.arguments;
            if(call.name == "VulkanReplay_CreateCommandPool") {
                require(a.size() == 4 && a[2].text == "nullptr" && output(a[3], "VkCommandPool") == id,
                        "unqualified command-pool creation identity");
                const auto f = fields(a[1], object.values, "VkCommandPoolCreateInfo", 4);
                structure(f, "VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO");
                const auto flags = literal_flags(f[2], "VkCommandPoolCreateFlags",
                                                 {{"VK_COMMAND_POOL_CREATE_TRANSIENT_BIT", 1},
                                                  {"VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT", 2}});
                require(flags <= 3 && number(f[3]) <= UINT32_MAX, "protected or unknown command-pool flags/family");
                pools.emplace(id, Pool{symbol(a[0], "VkDevice"), number(f[3]), (flags & 2) != 0});
            } else if(call.name == "VulkanReplay_CreateFence") {
                require(a.size() == 4 && a[2].text == "nullptr" && output(a[3], "VkFence") == id,
                        "unqualified fence creation identity");
                const auto f = fields(a[1], object.values, "VkFenceCreateInfo", 3);
                structure(f, "VK_STRUCTURE_TYPE_FENCE_CREATE_INFO");
                const auto flags = literal_flags(f[2], "VkFenceCreateFlags", {{"VK_FENCE_CREATE_SIGNALED_BIT", 1}});
                require(flags <= 1, "unknown fence creation flags");
                fences.emplace(id, Fence{symbol(a[0], "VkDevice"), flags != 0});
            }
        }
        const auto buffer_pool = [&](const std::string& id) -> const Pool& {
            const auto found = replay.objects.find(id);
            require(found != replay.objects.end() &&
                        found->second.creation.name == "VulkanReplay_AllocateCommandBuffers",
                    "command buffer has no unique captured allocation");
            const auto& object = found->second;
            require(object.creation.arguments.size() == 3, "unqualified command-buffer allocation");
            const auto f = fields(object.creation.arguments[1], object.values, "VkCommandBufferAllocateInfo", 5);
            structure(f, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
            const auto pool = symbol(f[2], "VkCommandPool");
            require(pools.contains(pool) && pools.at(pool).device == object.creation.arguments[0].text,
                    "command-buffer pool/device lifetime is unavailable");
            return pools.at(pool);
        };
        const auto update = [&](const ReplayCall& call) {
            const auto& a = call.arguments;
            require(a.size() == 6, "unqualified descriptor history call");
            if(number(a[1]) == 0) {
                descriptors.update(call, Json::array());
                return;
            }
            require(number(a[4]) == 0, "mixed serialized writes and literal copies need separate reader qualification");
            const Json* found = nullptr;
            for(const auto& read : reads)
                if(read.at("reference").at("source") == call.source) {
                    require(!found, "multiple descriptor reads claim one source effect");
                    found = &read;
                }
            require(found, "source descriptor write lacks its authenticated parent read");
            const auto& ref = found->at("reference");
            require(a[2].constructor == "Serialization::DATABASE_HANDLE" && a[2].arguments.size() == 1 &&
                        ref.at("handle") == number(a[2].arguments[0]) && ref.at("write_count") == number(a[1]) &&
                        ref.at("output_bytes") == number(a[3]) && ref.at("copy_count") == 0,
                    "descriptor read differs from its source handle/count/extent");
            descriptors.update(call, found->at("writes"),
                               {{"artifact_id", found->at("artifact_id")},
                                {"update_ref", found->at("update_ref")},
                                {"worker_sha256", found->at("worker_sha256")},
                                {"inputs", found->at("inputs")}});
        };
        const std::set<std::string> upload_calls{
            "VulkanReplay_MapMemory",      "VulkanReplay_UnmapMemory",   "VulkanHelper_UpdateMappedMemory2",
            "VulkanReplay_WaitForFences",  "VulkanReplay_ResetFences",   "VulkanReplay_QueueWaitIdle",
            "VulkanReplay_DeviceWaitIdle", "VulkanReplay_DestroyBuffer", "VulkanReplay_FreeMemory"};
        // These helper effects are safe only under the composing parent's exact
        // exported-source fingerprint contract. They target separate swapchain/
        // query state or serialization bookkeeping, never captured buffer bytes.
        require(replay.initial_state.size() >= 2 && replay.initial_state.front().name == "VulkanHelper_InitBegin" &&
                    std::count_if(replay.initial_state.begin(), replay.initial_state.end(),
                                  [](const auto& c) { return c.name == "VulkanHelper_InitEnd"; }) == 1,
                "value initialization requires the qualified helper begin/end envelope");
        bool ended = false;
        for(const auto& call : replay.initial_state) {
            checkpoint();
            require(!ended || call.name == "FreeCachedMemory", "resource effects follow the initialization completion");
            if(call.name == "VulkanHelper_InitEnd") {
                require(call.arguments.size() == 1 &&
                            call.arguments[0].text == replay.initial_state.front().arguments.at(0).text,
                        "value initialization begin/end instances differ");
                ended = true;
            }
            if(call.name == "VulkanHelper_UpdateDescriptorSetsFromDatabase")
                update(call);
            else if(call.name == "VulkanHelper_PopulateBuffer3" || upload_calls.contains(call.name))
                uploads.host(call, true);
            else
                initial_helper(call);
        }
        std::map<std::string, Json> recorded;
        std::map<std::string, std::set<std::string>> bound_sets;
        std::size_t recorded_bytes = 0, module_bytes = 0, result_bytes = 0;
        const auto recording = [&](const std::string& name) -> const Json& {
            checkpoint();
            require(replay.recordings.contains(name), "submitted value recording is absent");
            if(!recorded.contains(name)) {
                const auto& definition = replay.recordings.at(name);
                auto rows = value_recording_draws(replay, definition);
                recorded_bytes += rows.dump().size();
                require(recorded_bytes <= 16U * 1024U * 1024U, "aggregate value recording state exceeds bounds");
                for(const auto& call : definition.calls) {
                    if(call.name == "VulkanReplay_CmdPipelineBarrier")
                        barrier(call, definition);
                    if(call.name == "VulkanReplay_CmdBindDescriptorSets") {
                        const auto& a = call.arguments;
                        require(a.size() == 8 && number(a[4]) <= 8, "unqualified value descriptor bind");
                        for(const auto& set : array(a[5], definition.values, "VkDescriptorSet", number(a[4])))
                            bound_sets[name].insert(symbol(set, "VkDescriptorSet"));
                    }
                }
                recorded.emplace(name, std::move(rows));
            }
            return recorded.at(name);
        };
        std::map<std::pair<std::string, std::string>, ModuleProof> modules;
        const auto module = [&](const Json& stage) -> const ModuleProof& {
            checkpoint();
            const auto kind = stage.at("stage").get<std::string>();
            require(kind == "VK_SHADER_STAGE_VERTEX_BIT" || kind == "VK_SHADER_STAGE_FRAGMENT_BIT",
                    "only vertex/fragment value shader effects are qualified");
            const auto entry = Json::parse(stage.at("entry_point_expression").get<std::string>()).get<std::string>();
            const auto key = std::pair{stage.at("module").get<std::string>(), kind + ":" + entry};
            if(!modules.contains(key)) {
                require(modules.size() < 32, "value shader count exceeds bounds");
                auto data = load_shader(stage);
                checkpoint();
                const auto& read = data.read;
                same_inputs(read);
                const auto& ref = read.at("reference");
                require(data.bytes.size() <= 1024U * 1024U && data.bytes.size() == stage.at("declared_bytes") &&
                            read.at("offset") == 0 && read.at("returned_bytes") == data.bytes.size() &&
                            read.at("total_bytes") == data.bytes.size() && read.at("sha256") == sha256(data.bytes) &&
                            ref.at("handle") == stage.at("resource_handle") &&
                            ref.at("expected_bytes") == stage.at("declared_bytes") &&
                            ref.at("source").at("path") == stage.at("module_source").at("path") &&
                            ref.at("source").at("sha256") == stage.at("module_source").at("sha256"),
                        "value shader bytes differ from their exact source module/resource relationship");
                const auto at = ref.at("source").at("byte_offset").get<std::uint64_t>();
                const auto first = stage.at("module_source").at("byte_offset").get<std::uint64_t>();
                const auto bytes = stage.at("module_source").at("byte_count").get<std::uint64_t>();
                require(at >= first && at - first < bytes &&
                            ref.at("source").at("byte_count").get<std::uint64_t>() <= bytes - (at - first),
                        "value shader accessor is outside the selected module creation");
                module_bytes += data.bytes.size();
                require(module_bytes <= 8U * 1024U * 1024U, "aggregate value shader bytes exceed bounds");
                const auto model = kind == "VK_SHADER_STAGE_VERTEX_BIT" ? 0u : 4u;
                auto reflection = inspect_spirv_uniform_reads(data.bytes, entry, model);
                auto effects = inspect_spirv_buffer_effects(data.bytes, entry, model);
                checkpoint();
                require(reflection.at("status") == "qualified_static_uniform_loads" &&
                            effects.at("status") == "qualified_no_external_buffer_writes",
                        "submitted shader selection/layout or external buffer effects are unqualified");
                data.read.erase("data");
                modules.emplace(key, ModuleProof{std::move(reflection), std::move(effects), std::move(data.read)});
            }
            return modules.at(key);
        };
        std::map<std::string, std::string> active;
        std::map<std::string, std::size_t> generations;
        std::set<std::pair<std::string, std::size_t>> one_time;
        std::vector<Pending> pending;
        std::map<std::string, Json> labels;
        std::size_t submission = 0;
        for(const auto& call : replay.frame) {
            checkpoint();
            const auto& a = call.arguments;
            if(upload_calls.contains(call.name)) {
                if(call.name == "VulkanReplay_ResetFences" || call.name == "VulkanReplay_WaitForFences") {
                    require(a.size() == (call.name == "VulkanReplay_ResetFences" ? 3u : 5u) && number(a[1]) > 0 &&
                                number(a[1]) <= 64,
                            "unqualified captured fence wait/reset count");
                    for(const auto& f : array(a[2], call.visible, "VkFence", number(a[1]))) {
                        const auto id = symbol(f, "VkFence");
                        require(fences.contains(id) && fences.at(id).device == symbol(a[0], "VkDevice"),
                                "fence wait/reset lacks its captured device identity");
                        if(call.name == "VulkanReplay_ResetFences") {
                            require(!fences.at(id).pending, "fence reset lacks proven completion");
                            fences.at(id).signaled = false;
                        }
                    }
                }
                uploads.host(call);
                for(auto at = pending.begin(); at != pending.end();) {
                    const bool done =
                        (call.name == "VulkanReplay_QueueWaitIdle" && symbol(a[0], "VkQueue") == at->queue) ||
                        (call.name == "VulkanReplay_DeviceWaitIdle" && symbol(a[0], "VkDevice") == at->device) ||
                        infinite_all(call, *at);
                    if(done) {
                        if(!at->fence.empty()) {
                            fences.at(at->fence).pending = false;
                            fences.at(at->fence).signaled = true;
                        }
                        descriptors.completed(at->recording);
                        at = pending.erase(at);
                    } else
                        ++at;
                }
            } else if(call.name == "VulkanHelper_UpdateDescriptorSetsFromDatabase")
                update(call);
            else if(call.name == "VulkanReplay_FreeDescriptorSets" || call.name == "VulkanReplay_ResetDescriptorPool" ||
                    call.name == "VulkanReplay_DestroyDescriptorPool")
                descriptors.host(call);
            else if(call.name == "VulkanHelper_RecordCommandBuffer") {
                require(a.size() == 4 && number(a[2]) > 0 && number(a[2]) <= UINT32_MAX &&
                            a[3].text.starts_with("& Record_"),
                        "unqualified value command-buffer recording invocation");
                const auto buffer = symbol(a[1], "VkCommandBuffer"), name = a[3].text.substr(2);
                require(name == "Record_" + buffer + "_r" + std::to_string(number(a[2])),
                        "value recording/callback identity mismatch");
                const auto found = replay.objects.find(buffer);
                require(found != replay.objects.end() &&
                            found->second.creation.name == "VulkanReplay_AllocateCommandBuffers" &&
                            found->second.creation.arguments.at(0).text == a[0].text,
                        "value recording differs from its captured command-buffer device");
                if(active.contains(buffer))
                    require(buffer_pool(buffer).reset, "implicit recording reset lacks command-pool reset permission");
                if(active.contains(buffer))
                    descriptors.forget(active.at(buffer));
                (void)buffer_pool(buffer);
                (void)recording(name);
                descriptors.recorded(name, bound_sets[name]);
                active[buffer] = name;
                ++generations[buffer];
            } else if(call.name == "VulkanReplay_ResetCommandBuffer") {
                require(a.size() == 2 && number(unwrap(a[1], "VkCommandBufferResetFlags")) == 0,
                        "unqualified value command-buffer reset");
                const auto buffer = symbol(a[0], "VkCommandBuffer");
                require(buffer_pool(buffer).reset, "command-buffer reset lacks captured pool reset permission");
                if(active.contains(buffer)) {
                    descriptors.forget(active.at(buffer));
                    active.erase(buffer);
                }
            } else if(call.name == "VulkanReplay_QueueBeginDebugUtilsLabelEXT") {
                require(a.size() == 2, "unqualified value queue label");
                const auto queue = symbol(a[0], "VkQueue");
                const auto f = fields(a[1], call.visible, "VkDebugUtilsLabelEXT", 4);
                structure(f, "VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT");
                require(queues.contains(queue) && f[2].text.size() <= 258 && labels[queue].size() < 16,
                        "value queue label identity/size/depth exceeds bounds");
                if(labels[queue].is_null())
                    labels[queue] = Json::array();
                labels[queue].push_back({{"name", Json::parse(f[2].text)}, {"source", call.source}});
            } else if(call.name == "VulkanReplay_QueueEndDebugUtilsLabelEXT") {
                require(a.size() == 1, "unqualified value queue label end");
                auto& stack = labels[symbol(a[0], "VkQueue")];
                require(stack.is_array() && !stack.empty(), "value queue label end lacks its begin");
                stack.erase(stack.size() - 1);
            } else if(call.name == "VulkanReplay_QueueSubmit") {
                require(a.size() == 4 && number(a[1]) > 0 && number(a[1]) <= 16,
                        "unqualified value submit count/arity");
                const auto queue = symbol(a[0], "VkQueue");
                require(queues.contains(queue), "submitted queue lacks its captured device identity");
                const auto device = queues.at(queue).device, submitted_fence = fence(a[3]);
                if(!submitted_fence.empty())
                    require(fences.contains(submitted_fence) && fences.at(submitted_fence).device == device &&
                                !fences.at(submitted_fence).signaled && !fences.at(submitted_fence).pending,
                            "submission fence lacks a live, reset, same-device identity");
                const auto submits = array(a[2], call.visible, "VkSubmitInfo", number(a[1]));
                for(std::size_t si = 0; si < submits.size(); ++si) {
                    const auto& info = submits[si];
                    require(info.constructor == "VkSubmitInfo" && info.arguments.size() == 9,
                            "unqualified value submit info");
                    const auto& f = info.arguments;
                    structure(f, "VK_STRUCTURE_TYPE_SUBMIT_INFO");
                    require(number(f[5]) > 0 && number(f[5]) <= 64, "value submit buffer count exceeds bounds");
                    const auto buffers = array(f[6], call.visible, "VkCommandBuffer", number(f[5]));
                    for(std::size_t bi = 0; bi < buffers.size(); ++bi) {
                        const auto buffer = symbol(buffers[bi], "VkCommandBuffer");
                        require(active.contains(buffer),
                                "submitted value command buffer has no captured-frame recording");
                        const auto& name = active.at(buffer);
                        const auto& definition = replay.recordings.at(name);
                        const auto& allocation = replay.objects.at(buffer);
                        require(allocation.creation.arguments.at(0).text == device,
                                "submitted command buffer and queue belong to different devices");
                        require(buffer_pool(buffer).family == queues.at(queue).family,
                                "command-buffer pool and submitted queue families differ");
                        const auto alloc = fields(allocation.creation.arguments.at(1), allocation.values,
                                                  "VkCommandBufferAllocateInfo", 5);
                        structure(alloc, "VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO");
                        require(alloc[3].text == "VK_COMMAND_BUFFER_LEVEL_PRIMARY" && number(alloc[4]) == 1,
                                "secondary or batch value command-buffer allocation is unqualified");
                        const auto begin = fields(definition.calls.front().arguments.at(1), definition.values,
                                                  "VkCommandBufferBeginInfo", 4);
                        if(unwrap(begin[2], "VkCommandBufferUsageFlags").text ==
                           "VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT")
                            require(one_time.emplace(buffer, generations[buffer]).second,
                                    "one-time value recording was submitted twice without rerecording");
                        descriptors.require_recording(name);
                        std::set<std::string> used;
                        for(const auto& command : definition.calls)
                            if(command.name == "VulkanReplay_CmdCopyImageToBuffer") {
                                const auto target = symbol(command.arguments.at(3), "VkBuffer");
                                uploads.invalidate(target, "GPU image-to-buffer copy writes this captured buffer");
                                used.insert(target);
                            }
                        for(const auto& draw : recording(name)) {
                            checkpoint();
                            require(draw.at("state").at("pipeline").at("status") == "qualified_recorded_state" &&
                                        draw.at("state").at("shaders").at("status") == "qualified_recorded_state",
                                    "submitted draw lacks a qualified pipeline/shader relationship");
                            const auto& stages = draw.at("state").at("shaders").at("value");
                            std::set<std::string> kinds;
                            for(const auto& stage : stages)
                                kinds.insert(stage.at("stage"));
                            require(stages.size() == 2 &&
                                        kinds == std::set<std::string>{"VK_SHADER_STAGE_VERTEX_BIT",
                                                                       "VK_SHADER_STAGE_FRAGMENT_BIT"},
                                    "value graphics profile requires exactly vertex and fragment shaders");
                            const auto& bindings = draw.at("state").at("descriptors");
                            Json plans = Json::array();
                            std::size_t plan_bytes = 0;
                            for(const auto& stage : stages) {
                                const auto& proof = module(stage);
                                for(const auto& read : proof.reflection.at("reads")) {
                                    require(bindings.at("status") == "qualified_recorded_state",
                                            "uniform load lacks complete compatible descriptor bindings");
                                    const Json* bank = nullptr;
                                    for(const auto& set : bindings.at("value"))
                                        if(set.at("set") == read.at("descriptor_set"))
                                            bank = &set;
                                    require(bank, "uniform set is absent from the selected pipeline binding bank");
                                    auto descriptor =
                                        descriptors.element(bank->at("descriptor_set"), read.at("binding"), 0);
                                    require(descriptor.at("status") == "qualified_source_descriptor",
                                            "uniform descriptor write/copy lifetime is unqualified");
                                    const auto& value = descriptor.at("value");
                                    const auto type = value.at("descriptor_type").get<std::string>();
                                    require((type == "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER" ||
                                             type == "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC") &&
                                                !value.at("buffer").is_null() &&
                                                descriptor.at("allocation").at("device") == device &&
                                                (descriptor.at("binding").at("stage_mask").get<unsigned>() &
                                                 (stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? 1u : 16u)),
                                            "uniform descriptor type, object, device or shader-stage visibility is "
                                            "unqualified");
                                    const auto target = value.at("buffer").at("symbol").get<std::string>();
                                    require(memory.at("buffers").contains(target),
                                            "uniform buffer lacks captured memory identity");
                                    const auto& binding = memory.at("buffers").at(target);
                                    const auto usage = binding.at("usage_flags").get<std::uint64_t>();
                                    require(binding.at("device") == device && (usage & 16) &&
                                                (usage & ~std::uint64_t{17}) == 0,
                                            "uniform buffer requires this device and the qualified GPU-read-only usage "
                                            "profile");
                                    const auto size = binding.at("buffer_bytes").get<std::uint64_t>();
                                    const auto offset = value.at("offset").get<std::uint64_t>();
                                    std::uint64_t dynamic = 0;
                                    if(type == "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC") {
                                        const Json* selected = nullptr;
                                        for(const auto& d : bank->at("dynamic_offsets"))
                                            if(d.at("binding") == read.at("binding") && d.at("array_element") == 0)
                                                selected = &d;
                                        require(selected, "uniform dynamic offset is absent");
                                        dynamic = selected->at("offset");
                                    }
                                    auto range = value.at("range").get<std::uint64_t>();
                                    require(offset < size && dynamic <= size - offset,
                                            "uniform effective buffer offset exceeds bounds");
                                    if(range == UINT64_MAX) {
                                        require(dynamic == 0,
                                                "whole-size uniform descriptor has a nonzero dynamic offset");
                                        range = size - offset;
                                    }
                                    const auto prefix = read.at("offset").get<std::uint64_t>() +
                                                        read.at("layout").at("bytes").get<std::uint64_t>();
                                    require(range <= size - offset - dynamic && prefix > 0 && prefix <= range &&
                                                prefix <= 65536,
                                            "typed uniform load escapes its effective descriptor range");
                                    auto uploaded = uploads.range(target, offset + dynamic, prefix);
                                    require(uploaded.at("status") == "qualified_source_upload_range",
                                            "typed uniform load lacks complete coherent latest-upload coverage");
                                    used.insert(target);
                                    Json plan{{"shader", stage},
                                              {"shader_read", proof.read},
                                              {"shader_effects", proof.effects},
                                              {"uniform_read", read},
                                              {"descriptor_binding", *bank},
                                              {"descriptor", std::move(descriptor)},
                                              {"buffer", target},
                                              {"effective_offset", offset + dynamic},
                                              {"effective_range", range},
                                              {"prefix_bytes", prefix},
                                              {"upload", std::move(uploaded)}};
                                    plan_bytes += plan.dump().size();
                                    require(plan_bytes <= 8U * 1024U * 1024U &&
                                                result_bytes + plan_bytes <= 16U * 1024U * 1024U,
                                            "draw value-plan provenance exceeds its byte budget");
                                    plans.push_back(std::move(plan));
                                    require(plans.size() <= 128, "draw uniform-load plans exceed bounds");
                                }
                            }
                            Json row{{"selection",
                                      {{"frame", 0},
                                       {"submission_index", submission},
                                       {"submit_info_index", si},
                                       {"command_buffer_index", bi},
                                       {"command_buffer", buffer},
                                       {"recording", name},
                                       {"event_index", draw.at("event_index")},
                                       {"queue", queue}}},
                                     {"draw_source", draw.at("source")},
                                     {"submission_source", call.source},
                                     {"queue_labels", labels.contains(queue) ? labels.at(queue) : Json::array()},
                                     {"labels", draw.at("labels")},
                                     {"function", draw.at("function")},
                                     {"arguments", draw.at("arguments")},
                                     {"plans", std::move(plans)}};
                            result_bytes += row.dump().size();
                            require(result.at("executions").size() < 256 && result_bytes <= 16U * 1024U * 1024U,
                                    "value timeline result exceeds its count/byte bound");
                            result["executions"].push_back(std::move(row));
                        }
                        descriptors.submitted(name);
                        uploads.submit(queue, device, submitted_fence, used, call.source);
                        pending.push_back({queue, device, submitted_fence, name});
                        require(pending.size() <= 4096, "value pending recording count exceeds bounds");
                    }
                }
                if(!submitted_fence.empty())
                    fences.at(submitted_fence).pending = true;
                ++submission;
            } else
                passive_frame(call);
        }
        for(const auto& [queue, stack] : labels) {
            (void)queue;
            require(stack.empty(), "unclosed value queue-label scope");
        }
        require(pending.empty(), "captured frame ends before completion of selected buffer/descriptor uses is proven");
        checkpoint();
        result["status"] = "qualified_source_draw_value_plans";
    } catch(const Unqualified& error) {
        result["executions"] = Json::array();
        result["reason"] = error.what();
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Malformed value-plan evidence or source fields");
    } catch(const std::out_of_range&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Unresolved value-plan source identity");
    }
    return result;
}
} // namespace ngm::detail
