#include "PushState.hpp"
#include "ReplayValues.hpp"
#include "ngm/Hash.hpp"

#include <algorithm>
#include <cctype>
#include <tuple>

namespace ngm::detail {
namespace {
using Json = nlohmann::json;
constexpr std::array<const char*, 6> stage_names{"VK_SHADER_STAGE_VERTEX_BIT",
                                                 "VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT",
                                                 "VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT",
                                                 "VK_SHADER_STAGE_GEOMETRY_BIT",
                                                 "VK_SHADER_STAGE_FRAGMENT_BIT",
                                                 "VK_SHADER_STAGE_COMPUTE_BIT"};
std::uint32_t stages(const ReplayValue& value) {
    auto text = unwrap(value, "VkShaderStageFlags").text;
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); }), text.end());
    std::uint32_t result = 0;
    for(std::size_t first = 0; first < text.size();) {
        const auto end = text.find('|', first);
        const auto token = text.substr(first, end == std::string::npos ? text.size() - first : end - first);
        std::uint32_t flag = 0;
        if(token == "VK_SHADER_STAGE_ALL_GRAPHICS")
            flag = 31;
        else
            for(std::size_t n = 0; n < stage_names.size(); ++n)
                if(token == stage_names[n])
                    flag = 1u << n;
        require(flag != 0 && (result & flag) == 0, "unknown, empty or duplicate push stage flags");
        result |= flag;
        if(end == std::string::npos)
            break;
        require(end + 1 < text.size(), "trailing push stage separator");
        first = end + 1;
    }
    require(result != 0, "empty push stage mask");
    return result;
}
struct Range {
    std::uint32_t stages;
    std::size_t offset, size;
};
struct Layout {
    std::vector<Range> ranges;
    std::string signature;
    Json source;
};
Layout layout(const CppReplay& replay, const std::string& name) {
    const auto found = replay.objects.find(name);
    require(found != replay.objects.end() && found->second.creation.name == "VulkanReplay_CreatePipelineLayout",
            "pipeline layout creation is absent or ambiguous");
    const auto& object = found->second;
    const auto fields_ = fields(object.creation.arguments.at(1), object.values, "VkPipelineLayoutCreateInfo", 7);
    structure(fields_, "VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO", "VkPipelineLayoutCreateFlags");
    const auto count = number(fields_[5]);
    require(count <= 6, "push range count exceeds qualified stages");
    Layout result;
    result.source = object.source;
    std::uint32_t declared = 0;
    if(count != 0)
        for(const auto& value : array(fields_[6], object.values, "VkPushConstantRange", count)) {
            require(value.constructor == "VkPushConstantRange" && value.arguments.size() == 3,
                    "unqualified push range shape");
            const auto& f = value.arguments;
            const auto mask = stages(f[0]);
            const auto offset = number(f[1]), size = number(f[2]);
            require((declared & mask) == 0 && size > 0 && offset < 256 && size <= 256 - offset && offset % 4 == 0 &&
                        size % 4 == 0,
                    "invalid or oversized push range");
            declared |= mask;
            result.ranges.push_back({mask, offset, size});
        }
    std::sort(result.ranges.begin(), result.ranges.end(), [](const auto& a, const auto& b) {
        return std::tie(a.stages, a.offset, a.size) < std::tie(b.stages, b.offset, b.size);
    });
    Json identity = Json::array();
    for(const auto& r : result.ranges)
        identity.push_back({r.stages, r.offset, r.size});
    const auto serialized = identity.dump();
    result.signature = sha256(std::as_bytes(std::span(serialized.data(), serialized.size())));
    return result;
}
Json missing(const std::string& reason, const char* status = "unavailable") {
    return {{"status", status}, {"value", nullptr}, {"reason", reason}, {"evidence", Json::array()}};
}
} // namespace
PushState::PushState() {
    for(auto& stage : bytes_)
        stage.fill(-1);
}
void PushState::record(const CppReplay& replay, const ReplayCall& call, const ReplayDeclarations& values) {
    (void)values;
    if(!unsupported_.empty())
        return;
    try {
        const auto& a = call.arguments;
        require(a.size() == 6, "push command arity unsupported");
        const auto object = layout(replay, symbol(a[1], "VkPipelineLayout"));
        const auto mask = stages(a[2]);
        const auto offset = number(a[3]), size = number(a[4]);
        require(offset < 256 && size > 0 && size <= 256 - offset && offset % 4 == 0 && size % 4 == 0,
                "push update outside qualified byte range");
        for(auto byte = offset; byte < offset + size; ++byte) {
            std::uint32_t declared = 0;
            for(const auto& range : object.ranges)
                if(byte >= range.offset && byte < range.offset + range.size)
                    declared |= range.stages;
            require((mask & declared) == mask && (declared & mask) == declared,
                    "push update does not match overlapping layout stages/ranges");
        }
        const auto& resource = a[5];
        require(resource.constructor == "NV_GET_RESOURCE_STATIC" && resource.arguments.size() == 3 &&
                    (resource.arguments[0].text == "void *" || resource.arguments[0].text == "const void *") &&
                    number(resource.arguments[2]) == size,
                "push data is not an observed bounded opaque resource expression");
        const auto handle = number(resource.arguments[1]);
        require(handle <= 2147483647 && writes_.size() < 1024, "push resource/write count exceeds bounds");
        const auto index = static_cast<int>(writes_.size());
        writes_.push_back({object.signature, mask, offset, size, handle, call.event, call.source, resource.source});
        for(std::size_t stage = 0; stage < stage_names.size(); ++stage)
            if(mask & (1u << stage))
                for(auto byte = offset; byte < offset + size; ++byte)
                    bytes_[stage][byte] = index;
    } catch(const Unqualified& error) {
        unsupported_ = error.what();
    }
}
Json PushState::at_draw(const CppReplay& replay, const std::string& name) const {
    try {
        const auto object = layout(replay, name);
        Json ranges = Json::array();
        bool complete = true;
        for(std::size_t stage = 0; stage < 5; ++stage)
            for(const auto& range : object.ranges)
                if(range.stages & (1u << stage)) {
                    Json segments = Json::array();
                    for(auto at = range.offset; at < range.offset + range.size;) {
                        const auto written = bytes_[stage][at];
                        auto end = at + 1;
                        while(end < range.offset + range.size && bytes_[stage][end] == written)
                            ++end;
                        Json segment{{"offset", at},
                                     {"length", end - at},
                                     {"data", nullptr},
                                     {"input_sha256", nullptr},
                                     {"resource_ref", nullptr},
                                     {"read_artifact", nullptr}};
                        if(written < 0) {
                            segment["status"] = "unavailable";
                            segment["reason"] = "no recorded write for these stage bytes";
                            complete = false;
                        } else {
                            const auto& write = writes_.at(static_cast<std::size_t>(written));
                            segment["status"] = write.layout_signature == object.signature ? "not_read" : "unsupported";
                            segment["reason"] = write.layout_signature == object.signature
                                                    ? "serialized push input bytes have not been read"
                                                    : "last push layout is incompatible with this pipeline layout";
                            segment["write"] = {{"event_index", write.event},
                                                {"source", write.source},
                                                {"resource_source", write.resource_source},
                                                {"handle", write.handle},
                                                {"macro", "NV_GET_RESOURCE_STATIC"},
                                                {"declared_bytes", write.size},
                                                {"resource_offset", at - write.offset}};
                            complete &= write.layout_signature == object.signature;
                        }
                        segments.push_back(std::move(segment));
                        at = end;
                    }
                    ranges.push_back({{"stage", stage_names[stage]},
                                      {"offset", range.offset},
                                      {"length", range.size},
                                      {"segments", segments}});
                }
        if(ranges.empty())
            return missing("pipeline layout declares no graphics push constant ranges", "not_applicable");
        if(!unsupported_.empty())
            return missing(unsupported_, "unsupported");
        auto result = missing(complete ? "Request read_push_constants for opaque recorded input bytes"
                                       : "push ranges contain missing or incompatible writes");
        result["evidence"] = Json::array({object.source});
        result["coverage"] = {
            {"layout", name},
            {"layout_signature", object.signature},
            {"input_coverage_complete", complete},
            {"ranges", ranges},
            {"shader_usage", "layout-declared graphics stages; shader usage and data types are not reflected"}};
        return result;
    } catch(const Unqualified& error) {
        return missing(error.what(), "unsupported");
    }
}
void resolve_push_data(Json& field, const std::map<std::string, Json>& reads) {
    Json result = field;
    bool complete = true;
    Json values = Json::array();
    for(auto& range : result.at("coverage").at("ranges")) {
        std::string data;
        for(auto& segment : range.at("segments")) {
            if(segment.at("status") == "not_read" && segment.at("resource_ref").is_string()) {
                const auto ref = segment.at("resource_ref").get<std::string>();
                require(reads.contains(ref), "push input was not resolved by the verified reader");
                const auto& read = reads.at(ref);
                const auto& encoded = read.at("data").get_ref<const std::string&>();
                const auto offset = segment.at("write").at("resource_offset").get<std::size_t>(),
                           length = segment.at("length").get<std::size_t>();
                const auto declared = segment.at("write").at("declared_bytes").get<std::size_t>();
                require(declared <= 256 && declared > 0 && encoded.size() == declared * 2 && offset <= declared &&
                            length > 0 && length <= declared - offset &&
                            encoded.find_first_not_of("0123456789abcdef") == std::string::npos,
                        "push reader data does not cover the selected input bytes");
                segment["data"] = encoded.substr(offset * 2, length * 2);
                segment["input_sha256"] = read.at("sha256");
                segment["read_artifact"] = read.at("artifact_id");
                segment["status"] = "qualified_recorded_state";
                segment["reason"] = nullptr;
                data += segment.at("data").get<std::string>();
            } else
                complete = false;
        }
        values.push_back({{"stage", range.at("stage")},
                          {"offset", range.at("offset")},
                          {"length", range.at("length")},
                          {"data", data}});
    }
    if(complete) {
        result["status"] = "qualified_recorded_state";
        result["value"] = std::move(values);
        result["reason"] = nullptr;
    }
    field = std::move(result);
}
} // namespace ngm::detail
