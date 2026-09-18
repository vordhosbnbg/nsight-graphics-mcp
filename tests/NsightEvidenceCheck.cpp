#include "Check.hpp"
#include "ngm/NsightEvidence.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>

namespace {
using ngm::check::require;
using Json = nlohmann::json;
using Error = ngm::NsightEvidenceErrorCode;
using Limits = ngm::NsightEvidenceLimits;

std::string fixture(const std::filesystem::path& directory, const std::string& name) {
    std::ifstream input(directory / name, std::ios::binary);
    require(input.is_open(), "open sanitized evidence fixture " + name);
    std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    require(!input.bad() && text.size() <= Limits::maximum_bytes, "read bounded fixture " + name);
    return text;
}

template <typename Function>
void rejected(Error expected, Function&& function) {
    try {
        function();
    } catch(const ngm::NsightEvidenceError& error) {
        require(error.code() == expected,
                std::string("expected ") + std::string(ngm::nsight_evidence_error_name(expected)) + ", got " +
                    std::string(ngm::nsight_evidence_error_name(error.code())) + ": " + error.what());
        return;
    }
    require(false, "invalid evidence must fail with a typed error");
}

const ngm::NsightObject& object_with_uid(const std::vector<ngm::NsightObject>& objects, std::uint64_t uid) {
    const auto found =
        std::find_if(objects.begin(), objects.end(), [uid](const auto& object) { return object.uid == uid; });
    require(found != objects.end(), "observed object uid is retained");
    return *found;
}

std::string zero_array(std::size_t count) {
    std::string result = "[";
    result.reserve(2 * count + 2);
    for(std::size_t index = 0; index < count; ++index) {
        if(index != 0) {
            result += ',';
        }
        result += '0';
    }
    result += ']';
    return result;
}

void check_observed_pair(const std::filesystem::path& directory, bool second_release = false) {
    std::optional<std::string> reference_uuid;
    for(const auto* name : {"reference", "shader-error"}) {
        const auto prefix = std::string(name);
        const auto metadata = ngm::parse_nsight_metadata(fixture(directory, prefix + "-metadata.json"));
        require(metadata.metadata_version == 1 &&
                    metadata.nsight_version == (second_release ? "2026.2.0" : "2026.3.1") &&
                    metadata.nsight_version_build_id == (second_release ? "37991608" : "38722833"),
                "actual export schema and reported version/build are retained");
        require(metadata.primary_api == "Vulkan" && metadata.primary_gpu == "NVIDIA GeForce RTX 3080 Ti" &&
                    metadata.driver_vendor == "NVIDIA" && metadata.driver_version == "615.71",
                "metadata preserves exported GPU/driver text without padding its version");
        require(metadata.process_name == "ngm-vulkan-fixture" && metadata.captured_frame == "2" &&
                    metadata.resolution == "192x128" && metadata.non_portable == "False",
                "observed workload and portability fields retain their string representation");
        require(metadata.d3d12_core_version && metadata.d3d12_core_version->empty() &&
                    metadata.has_unsupported_operation == false,
                "present empty string and false boolean remain explicit values");
        require(metadata.graphics_apis && metadata.graphics_apis->at("Vulkan") == std::vector<std::string>{"general"} &&
                    metadata.graphics_features &&
                    metadata.graphics_features->at("general") == std::vector<std::string>{"Vulkan"},
                "observed API/feature maps retain their actual structure");
        require(metadata.collection && metadata.collection->info && metadata.collection->info->size() == 6,
                "the observed information collection is retained");
        if(second_release) {
            require(!metadata.collection->warnings,
                    "2026.2 omitted warnings remain absent, not an invented empty array");
        } else {
            require(metadata.collection->warnings && metadata.collection->warnings->size() == 1 &&
                        metadata.collection->warnings->front().find("replayer version 2026.3") != std::string::npos,
                    "the real replay-version warning is retained, not treated as successful GPU replay");
        }
        require(metadata.uuid.has_value(), "real capture UUID is present");
        if(reference_uuid) {
            require(metadata.uuid != reference_uuid, "the pair retains distinct capture UUIDs");
        } else {
            reference_uuid = metadata.uuid;
        }

        const auto events = ngm::parse_nsight_functions(fixture(directory, prefix + "-functions.json"));
        require(events.size() == 22 && events.front().function_name == "CaptureBegin" &&
                    events.back().function_name == "vkQueuePresentKHR",
                "the complete observed function inventory is retained in export order");
        require(events[14].event_index == 14 && events[14].function_name == "vkCmdDraw" && events[14].thread_index == 0,
                "draw event preserves its capture-scoped identity and thread");
        require(events[0].sequence_id == 0 && events[1].sequence_id == 0 && !events[8].sequence_id &&
                    !events[14].sequence_id && events[21].sequence_id == 8,
                "sequence ID zero, repeated sequence IDs, and absent sequence IDs are distinct");
        require(std::count_if(events.begin(), events.end(),
                              [](const auto& event) { return event.sequence_id.has_value(); }) == 10,
                "only ten observed events include sequence_id");

        const auto objects = ngm::parse_nsight_objects(fixture(directory, prefix + "-objects.json"));
        require(objects.size() == 32, "the complete observed object inventory is retained");
        const auto& palette = object_with_uid(objects, 26);
        require(palette.api == "Vulkan" && palette.object_name == "fixture.palette.primary" &&
                    palette.type_name == "Buffer" && palette.access_flags == 32,
                "object labels, types, and opaque access values are retained");
        require(object_with_uid(objects, 8).access_flags == 1280,
                "opaque access values retain their observed integers");
        for(const auto uid : {14U, 40U, 42U}) {
            require(object_with_uid(objects, uid).access_flags == (second_release ? 0U : 524288U),
                    "release-specific semaphore/fence access values are preserved without interpretation");
        }
        const auto& shader = object_with_uid(objects, 35);
        require(shader.type_name == "ShaderModule" &&
                    shader.object_name ==
                        (prefix == "reference" ? "fixture.scene.frag.spv" : "fixture.shader-error.frag.spv"),
                "the pair preserves actual shader-module labels without claiming shader contents or associations");
    }
}

void check_metadata_schema(const Json& observed) {
    const auto minimal = ngm::parse_nsight_metadata(R"({"metadata_version":1})");
    require(!minimal.uuid && !minimal.nsight_version && !minimal.nsight_version_build_id && !minimal.primary_api &&
                !minimal.primary_gpu && !minimal.driver_vendor && !minimal.driver_version && !minimal.os_information &&
                !minimal.process_name && !minimal.request_time && !minimal.captured_frame && !minimal.resolution &&
                !minimal.non_portable && !minimal.d3d12_core_version && !minimal.has_unsupported_operation &&
                !minimal.graphics_apis && !minimal.graphics_features && !minimal.collection,
            "missing optional fields are absent, not invented empty strings or false booleans");
    const auto empty = ngm::parse_nsight_metadata(
        R"({"metadata_version":1,"graphics_apis":{},"_metadata_collection_":{"warnings":[]}})");
    require(empty.graphics_apis && empty.graphics_apis->empty() && empty.collection && !empty.collection->info &&
                empty.collection->warnings && empty.collection->warnings->empty(),
            "present empty collections remain distinct from missing fields");
    rejected(Error::InvalidSchema, [] { ngm::parse_nsight_metadata("[]"); });
    rejected(Error::MissingField, [] { ngm::parse_nsight_metadata("{}"); });
    rejected(Error::UnsupportedVersion, [] { ngm::parse_nsight_metadata(R"({"metadata_version":2})"); });
    rejected(Error::UnsupportedVersion, [] { ngm::parse_nsight_metadata(R"({"metadata_version":0})"); });
    for(const auto& wrong : std::vector<Json>{-1, 1.0, "1", true, nullptr}) {
        auto value = observed;
        value["metadata_version"] = wrong;
        rejected(Error::InvalidField, [&] { ngm::parse_nsight_metadata(value.dump()); });
    }
    for(const auto* field : {"uuid", "nsight_version", "nsight_version_build_id", "primary_api", "primary_gpu",
                             "driver_vendor", "driver_version", "os_information", "process_name", "request_time",
                             "captured_frame", "resolution", "non_portable", "d3d12_core_version"}) {
        auto value = observed;
        value[field] = nullptr;
        rejected(Error::InvalidField, [&] { ngm::parse_nsight_metadata(value.dump()); });
    }
    auto wrong_boolean = observed;
    wrong_boolean["has_unsupported_operation"] = "False";
    rejected(Error::InvalidField, [&] { ngm::parse_nsight_metadata(wrong_boolean.dump()); });
    for(const auto* field : {"graphics_apis", "graphics_features", "_metadata_collection_"}) {
        auto value = observed;
        value[field] = Json::array();
        rejected(Error::InvalidSchema, [&] { ngm::parse_nsight_metadata(value.dump()); });
    }
    rejected(Error::InvalidSchema,
             [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"graphics_apis":{"Vulkan":"general"}})"); });
    rejected(Error::InvalidField,
             [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"graphics_features":{"general":[false]}})"); });
    rejected(Error::InvalidSchema,
             [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"_metadata_collection_":{"warnings":null}})"); });
    rejected(Error::InvalidField,
             [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"_metadata_collection_":{"info":[7]}})"); });
    const auto extended = ngm::parse_nsight_metadata(
        R"({"metadata_version":1,"extension":{"values":[1,true,null]},"process_environment":["SYNTHETIC_SECRET=do-not-return"],"process_command_line":"synthetic --secret do-not-return"})");
    require(extended.metadata_version == 1 && !extended.process_name,
            "bounded additional fields are accepted without adding environment, command line, or inferred identity");
}

void check_indirect_pair(const std::filesystem::path& directory, bool second_release) {
    for(const auto* name : {"indirect-reference", "indirect-parameter-error"}) {
        const auto events = ngm::parse_nsight_functions(fixture(directory, std::string(name) + "-functions.json"));
        require(events.size() == 23 && events[14].function_name == "vkCmdDrawIndirect" &&
                    events[15].function_name == "vkCmdDraw",
                "retain observed indirect workload inventory order");
        for(std::size_t index = 0; index < events.size(); ++index) {
            if(!second_release && index == 15) {
                require(events[index].indirect_index == 0, "observed indirect_index zero is distinct from absence");
            } else {
                require(!events[index].indirect_index, "absent indirect_index is not inferred from function names");
            }
        }
    }
}

void check_inventory_schema(const Json& functions, const Json& objects) {
    require(ngm::parse_nsight_functions("[]").empty() && ngm::parse_nsight_objects("[]").empty(),
            "empty inventories do not fabricate entries");
    rejected(Error::InvalidSchema, [] { ngm::parse_nsight_functions("{}"); });
    rejected(Error::InvalidSchema, [] { ngm::parse_nsight_objects("{}"); });
    rejected(Error::InvalidSchema, [] { ngm::parse_nsight_functions("[null]"); });
    rejected(Error::InvalidSchema, [] { ngm::parse_nsight_objects("[[]]"); });
    for(const auto* field : {"event_index", "function_name", "thread_index"}) {
        auto missing = functions;
        missing[0].erase(field);
        rejected(Error::MissingField, [&] { ngm::parse_nsight_functions(missing.dump()); });
    }
    for(const auto* field : {"uid", "api", "object_name", "type_name", "access_flags"}) {
        auto missing = objects;
        missing[0].erase(field);
        rejected(Error::MissingField, [&] { ngm::parse_nsight_objects(missing.dump()); });
    }
    const auto overflow = Json::parse("18446744073709551616");
    for(const auto& wrong : std::vector<Json>{-1, 0.5, 1.0, "1", true, nullptr, overflow}) {
        for(const auto* field : {"event_index", "thread_index", "sequence_id"}) {
            auto value = functions;
            value[0][field] = wrong;
            rejected(Error::InvalidField, [&] { ngm::parse_nsight_functions(value.dump()); });
        }
        for(const auto* field : {"uid", "access_flags"}) {
            auto value = objects;
            value[0][field] = wrong;
            rejected(Error::InvalidField, [&] { ngm::parse_nsight_objects(value.dump()); });
        }
    }
    auto wrong_name = functions;
    wrong_name[0]["function_name"] = nullptr;
    rejected(Error::InvalidField, [&] { ngm::parse_nsight_functions(wrong_name.dump()); });
    wrong_name[0]["function_name"] = "";
    rejected(Error::InvalidField, [&] { ngm::parse_nsight_functions(wrong_name.dump()); });
    for(const auto* field : {"api", "object_name", "type_name"}) {
        auto wrong = objects;
        wrong[0][field] = 42;
        rejected(Error::InvalidField, [&] { ngm::parse_nsight_objects(wrong.dump()); });
    }
    for(const auto* field : {"api", "type_name"}) {
        auto wrong = objects;
        wrong[0][field] = "";
        rejected(Error::InvalidField, [&] { ngm::parse_nsight_objects(wrong.dump()); });
    }
    auto unnamed = objects;
    unnamed[0]["object_name"] = "";
    require(ngm::parse_nsight_objects(unnamed.dump()).front().object_name.empty(),
            "an unnamed object is still an object");

    auto repeated_event = functions;
    repeated_event[1]["event_index"] = repeated_event[0]["event_index"];
    repeated_event[1]["thread_index"] = 17;
    rejected(Error::DuplicateIdentifier, [&] { ngm::parse_nsight_functions(repeated_event.dump()); });
    auto repeated_object = objects;
    repeated_object[1]["uid"] = repeated_object[0]["uid"];
    rejected(Error::DuplicateIdentifier, [&] { ngm::parse_nsight_objects(repeated_object.dump()); });

    auto noncontiguous = functions;
    noncontiguous[0]["event_index"] = std::numeric_limits<std::uint64_t>::max();
    noncontiguous[0]["thread_index"] = std::numeric_limits<std::uint64_t>::max();
    noncontiguous[0]["sequence_id"] = std::numeric_limits<std::uint64_t>::max();
    noncontiguous[0]["indirect_index"] = std::numeric_limits<std::uint64_t>::max();
    noncontiguous[0]["extension"] = {{"value", Json::array({false, nullptr})}};
    const auto preserved = ngm::parse_nsight_functions(noncontiguous.dump());
    require(preserved.front().event_index == std::numeric_limits<std::uint64_t>::max() &&
                preserved[1].event_index == 1 &&
                preserved.front().thread_index == std::numeric_limits<std::uint64_t>::max() &&
                preserved.front().sequence_id == std::numeric_limits<std::uint64_t>::max() &&
                preserved.front().indirect_index == std::numeric_limits<std::uint64_t>::max(),
            "unsigned 64-bit limits, noncontiguous IDs, extension fields, and export order are supported");
    auto maximum_object = objects;
    maximum_object[0]["uid"] = std::numeric_limits<std::uint64_t>::max();
    maximum_object[0]["access_flags"] = std::numeric_limits<std::uint64_t>::max();
    maximum_object[0]["extension"] = true;
    const auto preserved_object = ngm::parse_nsight_objects(maximum_object.dump()).front();
    require(preserved_object.uid == std::numeric_limits<std::uint64_t>::max() &&
                preserved_object.access_flags == std::numeric_limits<std::uint64_t>::max(),
            "opaque unsigned object fields keep their full range");
    for(const auto& invalid : std::vector<Json>{nullptr, true, -1, 1.5, "0", Json::array(), Json::object()}) {
        auto malformed = functions;
        malformed[0]["indirect_index"] = invalid;
        rejected(Error::InvalidField, [&] { ngm::parse_nsight_functions(malformed.dump()); });
    }
    rejected(Error::InvalidField, [] {
        ngm::parse_nsight_functions(
            R"([{"event_index":0,"function_name":"vkCmdDraw","thread_index":0,"indirect_index":18446744073709551616}])");
    });
}

void check_json_limits() {
    rejected(Error::MalformedJson, [] { ngm::parse_nsight_metadata(std::string_view{}); });
    for(const auto* text : {"", "[", "[1,]", "{} {}", "NaN", R"({"metadata_version":1,"text":"\uD800"})"}) {
        rejected(Error::MalformedJson, [&] { ngm::parse_nsight_metadata(text); });
    }
    const std::string invalid_utf8 = "{\"metadata_version\":1,\"text\":\"" + std::string(1, '\xff') + "\"}";
    rejected(Error::MalformedJson, [&] { ngm::parse_nsight_metadata(invalid_utf8); });
    const std::string nul_suffix = std::string(R"({"metadata_version":1})") + '\0' + "ignored";
    rejected(Error::MalformedJson, [&] { ngm::parse_nsight_metadata(nul_suffix); });
    require(ngm::parse_nsight_metadata(R"({"metadata_version":1,"uuid":"\u0000"})").uuid == std::string(1, '\0'),
            "escaped NUL is string data rather than a raw input terminator");
    rejected(Error::DuplicateKey, [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"metadata_version":1})"); });
    rejected(Error::DuplicateKey,
             [] { ngm::parse_nsight_metadata(R"({"metadata_version":1,"extension":{"x":0,"\u0078":1}})"); });
    rejected(Error::DuplicateKey, [] {
        ngm::parse_nsight_functions(
            R"([{"event_index":0,"event_index":1,"function_name":"CaptureBegin","thread_index":0}])");
    });
    try {
        ngm::parse_nsight_metadata(R"({"metadata_version":1,"process_environment":["SYNTHETIC_SECRET"],BROKEN})");
        require(false, "malformed raw process context must be rejected");
    } catch(const ngm::NsightEvidenceError& error) {
        require(error.code() == Error::MalformedJson &&
                    std::string(error.what()).find("SYNTHETIC_SECRET") == std::string::npos,
                "parse diagnostics do not copy sensitive raw input tokens");
    }

    const auto oversized = std::string(Limits::maximum_bytes + 1, ' ');
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_functions(oversized); });
    const auto nested = [](std::size_t arrays) {
        return "{\"metadata_version\":1,\"extension\":" + std::string(arrays, '[') + '0' + std::string(arrays, ']') +
               '}';
    };
    require(ngm::parse_nsight_metadata(nested(Limits::maximum_depth - 1)).metadata_version == 1,
            "the depth boundary includes the metadata root container");
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(nested(Limits::maximum_depth)); });
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(nested(10000)); });
    // Build adversarial deep text directly, never through a recursive JSON dump.
    const auto large_string = std::string(Limits::maximum_string_bytes, 'a');
    require(ngm::parse_nsight_metadata("{\"metadata_version\":1,\"uuid\":\"" + large_string + "\"}").uuid ==
                large_string,
            "maximum decoded string size is accepted");
    rejected(Error::LimitExceeded,
             [&] { ngm::parse_nsight_metadata("{\"metadata_version\":1,\"extension\":\"" + large_string + "a\"}"); });
    std::string escaped;
    escaped.reserve(Limits::maximum_string_bytes * 6);
    for(std::size_t index = 0; index < Limits::maximum_string_bytes; ++index) {
        escaped += "\\u0061";
    }
    require(ngm::parse_nsight_metadata("{\"metadata_version\":1,\"uuid\":\"" + escaped + "\"}").uuid == large_string,
            "string limits count decoded UTF-8 bytes rather than escape spelling");
    rejected(Error::LimitExceeded,
             [&] { ngm::parse_nsight_metadata("{\"metadata_version\":1,\"" + large_string + "a\":0}"); });

    const auto full_array = zero_array(Limits::maximum_container_entries);
    require(ngm::parse_nsight_metadata("{\"metadata_version\":1,\"extension\":" + full_array + '}').metadata_version ==
                1,
            "a bounded extension array at its entry limit is tolerated");
    const auto excessive_records = zero_array(Limits::maximum_records + 1);
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_functions(excessive_records); });
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_objects(excessive_records); });
    std::string too_many_values = "{\"metadata_version\":1,\"extension\":[";
    for(std::size_t index = 0; index <= Limits::maximum_json_values / Limits::maximum_container_entries; ++index) {
        if(index != 0) {
            too_many_values += ',';
        }
        too_many_values += full_array;
    }
    too_many_values += "]}";
    require(too_many_values.size() < Limits::maximum_bytes, "value-count probe remains inside the byte limit");
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(too_many_values); });

    Json metadata = {{"metadata_version", 1},
                     {"uuid", large_string},
                     {"nsight_version", large_string},
                     {"primary_api", large_string},
                     {"primary_gpu", large_string}};
    require(ngm::parse_nsight_metadata(metadata.dump()).uuid == large_string,
            "selected metadata accepts exactly 64 KiB of decoded strings");
    metadata["driver_vendor"] = "x";
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(metadata.dump()); });
    metadata = {
        {"metadata_version", 1},
        {"_metadata_collection_", {{"warnings", std::vector<std::string>(Limits::maximum_metadata_entries + 1)}}}};
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(metadata.dump()); });
    metadata = {{"metadata_version", 1}, {"graphics_apis", Json::object()}};
    for(std::size_t index = 0; index <= Limits::maximum_metadata_entries; ++index) {
        metadata["graphics_apis"][std::to_string(index)] = Json::array();
    }
    rejected(Error::LimitExceeded, [&] { ngm::parse_nsight_metadata(metadata.dump()); });
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "usage: NsightEvidenceCheck <2026.3.1 fixture directory> <2026.2.0 fixture directory>");
        const auto directory = std::filesystem::path(argv[1]);
        check_observed_pair(directory);
        check_observed_pair(argv[2], true);
        check_indirect_pair(directory, false);
        check_indirect_pair(argv[2], true);
        // Mutations below are synthetic parser regressions. The checked-in
        // fixture files retain fields/labels observed in actual Nsight exports.
        check_metadata_schema(Json::parse(fixture(directory, "reference-metadata.json")));
        check_inventory_schema(Json::parse(fixture(directory, "reference-functions.json")),
                               Json::parse(fixture(directory, "reference-objects.json")));
        check_json_limits();
    });
}
