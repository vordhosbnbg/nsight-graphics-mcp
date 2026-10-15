#include "ValueDescriptors.hpp"
#include "ReplayValues.hpp"

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
const std::map<std::uint64_t, std::string> types{{1, "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"},
                                                 {6, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER"},
                                                 {7, "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER"},
                                                 {8, "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC"},
                                                 {9, "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC"}};
std::string registered_symbol(const Json& object, const std::string& type) {
    require(object.is_object() && object.contains("symbol") && object.at("symbol").is_string(),
            "descriptor history needs a typed source-registered symbol");
    ReplayValue value;
    value.text = object.at("symbol").get<std::string>();
    return symbol(value, type);
}
} // namespace
ValueDescriptors::ValueDescriptors(const Json& allocations) {
    require(allocations.at("status") == "qualified_source_descriptor_allocations" &&
                allocations.at("sets").size() <= 4096,
            "descriptor history requires qualified source allocations");
    std::size_t elements = 0;
    for(const auto& [name, allocation] : allocations.at("sets").items()) {
        for(const auto& b : allocation.at("bindings"))
            elements += b.at("descriptor_count").get<std::size_t>();
        require(elements <= 16384, "descriptor history exceeds its declared element bound");
        sets_.emplace(name, Set{allocation, {}, true});
    }
}
ValueDescriptors::Set& ValueDescriptors::live(const std::string& name) {
    require(sets_.contains(name) && sets_.at(name).alive, "descriptor set allocation is absent, freed or reset");
    return sets_.at(name);
}
const Json& ValueDescriptors::binding(const std::string& name, std::uint64_t index, std::uint64_t first,
                                      std::uint64_t count) const {
    require(sets_.contains(name) && sets_.at(name).alive, "descriptor set allocation is absent, freed or reset");
    for(const auto& b : sets_.at(name).allocation.at("bindings"))
        if(b.at("binding") == index) {
            const auto extent = b.at("descriptor_count").get<std::uint64_t>();
            require(count > 0 && count <= 128 && first < extent && count <= extent - first,
                    "descriptor update/copy crosses an unqualified binding or array range");
            return b;
        }
    throw Unqualified("descriptor binding is absent from its allocated layout");
}
void ValueDescriptors::changed(const std::string& name) {
    for(auto& [recording, state] : recordings_)
        if(state.sets.contains(name)) {
            (void)recording;
            require(state.pending == 0, "descriptor changed while a bound recording remains pending");
            state.invalid = "Descriptor update, copy, free or pool reset invalidated this recorded binding";
        }
}
void ValueDescriptors::check_budget() const {
    std::size_t bytes = 0;
    for(const auto& [name, set] : sets_) {
        bytes += name.size() + set.allocation.dump().size();
        for(const auto& [key, entry] : set.entries) {
            (void)key;
            bytes += entry.value.dump().size() + entry.history.dump().size();
            require(bytes <= 16U * 1024U * 1024U, "descriptor history exceeds its byte bound");
        }
        require(bytes <= 16U * 1024U * 1024U, "descriptor allocation evidence exceeds its byte bound");
    }
}
void ValueDescriptors::update(const ReplayCall& call, const Json& writes, const Json& read_evidence) {
    const auto& a = call.arguments;
    require(++updates_ <= 1024 && call.name == "VulkanHelper_UpdateDescriptorSetsFromDatabase" && a.size() == 6 &&
                a[2].constructor == "Serialization::DATABASE_HANDLE" && a[2].arguments.size() == 1,
            "descriptor update helper interface is unqualified");
    const auto device = symbol(a[0], "VkDevice");
    const auto count = number(a[1]), copies = number(a[4]);
    const bool sentinel = a[2].arguments[0].text == "- 1";
    require(sentinel ? count == 0 && copies > 0 && number(a[3]) == 0 : number(a[2].arguments[0]) <= INT32_MAX,
            "invalid database sentinel is qualified only for zero-write, zero-byte literal copies");
    require(count <= 128 && copies <= 64 && writes.is_array() && writes.size() == count &&
                (count != 0 || number(a[3]) == 0),
            "descriptor update source count differs from its authenticated write list");
    for(std::size_t write_index = 0; write_index < writes.size(); ++write_index) {
        const auto& write = writes[write_index];
        const auto name = registered_symbol(write.at("dst_set"), "VkDescriptorSet");
        auto& set = live(name);
        const auto index = write.at("binding").get<std::uint64_t>();
        const auto first = write.at("array_element").get<std::uint64_t>();
        const auto extent = write.at("descriptor_count").get<std::uint64_t>();
        const auto type = write.at("descriptor_type").get<std::uint64_t>();
        const auto& layout = binding(name, index, first, extent);
        require(types.contains(type) && layout.at("descriptor_type") == types.at(type) &&
                    set.allocation.at("device") == device,
                "descriptor write type or device differs from the allocated binding");
        const auto& values = write.at(type == 1 ? "images" : "buffers");
        require(values.is_array() && values.size() == extent, "descriptor element count differs from its payload");
        changed(name);
        for(std::size_t n = 0; n < extent; ++n) {
            Json value = values[n];
            if(type != 1 && !value.at("buffer").is_null())
                (void)registered_symbol(value.at("buffer"), "VkBuffer");
            value["descriptor_type"] = types.at(type);
            set.entries[{index, first + n}] = {std::move(value), Json::array({{{"kind", "write"},
                                                                               {"source", call.source},
                                                                               {"read", read_evidence},
                                                                               {"write_index", write_index},
                                                                               {"element_index", n}}})};
        }
    }
    if(copies == 0)
        require(a[5].text == "nullptr", "empty descriptor copy list has a non-null source pointer");
    else
        for(const auto& copy : array(a[5], call.visible, "VkCopyDescriptorSet", copies)) {
            require(copy.constructor == "VkCopyDescriptorSet" && copy.arguments.size() == 9,
                    "descriptor copy needs one literal nine-field aggregate");
            const auto& c = copy.arguments;
            structure(c, "VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET");
            const auto source = symbol(c[2], "VkDescriptorSet"), destination = symbol(c[5], "VkDescriptorSet");
            auto& from = live(source);
            auto& to = live(destination);
            const auto src_binding = number(c[3]), src_element = number(c[4]);
            const auto dst_binding = number(c[6]), dst_element = number(c[7]), extent = number(c[8]);
            const auto& src = binding(source, src_binding, src_element, extent);
            const auto& dst = binding(destination, dst_binding, dst_element, extent);
            require(src.at("descriptor_type") == dst.at("descriptor_type") && from.allocation.at("device") == device &&
                        to.allocation.at("device") == device,
                    "descriptor copy type or source/destination device differs");
            require(source != destination || src_binding != dst_binding || src_element + extent <= dst_element ||
                        dst_element + extent <= src_element,
                    "descriptor copy overlaps within one set binding");
            changed(destination);
            for(std::uint64_t n = 0; n < extent; ++n) {
                const auto found = from.entries.find({src_binding, src_element + n});
                auto copied = found == from.entries.end() ? Entry{} : found->second;
                require(copied.history.size() < 64, "descriptor copy provenance exceeds its chain bound");
                copied.history.push_back({{"kind", "copy"},
                                          {"source", call.source},
                                          {"copy_source", copy.source},
                                          {"source_set", source},
                                          {"source_binding", src_binding},
                                          {"source_element", src_element + n}});
                to.entries[{dst_binding, dst_element + n}] = std::move(copied);
            }
        }
    check_budget();
}
void ValueDescriptors::recorded(const std::string& name, const std::set<std::string>& sets) {
    require(recordings_.size() < 4096 && sets.size() <= 64, "descriptor recording coverage exceeds bounds");
    require(!recordings_.contains(name) || recordings_.at(name).pending == 0,
            "recording replaced while its descriptor uses remain pending");
    for(const auto& set : sets)
        (void)live(set);
    recordings_[name] = {sets, 0, {}};
}
void ValueDescriptors::forget(const std::string& name) {
    if(recordings_.contains(name)) {
        require(recordings_.at(name).pending == 0, "recording reset while its descriptor uses remain pending");
        recordings_.erase(name);
    }
}
void ValueDescriptors::require_recording(const std::string& name) const {
    require(recordings_.contains(name), "descriptor recording generation is absent");
    const auto& state = recordings_.at(name);
    require(state.invalid.empty(), state.invalid.c_str());
}
void ValueDescriptors::submitted(const std::string& name) {
    require_recording(name);
    require(++recordings_.at(name).pending <= 10000, "descriptor pending-use count exceeds bounds");
}
void ValueDescriptors::completed(const std::string& name) {
    require(recordings_.contains(name) && recordings_.at(name).pending > 0,
            "descriptor completion lacks its pending recording generation");
    --recordings_.at(name).pending;
}
void ValueDescriptors::host(const ReplayCall& call) {
    const auto& a = call.arguments;
    const auto device = symbol(a.at(0), "VkDevice");
    if(call.name == "VulkanReplay_FreeDescriptorSets") {
        require(a.size() == 4, "descriptor free arity is unqualified");
        const auto pool = symbol(a[1], "VkDescriptorPool");
        std::set<std::string> names;
        for(const auto& value : array(a[3], call.visible, "VkDescriptorSet", number(a[2]))) {
            const auto name = symbol(value, "VkDescriptorSet");
            auto& set = live(name);
            require(names.insert(name).second && set.allocation.at("device") == device &&
                        set.allocation.at("pool") == pool && set.allocation.at("pool_allows_free") == true,
                    "descriptor free repeats a set or differs from its source pool/device");
            changed(name);
            set.alive = false;
            set.entries.clear();
        }
    } else {
        require((call.name == "VulkanReplay_ResetDescriptorPool" && a.size() == 3 &&
                 number(unwrap(a[2], "VkDescriptorPoolResetFlags")) == 0) ||
                    (call.name == "VulkanReplay_DestroyDescriptorPool" && a.size() == 3 && a[2].text == "nullptr"),
                "descriptor pool lifetime effect is unqualified");
        const auto pool = symbol(a[1], "VkDescriptorPool");
        bool found = false;
        for(auto& [name, set] : sets_)
            if(set.allocation.at("pool") == pool) {
                require(set.allocation.at("device") == device, "descriptor pool lifetime device differs");
                changed(name);
                found = true;
                set.alive = false;
                set.entries.clear();
            }
        require(found, "descriptor pool lifetime effect has no captured allocations");
    }
}
Json ValueDescriptors::element(const std::string& name, std::uint64_t index, std::uint64_t element) const {
    Json result{{"status", "unavailable"}, {"reason", nullptr}, {"value", nullptr}, {"history", Json::array()}};
    try {
        const auto& layout = binding(name, index, element, 1);
        const auto& set = sets_.at(name);
        result["allocation"] = set.allocation;
        result["binding"] = layout;
        const auto found = set.entries.find({index, element});
        require(found != set.entries.end(), "descriptor element has no preceding source write or copy");
        result["history"] = found->second.history;
        require(!found->second.value.is_null(), "descriptor copy has undefined source contents");
        result["status"] = "qualified_source_descriptor";
        result["value"] = found->second.value;
    } catch(const Unqualified& error) {
        result["reason"] = error.what();
    }
    return result;
}
} // namespace ngm::detail
