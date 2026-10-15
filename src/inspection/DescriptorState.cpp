#include "DescriptorState.hpp"
#include "PushState.hpp"
#include "ReplayValues.hpp"
#include "ngm/Hash.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
Json missing(const std::string& reason, const char* status = "unavailable") {
    return {{"status", status}, {"value", nullptr}, {"reason", reason}, {"evidence", Json::array()}};
}
std::string identity(const Json& value) {
    const auto text = value.dump();
    return sha256(std::as_bytes(std::span(text.data(), text.size())));
}
std::uint32_t stage_mask(const ReplayValue& value) {
    auto text = unwrap(value, "VkShaderStageFlags").text;
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }), text.end());
    const std::map<std::string, std::uint32_t> flags{{"VK_SHADER_STAGE_VERTEX_BIT", 1},
                                                     {"VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT", 2},
                                                     {"VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT", 4},
                                                     {"VK_SHADER_STAGE_GEOMETRY_BIT", 8},
                                                     {"VK_SHADER_STAGE_FRAGMENT_BIT", 16},
                                                     {"VK_SHADER_STAGE_COMPUTE_BIT", 32},
                                                     {"VK_SHADER_STAGE_ALL_GRAPHICS", 31},
                                                     {"VK_SHADER_STAGE_ALL", 2147483647}};
    if(text == "0")
        return 0;
    std::uint32_t result = 0;
    for(std::size_t at = 0; at < text.size();) {
        const auto end = text.find('|', at);
        const auto token = text.substr(at, end == std::string::npos ? text.size() - at : end - at);
        require(flags.contains(token), "unknown descriptor stage flags");
        result |= flags.at(token);
        if(end == std::string::npos)
            return result;
        require(end + 1 < text.size(), "trailing descriptor stage separator");
        at = end + 1;
    }
    throw Unqualified("empty descriptor stage flags");
}
struct SetLayout {
    std::string name, signature;
    Json bindings, source;
};
SetLayout set_layout(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_CreateDescriptorSetLayout",
            "descriptor set layout creation is absent or ambiguous");
    const auto& object = found->second;
    const auto f = fields(object.creation.arguments.at(1), object.values, "VkDescriptorSetLayoutCreateInfo", 5);
    structure(f, "VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO", "VkDescriptorSetLayoutCreateFlags");
    const auto count = number(f[3]);
    require(count <= 64, "descriptor layout exceeds 64 bindings");
    Json bindings = Json::array();
    std::set<std::uint64_t> used;
    const std::set<std::string> types{"VK_DESCRIPTOR_TYPE_SAMPLER",
                                      "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
                                      "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE",
                                      "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE",
                                      "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
                                      "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
                                      "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER",
                                      "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER",
                                      "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
                                      "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",
                                      "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT"};
    std::size_t descriptors = 0;
    if(count != 0)
        for(const auto& value : array(f[4], object.values, "VkDescriptorSetLayoutBinding", count)) {
            require(value.constructor == "VkDescriptorSetLayoutBinding" && value.arguments.size() == 5,
                    "unqualified descriptor layout binding shape");
            const auto& b = value.arguments;
            const auto index = number(b[0]), extent = number(b[2]);
            require(index <= 4294967295ull && used.insert(index).second && extent > 0 && extent <= 64 &&
                        types.contains(b[1].text) && b[4].text == "nullptr",
                    "unsupported descriptor binding, type, extent or immutable samplers");
            descriptors += extent;
            require(descriptors <= 256, "descriptor layout exceeds 256 elements");
            bindings.push_back({{"binding", index},
                                {"descriptor_type", b[1].text},
                                {"descriptor_count", extent},
                                {"stage_mask", stage_mask(b[3])}});
        }
    std::sort(bindings.begin(), bindings.end(),
              [](const auto& a, const auto& b) { return a.at("binding") < b.at("binding"); });
    return {name, identity(bindings), bindings, object.source};
}
struct PipelineLayout {
    std::string push_signature;
    std::vector<SetLayout> sets;
    Json source;
};
PipelineLayout pipeline_layout(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_CreatePipelineLayout",
            "pipeline layout creation is absent or ambiguous");
    const auto& object = found->second;
    const auto f = fields(object.creation.arguments.at(1), object.values, "VkPipelineLayoutCreateInfo", 7);
    structure(f, "VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO", "VkPipelineLayoutCreateFlags");
    const auto count = number(f[3]);
    require(count <= 8, "pipeline layout exceeds eight descriptor sets");
    PipelineLayout result{push_layout_signature(replay, name), {}, object.source};
    if(count != 0)
        for(const auto& value : array(f[4], object.values, "VkDescriptorSetLayout", count))
            result.sets.push_back(set_layout(replay, symbol(value, "VkDescriptorSetLayout")));
    return result;
}
std::pair<SetLayout, Json> allocated_layout(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_AllocateDescriptorSets",
            "descriptor set allocation is absent or ambiguous");
    const auto& object = found->second;
    const auto f = fields(object.creation.arguments.at(1), object.values, "VkDescriptorSetAllocateInfo", 5);
    structure(f, "VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO");
    (void)symbol(f[2], "VkDescriptorPool");
    const auto count = number(f[3]);
    require(count > 0 && count <= 8, "descriptor allocation count exceeds bounds");
    const auto layouts = array(f[4], object.values, "VkDescriptorSetLayout", count);
    const auto& output = object.creation.arguments.at(2);
    require(output.constructor == "NV_TO_OUTPUT_ARRAY" && output.arguments.size() == count,
            "descriptor allocation output/count mismatch");
    std::optional<std::size_t> index;
    std::set<std::string> unique;
    for(std::size_t n = 0; n < count; ++n) {
        auto value = output.arguments[n];
        require(value.text.starts_with("& "), "unqualified descriptor allocation output");
        value.text.erase(0, 2);
        const auto candidate = symbol(value, "VkDescriptorSet");
        require(unique.insert(candidate).second, "duplicate descriptor allocation output");
        if(candidate == name)
            index = n;
    }
    require(index.has_value(), "descriptor allocation does not produce selected set");
    return {set_layout(replay, symbol(layouts[*index], "VkDescriptorSetLayout")), object.source};
}
} // namespace
bool DescriptorState::compatible(const Layout& a, const Layout& b, std::size_t set) {
    return a.push_signature == b.push_signature && a.sets.size() > set && b.sets.size() > set &&
           std::equal(a.sets.begin(), a.sets.begin() + set + 1, b.sets.begin());
}
void DescriptorState::record(const CppReplay& replay, const ReplayCall& call, const ReplayDeclarations& values) {
    if(!unsupported_.empty())
        return;
    try {
        const auto& a = call.arguments;
        require(a.size() == 8, "descriptor bind arity unsupported");
        // Bind points have independent descriptor banks. The selected draw uses graphics.
        if(a[1].text == "VK_PIPELINE_BIND_POINT_COMPUTE")
            return;
        require(a[1].text == "VK_PIPELINE_BIND_POINT_GRAPHICS", "descriptor bind point unsupported");
        const auto layout_name = symbol(a[2], "VkPipelineLayout");
        const auto object = pipeline_layout(replay, layout_name);
        const auto first = number(a[3]), count = number(a[4]), dynamic_count = number(a[6]);
        require(count > 0 && count <= 8 && first < 8 && count <= 8 - first && first + count <= object.sets.size() &&
                    dynamic_count <= 64,
                "descriptor bind range exceeds qualified bounds");
        const auto bound = array(a[5], values, "VkDescriptorSet", count);
        const auto offsets =
            dynamic_count ? array(a[7], values, "uint32_t", dynamic_count) : std::vector<ReplayValue>{};
        Layout layout{layout_name, object.push_signature, {}};
        for(const auto& set : object.sets)
            layout.sets.push_back(set.signature);
        std::vector<Binding> bindings;
        std::size_t offset_index = 0;
        for(std::size_t n = 0; n < count; ++n) {
            const auto name = symbol(bound[n], "VkDescriptorSet");
            const auto [allocated, allocation_source] = allocated_layout(replay, name);
            const auto& expected = object.sets[first + n];
            require(allocated.signature == expected.signature,
                    "allocated descriptor layout differs from bound set layout");
            Json dynamic = Json::array();
            for(const auto& binding : allocated.bindings)
                if(binding.at("descriptor_type") == "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC" ||
                   binding.at("descriptor_type") == "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC")
                    for(std::size_t element = 0; element < binding.at("descriptor_count").get<std::size_t>();
                        ++element) {
                        require(offset_index < offsets.size(), "missing descriptor dynamic offset");
                        const auto value = number(offsets[offset_index]);
                        require(value <= 4294967295ull, "descriptor dynamic offset exceeds uint32");
                        dynamic.push_back({{"binding", binding.at("binding")},
                                           {"array_element", element},
                                           {"offset", value},
                                           {"source_index", offset_index++}});
                    }
            bindings.push_back({layout, name, allocated.name, dynamic,
                                Json::array({object.source,
                                             allocated.source,
                                             allocation_source,
                                             {{"event_index", call.event}, {"source", call.source}}})});
        }
        require(offset_index == offsets.size(), "extra descriptor dynamic offsets");
        // Apply a completely validated operation atomically. A disturbed slot
        // retains the disturbing layout, as required for subsequent disturbance.
        for(std::size_t n = 0; n < count; ++n) {
            const auto target = first + n;
            const auto disturb = [&](std::size_t index) {
                sets_[index] = Binding{layout,
                                       {},
                                       {},
                                       Json::array(),
                                       Json::array({{{"event_index", call.event}, {"source", call.source}}})};
            };
            for(std::size_t lower = 0; lower < target; ++lower)
                if(sets_[lower] && !compatible(sets_[lower]->layout, layout, lower))
                    disturb(lower);
            if(sets_[target] && !compatible(sets_[target]->layout, layout, target))
                for(std::size_t upper = target + 1; upper < sets_.size(); ++upper)
                    if(sets_[upper])
                        disturb(upper);
            sets_[target] = std::move(bindings[n]);
        }
    } catch(const Unqualified& error) {
        unsupported_ = error.what();
    }
}
Json DescriptorState::at_draw(const CppReplay& replay, const std::string& name) const {
    try {
        if(!unsupported_.empty())
            return missing(unsupported_, "unsupported");
        const auto object = pipeline_layout(replay, name);
        if(object.sets.empty())
            return missing("pipeline layout declares no descriptor sets", "not_applicable");
        Layout layout{name, object.push_signature, {}};
        for(const auto& set : object.sets)
            layout.sets.push_back(set.signature);
        Json coverage = Json::array(), values = Json::array();
        bool complete = true;
        for(std::size_t n = 0; n < object.sets.size(); ++n) {
            const auto& expected = object.sets[n];
            Json row{{"set", n},
                     {"pipeline_set_layout", expected.name},
                     {"layout_signature", expected.signature},
                     {"bindings", expected.bindings},
                     {"descriptor_set", nullptr},
                     {"allocated_set_layout", nullptr},
                     {"binding_layout", nullptr},
                     {"dynamic_offsets", Json::array()},
                     {"evidence", Json::array({expected.source})}};
            const auto& bound = sets_[n];
            if(!bound && expected.bindings.empty()) {
                row["status"] = "not_applicable";
                row["reason"] = "empty descriptor set layout has no bound descriptors";
            } else if(!bound || bound->descriptor_set.empty() || !compatible(bound->layout, layout, n)) {
                row["status"] = "unavailable";
                row["reason"] = !bound                          ? "no recorded descriptor set bind"
                                : bound->descriptor_set.empty() ? "descriptor set was disturbed by a later bind"
                                                                : "last bound layout is incompatible for this set";
                if(bound)
                    row["evidence"] = bound->evidence;
                complete = false;
            } else {
                row["status"] = "qualified_recorded_state";
                row["reason"] = nullptr;
                row["descriptor_set"] = bound->descriptor_set;
                row["allocated_set_layout"] = bound->set_layout;
                row["binding_layout"] = bound->layout.name;
                row["dynamic_offsets"] = bound->offsets;
                row["evidence"] = bound->evidence;
                values.push_back({{"set", n},
                                  {"descriptor_set", bound->descriptor_set},
                                  {"allocated_set_layout", bound->set_layout},
                                  {"binding_layout", bound->layout.name},
                                  {"dynamic_offsets", bound->offsets}});
            }
            coverage.push_back(std::move(row));
        }
        auto result = missing(complete ? "" : "one or more declared descriptor sets lack compatible recorded bindings");
        if(complete) {
            result["status"] = "qualified_recorded_state";
            result["reason"] = nullptr;
            result["value"] = values;
        }
        result["evidence"] = Json::array({object.source});
        result["coverage"] = {
            {"layout", name},
            {"bindings_complete", complete},
            {"sets", coverage},
            {"contents",
             missing("descriptor write/copy history and execution-time resource contents are not qualified")},
            {"shader_access",
             "layout-declared sets; shader usage, array selection and effective buffer addresses are not established"}};
        return result;
    } catch(const Unqualified& error) {
        return missing(error.what(), "unsupported");
    }
}
} // namespace ngm::detail
