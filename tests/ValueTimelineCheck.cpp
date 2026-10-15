#include "Check.hpp"
#include "CppReplay.hpp"
#include "DrawStateFixture.hpp"
#include "ReplayValues.hpp"
#include "ValueExecutionState.hpp"
#include "ValueTimeline.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"

#include <algorithm>
#include <functional>
#include <tuple>

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using namespace ngm::detail;
using namespace ngm::check::state;
// Hand-written partial source grammar and fake authenticated-reader receipts.
// These are ordinary CPU composition controls, never producer/GPU qualification.
const std::string definitions = R"cpp(
void CreateResourcesValues() {
 {
 NV_VK_REPLAY(VulkanHelper_GetCompatiblePhysicalDevice(VkInstance_uid_1,0u,VkPhysicalDeviceProperties{0u},
 1u,NV_TO_ARRAY(VkQueueFamilyProperties{VkQueueFlags(VK_QUEUE_GRAPHICS_BIT),1u,64u,VkExtent3D{1u,1u,1u}}),
 VkPhysicalDeviceMemoryProperties{1u,NV_TO_ARRAY(VkMemoryType{VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),0u}),
 1u,NV_TO_ARRAY(VkMemoryHeap{4096ull,VkMemoryHeapFlags(0)})},&VkPhysicalDevice_uid_2));
 }
 { NV_VK_REPLAY(VulkanReplay_CreateDevice(VkPhysicalDevice_uid_2,VkDeviceCreateInfo_info,nullptr,&VkDevice_uid_4)); }
 { NV_VK_REPLAY(VulkanReplay_GetDeviceQueue(VkDevice_uid_4,0u,0u,&VkQueue_uid_5)); }
 {
 static VkCommandPoolCreateInfo VkCommandPoolCreateInfo_info[1] = {VkCommandPoolCreateInfo{
 VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,nullptr,VkCommandPoolCreateFlags(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),0u}};
 NV_VK_REPLAY(VulkanReplay_CreateCommandPool(VkDevice_uid_4,VkCommandPoolCreateInfo_info,nullptr,&VkCommandPool_uid_41));
 }
 {
 static VkFenceCreateInfo VkFenceCreateInfo_info[1] = {VkFenceCreateInfo{
 VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,nullptr,VkFenceCreateFlags(VK_FENCE_CREATE_SIGNALED_BIT)}};
 NV_VK_REPLAY(VulkanReplay_CreateFence(VkDevice_uid_4,VkFenceCreateInfo_info,nullptr,&VkFence_uid_43));
 }
 {
 static VkBufferCreateInfo VkBufferCreateInfo_info[1] = {VkBufferCreateInfo{
 VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,nullptr,VkBufferCreateFlags(0),16ull,
 VkBufferUsageFlags(16),VK_SHARING_MODE_EXCLUSIVE,0u,nullptr}};
 static VkMemoryRequirements VkMemoryRequirements_info[1] = {VkMemoryRequirements{64ull,64ull,1u}};
 NV_VK_REPLAY(VulkanHelper_CreateBuffer3(VkDevice_uid_4,VkBufferCreateInfo_info,VkMemoryRequirements_info,
 VkDeviceAddress(0),VkDeviceAddress(0),0ull,nullptr,&VkBuffer_uid_7));
 NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_BUFFER,uint64_t(VkBuffer_uid_7),70ull));
 }
 {
 static VkMemoryAllocateInfo VkMemoryAllocateInfo_info[1] = {VkMemoryAllocateInfo{
 VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,nullptr,64ull,0u}};
 static VkBindBufferMemoryInfo VkBindBufferMemoryInfo_info[1] = {VkBindBufferMemoryInfo{
 VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,nullptr,VkBuffer_uid_7,VkDeviceMemory_uid_8,0ull}};
 static VulkanHelper_InitializeDeviceMemoryInfo VulkanHelper_InitializeDeviceMemoryInfo_info[1] = {
 VulkanHelper_InitializeDeviceMemoryInfo{2u,0ull,1u,VkBindBufferMemoryInfo_info,0u,nullptr,0u,nullptr,0u,nullptr,nullptr,VK_FALSE}};
 NV_VK_REPLAY(VulkanHelper_AllocateAndInitializeMemory3(VkDevice_uid_4,VkMemoryAllocateInfo_info,
 VulkanHelper_InitializeDeviceMemoryInfo_info,&VkDeviceMemory_uid_8,nullptr));
 }
 {
 static VkDescriptorPoolSize VkDescriptorPoolSize_info[1] = {VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1u}};
 static VkDescriptorPoolCreateInfo VkDescriptorPoolCreateInfo_info[1] = {VkDescriptorPoolCreateInfo{
 VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,nullptr,VkDescriptorPoolCreateFlags(0),1u,1u,VkDescriptorPoolSize_info}};
 NV_VK_REPLAY(VulkanReplay_CreateDescriptorPool(VkDevice_uid_4,VkDescriptorPoolCreateInfo_info,nullptr,&VkDescriptorPool_uid_90));
 }
 {
 static VkDescriptorSetLayoutBinding VkDescriptorSetLayoutBinding_info[1] = {VkDescriptorSetLayoutBinding{
 0u,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1u,VkShaderStageFlags(VK_SHADER_STAGE_FRAGMENT_BIT),nullptr}};
 static VkDescriptorSetLayoutCreateInfo VkDescriptorSetLayoutCreateInfo_info[1] = {VkDescriptorSetLayoutCreateInfo{
 VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,nullptr,VkDescriptorSetLayoutCreateFlags(0),1u,VkDescriptorSetLayoutBinding_info}};
 NV_VK_REPLAY(VulkanReplay_CreateDescriptorSetLayout(VkDevice_uid_4,VkDescriptorSetLayoutCreateInfo_info,nullptr,&VkDescriptorSetLayout_uid_91));
 }
 {
 static VkDescriptorSetLayout VkDescriptorSetLayout_values[1] = {VkDescriptorSetLayout_uid_91};
 static VkPipelineLayoutCreateInfo VkPipelineLayoutCreateInfo_info[1] = {VkPipelineLayoutCreateInfo{
 VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,nullptr,VkPipelineLayoutCreateFlags(0),1u,VkDescriptorSetLayout_values,0u,nullptr}};
 NV_VK_REPLAY(VulkanReplay_CreatePipelineLayout(VkDevice_uid_4,VkPipelineLayoutCreateInfo_info,nullptr,&VkPipelineLayout_uid_37));
 }
 {
 static VkDescriptorSetLayout VkDescriptorSetLayout_values[1] = {VkDescriptorSetLayout_uid_91};
 static VkDescriptorSetAllocateInfo VkDescriptorSetAllocateInfo_info[1] = {VkDescriptorSetAllocateInfo{
 VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,nullptr,VkDescriptorPool_uid_90,1u,VkDescriptorSetLayout_values}};
 NV_VK_REPLAY(VulkanReplay_AllocateDescriptorSets(VkDevice_uid_4,VkDescriptorSetAllocateInfo_info,NV_TO_OUTPUT_ARRAY(&VkDescriptorSet_uid_100)));
 NV_VK_REPLAY(VulkanHelper_RegisterObject(VK_OBJECT_TYPE_DESCRIPTOR_SET,uint64_t(VkDescriptorSet_uid_100),1000ull));
 }
 VERTEX_MODULE
}
)cpp";
const std::string initial = R"cpp(
void SetInitialFrameState00() {
 BEGIN_DATA_SCOPE_FUNCTION();
 NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));
 NV_VK_REPLAY(VulkanHelper_PopulateBuffer3(VkDevice_uid_4,0u,VkBuffer_uid_7,0ull,16ull,NV_GET_RESOURCE_CHECKED(const uint32_t*,5,16ull)));
 NV_VK_REPLAY(VulkanReplay_MapMemory(VkDevice_uid_4,VkDeviceMemory_uid_8,0ull,64ull,0u,&VkDeviceMemory_uid_8_MappedPointer));
 NV_VK_REPLAY(VulkanHelper_UpdateDescriptorSetsFromDatabase(VkDevice_uid_4,1u,Serialization::DATABASE_HANDLE(11),88ull,0u,nullptr));
 NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1));
}
)cpp";
const std::string reset =
    "NV_VK_REPLAY(VulkanReplay_ResetCommandBuffer(VkCommandBuffer_uid_42,VkCommandBufferResetFlags(0)));\n";
const std::string record =
    "VulkanHelper_RecordCommandBuffer(VkDevice_uid_4,VkCommandBuffer_uid_42,1u,&Record_VkCommandBuffer_uid_42_r1);\n";
const std::string reset_fence =
    "NV_VK_REPLAY(VulkanReplay_ResetFences(VkDevice_uid_4,1u,NV_TO_STACK_PTR(VkFence_uid_43)));\n";
const std::string idle = "NV_VK_REPLAY(VulkanReplay_QueueWaitIdle(VkQueue_uid_5));\n";
std::string update(unsigned handle) {
    return "{static VkDescriptorBufferInfo "
           "VkDescriptorBufferInfo_update[1]={VkDescriptorBufferInfo{VkBuffer_uid_7,0ull,16ull}};"
           "NV_VK_REPLAY(VulkanHelper_UpdateMappedMemory2(VkDevice_uid_4,VkDeviceMemory_uid_8,VkDeviceMemory_uid_8_"
           "MappedPointer,"
           "0ull,64ull,NV_GET_RESOURCE(const uint8_t*," +
           std::to_string(handle) + "),1ull,VkDescriptorBufferInfo_update));}\n";
}
std::vector<ngm::CppSource> input(std::size_t vertex_bytes, std::size_t fragment_bytes) {
    auto resource = objects();
    while(resource.find("128ull") != std::string::npos)
        resource = replace(resource, "128ull", std::to_string(fragment_bytes) + "ull");
    resource =
        replace(resource, "VkPipelineShaderStageCreateInfo_stages[1]", "VkPipelineShaderStageCreateInfo_stages[2]");
    resource =
        replace(resource, "1u, VkPipelineShaderStageCreateInfo_stages", "2u, VkPipelineShaderStageCreateInfo_stages");
    resource =
        replace(resource, "VkShaderModule_uid_36, \"main\", nullptr}",
                "VkShaderModule_uid_36, \"main\", nullptr},VkPipelineShaderStageCreateInfo{"
                "VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,VkPipelineShaderStageCreateFlags(0),"
                "VK_SHADER_STAGE_VERTEX_BIT,VkShaderModule_uid_35,\"main\",nullptr}");
    const auto vertex = "{static VkShaderModuleCreateInfo VkShaderModuleCreateInfo_vertex[1]={VkShaderModuleCreateInfo{"
                        "VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,nullptr,VkShaderModuleCreateFlags(0)," +
                        std::to_string(vertex_bytes) + "ull,NV_GET_RESOURCE_CHECKED(const uint32_t*,13," +
                        std::to_string(vertex_bytes) +
                        "ull)}};NV_VK_REPLAY(VulkanReplay_CreateShaderModule("
                        "VkDevice_uid_4,VkShaderModuleCreateInfo_vertex,nullptr,&VkShaderModule_uid_35));}";
    const auto binding =
        "NV_VK_REPLAY(VulkanReplay_CmdBindDescriptorSets(cmdBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,"
        "VkPipelineLayout_uid_37,0u,1u,NV_TO_ARRAY(VkDescriptorSet_uid_100),0u,nullptr)); // Event #50\n";
    return {{"Resources00.cpp", resource},
            {"Resources01.cpp", replace(definitions, "VERTEX_MODULE", vertex)},
            {"CommandList00.cpp", ngm::check::cpp::recording(begin + bind + binding + draw + end)},
            {"FrameSetup00.cpp", initial},
            {"Frame0Part00.cpp", "void RunFrame0Part00(uint64_t frameIndex){BEGIN_DATA_SCOPE_FUNCTION();\n" + reset +
                                     record + update(23) + reset_fence + submit + idle + reset + record + update(24) +
                                     reset_fence + submit + idle + "}"}};
}
const Json database{{"data.bin", {{"bytes", 4096}, {"sha256", std::string(64, 'a')}}},
                    {"data.bin.rec", {{"bytes", 64}, {"sha256", std::string(64, 'b')}}}};
std::vector<Json> descriptors(const CppReplay& replay) {
    for(const auto& call : replay.initial_state)
        if(call.name == "VulkanHelper_UpdateDescriptorSetsFromDatabase")
            return {{{"capture_id", "test-capture"},
                     {"inputs", database},
                     {"reference",
                      {{"source", call.source},
                       {"handle", 11},
                       {"write_count", 1},
                       {"output_bytes", 88},
                       {"copy_count", 0}}},
                     {"artifact_id", "test-descriptor-read"},
                     {"update_ref", std::string(64, 'c')},
                     {"worker_sha256", std::string(64, 'd')},
                     {"writes", Json::array({{{"dst_set", {{"symbol", "VkDescriptorSet_uid_100"}}},
                                              {"binding", 0},
                                              {"array_element", 0},
                                              {"descriptor_count", 1},
                                              {"descriptor_type", 6},
                                              {"buffers", Json::array({{{"buffer", {{"symbol", "VkBuffer_uid_7"}}},
                                                                        {"offset", 0},
                                                                        {"range", 16}}})}}})}}};
    throw std::runtime_error("missing synthetic descriptor source");
}
std::vector<std::byte> load(const char* path) {
    const auto source = ngm::read_regular_file(path, 1024U * 1024U);
    const auto bytes = std::as_bytes(std::span(source));
    return {bytes.begin(), bytes.end()};
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 4, "actual source-compiled vertex/fragment/writer modules required");
        const auto vertex = load(argv[1]), fragment = load(argv[2]), writer = load(argv[3]);
        const auto sources = input(vertex.size(), fragment.size());
        const auto parsed = parse_cpp_replay(sources, ReplayParseScope::Values);
        const auto candidates = value_execution_candidates(parsed);
        require(candidates.at("executions").size() == 2 && candidates.at("unsupported").empty() &&
                    candidates.at("captured_gpu_execution_correspondence") == false,
                "candidate discovery is worker-free source association only");
        const auto reads = descriptors(parsed);
        auto references = ngm::inspect_cpp_resources(sources).at("resources");
        std::size_t callbacks = 0;
        auto shader = [&](const Json& stage) {
            ++callbacks;
            const auto& bytes = stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? vertex : fragment;
            const auto found = std::find_if(references.begin(), references.end(), [&](const Json& r) {
                return r.at("handle") == stage.at("resource_handle");
            });
            require(found != references.end(), "synthetic shader has its actual parsed resource accessor");
            return ValueShader{bytes,
                               {{"capture_id", "test-capture"},
                                {"inputs", database},
                                {"offset", 0},
                                {"returned_bytes", bytes.size()},
                                {"total_bytes", bytes.size()},
                                {"sha256", ngm::sha256(bytes)},
                                {"reference", *found}}};
        };
        const auto positive = value_draw_plans(parsed, "test-capture", database, reads, shader);
        require(positive.at("status") == "qualified_source_draw_value_plans", positive.at("reason").dump());
        const auto& executions = positive.at("executions");
        for(unsigned i = 0; i < 2; ++i)
            require(candidates.at("executions")[i].at("selection") == executions[i].at("selection"),
                    "worker-free candidates and full lifetime/value plans select the same submitted occurrences");
        require(executions.size() == 2 && callbacks == 2 &&
                    positive.at("captured_gpu_execution_correspondence") == false,
                "two distinct source submissions reuse two checked shader modules without claiming GPU values");
        require(executions[0].at("selection").at("submission_index") == 0 &&
                    executions[1].at("selection").at("submission_index") == 1 &&
                    executions[0].at("selection").at("event_index") == executions[1].at("selection").at("event_index"),
                "reused recording/event IDs retain separate submitted occurrences");
        for(unsigned i = 0; i < 2; ++i) {
            const auto& plan = executions[i].at("plans").at(0);
            require(plan.at("buffer") == "VkBuffer_uid_7" && plan.at("effective_range") == 16 &&
                        !plan.at("shader_read").contains("data") &&
                        plan.at("uniform_read").at("layout").at("scalar") == "float32" &&
                        plan.at("upload").at("segments")[0].at("payload").at("handle") == 23 + i,
                    "selected descriptor/layout uses the latest distinct mapped upload at each occurrence");
        }
        require(executions[1].at("plans")[0].at("upload").at("segments")[0].at("completed_prior_uses").size() == 1,
                "later upload preserves the actual queue-completion boundary for the preceding use");
        const auto unavailable = [](const Json& result, const std::string& expected = {}) {
            require(result.at("status") == "unavailable" && result.at("executions").empty() &&
                        result.at("reason").is_string(),
                    "unsupported composition discards every provisional selected value");
            require(expected.empty() || result.at("reason").get<std::string>().find(expected) != std::string::npos,
                    "negative control expected " + expected + "; observed " + result.at("reason").dump());
        };
        auto changed = reads;
        unavailable(value_draw_plans(parsed, "test-capture", database, reads, shader, std::chrono::steady_clock::now()),
                    "shared deadline");
        changed[0]["capture_id"] = "other-capture";
        unavailable(value_draw_plans(parsed, "test-capture", database, changed, shader));
        changed = reads;
        changed[0]["inputs"]["data.bin"]["sha256"] = std::string(64, 'e');
        unavailable(value_draw_plans(parsed, "test-capture", database, changed, shader));
        changed = reads;
        changed[0]["writes"][0]["buffers"][0]["range"] = 8;
        unavailable(value_draw_plans(parsed, "test-capture", database, changed, shader));
        unavailable(value_draw_plans(parsed, "test-capture", database, reads, [&](const Json& stage) {
            auto value = shader(stage);
            value.read["capture_id"] = "other-capture";
            return value;
        }));
        unavailable(value_draw_plans(parsed, "test-capture", database, reads, [&](const Json& stage) {
            auto value = shader(stage);
            value.read["sha256"] = std::string(64, '0');
            return value;
        }));
        for(const auto& [source_at, from, to, reason] :
            std::vector<std::tuple<std::size_t, std::string, std::string, std::string>>{
                {4, idle, "", "recording reset"},
                {4, submit + idle, submit + update(25) + idle, "host upload overlaps"},
                {1, "VkBufferUsageFlags(16)", "VkBufferUsageFlags(19)", "GPU-read-only usage profile"},
                {1, "GetDeviceQueue(VkDevice_uid_4", "GetDeviceQueue(VkDevice_uid_99", "same-device identity"},
                {1, "GetDeviceQueue(VkDevice_uid_4,0u,0u", "GetDeviceQueue(VkDevice_uid_4,1u,0u",
                 "queue families differ"},
                {1, "VkCommandPoolCreateFlags(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)",
                 "VkCommandPoolCreateFlags(0)", "pool reset permission"},
                {4, reset_fence, "", "submission fence"},
                {2, "VulkanReplay_CmdDraw(cmdBuffer,3u,1u,0u,0u)",
                 "VulkanReplay_CmdFillBuffer(cmdBuffer,VkBuffer_uid_7,0ull,16ull,0u)", "unqualified command"},
                {4, reset,
                 reset + "NV_VK_REPLAY(VulkanReplay_AcquireNextImageKHR(VkDevice_uid_4,VkSwapchainKHR_uid_6,"
                         "0ull,VkSemaphore_uid_45,VkFence(VK_NULL_HANDLE),VkDeviceMemory_uid_8_MappedPointer));\n",
                 "acquire output must"}}) {
            auto altered = sources;
            altered[source_at].text = replace(altered[source_at].text, from, to);
            const auto candidate = parse_cpp_replay(altered, ReplayParseScope::Values);
            references = ngm::inspect_cpp_resources(altered).at("resources");
            unavailable(value_draw_plans(candidate, "test-capture", database, descriptors(candidate), shader), reason);
        }
        references = ngm::inspect_cpp_resources(sources).at("resources");
        auto unfinished = sources;
        const auto last_idle = unfinished[4].text.rfind(idle);
        require(last_idle != std::string::npos, "last completion boundary exists");
        unfinished[4].text.erase(last_idle, idle.size());
        const auto no_completion = parse_cpp_replay(unfinished, ReplayParseScope::Values);
        unavailable(value_draw_plans(no_completion, "test-capture", database, descriptors(no_completion), shader),
                    "frame ends before completion");
        const auto writing_sources = input(vertex.size(), writer.size());
        const auto writing = parse_cpp_replay(writing_sources, ReplayParseScope::Values);
        const auto writer_refs = ngm::inspect_cpp_resources(writing_sources).at("resources");
        unavailable(value_draw_plans(writing, "test-capture", database, descriptors(writing), [&](const Json& stage) {
            const auto& bytes = stage.at("stage") == "VK_SHADER_STAGE_VERTEX_BIT" ? vertex : writer;
            const auto r = std::find_if(writer_refs.begin(), writer_refs.end(),
                                        [&](const Json& v) { return v.at("handle") == stage.at("resource_handle"); });
            require(r != writer_refs.end(), "storage writer accessor exists");
            return ValueShader{bytes,
                               {{"capture_id", "test-capture"},
                                {"inputs", database},
                                {"offset", 0},
                                {"returned_bytes", bytes.size()},
                                {"total_bytes", bytes.size()},
                                {"sha256", ngm::sha256(bytes)},
                                {"reference", *r}}};
        }));
    });
}
