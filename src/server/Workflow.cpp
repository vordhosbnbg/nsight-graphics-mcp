#include "Workflow.hpp"

#include "ngm/Inspection.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/Version.hpp"

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
    return Json::array({"capabilities", "capture", "job_status", "job_cancel", "artifact_list", "artifact_info",
                        "artifact_files", "artifact_read", "artifact_pin", "artifact_usage", "artifact_prune",
                        "artifact_import", "capture_metadata", "capture_events", "capture_objects"});
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
    add_tool(
        tools, "capture",
        object_schema(
            {{"executable", string_schema(4096, 1)},
             {"arguments", array_schema(string_schema(4096), 256)},
             {"working_directory", string_schema(4096, 1)},
             {"capture_frame", integer_schema(2)},
             {"delimiter",
              {{"type", "string"}, {"minLength", 1}, {"maxLength", 20}, {"enum", {"present", "graphics_capture_api"}}}},
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
        "boundaries. capture_frame counts the selected delimiters, not application frame indices. "
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
}
} // namespace ngm
