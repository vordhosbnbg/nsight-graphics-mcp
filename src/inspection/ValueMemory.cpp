#include "ValueMemory.hpp"
#include "ReplayValues.hpp"
#include "ngm/Inspection.hpp"

#include <cctype>
#include <map>
#include <set>

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
std::string output_symbol(const ReplayValue& value, const std::string& type) {
    require(value.text.starts_with("& "), "memory identity needs one literal output symbol");
    auto unwrapped = value;
    unwrapped.text = value.text.substr(2);
    return symbol(unwrapped, type);
}
std::uint64_t flags(const ReplayValue& value, const char* type, const std::map<std::string, std::uint64_t>& names) {
    const auto text = unwrap(value, type).text;
    if(!text.empty() && std::isdigit(static_cast<unsigned char>(text.front())))
        return number(text);
    std::uint64_t result = 0;
    for(std::size_t at = 0; at < text.size();) {
        const auto next = text.find(" | ", at);
        const auto token = text.substr(at, next == std::string::npos ? next : next - at);
        require(names.contains(token), "unqualified captured memory/buffer flags");
        result |= names.at(token);
        if(next == std::string::npos)
            return result;
        at = next + 3;
    }
    throw Unqualified("empty captured memory/buffer flags");
}
Json physical_types(const ReplayCall& call) {
    const auto& a = call.arguments;
    require(a.size() == 7 && a[5].constructor == "VkPhysicalDeviceMemoryProperties" && a[5].arguments.size() == 4,
            "physical memory properties lack the captured interface");
    const auto& p = a[5].arguments;
    const auto count = number(p[0]), heaps = number(p[2]);
    require(count > 0 && count <= 32 && heaps > 0 && heaps <= 16 && p[1].constructor == "NV_TO_ARRAY" &&
                p[3].constructor == "NV_TO_ARRAY" && p[1].arguments.size() >= count && p[1].arguments.size() <= 32 &&
                p[3].arguments.size() >= heaps && p[3].arguments.size() <= 16,
            "captured memory-type/heap counts exceed their declared storage");
    Json types = Json::array();
    for(std::size_t n = 0; n < count; ++n) {
        const auto& type = p[1].arguments[n];
        require(type.constructor == "VkMemoryType" && type.arguments.size() == 2,
                "captured memory type is not a literal two-field aggregate");
        const auto bits = flags(type.arguments[0], "VkMemoryPropertyFlags",
                                {{"VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT", 1},
                                 {"VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT", 2},
                                 {"VK_MEMORY_PROPERTY_HOST_COHERENT_BIT", 4},
                                 {"VK_MEMORY_PROPERTY_HOST_CACHED_BIT", 8},
                                 {"VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT", 16},
                                 {"VK_MEMORY_PROPERTY_PROTECTED_BIT", 32},
                                 {"VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD", 64},
                                 {"VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD", 128},
                                 {"VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV", 256}});
        const auto heap = number(type.arguments[1]);
        require(heap < heaps && bits <= 511, "captured memory type has unknown flags or heap index");
        const auto& storage = p[3].arguments[heap];
        require(storage.constructor == "VkMemoryHeap" && storage.arguments.size() == 2,
                "captured memory heap is not a literal size/flags aggregate");
        types.push_back({{"index", n},
                         {"property_flags", bits},
                         {"host_visible", (bits & 2) != 0},
                         {"host_coherent", (bits & 4) != 0},
                         {"heap", heap},
                         {"heap_bytes", number(storage.arguments[0])},
                         {"source", type.source}});
    }
    return types;
}
struct Buffer {
    std::string device;
    std::uint64_t bytes, requirement_bytes, alignment, type_bits, usage;
    Json source;
    std::string limitation;
};
struct Allocation {
    std::string device;
    std::uint64_t bytes, type_index, images, acceleration_structures, video;
    std::vector<ReplayValue> bindings;
    Json source;
    Json extensions;
    std::string limitation;
};
Json allocation_extensions(ReplayValue next, const ReplayDeclarations& declarations) {
    std::set<std::string> seen;
    std::uint64_t address_flags = 0, opaque = 0;
    Json provenance = Json::array();
    while(next.text != "nullptr") {
        std::string type = next.constructor;
        if(type.empty()) {
            const auto found = declarations.find(next.text);
            require(found != declarations.end(), "allocation extension declaration is absent");
            type = found->second.type;
        }
        require(seen.size() < 2 && seen.insert(type).second,
                "allocation extension chain is cyclic, duplicated or exceeds bounds");
        if(type == "VkMemoryAllocateFlagsInfo") {
            const auto f = fields(next, declarations, type, 4);
            require(f[0].text == "VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO",
                    "memory allocation flags structure type mismatch");
            address_flags = flags(f[2], "VkMemoryAllocateFlags",
                                  {{"VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT", 1},
                                   {"VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT", 2},
                                   {"VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT", 4}});
            require((address_flags & ~std::uint64_t{6}) == 0 && number(f[3]) == 0,
                    "device-mask or unknown allocation flags need separate instance proof");
            provenance.push_back({{"type", type}, {"source", next.source}});
            next = f[1];
        } else if(type == "VkMemoryOpaqueCaptureAddressAllocateInfo") {
            const auto f = fields(next, declarations, type, 3);
            require(f[0].text == "VK_STRUCTURE_TYPE_MEMORY_OPAQUE_CAPTURE_ADDRESS_ALLOCATE_INFO",
                    "memory opaque-address structure type mismatch");
            opaque = number(f[2]);
            provenance.push_back({{"type", type}, {"source", next.source}});
            next = f[1];
        } else
            throw Unqualified("unqualified memory allocation extension");
    }
    require((address_flags & 4) == 0 || (address_flags & 2) != 0,
            "capture/replay allocation lacks device-address enablement");
    require(opaque == 0 || (address_flags & 4) != 0, "opaque capture allocation lacks qualified capture/replay flags");
    return {{"allocation_flags", address_flags},
            {"has_opaque_capture_address", opaque != 0},
            {"source_chain", std::move(provenance)}};
}
} // namespace

Json inspect_value_memory_bindings(const CppReplay& replay) {
    Json result{{"status", "unavailable"}, {"reason", nullptr}, {"buffers", Json::object()}};
    try {
        std::map<std::string, Json> physical;
        std::map<std::string, std::pair<std::string, Json>> devices;
        std::map<std::string, Buffer> buffers;
        std::map<std::string, Allocation> allocations;
        for(const auto& call : replay.resource_memory) {
            const auto& a = call.arguments;
            if(call.name == "VulkanHelper_GetCompatiblePhysicalDevice") {
                require(a.size() == 7, "physical-device helper arity is unqualified");
                const auto id = output_symbol(a[6], "VkPhysicalDevice");
                require(physical.emplace(id, Json{{"types", physical_types(call)}, {"source", call.source}}).second,
                        "ambiguous physical memory properties");
            } else if(call.name == "VulkanReplay_CreateDevice") {
                require(a.size() == 4 && a[2].text == "nullptr", "device identity interface is unqualified");
                require(devices
                            .emplace(output_symbol(a[3], "VkDevice"),
                                     std::make_pair(symbol(a[0], "VkPhysicalDevice"), call.source))
                            .second,
                        "ambiguous logical-device identity");
            } else if(call.name == "VulkanHelper_CreateBuffer3") {
                require(a.size() == 8, "captured buffer helper arity is unqualified");
                const auto id = output_symbol(a[7], "VkBuffer");
                const auto info = fields(a[1], call.visible, "VkBufferCreateInfo", 8);
                const auto requirements = fields(a[2], call.visible, "VkMemoryRequirements", 3);
                structure(info, "VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO");
                const auto usage = flags(info[4], "VkBufferUsageFlags",
                                         {{"VK_BUFFER_USAGE_TRANSFER_SRC_BIT", 1},
                                          {"VK_BUFFER_USAGE_TRANSFER_DST_BIT", 2},
                                          {"VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT", 4},
                                          {"VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT", 8},
                                          {"VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT", 16},
                                          {"VK_BUFFER_USAGE_STORAGE_BUFFER_BIT", 32},
                                          {"VK_BUFFER_USAGE_INDEX_BUFFER_BIT", 64},
                                          {"VK_BUFFER_USAGE_VERTEX_BUFFER_BIT", 128},
                                          {"VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT", 256},
                                          {"VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT", 131072}});
                Buffer buffer{symbol(a[0], "VkDevice"),
                              number(info[3]),
                              number(requirements[0]),
                              number(requirements[1]),
                              number(requirements[2]),
                              usage,
                              call.source,
                              {}};
                require(buffer.bytes > 0 && buffer.requirement_bytes >= buffer.bytes && buffer.alignment > 0 &&
                            (buffer.alignment & (buffer.alignment - 1)) == 0 && buffer.type_bits <= UINT32_MAX,
                        "captured buffer requirements are inconsistent");
                if(number(unwrap(info[2], "VkBufferCreateFlags")) != 0 || info[5].text != "VK_SHARING_MODE_EXCLUSIVE" ||
                   number(info[6]) != 0 || info[7].text != "nullptr" || number(unwrap(a[3], "VkDeviceAddress")) != 0 ||
                   number(unwrap(a[4], "VkDeviceAddress")) != 0 || number(a[5]) != 0 || a[6].text != "nullptr")
                    buffer.limitation = "Sparse/shared/address-qualified buffer creation needs separate proof";
                require(buffers.emplace(id, std::move(buffer)).second, "buffer identity is recreated or ambiguous");
            } else if(call.name == "VulkanHelper_AllocateAndInitializeMemory3") {
                require(a.size() == 5 && a[4].text == "nullptr", "captured memory helper interface is unqualified");
                const auto id = output_symbol(a[3], "VkDeviceMemory");
                const auto info = fields(a[1], call.visible, "VkMemoryAllocateInfo", 4);
                const auto binding = fields(a[2], call.visible, "VulkanHelper_InitializeDeviceMemoryInfo", 12);
                require(info[0].text == "VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO",
                        "captured memory allocation structure type mismatch");
                Allocation allocation{symbol(a[0], "VkDevice"),
                                      number(info[2]),
                                      number(info[3]),
                                      number(binding[4]),
                                      number(binding[6]),
                                      number(binding[8]),
                                      {},
                                      call.source,
                                      allocation_extensions(info[1], call.visible),
                                      {}};
                require(allocation.bytes > 0 && number(binding[2]) <= 64,
                        "captured memory allocation or buffer binding count is unqualified");
                if(number(binding[0]) != 2 || number(binding[1]) != 0 || binding[11].text != "VK_FALSE")
                    allocation.limitation = "Memory initialization version/address/flags need separate proof";
                if(number(binding[2]))
                    allocation.bindings = array(binding[3], call.visible, "VkBindBufferMemoryInfo", number(binding[2]));
                else
                    require(binding[3].text == "nullptr", "empty memory buffer bindings have a non-null pointer");
                require(allocations.emplace(id, std::move(allocation)).second,
                        "memory allocation identity is recreated or ambiguous");
            } else
                require(call.name == "VulkanHelper_RegisterObject", "unmodeled resource memory side effect");
        }
        std::set<std::string> bound;
        for(const auto& [memory, allocation] : allocations) {
            require(devices.contains(allocation.device) && physical.contains(devices.at(allocation.device).first),
                    "allocation lacks captured device/physical memory-type identity");
            const auto& p = physical.at(devices.at(allocation.device).first);
            require(allocation.type_index < p.at("types").size(),
                    "allocation memory-type index exceeds captured types");
            const auto& type = p.at("types").at(allocation.type_index);
            struct Range {
                std::string buffer;
                std::uint64_t first, last;
            };
            std::vector<Range> ranges;
            for(const auto& binding : allocation.bindings) {
                require(binding.constructor == "VkBindBufferMemoryInfo" && binding.arguments.size() == 5,
                        "buffer binding is not one literal aggregate");
                const auto& b = binding.arguments;
                structure(b, "VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO");
                const auto buffer = symbol(b[2], "VkBuffer"), owner = symbol(b[3], "VkDeviceMemory");
                require(owner == memory && buffers.contains(buffer) && bound.insert(buffer).second,
                        "buffer binding memory/output is missing, duplicated or mismatched");
                const auto& source = buffers.at(buffer);
                const auto offset = number(b[4]);
                require(source.device == allocation.device && offset <= allocation.bytes &&
                            source.requirement_bytes <= allocation.bytes - offset && offset % source.alignment == 0 &&
                            (source.type_bits & (std::uint64_t{1} << allocation.type_index)) != 0,
                        "buffer allocation range, alignment or memory-type eligibility is inconsistent");
                ranges.push_back({buffer, offset, offset + source.bytes});
                auto reason = source.limitation.empty() ? allocation.limitation : source.limitation;
                if(allocation.images || allocation.acceleration_structures || allocation.video)
                    reason = "Mixed buffer/image/acceleration/video allocation aliases need separate proof";
                result["buffers"][buffer] = {
                    {"status", reason.empty() ? "qualified_source_memory_binding" : "unavailable"},
                    {"reason", reason.empty() ? Json(nullptr) : Json(reason)},
                    {"device", source.device},
                    {"buffer_bytes", source.bytes},
                    {"usage_flags", source.usage},
                    {"memory", memory},
                    {"memory_offset", offset},
                    {"allocation_bytes", allocation.bytes},
                    {"memory_type", type},
                    {"allocation_extensions", allocation.extensions},
                    {"evidence", Json::array({source.source, binding.source, allocation.source,
                                              devices.at(allocation.device).second, p.at("source")})}};
            }
            for(const auto& a : ranges)
                for(const auto& b : ranges)
                    if(a.buffer != b.buffer && a.first < b.last && b.first < a.last) {
                        result["buffers"][a.buffer]["status"] = "unavailable";
                        result["buffers"][a.buffer]["reason"] =
                            "Overlapping allocation aliases need separate write-lifetime proof";
                    }
        }
        for(const auto& [name, buffer] : buffers)
            if(!bound.contains(name))
                result["buffers"][name] = {{"status", "unavailable"},
                                           {"reason", "Buffer has no captured memory binding"},
                                           {"evidence", Json::array({buffer.source})}};
        result["status"] = "qualified_source_memory_graph";
    } catch(const Unqualified& error) {
        result["buffers"] = Json::object();
        result["reason"] = error.what();
    } catch(const Json::exception&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Malformed captured memory definition");
    } catch(const std::out_of_range&) {
        throw InspectionError(InspectionErrorCode::InvalidExport, "Truncated captured memory definition");
    }
    result["scope"] = "Captured static memory bindings/properties; host coherence alone does not prove upload "
                      "visibility or executed shader values";
    if(result.dump().size() > 16U * 1024U * 1024U)
        throw InspectionError(InspectionErrorCode::LimitExceeded, "Captured memory graph exceeds its result budget");
    return result;
}
} // namespace ngm::detail
