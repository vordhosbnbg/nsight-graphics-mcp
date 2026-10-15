#include "Check.hpp"
#include "CppReplay.hpp"
#include "ReplayValues.hpp"
#include "ValueUploads.hpp"

namespace {
using ngm::check::require;
using namespace ngm::detail;
using Json = nlohmann::json;
Json graph(bool coherent = true) {
    Json buffers = Json::object();
    for(const auto& [name, offset] :
        std::map<std::string, std::uint64_t>{{"VkBuffer_uid_7", 16}, {"VkBuffer_uid_9", 80}})
        buffers[name] = {
            {"status", "qualified_source_memory_binding"},
            {"device", "VkDevice_uid_4"},
            {"buffer_bytes", 64},
            {"usage_flags", 19},
            {"memory", "VkDeviceMemory_uid_8"},
            {"memory_offset", offset},
            {"allocation_bytes", 160},
            {"memory_type",
             {{"property_flags", coherent ? 6 : 10}, {"host_visible", true}, {"host_coherent", coherent}}}};
    return {{"status", "qualified_source_memory_graph"}, {"buffers", buffers}};
}
ReplayCall call(const std::string& text) {
    const auto replay = parse_cpp_replay(
        {{"FrameSetup00.cpp", "void SetInitialFrameState00() { BEGIN_DATA_SCOPE_FUNCTION(); "
                              "NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));"
                              "NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1)); }"},
         {"Frame0Part00.cpp",
          "void RunFrame0Part00(uint64_t frameIndex) { BEGIN_DATA_SCOPE_FUNCTION();\n" + text + "\n}"}},
        ReplayParseScope::Values);
    require(replay.frame.size() == 1, "upload test uses one actual normalized source call");
    return replay.frame[0];
}
ReplayCall map(const std::string& size = "128ull", std::uint64_t offset = 16) {
    return call("NV_VK_REPLAY(VulkanReplay_MapMemory(VkDevice_uid_4,VkDeviceMemory_uid_8," + std::to_string(offset) +
                "ull," + size + ",0u,&VkDeviceMemory_uid_8_MappedPointer));");
}
ReplayCall populate(unsigned resource = 5, std::uint64_t bytes = 64) {
    return call("NV_VK_REPLAY(VulkanHelper_PopulateBuffer3(VkDevice_uid_4,0u,VkBuffer_uid_7,0ull," +
                std::to_string(bytes) + "ull,NV_GET_RESOURCE_CHECKED(const uint32_t*," + std::to_string(resource) +
                "," + std::to_string(bytes) + "ull)));");
}
ReplayCall update(std::uint64_t offset = 8, std::uint64_t bytes = 16, unsigned resource = 23,
                  const std::string& name = "VkBuffer_uid_7", std::uint64_t relative = 8) {
    return call("static VkDescriptorBufferInfo VkDescriptorBufferInfo_info[1] = {VkDescriptorBufferInfo{" + name + "," +
                std::to_string(relative) + "ull," + std::to_string(bytes) +
                "ull}};\n"
                "NV_VK_REPLAY(VulkanHelper_UpdateMappedMemory2(VkDevice_uid_4,VkDeviceMemory_uid_8,"
                "VkDeviceMemory_uid_8_MappedPointer," +
                std::to_string(offset) + "ull," + std::to_string(bytes) + "ull,NV_GET_RESOURCE(const uint8_t*," +
                std::to_string(resource) + "),1ull,VkDescriptorBufferInfo_info));");
}
const std::string queue = "VkQueue_uid_5", device = "VkDevice_uid_4", fence = "VkFence_uid_43",
                  buffer = "VkBuffer_uid_7";
const Json use_source{{"path", "Frame0Part00.cpp"}, {"byte_offset", 0}, {"byte_count", 10}};
template <class F>
void refuses(F&& run) {
    bool rejected = false;
    try {
        run();
    } catch(const Unqualified&) {
        rejected = true;
    }
    require(rejected, "unsupported or inconsistent source lifetime refuses");
}
} // namespace
int main() {
    return ngm::check::run([] {
        ValueUploads uploads(graph());
        uploads.host(populate(), true);
        uploads.host(map());
        uploads.host(update());
        uploads.host(update(16, 16, 24, buffer, 16));
        const auto selected = uploads.range(buffer, 0, 32);
        require(selected.at("status") == "qualified_source_upload_range", "complete latest-write source coverage");
        const auto& pieces = selected.at("segments");
        require(pieces.size() == 3 && pieces[0].at("buffer_offset") == 0 && pieces[0].at("length") == 8 &&
                    pieces[0].at("payload").at("handle") == 5 && pieces[1].at("buffer_offset") == 8 &&
                    pieces[1].at("length") == 8 && pieces[1].at("payload").at("handle") == 23 &&
                    pieces[2].at("buffer_offset") == 16 && pieces[2].at("length") == 16 &&
                    pieces[2].at("payload").at("handle") == 24,
                "overlapping writes select the latest bytes without replacing untouched portions");
        const auto tail = uploads.range(buffer, 30, 12).at("segments");
        require(tail.size() == 2 && tail[0].at("payload_offset") == 14 && tail[1].at("payload_offset") == 32 &&
                    tail[1].at("payload").at("handle") == 5 && tail[0].at("payload").contains("mapping_source"),
                "payload slicing and exact mapping provenance survive latest-write resolution");
        require(uploads.range(buffer, 60, 8).at("status") == "unavailable", "out-of-buffer selected range refuses");
        ValueUploads holes(graph());
        holes.host(populate(5, 8), true);
        require(holes.range(buffer, 0, 16).at("status") == "unavailable" &&
                    holes.range(buffer, 0, 16).at("segments").empty(),
                "a coverage hole never returns partial old bytes");
        ValueUploads absent(graph());
        refuses([&] { absent.host(update()); });
        ValueUploads wrong(graph());
        wrong.host(map());
        refuses([&] { wrong.host(update(8, 16, 23, buffer, 0)); });
        refuses([&] { wrong.host(update(127, 16, 23, buffer, 127)); });
        refuses([&] { wrong.host(map()); });
        ValueUploads whole(graph());
        whole.host(map("VK_WHOLE_SIZE"));
        whole.host(update(64, 64, 30, "VkBuffer_uid_9", 0));
        require(whole.range("VkBuffer_uid_9", 0, 64).at("segments")[0].at("payload").at("handle") == 30,
                "allocation-relative map offset is combined with update and nonzero buffer binding offset");
        ValueUploads padding(graph());
        padding.host(map("64ull", 16));
        padding.host(update(0, 64, 31, buffer, 0));
        require(padding.range(buffer, 0, 64).at("status") == "qualified_source_upload_range",
                "whole mapped buffer bytes stay inside their literal allocation and buffer");
        ValueUploads noncoherent(graph(false));
        noncoherent.host(map());
        noncoherent.host(update());
        require(noncoherent.range(buffer, 8, 16).at("status") == "unavailable",
                "host-visible noncoherent bytes never inherit coherent upload visibility");
        ValueUploads inflight(graph());
        inflight.host(map());
        inflight.submit(queue, device, fence, {buffer}, use_source);
        const auto finite_wait = call("NV_VK_REPLAY(VulkanReplay_WaitForFences(VkDevice_uid_4,1u,"
                                      "NV_TO_STACK_PTR(VkFence_uid_43),VK_TRUE,10000000000ull));");
        inflight.host(finite_wait);
        refuses([&] { inflight.host(update()); });
        refuses([&] {
            inflight.host(call("NV_VK_REPLAY(VulkanReplay_ResetFences(VkDevice_uid_4,1u,"
                               "NV_TO_STACK_PTR(VkFence_uid_43)));"));
        });
        inflight.host(call("NV_VK_REPLAY(VulkanReplay_QueueWaitIdle(VkQueue_uid_6));"));
        refuses([&] { inflight.host(update()); });
        inflight.host(call("NV_VK_REPLAY(VulkanReplay_QueueWaitIdle(VkQueue_uid_5));"));
        inflight.host(update());
        require(inflight.range(buffer, 8, 16).at("status") == "qualified_source_upload_range",
                "the exact queue's idle boundary retires the old buffer use");
        const auto completed = inflight.range(buffer, 8, 16).at("segments")[0].at("completed_prior_uses");
        require(completed.size() == 1 && completed[0].at("method") == "VulkanReplay_QueueWaitIdle" &&
                    completed[0].at("fence") == fence && completed[0].at("submission_source") == use_source &&
                    completed[0].at("completion_source").contains("sha256"),
                "the upload retains the exact source use and completion boundary that permitted the overwrite");
        inflight.submit(queue, device, fence, {buffer}, use_source);
        inflight.host(call("NV_VK_REPLAY(VulkanReplay_WaitForFences(VkDevice_uid_4,1u,"
                           "NV_TO_STACK_PTR(VkFence_uid_43),VK_TRUE,18446744073709551615ull));"));
        inflight.host(update(16, 16, 32, buffer, 16));
        require(inflight.range(buffer, 16, 16).at("segments")[0].at("payload").at("handle") == 32,
                "an infinite wait-all boundary permits a later, distinct payload");
        inflight.invalidate(buffer, "Unqualified GPU write");
        inflight.host(update(16, 16, 33, buffer, 16));
        require(inflight.range(buffer, 16, 16).at("status") == "unavailable",
                "unknown GPU effects cannot be repaired by reusing earlier visibility assumptions");
        ValueUploads destroyed(graph());
        destroyed.host(populate(), true);
        destroyed.host(call("NV_VK_REPLAY(VulkanReplay_DestroyBuffer(VkDevice_uid_4,VkBuffer_uid_7,nullptr));"));
        require(destroyed.range(buffer, 0, 16).at("status") == "unavailable", "destroyed buffer loses its old upload");
        refuses([&] { destroyed.host(populate(), true); });
        ValueUploads freed(graph());
        freed.host(map());
        refuses([&] {
            freed.host(call("NV_VK_REPLAY(VulkanReplay_FreeMemory(VkDevice_uid_4,VkDeviceMemory_uid_8,nullptr));"));
        });
        freed.host(call("NV_VK_REPLAY(VulkanReplay_UnmapMemory(VkDevice_uid_4,VkDeviceMemory_uid_8));"));
        freed.host(call("NV_VK_REPLAY(VulkanReplay_FreeMemory(VkDevice_uid_4,VkDeviceMemory_uid_8,nullptr));"));
        require(freed.range(buffer, 0, 16).at("status") == "unavailable" &&
                    freed.range("VkBuffer_uid_9", 0, 16).at("status") == "unavailable",
                "freeing one allocation invalidates every bound buffer identity");
        refuses([&] { absent.host(call("NV_VK_REPLAY(VulkanReplay_UnknownHostWrite(VkDevice_uid_4));")); });
    });
}
