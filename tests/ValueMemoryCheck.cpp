#include "Check.hpp"
#include "CppReplay.hpp"
#include "DrawStateFixture.hpp"
#include "ValueMemory.hpp"

namespace {
using ngm::check::require;
using namespace ngm::detail;
std::string source(bool second = false, std::uint64_t second_offset = 80) {
    std::string text = R"cpp(
void CreateResources00() {
 {
 NV_VK_REPLAY(VulkanHelper_GetCompatiblePhysicalDevice(VkInstance_uid_1,0u,VkPhysicalDeviceProperties{0u},
 1u,NV_TO_ARRAY(VkQueueFamilyProperties{VkQueueFlags(VK_QUEUE_GRAPHICS_BIT),1u,64u,VkExtent3D{1u,1u,1u}}),
 VkPhysicalDeviceMemoryProperties{1u,NV_TO_ARRAY(VkMemoryType{VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),0u}),
 1u,NV_TO_ARRAY(VkMemoryHeap{4096ull,VkMemoryHeapFlags(0)})},&VkPhysicalDevice_uid_2));
 }
 { NV_VK_REPLAY(VulkanReplay_CreateDevice(VkPhysicalDevice_uid_2,VkDeviceCreateInfo_info,nullptr,&VkDevice_uid_4)); }
)cpp";
    for(const auto id : second ? std::vector<unsigned>{7, 9} : std::vector<unsigned>{7}) {
        text += R"cpp(
 {
 static VkBufferCreateInfo VkBufferCreateInfo_info[1] = {VkBufferCreateInfo{
 VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,nullptr,VkBufferCreateFlags(0),64ull,VkBufferUsageFlags(19),VK_SHARING_MODE_EXCLUSIVE,0u,nullptr}};
 static VkMemoryRequirements VkMemoryRequirements_info[1] = {VkMemoryRequirements{64ull,16ull,1u}};
 NV_VK_REPLAY(VulkanHelper_CreateBuffer3(VkDevice_uid_4,VkBufferCreateInfo_info,VkMemoryRequirements_info,VkDeviceAddress(0),VkDeviceAddress(0),0ull,nullptr,&VkBuffer_uid_)cpp" +
                std::to_string(id) + "));\n }\n";
    }
    text += R"cpp(
 {
 static VkMemoryAllocateInfo VkMemoryAllocateInfo_info[1] = {VkMemoryAllocateInfo{
 VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,nullptr,160ull,0u}};
 static VkBindBufferMemoryInfo VkBindBufferMemoryInfo_info[)cpp" +
            std::to_string(second ? 2 : 1) +
            "] = "
            "{VkBindBufferMemoryInfo{VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,nullptr,VkBuffer_uid_7,VkDeviceMemory_"
            "uid_8,16ull}";
    if(second)
        text += ",VkBindBufferMemoryInfo{VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,nullptr,VkBuffer_uid_9,"
                "VkDeviceMemory_uid_8," +
                std::to_string(second_offset) + "ull}";
    text += R"cpp(};
 static VulkanHelper_InitializeDeviceMemoryInfo VulkanHelper_InitializeDeviceMemoryInfo_info[1] = {
 VulkanHelper_InitializeDeviceMemoryInfo{2u,0ull,)cpp" +
            std::to_string(second ? 2 : 1) +
            R"cpp(u,VkBindBufferMemoryInfo_info,0u,nullptr,0u,nullptr,0u,nullptr,nullptr,VK_FALSE}};
 NV_VK_REPLAY(VulkanHelper_AllocateAndInitializeMemory3(VkDevice_uid_4,VkMemoryAllocateInfo_info,VulkanHelper_InitializeDeviceMemoryInfo_info,&VkDeviceMemory_uid_8,nullptr));
 }
}
)cpp";
    return text;
}
auto inspect(const std::string& text) {
    const std::vector<ngm::CppSource> files{
        {"Resources00.cpp", text},
        {"Frame0Part00.cpp", "void RunFrame0Part00(uint64_t frameIndex) { BEGIN_DATA_SCOPE_FUNCTION(); }"},
        {"FrameSetup00.cpp", "void SetInitialFrameState00() { BEGIN_DATA_SCOPE_FUNCTION(); "
                             "NV_VK_REPLAY(VulkanHelper_InitBegin(VkInstance_uid_1));"
                             "NV_VK_REPLAY(VulkanHelper_InitEnd(VkInstance_uid_1)); }"}};
    return inspect_value_memory_bindings(parse_cpp_replay(files, ReplayParseScope::Values));
}
} // namespace
int main() {
    return ngm::check::run([] {
        const auto baseline = source();
        const auto result = inspect(baseline);
        require(result.at("status") == "qualified_source_memory_graph", "captured static graph has unique definitions");
        const auto& buffer = result.at("buffers").at("VkBuffer_uid_7");
        require(buffer.at("status") == "qualified_source_memory_binding" && buffer.at("memory_offset") == 16 &&
                    buffer.at("buffer_bytes") == 64 && buffer.at("allocation_bytes") == 160 &&
                    buffer.at("memory_type").at("property_flags") == 6 &&
                    buffer.at("memory_type").at("host_visible") == true &&
                    buffer.at("memory_type").at("host_coherent") == true && buffer.at("evidence").size() == 5,
                "nonzero binding range, actual memory flags and all source relationships retained");
        const auto noncoherent = inspect(ngm::check::state::replace(
            baseline, "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT",
            "VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT"));
        require(noncoherent.at("buffers").at("VkBuffer_uid_7").at("memory_type").at("host_coherent") == false,
                "host-visible alone never becomes coherent or proves a visible upload");
        const auto adjacent = inspect(source(true, 80));
        require(adjacent.at("buffers").at("VkBuffer_uid_7").at("status") == "qualified_source_memory_binding" &&
                    adjacent.at("buffers").at("VkBuffer_uid_9").at("status") == "qualified_source_memory_binding",
                "adjacent disjoint ranges remain distinguishable within one allocation");
        const auto overlap = inspect(source(true, 64));
        require(overlap.at("buffers").at("VkBuffer_uid_7").at("status") == "unavailable" &&
                    overlap.at("buffers").at("VkBuffer_uid_9").at("status") == "unavailable",
                "overlapping aliases refuse until write lifetimes are proven");
        const auto unavailable = [&](const std::string& from, const std::string& to) {
            const auto altered = inspect(ngm::check::state::replace(baseline, from, to));
            require(altered.at("status") == "unavailable" && altered.at("buffers").empty() &&
                        altered.at("reason").is_string(),
                    "inconsistent range, identity, type eligibility or allocation refuses the graph");
        };
        unavailable("nullptr,160ull,0u", "nullptr,79ull,0u");
        unavailable("VkDeviceMemory_uid_8,16ull", "VkDeviceMemory_uid_8,17ull");
        unavailable("VkDeviceMemory_uid_8,16ull", "VkDeviceMemory_uid_8,18446744073709551615ull");
        unavailable("VkMemoryRequirements{64ull,16ull,1u}", "VkMemoryRequirements{64ull,16ull,2u}");
        unavailable("nullptr,160ull,0u", "nullptr,160ull,1u");
        unavailable("VkDeviceMemory_uid_8,16ull", "VkDeviceMemory_uid_10,16ull");
        unavailable("&VkDevice_uid_4", "&VkDevice_uid_5");
        auto duplicate = source(true, 80);
        duplicate = ngm::check::state::replace(duplicate, "&VkBuffer_uid_9", "&VkBuffer_uid_7");
        require(inspect(duplicate).at("status") == "unavailable",
                "recreated/ambiguous buffer IDs never overwrite facts");
        const auto flags =
            inspect(ngm::check::state::replace(baseline, "VkBufferCreateFlags(0)", "VkBufferCreateFlags(1)"));
        require(flags.at("buffers").at("VkBuffer_uid_7").at("status") == "unavailable",
                "sparse or extension-qualified creation cannot inherit ordinary binding proof");
        const auto version = inspect(ngm::check::state::replace(baseline, "{2u,0ull,", "{3u,0ull,"));
        require(version.at("buffers").at("VkBuffer_uid_7").at("status") == "unavailable",
                "unqualified memory helper versions remain explicit");
        const auto address =
            ngm::check::state::replace(baseline, "static VkMemoryAllocateInfo VkMemoryAllocateInfo_info", R"cpp(
 static VkMemoryOpaqueCaptureAddressAllocateInfo VkMemoryOpaqueCaptureAddressAllocateInfo_next[1] = {
 VkMemoryOpaqueCaptureAddressAllocateInfo{VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,nullptr,4096ull}};
 static VkMemoryAllocateFlagsInfo VkMemoryAllocateFlagsInfo_next[1] = {
 VkMemoryAllocateFlagsInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,VkMemoryOpaqueCaptureAddressAllocateInfo_next,
 VkMemoryAllocateFlags(VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT),0u}};
 static VkMemoryAllocateInfo VkMemoryAllocateInfo_info)cpp");
        const auto chained =
            ngm::check::state::replace(address, "nullptr,160ull,0u", "VkMemoryAllocateFlagsInfo_next,160ull,0u");
        const auto qualified = inspect(chained);
        require(
            qualified.at("status") == "qualified_source_memory_graph" &&
                qualified.at("buffers").at("VkBuffer_uid_7").at("allocation_extensions").at("allocation_flags") == 6 &&
                qualified.at("buffers")
                        .at("VkBuffer_uid_7")
                        .at("allocation_extensions")
                        .at("has_opaque_capture_address") == true &&
                qualified.at("buffers").at("VkBuffer_uid_7").at("allocation_extensions").at("source_chain").size() == 2,
            "captured allocation-address metadata preserves flags and provenance without a live-address claim");
        const auto extension_refuses = [&](const std::string& from, const std::string& to) {
            require(inspect(ngm::check::state::replace(chained, from, to)).at("status") == "unavailable",
                    "unknown, contradictory or cyclic allocation extension metadata refuses");
        };
        extension_refuses(
            "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT",
            "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT");
        extension_refuses(
            "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT",
            "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT");
        extension_refuses(
            "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT | VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT",
            "VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT");
        extension_refuses("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT),0u",
                          "VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT),1u");
        extension_refuses("VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT),0u", "128u),0u");
        extension_refuses(
            "VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,nullptr,4096ull",
            "VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,VkMemoryAllocateFlagsInfo_next,"
            "4096ull");
        extension_refuses("VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO,nullptr,4096ull",
                          "VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,nullptr,4096ull");
    });
}
