#include "Workflow.hpp"

#include "ngm/ImageEvidence.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/ProfileInspection.hpp"
#include "ngm/Version.hpp"

#include <fastmcpp/util/pagination.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <limits>
#include <string_view>
#include <unistd.h>

namespace ngm {
namespace {
using Json = fastmcpp::Json;
using Schema = Json;
constexpr std::size_t text_limit = 64 * 1024;
constexpr std::size_t result_limit = 1024 * 1024;

Schema string_schema(std::size_t maximum, std::size_t minimum = 0) {
    return {{"type", "string"}, {"minLength", minimum}, {"maxLength", maximum}};
}
Schema integer_schema(std::uint64_t minimum = 0, std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max()) {
    return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum}};
}
Schema boolean_schema() {
    return {{"type", "boolean"}};
}
Schema array_schema(Schema items, std::size_t maximum) {
    return {{"type", "array"}, {"items", std::move(items)}, {"maxItems", maximum}};
}
Schema object_schema(Json properties = Json::object(), Json required = Json::array()) {
    return {{"type", "object"},
            {"properties", std::move(properties)},
            {"required", std::move(required)},
            {"additionalProperties", false}};
}
Schema nullable(Schema value) {
    value["type"] = Json::array({value["type"], "null"});
    return value;
}
Schema identity_schema() {
    return object_schema(
        {{"job_id", string_schema(256, 1)}, {"capture_id", string_schema(39, 39)}, {"attempt", integer_schema(1)}},
        {"job_id", "capture_id", "attempt"});
}
Schema summary_schema() {
    return object_schema(
        {{"id", string_schema(39, 39)},
         {"status", {{"type", "string"}, {"enum", {"staging", "complete", "failed", "expired"}}}},
         {"created_at_ms", {{"type", "integer"}}},
         {"completed_at_ms", {{"type", {"integer", "null"}}}},
         {"bytes", integer_schema()},
         {"pinned", boolean_schema()},
         {"in_use", boolean_schema()},
         {"quarantined", boolean_schema()},
         {"reason", string_schema(4096)}},
        {"id", "status", "created_at_ms", "completed_at_ms", "bytes", "pinned", "in_use", "quarantined", "reason"});
}
Schema info_schema() {
    auto result = summary_schema();
    result["properties"]["provenance"] = {{"type", "object"},
                                          {"description", "At most 64 KiB of recorded provenance."}};
    result["properties"]["required_outputs"] = array_schema(string_schema(512, 1), 128);
    result["properties"]["file_count"] = integer_schema(0, 4096);
    for(const auto* name : {"provenance", "required_outputs", "file_count"}) {
        result["required"].push_back(name);
    }
    return result;
}
Schema usage_schema() {
    Json properties = Json::object();
    Json required = Json::array();
    for(const auto* name : {"completed_bytes", "staging_bytes", "trash_bytes", "metadata_bytes", "total_bytes",
                            "pinned_bytes", "in_use_bytes", "max_bytes"}) {
        properties[name] = integer_schema();
        required.push_back(name);
    }
    properties["quota_exceeded"] = boolean_schema();
    required.push_back("quota_exceeded");
    return object_schema(std::move(properties), std::move(required));
}
Schema job_schema() {
    return object_schema(
        {{"identity", identity_schema()},
         {"gpu_key", string_schema(256, 1)},
         {"state",
          {{"type", "string"}, {"enum", {"queued", "running", "succeeded", "failed", "cancelled", "timed_out"}}}},
         {"stop_reason", {{"type", {"string", "null"}}, {"enum", {"cancellation", "deadline", "shutdown", nullptr}}}},
         {"worker_running", boolean_schema()},
         {"finalization_pending", boolean_schema()},
         {"cleanup_confirmed", boolean_schema()},
         {"gpu_reserved", boolean_schema()},
         {"error", string_schema(4096)},
         {"artifact_ids", array_schema(string_schema(39, 39), 1)},
         {"elapsed_ms", integer_schema()}},
        {"identity", "gpu_key", "state", "stop_reason", "worker_running", "finalization_pending", "cleanup_confirmed",
         "gpu_reserved", "error", "artifact_ids", "elapsed_ms"});
}

Schema inspection_schema() {
    const auto producer = object_schema(
        {{"path", string_schema(4096, 1)}, {"version", string_schema(64, 1)}, {"build", string_schema(64, 1)}},
        {"path", "version", "build"});
    return object_schema({{"capture_id", string_schema(39, 39)},
                          {"capture_uuid", nullable(string_schema(NsightEvidenceLimits::maximum_string_bytes))},
                          {"evidence_origin", {{"type", "string"}, {"enum", {"nsight_export"}}}},
                          {"schema_profile", string_schema(64, 1)},
                          {"metadata_version", integer_schema(1, 1)},
                          {"producer", object_schema({{"capture_tool", producer},
                                                      {"replay_tool", producer},
                                                      {"metadata_nsight_version", string_schema(64, 1)},
                                                      {"metadata_nsight_version_build_id", string_schema(64, 1)}},
                                                     {"capture_tool", "replay_tool", "metadata_nsight_version",
                                                      "metadata_nsight_version_build_id"})},
                          {"source", object_schema({{"kind", string_schema(32, 1)},
                                                    {"path", string_schema(512, 1)},
                                                    {"bytes", integer_schema(0, NsightEvidenceLimits::maximum_bytes)}},
                                                   {"kind", "path", "bytes"})},
                          {"metadata_source", string_schema(512, 1)},
                          {"report_source", string_schema(512, 1)},
                          {"unavailable_from_these_exports", array_schema(string_schema(64, 1), 8)}},
                         {"capture_id", "capture_uuid", "evidence_origin", "schema_profile", "metadata_version",
                          "producer", "source", "metadata_source", "report_source", "unavailable_from_these_exports"});
}

Schema metadata_schema() {
    auto properties = Json::object();
    auto required = Json::array();
    for(const auto* name : {"uuid", "nsight_version", "nsight_version_build_id", "primary_api", "primary_gpu",
                            "driver_vendor", "driver_version", "os_information", "process_name", "request_time",
                            "captured_frame", "resolution", "non_portable", "d3d12_core_version"}) {
        properties[name] = nullable(string_schema(NsightEvidenceLimits::maximum_string_bytes));
    }
    properties["metadata_version"] = integer_schema(1, 1);
    properties["has_unsupported_operation"] = nullable(boolean_schema());
    const auto strings = array_schema(string_schema(NsightEvidenceLimits::maximum_string_bytes),
                                      NsightEvidenceLimits::maximum_metadata_entries);
    for(const auto* name : {"graphics_apis", "graphics_features"}) {
        properties[name] = {{"type", {"object", "null"}},
                            {"maxProperties", NsightEvidenceLimits::maximum_metadata_entries},
                            {"propertyNames", string_schema(NsightEvidenceLimits::maximum_string_bytes)},
                            {"additionalProperties", strings}};
    }
    properties["_metadata_collection_"] =
        nullable(object_schema({{"info", nullable(strings)}, {"warnings", nullable(strings)}}, {"info", "warnings"}));
    for(const auto& [name, unused] : properties.items()) {
        (void)unused;
        required.push_back(name);
    }
    auto result = inspection_schema();
    result["properties"]["metadata"] = object_schema(std::move(properties), std::move(required));
    result["required"].push_back("metadata");
    return result;
}

Schema inventory_schema(const char* name, Schema record) {
    auto result = inspection_schema();
    result["properties"][name] = array_schema(std::move(record), InspectionService::maximum_page_records);
    result["properties"]["offset"] = integer_schema(0, NsightEvidenceLimits::maximum_records);
    result["properties"]["total"] = integer_schema(0, NsightEvidenceLimits::maximum_records);
    result["properties"]["next_offset"] = nullable(integer_schema(0, NsightEvidenceLimits::maximum_records));
    for(const auto* field : {name, "offset", "total", "next_offset"}) {
        result["required"].push_back(field);
    }
    return result;
}

Schema cpp_inspection_schema() {
    const auto tool = object_schema(
        {{"path", string_schema(4096, 1)}, {"version", string_schema(64, 1)}, {"build", string_schema(64, 1)}},
        {"path", "version", "build"});
    return object_schema(
        {{"capture_id", string_schema(39, 39)},
         {"evidence_origin", {{"type", "string"}, {"enum", {"nsight_generated_cpp"}}}},
         {"schema_profile", string_schema(80, 1)},
         {"producer", object_schema({{"capture_tool", tool}, {"replay_tool", tool}, {"cli_tool", tool}},
                                    {"capture_tool", "replay_tool", "cli_tool"})},
         {"index_source", string_schema(512, 1)},
         {"metadata_source", string_schema(512, 1)},
         {"report_source", string_schema(512, 1)},
         {"has_unsupported_operation", boolean_schema()}},
        {"capture_id", "evidence_origin", "schema_profile", "producer", "index_source", "metadata_source",
         "report_source", "has_unsupported_operation"});
}
Schema cpp_file_schema() {
    return object_schema({{"path", string_schema(512, 1)},
                          {"bytes", integer_schema(0, 4U * 1024U * 1024U)},
                          {"sha256", string_schema(64, 64)}},
                         {"path", "bytes", "sha256"});
}
Schema cpp_source_schema() {
    auto result = cpp_inspection_schema();
    result["properties"]["source"] = cpp_file_schema();
    result["properties"]["start_line"] = integer_schema(1, 4194304);
    result["properties"]["total_lines"] = integer_schema(0, 4194304);
    result["properties"]["next_line"] = nullable(integer_schema(1, 4194304));
    result["properties"]["lines"] =
        array_schema(object_schema({{"number", integer_schema(1, 4194304)},
                                    {"text", string_schema(InspectionService::maximum_page_bytes)}},
                                   {"number", "text"}),
                     200);
    for(const auto* key : {"source", "start_line", "total_lines", "next_line", "lines"})
        result["required"].push_back(key);
    return result;
}
Schema cpp_draws_schema() {
    auto result = cpp_inspection_schema();
    const auto span = object_schema({{"path", string_schema(512, 1)},
                                     {"start_line", integer_schema(1, 4194304)},
                                     {"end_line", integer_schema(1, 4194304)}},
                                    {"path", "start_line", "end_line"});
    const auto stage = object_schema({{"stage", string_schema(64, 1)},
                                      {"module", string_schema(64, 1)},
                                      {"entry_point_expression", string_schema(258, 1)},
                                      {"stage_source", span},
                                      {"module_source", span},
                                      {"resource_handle", integer_schema()},
                                      {"declared_bytes", integer_schema(1, 16U * 1024U * 1024U)}},
                                     {"stage", "module", "entry_point_expression", "stage_source", "module_source",
                                      "resource_handle", "declared_bytes"});
    const auto pipeline =
        object_schema({{"symbol", string_schema(64, 1)}, {"source", span}, {"stages", array_schema(stage, 8)}},
                      {"symbol", "source", "stages"});
    const auto binding =
        object_schema({{"symbol", string_schema(64, 1)}, {"event_index", integer_schema()}, {"source", span}},
                      {"symbol", "event_index", "source"});
    const auto draw = object_schema(
        {{"recording", string_schema(65536, 1)},
         {"event_index", integer_schema()},
         {"function", string_schema(64, 1)},
         {"arguments", array_schema(string_schema(16384, 1), 5)},
         {"source", span},
         {"association_status", {{"type", "string"}, {"enum", {"resolved_source_relationship", "unavailable"}}}},
         {"reason", nullable(string_schema(256, 1))},
         {"pipeline_bind", nullable(binding)},
         {"pipeline", nullable(pipeline)}},
        {"recording", "event_index", "function", "arguments", "source", "association_status", "reason", "pipeline_bind",
         "pipeline"});
    result["properties"]["draws"] = array_schema(draw, 100);
    result["properties"]["unsupported_recordings"] = array_schema(
        object_schema({{"recording", string_schema(65536, 1)}, {"source", span}, {"reason", string_schema(256, 1)}},
                      {"recording", "source", "reason"}),
        100);
    result["properties"]["unsupported_objects"] = array_schema(
        object_schema({{"symbol", string_schema(65536)}, {"source", span}, {"reason", string_schema(256, 1)}},
                      {"symbol", "reason"}),
        100);
    result["properties"]["source_files"] = array_schema(cpp_file_schema(), 64);
    result["properties"]["association_scope"] = string_schema(512, 1);
    result["properties"]["section"] = {
        {"type", "string"}, {"maxLength", 32}, {"enum", {"draws", "unsupported_recordings", "unsupported_objects"}}};
    for(const auto* key :
        {"draws_total", "unsupported_recordings_total", "unsupported_objects_total", "offset", "total"})
        result["properties"][key] = integer_schema(0, 100000);
    result["properties"]["next_offset"] = nullable(integer_schema(0, 100000));
    for(const auto& [key, unused] : result["properties"].items()) {
        (void)unused;
        if(std::find(result["required"].begin(), result["required"].end(), key) == result["required"].end())
            result["required"].push_back(key);
    }
    return result;
}

Schema resource_span_schema() {
    return object_schema({{"path", string_schema(512, 1)},
                          {"start_line", integer_schema(1, 4194304)},
                          {"end_line", integer_schema(1, 4194304)},
                          {"sha256", string_schema(64, 64)},
                          {"byte_offset", integer_schema(0, 4194304)},
                          {"byte_count", integer_schema(1, 4194304)}},
                         {"path", "start_line", "end_line"});
}
Schema resource_reference_schema() {
    auto span = resource_span_schema();
    for(const auto* key : {"sha256", "byte_offset", "byte_count"})
        span["required"].push_back(key);
    return object_schema(
        {{"resource_ref", string_schema(64, 64)},
         {"handle", integer_schema(0, 2147483647)},
         {"macro",
          {{"type", "string"}, {"enum", {"NV_GET_RESOURCE", "NV_GET_RESOURCE_CHECKED", "NV_GET_RESOURCE_STATIC"}}}},
         {"type_expression", string_schema(256, 1)},
         {"declared_bytes", nullable(integer_schema(1, 16777216))},
         {"expected_bytes", nullable(integer_schema(1, 16777216))},
         {"conditional", boolean_schema()},
         {"readable", boolean_schema()},
         {"reason", nullable(string_schema(256, 1))},
         {"source", span}},
        {"resource_ref", "handle", "macro", "type_expression", "declared_bytes", "expected_bytes", "conditional",
         "readable", "reason", "source"});
}
Schema cpp_resources_schema() {
    auto result = cpp_inspection_schema();
    result["properties"]["resources"] = array_schema(resource_reference_schema(), 100);
    result["properties"]["unsupported"] = array_schema(
        object_schema({{"source", resource_span_schema()}, {"reason", string_schema(256, 1)}}, {"source", "reason"}),
        100);
    result["properties"]["source_files"] = array_schema(cpp_file_schema(), 64);
    result["properties"]["association_scope"] = string_schema(512, 1);
    result["properties"]["source_scope"] = string_schema(512, 1);
    result["properties"]["section"] = {{"type", "string"}, {"enum", {"resources", "unsupported"}}};
    for(const auto* key : {"resources_total", "unsupported_total", "offset", "total"})
        result["properties"][key] = integer_schema(0, 100000);
    result["properties"]["next_offset"] = nullable(integer_schema(0, 100000));
    for(const auto& [key, unused] : result["properties"].items()) {
        (void)unused;
        if(std::find(result["required"].begin(), result["required"].end(), key) == result["required"].end())
            result["required"].push_back(key);
    }
    return result;
}
Schema cpp_resource_schema() {
    const auto input = object_schema({{"bytes", integer_schema(0, 268435456)}, {"sha256", string_schema(64, 64)}},
                                     {"bytes", "sha256"});
    auto result = object_schema(
        {{"capture_id", string_schema(39, 39)},
         {"resource_ref", string_schema(64, 64)},
         {"reference", resource_reference_schema()},
         {"helper_profile", string_schema(128, 1)},
         {"worker_sha256", string_schema(64, 64)},
         {"inputs", object_schema({{"data.bin", input}, {"data.bin.rec", input}}, {"data.bin", "data.bin.rec"})},
         {"offset", integer_schema(0, 16777216)},
         {"total_bytes", integer_schema(1, 16777216)},
         {"returned_bytes", integer_schema(0, 65536)},
         {"next_offset", nullable(integer_schema(0, 16777216))},
         {"encoding", {{"type", "string"}, {"enum", {"hex"}}}},
         {"data", string_schema(131072)},
         {"sha256", string_schema(64, 64)},
         {"evidence_origin", {{"type", "string"}, {"enum", {"nsight_generated_resource_read"}}}},
         {"scope", string_schema(256, 1)},
         {"artifact_id", string_schema(39, 39)},
         {"report_path", string_schema(512, 1)},
         {"producer", cpp_inspection_schema()["properties"]["producer"]}});
    for(const auto& [key, unused] : result["properties"].items()) {
        (void)unused;
        result["required"].push_back(key);
    }
    return result;
}

// fastmcpp's optional validator maps failures to protocol errors and does not
// enforce closed objects. This small validator applies the declared input
// subset before any operation; malformed tool arguments become tool errors.
void validate(const Json& value, const Schema& schema, const std::string& field = "arguments") {
    const auto type = schema.at("type").get<std::string>();
    if(type == "object") {
        if(!value.is_object()) {
            throw std::invalid_argument(field + " must be an object");
        }
        for(const auto& required : schema.at("required")) {
            if(!value.contains(required.get<std::string>())) {
                throw std::invalid_argument(field + " requires " + required.get<std::string>());
            }
        }
        for(const auto& [name, item] : value.items()) {
            if(!schema.at("properties").contains(name)) {
                throw std::invalid_argument("Unknown " + field + " property: " + name);
            }
            validate(item, schema.at("properties").at(name), field + '.' + name);
        }
    } else if(type == "array") {
        if(!value.is_array() || value.size() > schema.at("maxItems").get<std::size_t>()) {
            throw std::invalid_argument(field + " must be an array within its item limit");
        }
        for(const auto& item : value) {
            validate(item, schema.at("items"), field + "[]");
        }
    } else if(type == "string") {
        if(!value.is_string()) {
            throw std::invalid_argument(field + " must be a string");
        }
        const auto& text = value.get_ref<const std::string&>();
        if(text.size() < schema.value("minLength", std::size_t{0}) ||
           text.size() > schema.at("maxLength").get<std::size_t>() || text.find('\0') != std::string::npos) {
            throw std::invalid_argument(field + " exceeds its byte limits or contains a NUL byte");
        }
    } else if(type == "integer") {
        if(!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
           value.get<std::uint64_t>() < schema.at("minimum").get<std::uint64_t>() ||
           value.get<std::uint64_t>() > schema.at("maximum").get<std::uint64_t>()) {
            throw std::invalid_argument(field + " must be an integer within the declared range");
        }
    } else if(type == "boolean" && !value.is_boolean()) {
        throw std::invalid_argument(field + " must be a boolean");
    }
}

std::filesystem::path absolute_path(const Json& value, bool executable = false) {
    const std::filesystem::path path(value.get<std::string>());
    std::error_code error;
    if(!path.is_absolute()) {
        throw std::invalid_argument("Expected an absolute path");
    }
    if(executable) {
        if(!std::filesystem::is_regular_file(path, error) || error || access(path.c_str(), X_OK) != 0) {
            throw std::invalid_argument("Application executable must be an existing executable regular file");
        }
    } else if(!std::filesystem::is_directory(path, error) || error) {
        throw std::invalid_argument("Expected an existing absolute directory");
    }
    return path;
}

bool path_contains(const std::filesystem::path& parent, const std::filesystem::path& child) {
    const auto prefix = parent.has_filename() ? parent : parent.parent_path();
    return std::mismatch(prefix.begin(), prefix.end(), child.begin(), child.end()).first == prefix.end();
}
void validate_import_location(const std::filesystem::path& source, const std::filesystem::path& root) {
    if(root.empty()) {
        return; // service() reports the missing explicit root without opening it.
    }
    const auto overlaps = [](const auto& left, const auto& right) {
        return path_contains(left, right) || path_contains(right, left);
    };
    // Match the core's lexical paths, then resolve existing ancestors (including
    // aliases) without creating the possibly absent store. The store repeats
    // its authoritative containment/no-follow checks when importing.
    const auto normalized_source = source.lexically_normal();
    const auto normalized_root = root.lexically_normal();
    if(overlaps(normalized_source, normalized_root) || overlaps(std::filesystem::weakly_canonical(normalized_source),
                                                                std::filesystem::weakly_canonical(normalized_root))) {
        throw std::invalid_argument("Import source must not contain or reside within the artifact store");
    }
}

std::string artifact_id(const Json& arguments, const char* field = "artifact_id") {
    const auto id = arguments.at(field).get<std::string>();
    if(id.size() != 39 || !id.starts_with("bundle-") || !std::all_of(id.begin() + 7, id.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
       })) {
        throw std::invalid_argument(std::string(field) + " must be a returned bundle ID");
    }
    return id;
}
std::string job_id(const Json& arguments) {
    const auto id = arguments.at("job_id").get<std::string>();
    if(!id.starts_with("job-") ||
       !std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isalnum(c) || c == '-'; })) {
        throw std::invalid_argument("job_id must be a returned job ID");
    }
    return id;
}
void relative_path(const std::string& value, bool partition = true) {
    const std::filesystem::path path(value);
    if(path.is_absolute() || value.empty() || value.back() == '/' || path.generic_string() != value ||
       (partition && !value.starts_with("raw/") && !value.starts_with("derived/"))) {
        throw std::invalid_argument("Use an inventoried relative file path below raw/ or derived/");
    }
    for(const auto& component : path) {
        if(component.empty() || component == "." || component == "..") {
            throw std::invalid_argument("Artifact paths cannot traverse directories");
        }
    }
}

std::string bounded_diagnostic(std::string_view message, std::size_t maximum) {
    // External errors can contain filesystem bytes or end within a UTF-8
    // sequence. Replacement is confined to diagnostics: evidence text is
    // checked strictly and never repaired.
    return Json::parse(Json(message.substr(0, maximum)).dump(-1, ' ', false, Json::error_handler_t::replace))
        .get<std::string>();
}
Json tool_error(std::string_view code, std::string message) {
    return {{"isError", true},
            {"content", Json::array({{{"type", "text"},
                                      {"text", std::string(code) + ": " + bounded_diagnostic(message, 4096)}}})}};
}

void add_tool(fastmcpp::tools::ToolManager& tools, const char* name, Schema input, Schema output,
              const char* description, bool read_only, bool destructive, fastmcpp::tools::Tool::Fn operation) {
    fastmcpp::tools::Tool tool(
        name, input, std::move(output), [input, operation = std::move(operation)](const Json& arguments) {
            try {
                validate(arguments, input);
                auto result = operation(arguments);
                if(result.dump().size() > result_limit) {
                    return tool_error("result_limit", "Result exceeds 1 MiB; request a smaller page");
                }
                return result;
            } catch(const InspectionError& error) {
                return tool_error(inspection_error_name(error.code()), error.what());
            } catch(const ArtifactError& error) {
                return tool_error(artifact_error_name(error.code()), error.what());
            } catch(const std::invalid_argument& error) {
                return tool_error("invalid_argument", error.what());
            } catch(const std::exception& error) {
                return tool_error("operation_failed", error.what());
            }
        });
    tool.set_description(description);
    tool.set_validate_args(false);
    tool.set_annotations({{"readOnlyHint", read_only}, {"destructiveHint", destructive}, {"openWorldHint", false}});
    tools.register_tool(tool);
}
} // namespace

Json implemented_tool_names() {
    return Json::array({"capabilities",
                        "capture",
                        "capture_cpp",
                        "job_status",
                        "job_cancel",
                        "artifact_list",
                        "artifact_info",
                        "artifact_files",
                        "artifact_read",
                        "artifact_pin",
                        "artifact_usage",
                        "artifact_prune",
                        "artifact_import",
                        "artifact_compare_images",
                        "artifact_preview_image",
                        "capture_metadata",
                        "capture_events",
                        "capture_objects",
                        "capture_cpp_source",
                        "capture_cpp_draws",
                        "capture_cpp_resources",
                        "capture_cpp_resource",
                        "profile",
                        "profile_metadata",
                        "profile_metrics",
                        "profile_compare"});
}

WorkflowTools::WorkflowTools(ServerOptions options) : options_(std::move(options)) {}
CaptureService& WorkflowTools::service() {
    if(options_.artifacts.root.empty()) {
        throw std::invalid_argument("Workflow tools require launching the server with --artifact-root ABS_PATH");
    }
    if(!service_) {
        service_ = std::make_unique<CaptureService>(
            CaptureServiceOptions{options_.artifacts, options_.nsight_root, capture_environment()});
    }
    return *service_;
}
bool WorkflowTools::shutdown() {
    if(!service_) {
        return true;
    }
    const auto result = service_->shutdown(std::chrono::seconds(5));
    return result.workers_joined && result.cleanup_confirmed;
}

void WorkflowTools::register_tools(fastmcpp::tools::ToolManager& tools) {
    const auto profile_ref =
        object_schema({{"path", string_schema(512, 1)}, {"sha256", string_schema(64, 64)}}, {"path", "sha256"});
    const auto settings_input =
        object_schema({{"delimiter", {{"type", "string"}, {"maxLength", 7}, {"enum", {"frames", "submits"}}}},
                       {"start_after", integer_schema(0, 1000000)},
                       {"limit", integer_schema(1, 1000)},
                       {"duration_ms", integer_schema(1, 10000)},
                       {"architecture", string_schema(64, 1)},
                       {"metric_set", string_schema(64, 1)}},
                      {"architecture"});
    auto profile_input = object_schema({{"executable", string_schema(4096, 1)},
                                        {"working_directory", string_schema(4096, 1)},
                                        {"arguments", array_schema(string_schema(4096), 256)},
                                        {"settings", settings_input},
                                        {"timeout_ms", integer_schema(1, 600000)},
                                        {"pin", boolean_schema()},
                                        {"application_output_option", string_schema(64)}},
                                       {"executable", "working_directory", "settings"});
    add_tool(tools, "profile", profile_input,
             object_schema({{"identity", identity_schema()}, {"artifact_id", string_schema(39, 39)}},
                           {"identity", "artifact_id"}),
             "Submit a fresh live-target GPU Trace job, serialized with captures. Requires profiling permission, "
             "matching qualified "
             "Nsight tools and an explicit architecture name from ngfx help. Defaults: start after 30 submits, limit 3 "
             "submits, "
             "duration 1000 ms, Throughput Metrics, deadline 120000 ms. Clocks are always unaltered; permissions are "
             "never changed. "
             "Poll job_status. This does not verify application correctness, warmup sufficiency or repeated-run "
             "equivalence.",
             false, false, [this](const Json& arguments) {
                 CaptureRequest request;
                 request.format = CaptureFormat::Profile;
                 request.executable = absolute_path(arguments.at("executable"), true);
                 request.working_directory = absolute_path(arguments.at("working_directory"));
                 request.arguments = arguments.value("arguments", std::vector<std::string>{});
                 request.timeout = std::chrono::milliseconds(arguments.value("timeout_ms", std::uint64_t{120000}));
                 request.pin = arguments.value("pin", false);
                 request.application_output_option = arguments.value("application_output_option", "");
                 if(!request.application_output_option.empty() &&
                    (request.application_output_option.front() != '-' ||
                     request.application_output_option.find_first_of(" \t\r\n") != std::string::npos))
                     throw std::invalid_argument("application_output_option must be one option starting with '-'");
                 const auto& settings = arguments.at("settings");
                 request.profile = {settings.value("delimiter", "submits"),
                                    settings.value("start_after", std::uint64_t{30}),
                                    settings.value("limit", std::uint64_t{3}),
                                    settings.value("duration_ms", std::uint64_t{1000}),
                                    settings.at("architecture"),
                                    settings.value("metric_set", "Throughput Metrics")};
                 return Json(service().capture(std::move(request)));
             });
    Json exported_fields = Json::object();
    for(const auto key : {"API", "Chip Name", "Start After", "Max Duration ", "Limited To", "V-Sync Mode", "GPU Clocks",
                          "Metric Set", "Real-Time Shader Profiler", "Multi-Pass Metrics", "Time Every Action"})
        exported_fields[key] = string_schema(4096);
    auto requested_schema = settings_input;
    requested_schema["properties"]["format"] = {{"type", "string"}, {"enum", {"profile"}}};
    requested_schema["properties"]["gpu_clocks"] = {{"type", "string"}, {"enum", {"unaltered"}}};
    requested_schema["properties"]["multi_pass"] = boolean_schema();
    requested_schema["properties"]["timeout_ms"] = integer_schema(1, 600000);
    requested_schema["required"] = {"format",       "delimiter",  "start_after", "limit",      "duration_ms",
                                    "architecture", "metric_set", "gpu_clocks",  "multi_pass", "timeout_ms"};
    const auto profile_tables = object_schema({{"frame_duration", profile_ref},
                                               {"frame_metrics", profile_ref},
                                               {"event_durations", profile_ref},
                                               {"regime_metrics", profile_ref}},
                                              {"frame_duration", "frame_metrics", "event_durations", "regime_metrics"});
    add_tool(tools, "profile_metadata", object_schema({{"profile_id", string_schema(39, 39)}}, {"profile_id"}),
             object_schema({{"profile_id", string_schema(39, 39)},
                            {"producer", string_schema(128, 1)},
                            {"gpu", string_schema(4096, 1)},
                            {"driver", string_schema(4096, 1)},
                            {"settings", object_schema(exported_fields)},
                            {"requested", requested_schema},
                            {"trace", profile_ref},
                            {"reproduction", profile_ref},
                            {"tables", profile_tables},
                            {"scope", string_schema(512)}},
                           {"profile_id", "producer", "gpu", "driver", "settings", "requested", "trace", "reproduction",
                            "tables", "scope"}),
             "Inspect a complete service-produced GPU Trace using retained producer/settings evidence. No live tool or "
             "GPU is required. "
             "Imports and failed attempts are not qualified profiles; inspect them through artifact tools.",
             true, false, [this](const Json& args) {
                 return ProfileInspection(service().artifacts()).metadata(artifact_id(args, "profile_id"));
             });
    const auto number = object_schema({{"column_index", integer_schema(0, 4096)},
                                       {"column_name", nullable(string_schema(4096))},
                                       {"text", string_schema(4096, 1)},
                                       {"value", {{"type", "number"}}},
                                       {"unit", nullable(string_schema(16))}},
                                      {"column_index", "column_name", "text", "value", "unit"});
    const auto profile_row = object_schema({{"row_index", integer_schema(0, 4096)},
                                            {"label", string_schema(4096, 1)},
                                            {"values", array_schema(number, 64)}},
                                           {"row_index", "label", "values"});
    add_tool(tools, "profile_metrics",
             object_schema({{"profile_id", string_schema(39, 39)},
                            {"table",
                             {{"type", "string"},
                              {"maxLength", 16},
                              {"enum", {"frame_duration", "frame_metrics", "event_durations", "regime_metrics"}}}},
                            {"offset", integer_schema(0, 4096)},
                            {"limit", integer_schema(1, 100)},
                            {"column_offset", integer_schema(0, 4096)},
                            {"column_limit", integer_schema(1, 64)}},
                           {"profile_id", "table"}),
             object_schema({{"profile_id", string_schema(39, 39)},
                            {"table", string_schema(16)},
                            {"source", profile_ref},
                            {"total_rows", integer_schema(0, 4096)},
                            {"total_columns", integer_schema(0, 4096)},
                            {"offset", integer_schema(0, 4096)},
                            {"column_offset", integer_schema(0, 4096)},
                            {"rows", array_schema(profile_row, 100)},
                            {"next_offset", nullable(integer_schema(0, 4096))},
                            {"next_column_offset", nullable(integer_schema(0, 4096))},
                            {"scope", string_schema(512)}},
                           {"profile_id", "table", "source", "total_rows", "total_columns", "offset", "column_offset",
                            "rows", "next_offset", "next_column_offset", "scope"}),
             "Page retained GPU Trace TSV rows and numeric columns after provenance/hash checks. Positions and "
             "duplicate labels are "
             "preserved, not joined to capture events or interpreted as min/mean/max. Only event_durations has "
             "explicit ms units; "
             "other units are null because physical scaling is unresolved. Defaults: 50 rows and 32 columns; pages may "
             "be byte-limited.",
             true, false, [this](const Json& args) {
                 return ProfileInspection(service().artifacts())
                     .metrics(artifact_id(args, "profile_id"), args.at("table"), args.value("offset", std::size_t{0}),
                              args.value("limit", std::size_t{50}), args.value("column_offset", std::size_t{0}),
                              args.value("column_limit", std::size_t{32}));
             });

    const auto statistic = object_schema({{"count", integer_schema(1, 4096)},
                                          {"minimum", {{"type", "number"}}},
                                          {"maximum", {{"type", "number"}}},
                                          {"median", {{"type", "number"}}},
                                          {"mean", {{"type", "number"}}}},
                                         {"count", "minimum", "maximum", "median", "mean"});
    const auto comparison_run =
        object_schema({{"profile_id", string_schema(39, 39)},
                       {"job", identity_schema()},
                       {"source", profile_ref},
                       {"report", profile_ref},
                       {"project_version", string_schema(64)},
                       {"within_trace", statistic}},
                      {"profile_id", "job", "source", "report", "project_version", "within_trace"});
    const auto comparison_group =
        object_schema({{"runs", array_schema(comparison_run, 8)}, {"run_medians", statistic}}, {"runs", "run_medians"});
    auto collection_schema = requested_schema;
    collection_schema["properties"].erase("timeout_ms");
    collection_schema["required"] = {"format",       "delimiter",  "start_after", "limit",     "duration_ms",
                                     "architecture", "metric_set", "gpu_clocks",  "multi_pass"};
    auto comparison_ids = array_schema(string_schema(39, 39), 8);
    comparison_ids["minItems"] = 2;
    add_tool(
        tools, "profile_compare",
        object_schema({{"baseline", comparison_ids},
                       {"candidate", comparison_ids},
                       {"table",
                        {{"type", "string"},
                         {"maxLength", 16},
                         {"enum", {"frame_duration", "frame_metrics", "event_durations", "regime_metrics"}}}},
                       {"label", string_schema(4096, 1)},
                       {"column_index", integer_schema(0, 4095)},
                       {"workload_policy", string_schema(2048, 1)},
                       {"warmup_policy", string_schema(2048, 1)}},
                      {"baseline", "candidate", "table", "label", "column_index", "workload_policy", "warmup_policy"}),
        object_schema({{"table", string_schema(16)},
                       {"label", string_schema(4096, 1)},
                       {"column_index", integer_schema(0, 4095)},
                       {"column_name", nullable(string_schema(4096))},
                       {"unit", nullable(string_schema(16))},
                       {"common", object_schema({{"producer", string_schema(128, 1)},
                                                 {"gpu", string_schema(4096, 1)},
                                                 {"driver", string_schema(4096, 1)},
                                                 {"settings", object_schema(exported_fields)},
                                                 {"collection", collection_schema}},
                                                {"producer", "gpu", "driver", "settings", "collection"})},
                       {"declarations", object_schema({{"origin", string_schema(32)},
                                                       {"workload_policy", string_schema(2048, 1)},
                                                       {"warmup_policy", string_schema(2048, 1)}},
                                                      {"origin", "workload_policy", "warmup_policy"})},
                       {"baseline", comparison_group},
                       {"candidate", comparison_group},
                       {"median_difference", nullable({{"type", "number"}})},
                       {"candidate_over_baseline", nullable({{"type", "number"}})},
                       {"run_median_range_order",
                        {{"type", "string"},
                         {"maxLength", 32},
                         {"enum", {"candidate_lower", "candidate_higher", "overlap_or_touch"}}}},
                       {"scope", string_schema(1024)}},
                      {"table", "label", "column_index", "column_name", "unit", "common", "declarations", "baseline",
                       "candidate", "median_difference", "candidate_over_baseline", "run_median_range_order", "scope"}),
        "Compare 2..8 distinct fresh GPU Trace jobs per group with identical producer, GPU, driver and collection "
        "settings. Select one exact row label and zero-based numeric column. Each trace contributes one median "
        "of its matching rows; run medians receive equal weight. Returns descriptive ranges/means/medians and "
        "report hashes for launch/build provenance. Required workload/warmup policies are unverified caller "
        "declarations. This does not establish equivalent inputs, correctness, statistical significance or "
        "clock stability. Only event_durations has explicit ms units. No live tools or GPU required.",
        true, false, [this](const Json& args) {
            return ProfileInspection(service().artifacts())
                .compare({args.at("baseline"), args.at("candidate"), args.at("table"), args.at("label"),
                          args.at("column_index"), args.at("workload_policy"), args.at("warmup_policy")});
        });

    add_tool(tools, "capture",
             object_schema({{"executable", string_schema(4096, 1)},
                            {"arguments", array_schema(string_schema(4096), 256)},
                            {"working_directory", string_schema(4096, 1)},
                            {"capture_frame", integer_schema(2)},
                            {"delimiter",
                             {{"type", "string"},
                              {"minLength", 1},
                              {"maxLength", 20},
                              {"enum", {"present", "graphics_capture_api", "vk_frame_boundary"}}}},
                            {"timeout_ms", integer_schema(1, 600000)},
                            {"pin", boolean_schema()},
                            {"application_output_option", string_schema(64)}},
                           {"executable", "working_directory"}),
             object_schema({{"identity", identity_schema()}, {"artifact_id", string_schema(39, 39)}},
                           {"identity", "artifact_id"}),
             "Submit asynchronous Nsight capture of a fresh application. Absolute executable and working directory "
             "are required; arguments are bounded strings. Defaults: frame 2, timeout 120000 ms, pin false. "
             "Optional application_output_option appends an option and a fresh output directory for application "
             "evidence, kept distinct from Nsight exports. The delimiter defaults to present. "
             "graphics_capture_api requires an application that initializes the NGFX SDK and emits its own "
             "boundaries. vk_frame_boundary requires the application to enable VK_EXT_frame_boundary and emit "
             "frame-end submissions. capture_frame counts the selected delimiters, not application frame indices. "
             "Poll job_status; submission is not capture success.",
             false, false, [this](const Json& arguments) {
                 CaptureRequest request;
                 request.executable = absolute_path(arguments.at("executable"), true);
                 request.working_directory = absolute_path(arguments.at("working_directory"));
                 request.arguments = arguments.value("arguments", std::vector<std::string>{});
                 request.capture_frame = arguments.value("capture_frame", std::uint64_t{2});
                 request.delimiter = parse_capture_delimiter(arguments.value("delimiter", "present"));
                 request.timeout = std::chrono::milliseconds(arguments.value("timeout_ms", std::uint64_t{120000}));
                 request.pin = arguments.value("pin", false);
                 request.application_output_option = arguments.value("application_output_option", "");
                 if(!request.application_output_option.empty() &&
                    (request.application_output_option.front() != '-' ||
                     request.application_output_option.find_first_of(" \t\r\n") != std::string::npos)) {
                     throw std::invalid_argument("application_output_option must be one option starting with '-'");
                 }
                 return Json(service().capture(std::move(request)));
             });
    add_tool(tools, "capture_cpp",
             object_schema({{"executable", string_schema(4096, 1)},
                            {"arguments", array_schema(string_schema(4096), 256)},
                            {"working_directory", string_schema(4096, 1)},
                            {"wait_frames", integer_schema(2, 1000000)},
                            {"timeout_ms", integer_schema(1, 600000)},
                            {"pin", boolean_schema()},
                            {"application_output_option", string_schema(64)}},
                           {"executable", "working_directory"}),
             object_schema({{"identity", identity_schema()}, {"artifact_id", string_schema(39, 39)}},
                           {"identity", "artifact_id"}),
             "Submit documented Generate C++ Capture of a fresh Vulkan application on the two qualified Nsight "
             "2026.2/2026.3 producer builds. Defaults: wait_frames 2, timeout 120000 ms, pin false. "
             "wait_frames controls ngfx --wait-frames, separately from graphics-capture delimiter ordinals. "
             "Poll job_status, then artifact_read derived/cpp-project.json for generated source/data/screenshot paths. "
             "Artifact tools retrieve source; capture_metadata/events/objects apply to the separate graphics format. "
             "No source compilation, resource extraction, or GPU replay is performed. SDK delimiters are unavailable. "
             "Application output, when requested, remains separate from generated Nsight evidence.",
             false, false, [this](const Json& arguments) {
                 CaptureRequest request;
                 request.format = CaptureFormat::Cpp;
                 request.executable = absolute_path(arguments.at("executable"), true);
                 request.working_directory = absolute_path(arguments.at("working_directory"));
                 request.arguments = arguments.value("arguments", std::vector<std::string>{});
                 request.cpp_wait_frames = arguments.value("wait_frames", std::uint64_t{2});
                 request.timeout = std::chrono::milliseconds(arguments.value("timeout_ms", std::uint64_t{120000}));
                 request.pin = arguments.value("pin", false);
                 request.application_output_option = arguments.value("application_output_option", "");
                 if(!request.application_output_option.empty() &&
                    (request.application_output_option.front() != '-' ||
                     request.application_output_option.find_first_of(" \t\r\n") != std::string::npos)) {
                     throw std::invalid_argument("application_output_option must be one option starting with '-'");
                 }
                 return Json(service().capture(std::move(request)));
             });
    const auto job_input = object_schema({{"job_id", string_schema(256, 1)}}, {"job_id"});
    add_tool(
        tools, "job_status", job_input, job_schema(),
        "Return the current snapshot of a job from this server session. A terminal state with cleanup_confirmed "
        "and both worker_running/finalization_pending false confirms cleanup; artifact IDs refer to retained evidence. "
        "The error is a bounded diagnostic; inspect retained report/log files for full evidence.",
        true, false, [this](const Json& arguments) {
            const auto id = job_id(arguments);
            const auto result = service().status(id);
            if(!result) {
                throw ArtifactError(ArtifactErrorCode::NotFound, "Unknown job ID in this server session: " + id);
            }
            Json snapshot = *result;
            snapshot["error"] = bounded_diagnostic(result->error, 4096);
            return snapshot;
        });
    add_tool(tools, "job_cancel", job_input,
             object_schema(
                 {{"job_id", string_schema(256, 1)},
                  {"result", {{"type", "string"}, {"enum", {"requested", "already_requested", "already_terminal"}}}}},
                 {"job_id", "result"}),
             "Request cancellation of this server's queued/running job. Poll job_status to observe owned-process "
             "cleanup; a cancellation request alone does not release its GPU reservation.",
             false, true, [this](const Json& arguments) {
                 const auto id = job_id(arguments);
                 const auto result = service().cancel(id);
                 if(result == JobCancelResult::not_found) {
                     throw ArtifactError(ArtifactErrorCode::NotFound, "Unknown job ID in this server session: " + id);
                 }
                 const auto* name = result == JobCancelResult::requested           ? "requested"
                                    : result == JobCancelResult::already_requested ? "already_requested"
                                                                                   : "already_terminal";
                 return Json{{"job_id", id}, {"result", name}};
             });
    add_tool(tools, "artifact_list",
             object_schema({{"after_id", string_schema(39, 39)}, {"limit", integer_schema(1, 100)}}),
             object_schema(
                 {{"artifacts", array_schema(summary_schema(), 100)}, {"next_after", nullable(string_schema(39, 39))}},
                 {"artifacts", "next_after"}),
             "List retained, staging, failed, and expired artifact summaries. Default limit 50, maximum 100; "
             "pass next_after as after_id for the next page. This opens configured storage lazily.",
             true, false, [this](const Json& arguments) {
                 const auto after = arguments.contains("after_id") ? artifact_id(arguments, "after_id") : "";
                 return Json(service().artifacts().list(after, arguments.value("limit", std::size_t{50})));
             });
    const auto artifact_input = object_schema({{"artifact_id", string_schema(39, 39)}}, {"artifact_id"});
    add_tool(tools, "artifact_info", artifact_input, info_schema(),
             "Return manifest provenance, required outputs, retention state, and file count for one artifact. "
             "Expired IDs retain an explanation. Provenance is bounded to 64 KiB.",
             true, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments);
                 return Json(service().artifacts().inspect(id));
             });
    add_tool(tools, "artifact_files",
             object_schema({{"artifact_id", string_schema(39, 39)},
                            {"offset", integer_schema(0, 4096)},
                            {"limit", integer_schema(1, 100)}},
                           {"artifact_id"}),
             object_schema(
                 {{"artifact_id", string_schema(39, 39)},
                  {"files", array_schema(object_schema({{"path", string_schema(512, 1)}, {"bytes", integer_schema()}},
                                                       {"path", "bytes"}),
                                         100)},
                  {"next_offset", nullable(integer_schema(0, 4096))}},
                 {"artifact_id", "files", "next_offset"}),
             "List published raw/derived files and sizes, default limit 100. Paths are scoped to artifact_id; "
             "pass next_offset for the next page. File contents are not returned.",
             true, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments);
                 Json result = service().artifacts().files(id, arguments.value("offset", std::size_t{0}),
                                                           arguments.value("limit", std::size_t{100}));
                 result["artifact_id"] = id;
                 return result;
             });
    add_tool(tools, "artifact_read",
             object_schema({{"artifact_id", string_schema(39, 39)},
                            {"path", string_schema(512, 1)},
                            {"max_bytes", integer_schema(1, text_limit)}},
                           {"artifact_id", "path"}),
             object_schema({{"artifact_id", string_schema(39, 39)},
                            {"path", string_schema(512, 1)},
                            {"local_path", string_schema(8192, 1)},
                            {"bytes", integer_schema()},
                            {"status", {{"type", "string"}, {"enum", {"text", "reference_only"}}}},
                            {"text", nullable(string_schema(text_limit))},
                            {"reason", nullable(string_schema(512))}},
                           {"artifact_id", "path", "local_path", "bytes", "status", "text", "reason"}),
             "Read one complete UTF-8 text file up to max_bytes (default and maximum 65536). Larger files return "
             "a local file reference and metadata without reading their contents. NUL or malformed UTF-8 is a "
             "tool error; binary captures/images remain files. Reads hold an artifact usage lease.",
             true, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments);
                 const auto path = arguments.at("path").get<std::string>();
                 relative_path(path);
                 auto& store = service().artifacts();
                 auto protection = store.lease(id);
                 std::optional<ArtifactFile> file;
                 std::size_t offset = 0;
                 do {
                     const auto page = store.files(id, offset, 100);
                     for(const auto& item : page.files) {
                         if(item.path == path) {
                             file = item;
                             break;
                         }
                     }
                     if(file || !page.next_offset) {
                         break;
                     }
                     offset = *page.next_offset;
                 } while(true);
                 if(!file) {
                     throw ArtifactError(ArtifactErrorCode::NotFound,
                                         "File is not in the published artifact inventory");
                 }
                 Json result{{"artifact_id", id},
                             {"path", path},
                             {"local_path", (protection.directory() / path).string()},
                             {"bytes", file->bytes},
                             {"status", "reference_only"},
                             {"text", nullptr},
                             {"reason", nullptr}};
                 const auto maximum = arguments.value("max_bytes", text_limit);
                 if(file->bytes > maximum) {
                     result["reason"] = "File exceeds max_bytes; use the file reference for external inspection";
                     return result;
                 }
                 const auto text = store.read(id, path, maximum);
                 if(text.find('\0') != std::string::npos) {
                     throw std::invalid_argument(
                         "Artifact is binary (contains NUL); use artifact_files and its file reference");
                 }
                 try {
                     (void)Json(text).dump();
                 } catch(const Json::type_error&) {
                     throw std::invalid_argument("Artifact is not valid UTF-8 text; use its file reference");
                 }
                 result["status"] = "text";
                 result["text"] = text;
                 return result;
             });
    const auto image_reference = object_schema(
        {{"artifact_id", string_schema(39, 39)}, {"path", string_schema(512, 1)}}, {"artifact_id", "path"});
    auto image_result = image_reference;
    image_result["properties"]["artifact_status"] = {{"type", "string"}, {"enum", {"complete", "failed"}}};
    image_result["properties"]["encoded_bytes"] = integer_schema(1, 16U * 1024U * 1024U);
    image_result["properties"]["encoded_sha256"] = string_schema(64, 64);
    image_result["properties"]["rgb_sha256"] = string_schema(64, 64);
    for(const auto* field : {"artifact_status", "encoded_bytes", "encoded_sha256", "rgb_sha256"}) {
        image_result["required"].push_back(field);
    }
    const auto region_schema = object_schema({{"x", integer_schema(0, 4095)},
                                              {"y", integer_schema(0, 4095)},
                                              {"width", integer_schema(1, 4096)},
                                              {"height", integer_schema(1, 4096)}},
                                             {"x", "y", "width", "height"});
    auto preview_input = image_reference;
    preview_input["properties"]["region"] = region_schema;
    preview_input["properties"]["max_edge"] = integer_schema(1, 384);
    add_tool(
        tools, "artifact_preview_image", preview_input,
        object_schema({{"evidence_origin", {{"type", "string"}, {"enum", {"caller_selected_artifact_image_preview"}}}},
                       {"source", image_result},
                       {"source_width", integer_schema(1, 4096)},
                       {"source_height", integer_schema(1, 4096)},
                       {"region", region_schema},
                       {"width", integer_schema(1, 384)},
                       {"height", integer_schema(1, 384)},
                       {"max_edge", integer_schema(1, 384)},
                       {"resampled", boolean_schema()},
                       {"sampling", {{"type", "string"}, {"enum", {"nearest_neighbor_top_left"}}}},
                       {"mime_type", {{"type", "string"}, {"enum", {"image/png"}}}},
                       {"png_bytes", integer_schema(1, 512 * 1024)},
                       {"png_sha256", string_schema(64, 64)},
                       {"preview_rgb_sha256", string_schema(64, 64)},
                       {"preview_scope", string_schema(512)}},
                      {"evidence_origin", "source", "source_width", "source_height", "region", "width", "height",
                       "max_edge", "resampled", "sampling", "mime_type", "png_bytes", "png_sha256",
                       "preview_rgb_sha256", "preview_scope"}),
        "Preview a retained P6, opaque RGB/RGBA8 PNG or supported BMP as PNG image content. Input at most 16 MiB. "
        "Optional in-bounds region; max_edge 1..384 (default 384), nearest-neighbor downsampling, no upscale. "
        "Returns source/crop/pixel identities and explicit resizing. Imports and failed bundles are allowed. "
        "Use original-image comparisons for verification; the preview may omit pixels.",
        true, false, [this](const Json& arguments) {
            const ImageReference source{artifact_id(arguments), arguments.at("path").get<std::string>()};
            relative_path(source.path);
            std::optional<ImageRegion> region;
            if(arguments.contains("region")) {
                const auto& value = arguments.at("region");
                region = ImageRegion{value.at("x").get<std::uint32_t>(), value.at("y").get<std::uint32_t>(),
                                     value.at("width").get<std::uint32_t>(), value.at("height").get<std::uint32_t>()};
            }
            const auto preview =
                preview_artifact_image(service().artifacts(), source, region, arguments.value("max_edge", 384U));
            return Json{{"structuredContent", preview.metadata},
                        {"content", Json::array({{{"type", "text"}, {"text", preview.metadata.dump()}},
                                                 {{"type", "image"},
                                                  {"mimeType", "image/png"},
                                                  {"data", fastmcpp::util::pagination::base64_encode(preview.png)}}})}};
        });
    add_tool(
        tools, "artifact_compare_images",
        object_schema({{"reference", image_reference},
                       {"candidate", image_reference},
                       {"channel_tolerance", integer_schema(0, 255)}},
                      {"reference", "candidate"}),
        object_schema({{"evidence_origin", {{"type", "string"}, {"enum", {"caller_selected_artifact_images"}}}},
                       {"reference", image_result},
                       {"candidate", image_result},
                       {"width", integer_schema(1, 4096)},
                       {"height", integer_schema(1, 4096)},
                       {"channel_tolerance", integer_schema(0, 255)},
                       {"differing_pixels", integer_schema(0, 4096U * 4096U)},
                       {"max_channel_difference", integer_schema(0, 255)},
                       {"mean_absolute_channel_difference", {{"type", "number"}, {"minimum", 0}, {"maximum", 255}}},
                       {"matches_within_tolerance", boolean_schema()},
                       {"units", {{"type", "string"}, {"enum", {"RGB8_channel_steps"}}}},
                       {"comparison_scope", string_schema(512)}},
                      {"evidence_origin", "reference", "candidate", "width", "height", "channel_tolerance",
                       "differing_pixels", "max_channel_difference", "mean_absolute_channel_difference",
                       "matches_within_tolerance", "units", "comparison_scope"}),
        "Compare two caller-selected artifact images under usage leases. P6 RGB8 or BMP 40-byte BI_RGB "
        "24/32-bit without palette; at most 16 MiB per encoded file and 4096 pixels per dimension. "
        "Top-down RGB8 without resizing/color conversion; BMP unused fourth byte ignored. Default inclusive "
        "channel_tolerance 0; mismatched dimensions fail. Returns hashes and pixel/channel differences, "
        "not proof of workload equivalence or a verified repair. Imports and readable failed bundles are allowed.",
        true, false, [this](const Json& arguments) {
            const auto reference_id = artifact_id(arguments.at("reference"));
            const auto candidate_id = artifact_id(arguments.at("candidate"));
            const auto reference_path = arguments.at("reference").at("path").get<std::string>();
            const auto candidate_path = arguments.at("candidate").at("path").get<std::string>();
            relative_path(reference_path);
            relative_path(candidate_path);
            return compare_artifact_images(service().artifacts(), {reference_id, reference_path},
                                           {candidate_id, candidate_path},
                                           arguments.value("channel_tolerance", std::uint8_t{0}));
        });
    add_tool(tools, "artifact_pin",
             object_schema({{"artifact_id", string_schema(39, 39)}, {"pinned", boolean_schema()}},
                           {"artifact_id", "pinned"}),
             info_schema(),
             "Persist an artifact's retention pin across server restarts. Pin investigation evidence and "
             "verification baselines explicitly. Quarantined attempts cannot be unpinned until cleanup is confirmed.",
             false, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments);
                 return Json(service().artifacts().pin(id, arguments.at("pinned").get<bool>()));
             });
    add_tool(tools, "artifact_usage", object_schema(), usage_schema(),
             "Report managed storage bytes and quota state, including pinned/in-use subsets. Opens only the "
             "explicit artifact root; does not run Nsight.",
             true, false, [this](const Json&) { return Json(service().artifacts().usage()); });
    add_tool(tools, "artifact_prune", object_schema(),
             object_schema({{"expired_ids", array_schema(string_schema(39, 39), 100)},
                            {"errors", array_schema(string_schema(512), 100)},
                            {"expired_count", integer_schema()},
                            {"error_count", integer_schema()},
                            {"truncated", boolean_schema()},
                            {"usage", usage_schema()}},
                           {"expired_ids", "errors", "expired_count", "error_count", "truncated", "usage"}),
             "Apply configured age/storage limits to unpinned, unused completed bundles. Deletes whole managed "
             "bundles and preserves expiration records. Returns at most 100 IDs/errors plus totals and quota state.",
             false, true, [this](const Json&) {
                 const auto result = service().artifacts().prune();
                 Json ids = Json::array();
                 Json errors = Json::array();
                 for(std::size_t index = 0; index < std::min(result.expired_ids.size(), std::size_t{100}); ++index) {
                     ids.push_back(result.expired_ids[index]);
                 }
                 for(std::size_t index = 0; index < std::min(result.errors.size(), std::size_t{100}); ++index) {
                     errors.push_back(bounded_diagnostic(result.errors[index], 512));
                 }
                 return Json{{"expired_ids", ids},
                             {"errors", errors},
                             {"expired_count", result.expired_ids.size()},
                             {"error_count", result.errors.size()},
                             {"truncated", result.expired_ids.size() > 100 || result.errors.size() > 100},
                             {"usage", result.usage}};
             });
    add_tool(tools, "artifact_import",
             object_schema({{"source", string_schema(4096, 1)},
                            {"required_outputs", array_schema(string_schema(499, 1), 128)},
                            {"pin", boolean_schema()}},
                           {"source"}),
             info_schema(),
             "Copy an existing evidence directory into a managed bundle without following links or changing its "
             "source. Requires an absolute source outside the store; required_outputs are source-relative paths. "
             "Pinned by default. Imported evidence is labelled caller-provided and does not prove Nsight support.",
             false, false, [this](const Json& arguments) {
                 const auto source = absolute_path(arguments.at("source"));
                 validate_import_location(source, options_.artifacts.root);
                 const auto required = arguments.value("required_outputs", std::vector<std::string>{});
                 for(const auto& path : required) {
                     relative_path(path, false);
                 }
                 const Json provenance{{"project_version", project_version()},
                                       {"evidence_origin", "caller_provided_import"},
                                       {"source_directory", source.string()}};
                 return Json(service().artifacts().import_directory(source, provenance, required,
                                                                    arguments.value("pin", true)));
             });
    const auto capture_input = object_schema({{"capture_id", string_schema(39, 39)}}, {"capture_id"});
    add_tool(tools, "capture_metadata", capture_input, metadata_schema(),
             "Read selected metadata from a complete server capture bundle (capture_id is its artifact_id). "
             "Validates retained service/export success, exact observed producer provenance, and metadata schema. "
             "Omits process environment/command line; raw files remain available through artifact tools. "
             "A usage lease protects the bundle. Detailed state is explicitly unavailable from these exports.",
             true, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments, "capture_id");
                 return InspectionService(service().artifacts()).metadata(id);
             });
    const auto inventory_input = object_schema({{"capture_id", string_schema(39, 39)},
                                                {"offset", integer_schema(0, NsightEvidenceLimits::maximum_records)},
                                                {"limit", integer_schema(1, InspectionService::maximum_page_records)}},
                                               {"capture_id"});
    add_tool(
        tools, "capture_events", inventory_input,
        inventory_schema(
            "events", object_schema({{"event_index", integer_schema()},
                                     {"function_name", string_schema(NsightEvidenceLimits::maximum_string_bytes, 1)},
                                     {"thread_index", integer_schema()},
                                     {"sequence_id", nullable(integer_schema())},
                                     {"indirect_index", nullable(integer_schema())}},
                                    {"event_index", "function_name", "thread_index", "sequence_id", "indirect_index"})),
        "Page the observed function inventory in retained export order, after validating metadata from the "
        "same complete server capture. IDs belong only to capture_id; sequence_id and indirect_index are opaque. No "
        "API "
        "arguments, event/object association, pipeline state, or pass hierarchy is inferred. Default limit "
        "50, maximum 100; pages also stop at 256 KiB. Continue with next_offset until null; total is full count.",
        true, false, [this](const Json& arguments) {
            const auto id = artifact_id(arguments, "capture_id");
            return InspectionService(service().artifacts())
                .events(id, arguments.value("offset", std::size_t{0}), arguments.value("limit", std::size_t{50}));
        });
    add_tool(tools, "capture_objects", inventory_input,
             inventory_schema(
                 "objects", object_schema({{"uid", integer_schema()},
                                           {"api", string_schema(NsightEvidenceLimits::maximum_string_bytes, 1)},
                                           {"object_name", string_schema(NsightEvidenceLimits::maximum_string_bytes)},
                                           {"type_name", string_schema(NsightEvidenceLimits::maximum_string_bytes, 1)},
                                           {"access_flags", integer_schema()}},
                                          {"uid", "api", "object_name", "type_name", "access_flags"})),
             "Page the observed object inventory in retained export order, after validating metadata from the "
             "same complete server capture. UIDs belong only to capture_id; access_flags is opaque. Names do not "
             "establish shader contents, resource bytes, bindings, or event associations. Default limit 50, maximum "
             "100; pages also stop at 256 KiB. Continue with next_offset until null; total is full count.",
             true, false, [this](const Json& arguments) {
                 const auto id = artifact_id(arguments, "capture_id");
                 return InspectionService(service().artifacts())
                     .objects(id, arguments.value("offset", std::size_t{0}), arguments.value("limit", std::size_t{50}));
             });
    add_tool(
        tools, "capture_cpp_source",
        object_schema({{"capture_id", string_schema(39, 39)},
                       {"source_path", string_schema(512, 1)},
                       {"start_line", integer_schema(1, 4194304)},
                       {"max_lines", integer_schema(1, 200)}},
                      {"capture_id", "source_path"}),
        cpp_source_schema(),
        "Read numbered lines from an indexed generated C++ source file in a complete capture_cpp bundle. "
        "Validates same-bundle producer, report, index and metadata under a usage lease. Source is limited to 4 MiB; "
        "default 100 lines, maximum 200 and 256 KiB per page. Continue with next_line. Hash identifies inspected "
        "bytes, not producer authenticity. This reads generated replay source, not original shader source.",
        true, false, [this](const Json& arguments) {
            const auto id = artifact_id(arguments, "capture_id");
            const auto path = arguments.at("source_path").get<std::string>();
            relative_path(path);
            return InspectionService(service().artifacts())
                .cpp_source(id, path, arguments.value("start_line", std::size_t{1}),
                            arguments.value("max_lines", std::size_t{100}));
        });
    auto cpp_input = inventory_input;
    cpp_input["properties"]["section"] = {
        {"type", "string"}, {"maxLength", 32}, {"enum", {"draws", "unsupported_recordings", "unsupported_objects"}}};
    add_tool(
        tools, "capture_cpp_draws", cpp_input, cpp_draws_schema(),
        "Page literal draw-to-pipeline-to-shader source relationships in a retained capture_cpp bundle. "
        "Accepts qualified straight-line single-part recording functions and direct creation initializers. "
        "Does not establish GPU execution, descriptor contents or extracted shader bytes. Counts report unsupported "
        "recordings/objects; page those sections separately for reasons and source references. Default section draws, "
        "limit 50; maximum 100 and 256 KiB per page. Source identities are capture-scoped. "
        "Unrecognized producers or captures reporting unsupported operations fail explicitly.",
        true, false, [this](const Json& arguments) {
            return InspectionService(service().artifacts())
                .cpp_draws(artifact_id(arguments, "capture_id"), arguments.value("section", std::string("draws")),
                           arguments.value("offset", std::size_t{0}), arguments.value("limit", std::size_t{50}));
        });
    auto resource_input = inventory_input;
    resource_input["properties"]["section"] = {
        {"type", "string"}, {"maxLength", 32}, {"enum", {"resources", "unsupported"}}};
    add_tool(
        tools, "capture_cpp_resources", resource_input, cpp_resources_schema(),
        "List literal resource references in indexed CommandList*, Resources* and Frame* generated C++ files. "
        "Includes capture-local IDs bound to source hashes/spans, byte declarations, preprocessor conditional flags "
        "and explicit unsupported coverage. Does not establish execution, descriptor selection or GPU event state. "
        "Default section resources, limit 50; maximum 100 and 256 KiB per page. No reader executable required.",
        true, false, [this](const Json& arguments) {
            return InspectionService(service().artifacts(), options_.resource_workers)
                .cpp_resources(artifact_id(arguments, "capture_id"),
                               arguments.value("section", std::string("resources")),
                               arguments.value("offset", std::size_t{0}), arguments.value("limit", std::size_t{50}));
        });
    add_tool(
        tools, "capture_cpp_resource",
        object_schema({{"capture_id", string_schema(39, 39)},
                       {"resource_ref", string_schema(64, 64)},
                       {"offset", integer_schema(0, 16777216)},
                       {"length", integer_schema(1, 65536)},
                       {"pin", boolean_schema()}},
                      {"capture_id", "resource_ref"}),
        cpp_resource_schema(),
        "Read a bounded range of opaque serialized bytes using a listed resource reference and the configured "
        "qualified reader for that producer. Requires --resource-worker-2026-3 or --resource-worker-2026-2. "
        "Revalidates source, fingerprints helpers, snapshots inputs under leases and validates the worker response. "
        "Default offset 0, length 65536; returns hex and byte hashes. Each call creates an evidence bundle retaining "
        "the input databases (up to 272 MiB), output and logs, subject to artifact quotas. Optional pin defaults "
        "false; "
        "pinning the read does not pin the source capture. Ten-second operation deadline. Unsupported confinement "
        "fails closed. This does not reconstruct descriptor contents or resource state after a GPU event.",
        false, false, [this](const Json& arguments) {
            return InspectionService(service().artifacts(), options_.resource_workers)
                .cpp_resource(artifact_id(arguments, "capture_id"), arguments.at("resource_ref").get<std::string>(),
                              arguments.value("offset", std::size_t{0}), arguments.value("length", std::size_t{65536}),
                              arguments.value("pin", false));
        });
}
} // namespace ngm
