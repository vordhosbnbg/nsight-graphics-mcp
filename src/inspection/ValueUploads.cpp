#include "ValueUploads.hpp"
#include "ReplayValues.hpp"

#include <algorithm>
#include <optional>

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
std::uint64_t extent(const ReplayValue& value, std::uint64_t remaining) {
    const auto& literal = unwrap(value, "VkDeviceSize");
    const auto size = literal.text == "VK_WHOLE_SIZE" ? UINT64_MAX : number(literal);
    return size == UINT64_MAX ? remaining : size;
}
Json payload(const ReplayValue& value, std::uint64_t bytes) {
    const bool checked = value.constructor == "NV_GET_RESOURCE_CHECKED";
    require((checked || value.constructor == "NV_GET_RESOURCE") && value.arguments.size() == (checked ? 3 : 2) &&
                (value.arguments[0].text == "const uint8_t *" || value.arguments[0].text == "const uint32_t *") &&
                number(value.arguments[1]) <= INT32_MAX && (!checked || number(value.arguments[2]) == bytes),
            "upload payload needs an exact bounded serialized-resource accessor");
    require(bytes > 0 && bytes <= 16U * 1024U * 1024U, "upload source extent exceeds qualified bounds");
    return {{"handle", number(value.arguments[1])},
            {"accessor", value.constructor},
            {"cpp_type", value.arguments[0].text},
            {"expected_bytes", checked ? Json(bytes) : Json(nullptr)},
            {"required_bytes", bytes},
            {"source", value.source}};
}
std::string null_fence(const ReplayValue& value) {
    if(value.text == "VK_NULL_HANDLE" || value.text == "VkFence ( VK_NULL_HANDLE )")
        return {};
    return symbol(value, "VkFence");
}
} // namespace
ValueUploads::ValueUploads(const Json& graph) {
    require(graph.at("status") == "qualified_source_memory_graph" && graph.at("buffers").size() <= 4096,
            "uploads require a qualified captured memory graph");
    for(const auto& [name, binding] : graph.at("buffers").items())
        buffers_.emplace(name, Buffer{binding,
                                      {},
                                      binding.at("status") == "qualified_source_memory_binding"
                                          ? ""
                                          : binding.value("reason", "Captured memory binding unavailable"),
                                      true});
}
ValueUploads::Buffer& ValueUploads::buffer(const std::string& name) {
    require(buffers_.contains(name), "upload refers to a buffer absent from captured memory definitions");
    auto& result = buffers_.at(name);
    require(result.alive, "buffer or its allocation was destroyed before this source effect");
    return result;
}
bool ValueUploads::pending(const std::string& name) const {
    return std::any_of(pending_.begin(), pending_.end(), [&](const auto& p) { return p.buffers.contains(name); });
}
void ValueUploads::invalidate(const std::string& name, const std::string& reason) {
    auto& b = buffer(name);
    b.writes.clear();
    b.reason = reason;
}
void ValueUploads::write(const std::string& name, std::uint64_t offset, std::uint64_t bytes, Json data,
                         const ReplayCall& call, const std::string& kind) {
    auto& b = buffer(name);
    require(!pending(name), "host upload overlaps a submitted buffer without proven completion");
    require(++writes_ <= 4096 && b.writes.size() < 256, "upload history exceeds qualified bounds");
    if(!b.reason.empty())
        return;
    const auto size = b.binding.at("buffer_bytes").get<std::uint64_t>();
    require(bytes > 0 && offset <= size && bytes <= size - offset, "upload exceeds captured buffer range");
    b.writes.push_back({offset, bytes, std::move(data), call.source, kind, std::move(b.completions)});
    b.completions = Json::array();
}
void ValueUploads::submit(const std::string& queue, const std::string& device, const std::string& fence,
                          const std::set<std::string>& buffers, const Json& source) {
    require(pending_.size() < 10000 && buffers.size() <= 4096, "pending buffer-use history exceeds bounds");
    if(!fence.empty())
        require(std::none_of(pending_.begin(), pending_.end(),
                             [&](const auto& p) { return p.device == device && p.fence == fence; }),
                "fence reused while a buffer submission remains pending");
    for(const auto& name : buffers)
        require(buffer(name).binding.at("device") == device, "submitted buffer belongs to a different device");
    if(!buffers.empty())
        pending_.push_back({queue, device, fence, buffers, source});
}
void ValueUploads::host(const ReplayCall& call, bool initialization) {
    const auto& a = call.arguments;
    const auto arity = [&](std::size_t count) { require(a.size() == count, "unqualified upload/lifetime call arity"); };
    const auto complete = [&](auto matches) {
        for(auto at = pending_.begin(); at != pending_.end();) {
            if(!matches(*at)) {
                ++at;
                continue;
            }
            for(const auto& name : at->buffers) {
                auto& b = buffers_.at(name);
                require(b.completions.size() < 64, "completion boundary provenance exceeds bounds");
                b.completions.push_back({{"queue", at->queue},
                                         {"device", at->device},
                                         {"fence", at->fence},
                                         {"submission_source", at->source},
                                         {"completion_source", call.source},
                                         {"method", call.name}});
            }
            at = pending_.erase(at);
        }
    };
    if(call.name == "VulkanReplay_MapMemory") {
        arity(6);
        const auto memory = symbol(a[1], "VkDeviceMemory"), device = symbol(a[0], "VkDevice");
        require(!mappings_.contains(memory) && a[5].text == "& " + memory + "_MappedPointer" &&
                    number(unwrap(a[4], "VkMemoryMapFlags")) == 0,
                "memory mapping is duplicated, aliased or extension-qualified");
        const auto offset = number(a[2]);
        std::optional<std::uint64_t> allocation;
        for(const auto& [name, b] : buffers_)
            if(b.reason.empty() && b.binding.at("memory") == memory) {
                (void)name;
                require(b.alive && b.binding.at("device") == device &&
                            b.binding.at("memory_type").at("host_visible") == true,
                        "mapping lacks a live host-visible allocation on this device");
                const auto bytes = b.binding.at("allocation_bytes").get<std::uint64_t>();
                require(!allocation || *allocation == bytes, "ambiguous mapped allocation size");
                allocation = bytes;
            }
        require(allocation && offset < *allocation, "mapping allocation or starting range is unavailable");
        const auto bytes = extent(a[3], *allocation - offset);
        require(bytes > 0 && bytes <= *allocation - offset, "mapping exceeds captured allocation");
        mappings_.emplace(memory, Mapping{device, memory + "_MappedPointer", offset, bytes, call.source});
    } else if(call.name == "VulkanReplay_UnmapMemory") {
        arity(2);
        const auto memory = symbol(a[1], "VkDeviceMemory");
        require(mappings_.contains(memory) && mappings_.at(memory).device == symbol(a[0], "VkDevice"),
                "unmap lacks its exact active mapping");
        mappings_.erase(memory);
    } else if(call.name == "VulkanHelper_PopulateBuffer3") {
        arity(6);
        require(initialization && number(a[1]) == 0, "only initial single-device buffer population is qualified");
        const auto name = symbol(a[2], "VkBuffer");
        auto& b = buffer(name);
        require(b.reason.empty() && b.binding.at("device") == symbol(a[0], "VkDevice"),
                "initial population lacks a qualified captured binding on this device");
        const auto offset = number(a[3]), bytes = number(a[4]);
        const auto data = payload(a[5], bytes);
        // The qualified helper has a direct host copy for this form. Other
        // forms use queued staging work and need separate visibility evidence.
        if(offset != 0 || b.binding.at("memory_type").at("host_visible") != true ||
           b.binding.at("memory_type").at("host_coherent") != true) {
            invalidate(name, "Initial staging or noncoherent buffer population needs separate visibility proof");
            return;
        }
        const auto memory = b.binding.at("memory").get<std::string>();
        if(mappings_.contains(memory)) {
            const auto& m = mappings_.at(memory);
            const auto first = b.binding.at("memory_offset").get<std::uint64_t>();
            require(first >= m.offset && first - m.offset <= m.bytes && bytes <= m.bytes - (first - m.offset),
                    "initial buffer population is outside its active mapping");
        }
        write(name, offset, bytes, data, call, "initial_host_coherent_population");
    } else if(call.name == "VulkanHelper_UpdateMappedMemory2") {
        arity(8);
        const auto memory = symbol(a[1], "VkDeviceMemory"), device = symbol(a[0], "VkDevice");
        require(mappings_.contains(memory), "mapped update has no preceding active map");
        const auto& m = mappings_.at(memory);
        const auto offset = number(a[3]), bytes = number(a[4]), count = number(a[6]);
        require(device == m.device && a[2].text == m.pointer && offset <= m.bytes && bytes > 0 &&
                    bytes <= m.bytes - offset && count > 0 && count <= 64,
                "mapped update pointer, range or buffer list is unqualified");
        const auto data = payload(a[5], bytes);
        const auto first = m.offset + offset, last = first + bytes;
        std::set<std::string> covered;
        for(const auto& info : array(a[7], call.visible, "VkDescriptorBufferInfo", count)) {
            require(info.constructor == "VkDescriptorBufferInfo" && info.arguments.size() == 3,
                    "mapped buffer-info aggregate is unqualified");
            const auto name = symbol(info.arguments[0], "VkBuffer");
            auto& b = buffer(name);
            require(covered.insert(name).second && b.binding.at("status") == "qualified_source_memory_binding" &&
                        b.binding.at("memory") == memory && b.binding.at("device") == device,
                    "mapped buffer list is duplicated or differs from its captured allocation");
            const auto binding = b.binding.at("memory_offset").get<std::uint64_t>();
            const auto size = b.binding.at("buffer_bytes").get<std::uint64_t>();
            const auto relative = number(info.arguments[1]), range = number(info.arguments[2]);
            require(first >= binding && relative <= INT64_MAX && relative == first - binding && relative < size &&
                        range >= std::min(bytes, size - relative),
                    "native and virtualized mapped-buffer ranges lack the same nonnegative byte interpretation");
            if(b.binding.at("memory_type").at("host_coherent") != true) {
                invalidate(name, "Noncoherent mapped upload needs an independently qualified flush/visibility path");
                continue;
            }
            auto with_map = data;
            with_map["mapping_source"] = m.source;
            write(name, relative, std::min(bytes, size - relative), std::move(with_map), call,
                  "mapped_host_coherent_update");
        }
        // A source list that omits an intersected alias cannot be used to
        // assume that buffer's old contents survived the allocation write.
        for(const auto& [name, b] : buffers_)
            if(b.reason.empty() && b.binding.at("memory") == memory) {
                const auto begin = b.binding.at("memory_offset").get<std::uint64_t>();
                const auto end = begin + b.binding.at("buffer_bytes").get<std::uint64_t>();
                require(!(first < end && begin < last) || covered.contains(name),
                        "mapped allocation write intersects a buffer omitted from its source list");
            }
    } else if(call.name == "VulkanReplay_QueueWaitIdle") {
        arity(1);
        const auto queue = symbol(a[0], "VkQueue");
        complete([&](const auto& p) { return p.queue == queue; });
    } else if(call.name == "VulkanReplay_DeviceWaitIdle") {
        arity(1);
        const auto device = symbol(a[0], "VkDevice");
        complete([&](const auto& p) { return p.device == device; });
    } else if(call.name == "VulkanReplay_WaitForFences" || call.name == "VulkanReplay_ResetFences") {
        arity(call.name == "VulkanReplay_WaitForFences" ? 5 : 3);
        const auto device = symbol(a[0], "VkDevice");
        const auto count = number(a[1]);
        std::set<std::string> fences;
        for(const auto& value : array(a[2], call.visible, "VkFence", count))
            require(fences.insert(null_fence(value)).second && !null_fence(value).empty(),
                    "fence wait/reset contains a repeated or null fence");
        if(call.name == "VulkanReplay_ResetFences") {
            require(std::none_of(pending_.begin(), pending_.end(),
                                 [&](const auto& p) { return p.device == device && fences.contains(p.fence); }),
                    "fence reset lacks proven completion of its buffer uses");
        } else {
            const bool all = a[3].text == "VK_TRUE" || a[3].text == "1u" || a[3].text == "1";
            // Finite waits and status queries have no captured return-value
            // evidence here. They cannot silently retire a pending buffer use.
            if(all && number(a[4]) == UINT64_MAX)
                complete([&](const auto& p) { return p.device == device && fences.contains(p.fence); });
        }
    } else if(call.name == "VulkanReplay_DestroyBuffer") {
        arity(3);
        const auto name = symbol(a[1], "VkBuffer");
        auto& b = buffer(name);
        require(a[2].text == "nullptr" && b.binding.at("device") == symbol(a[0], "VkDevice") && !pending(name),
                "buffer destruction lacks its device or proven completion");
        b.alive = false;
        b.writes.clear();
        b.reason = "Buffer was destroyed before the selected occurrence";
    } else if(call.name == "VulkanReplay_FreeMemory") {
        arity(3);
        const auto memory = symbol(a[1], "VkDeviceMemory"), device = symbol(a[0], "VkDevice");
        require(a[2].text == "nullptr" && !mappings_.contains(memory), "freeing mapped/extension-qualified allocation");
        bool found = false;
        for(auto& [name, b] : buffers_)
            if(b.binding.value("memory", "") == memory) {
                require(b.binding.at("device") == device && !pending(name),
                        "memory destruction lacks its device or proven completion");
                found = true;
                b.alive = false;
                b.writes.clear();
                b.reason = "Allocation was freed before the selected occurrence";
            }
        require(found, "memory free has no captured buffer allocation");
    } else
        throw Unqualified("unmodeled host upload/lifetime effect: " + call.name);
}
Json ValueUploads::range(const std::string& name, std::uint64_t offset, std::uint64_t bytes) const {
    Json result{{"status", "unavailable"}, {"reason", nullptr}, {"segments", Json::array()}};
    try {
        require(buffers_.contains(name), "selected buffer is absent from the captured memory graph");
        const auto& b = buffers_.at(name);
        require(b.alive && b.reason.empty(), b.reason.empty() ? "selected buffer is no longer live" : b.reason.c_str());
        const auto size = b.binding.at("buffer_bytes").get<std::uint64_t>();
        require(bytes > 0 && bytes <= 65536 && offset <= size && bytes <= size - offset,
                "selected value range is outside the buffer or 64 KiB bound");
        struct Hole {
            std::uint64_t begin, end;
        };
        std::vector<Hole> holes{{offset, offset + bytes}};
        for(auto write = b.writes.rbegin(); write != b.writes.rend() && !holes.empty(); ++write) {
            std::vector<Hole> rest;
            for(const auto& h : holes) {
                const auto first = std::max(h.begin, write->offset),
                           last = std::min(h.end, write->offset + write->bytes);
                if(first >= last) {
                    rest.push_back(h);
                    continue;
                }
                if(h.begin < first)
                    rest.push_back({h.begin, first});
                if(last < h.end)
                    rest.push_back({last, h.end});
                result["segments"].push_back({{"buffer_offset", first},
                                              {"length", last - first},
                                              {"payload_offset", first - write->offset},
                                              {"payload", write->payload},
                                              {"write_kind", write->kind},
                                              {"write_source", write->source},
                                              {"completed_prior_uses", write->completions}});
            }
            holes = std::move(rest);
        }
        require(holes.empty(), "no complete latest-write coverage for the selected buffer range");
        auto& segments = result["segments"];
        std::sort(segments.begin(), segments.end(),
                  [](const auto& a, const auto& b) { return a.at("buffer_offset") < b.at("buffer_offset"); });
        result["status"] = "qualified_source_upload_range";
        result["binding"] = b.binding;
    } catch(const Unqualified& error) {
        result["segments"] = Json::array();
        result["reason"] = error.what();
    }
    return result;
}
} // namespace ngm::detail
