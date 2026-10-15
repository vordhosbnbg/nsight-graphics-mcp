#pragma once

#include "ngm/Hash.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace ngm::check::descriptor_read {
using Json = nlohmann::json;
inline std::string digest(std::string_view value) {
    return ngm::sha256(std::as_bytes(std::span(value.data(), value.size())));
}
inline std::string helper(const std::string& name) {
    return "synthetic descriptor helper " + name + '\n';
}
inline Json profile() {
    Json files = Json::object();
    for(const auto* name : {"ReadOnlyDatabase.cpp", "ReadOnlyDatabase.h", "DataScope.cpp", "DataScope.h", "DllCommon.h",
                            "VulkanStructHydrator.cpp", "VulkanStructHydrator.h"})
        files[name] = digest(helper(name));
    return {{"schema_version", 1},
            {"profile", "cpu-descriptor-standin"},
            {"original_files", files},
            {"instrumented_files",
             {{"VulkanStructHydrator.cpp", std::string(64, 'a')}, {"VulkanStructHydrator.h", std::string(64, 'b')}}},
            {"bounds_sha256", std::string(64, 'c')},
            {"recipe_sha256", std::string(64, 'd')},
            {"patch_sha256", std::string(64, 'e')}};
}
inline Json registrations() {
    Json result = Json::array();
    const auto add = [&](unsigned type, const std::string& name, unsigned id, const std::string& symbol) {
        result.push_back({{"object_type", type},
                          {"type_name", name},
                          {"serialized_id", id},
                          {"symbol", symbol},
                          {"sources", Json::array({{{"path", "raw/project/FrameSetup00.cpp"},
                                                    {"start_line", 3},
                                                    {"end_line", 3},
                                                    {"sha256", std::string(64, 'a')},
                                                    {"byte_offset", 32},
                                                    {"byte_count", 64}}})}});
    };
    add(23, "VK_OBJECT_TYPE_DESCRIPTOR_SET", 100, "VkDescriptorSet_uid_29");
    add(9, "VK_OBJECT_TYPE_BUFFER", 200, "VkBuffer_uid_33");
    add(9, "VK_OBJECT_TYPE_BUFFER", 201, "VkBuffer_uid_34");
    add(14, "VK_OBJECT_TYPE_IMAGE_VIEW", 300, "VkImageView_uid_71");
    add(21, "VK_OBJECT_TYPE_SAMPLER", 400, "VkSampler_uid_72");
    return result;
}
inline Json object(unsigned type, unsigned id) {
    return {{"object_type", type}, {"serialized_id", id}};
}
inline Json response(const std::string& mode, std::uint64_t handle, std::uint64_t count, std::uint64_t capacity) {
    const bool images = mode == "images", array = mode == "array";
    const unsigned extent = images || array ? 2 : 1;
    Json write{{"dst_set", object(23, 100)}, {"binding", 2},
               {"array_element", 1},         {"descriptor_type", images ? 1 : array ? 8 : 6},
               {"descriptor_count", extent}, {"buffers", Json::array()},
               {"images", Json::array()}};
    for(unsigned i = 0; i < extent; ++i)
        if(images)
            write["images"].push_back(
                {{"image_view", object(14, 300)}, {"sampler", object(21, 400)}, {"image_layout", 5}});
        else
            write["buffers"].push_back({{"buffer", object(9, 200 + i)},
                                        {"offset", 32},
                                        {"range", mode == "whole" ? Json(UINT64_MAX) : Json(16)}});
    if(mode == "null")
        write["buffers"][0]["buffer"] = nullptr;
    return {{"schema_version", 1},
            {"profile", "cpu-descriptor-standin"},
            {"evidence_origin", "generated_helper_descriptor_writes"},
            {"scope", "CPU standin only"},
            {"handle", handle},
            {"write_count", count},
            {"input_bytes", 64 + 24 * extent},
            {"source_alignment_remainder", 4},
            {"input_sha256", digest("synthetic opaque descriptor")},
            {"output_capacity", capacity},
            {"output_used_bytes", 64 + 24 * extent},
            {"writes", Json::array({write})}};
}
} // namespace ngm::check::descriptor_read
