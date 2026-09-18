#include "ngm/Inspection.hpp"

#include "ngm/CppEvidence.hpp"
#include "ngm/Hash.hpp"
#include "ngm/NsightEvidence.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <utility>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Error = InspectionErrorCode;
constexpr std::size_t report_limit = 2U * 1024U * 1024U;
constexpr std::string_view report_path = "raw/report.json";
constexpr std::string_view metadata_path = "raw/exports/metadata.raw";

struct ProducerProfile {
    const char* cli_version;
    const char* metadata_version;
    const char* build;
    const char* name;
};

constexpr std::array profiles{
    ProducerProfile{"2026.3.1.0", "2026.3.1", "38722833", "nsight-2026.3.1-build-38722833-vulkan"},
    ProducerProfile{"2026.2.0.0", "2026.2.0", "37991608", "nsight-2026.2.0-build-37991608-vulkan"}};

[[noreturn]] void fail(Error code, const std::string& message) {
    throw InspectionError(code, message);
}

void require(bool condition, const std::string& message) {
    if(!condition) {
        fail(Error::InvalidReport, message + "; inspect raw/report.json and artifact_info");
    }
}

const Json& member(const Json& object, const char* name) {
    require(object.is_object() && object.contains(name), std::string("Capture report requires ") + name);
    return object.at(name);
}

std::string text(const Json& object, const char* name, std::size_t maximum = 4096) {
    const auto& value = member(object, name);
    require(value.is_string(), std::string("Capture report field must be a string: ") + name);
    const auto& result = value.get_ref<const std::string&>();
    require(!result.empty() && result.size() <= maximum && result.find('\0') == std::string::npos,
            std::string("Capture report field exceeds its text bounds: ") + name);
    return result;
}

std::uint64_t number(const Json& object, const char* name) {
    const auto& value = member(object, name);
    require(value.is_number_integer() && (value.is_number_unsigned() || value.get<std::int64_t>() >= 0),
            std::string("Capture report field must be an unsigned integer: ") + name);
    return value.get<std::uint64_t>();
}

Json parse_report(const std::string& input) {
    if(input.find('\0') != std::string::npos) {
        fail(Error::InvalidReport, "Capture report contains a raw NUL byte; inspect raw/report.json");
    }
    // Reports are service-written, but retained files may be damaged. Bound
    // nesting and reject duplicate keys before accepting any success claim.
    std::vector<std::set<std::string>> keys;
    try {
        return Json::parse(input, [&](int depth, Json::parse_event_t event, Json& value) {
            if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start) {
                require(depth < 32, "Capture report exceeds the nesting limit");
                keys.emplace_back();
            } else if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end) {
                keys.pop_back();
            } else if(event == Json::parse_event_t::key) {
                require(keys.back().insert(value.get<std::string>()).second, "Capture report has a duplicate key");
            }
            return true;
        });
    } catch(const Json::exception&) {
        fail(Error::InvalidReport, "Capture report is not valid JSON; inspect raw/report.json");
    }
}

struct Capture {
    ArtifactLease lease;
    ArtifactInfo info;
    Json report;
    std::map<std::string, std::uint64_t> files;
    NsightMetadata metadata;
    NsightCppMetadata cpp_metadata;
    Json cpp_index;
    Json producer;
    const ProducerProfile* profile = nullptr;
};

void successful_process(const Json& operation) {
    const auto& process = member(operation, "process");
    require(member(operation, "launched") == true && member(process, "exit_code") == 0 &&
                member(process, "signal").is_null() && member(process, "cleanup_confirmed") == true &&
                member(process, "cancelled") == false && member(process, "timed_out") == false,
            "Successful capture/export requires an exited, cleanup-confirmed process");
}

std::uint64_t inventoried(const Capture& capture, const std::string& path, std::size_t maximum) {
    const auto found = capture.files.find(path);
    if(found == capture.files.end()) {
        fail(Error::ExportUnavailable, "Required evidence is absent from this bundle's published inventory: " + path +
                                           "; retain a fresh capture with this export");
    }
    if(found->second > maximum) {
        fail(Error::LimitExceeded, "Evidence exceeds the inspection byte limit: " + path +
                                       "; retrieve its file reference with artifact_files/artifact_read");
    }
    return found->second;
}

const Json& export_record(const Capture& capture, const std::string& kind, const std::string& path) {
    const auto& exports = member(capture.report, "exports");
    require(exports.is_array() && exports.size() <= 5, "Capture exports must be a bounded array");
    const Json* result = nullptr;
    for(const auto& entry : exports) {
        if(text(entry, "kind", 32) == kind) {
            require(result == nullptr, "Capture report has duplicate export kinds");
            result = &entry;
        }
    }
    if(result == nullptr || member(*result, "outcome") != "success") {
        fail(Error::ExportUnavailable, "This capture has no successful " + kind +
                                           " export; inspect raw/report.json and export logs, or recapture with a "
                                           "matching replayer advertising the documented export");
    }
    require(text(*result, "evidence_origin", 64) == "nsight_export" && text(*result, "output", 512) == path,
            "Export provenance/output does not match its documented capture path");
    require(text(*result, "executable") == text(capture.producer.at("replay_tool"), "path"),
            "Export executable differs from the retained replayer observation");
    successful_process(*result);
    require(number(*result, "output_bytes") == inventoried(capture, path, NsightEvidenceLimits::maximum_bytes),
            "Export byte count differs from the published inventory");
    return *result;
}

template <typename Function>
auto parse_export(ArtifactStore& store, Capture& capture, const std::string& kind, const std::string& path,
                  Function&& parse) {
    export_record(capture, kind, path);
    try {
        return parse(store.read(capture.info.summary.id, path, NsightEvidenceLimits::maximum_bytes));
    } catch(const NsightEvidenceError& error) {
        const auto code =
            error.code() == NsightEvidenceErrorCode::LimitExceeded ? Error::LimitExceeded : Error::InvalidExport;
        fail(code, kind + " export " + std::string(nsight_evidence_error_name(error.code())) + ": " + error.what() +
                       "; inspect " + path + " and retain a fresh export from the supported producer");
    }
}

Capture load_capture(ArtifactStore& store, const std::string& id, bool cpp = false) {
    Capture capture;
    capture.lease = store.lease(id);
    capture.info = store.inspect(id);
    if(capture.info.summary.status != "complete" || capture.info.summary.quarantined) {
        fail(Error::IncompleteCapture, "Inspection requires a complete successful capture bundle; poll job_status "
                                       "for completion and confirmed cleanup or submit a fresh capture");
    }
    const auto& manifest = capture.info.provenance;
    if(!manifest.is_object() ||
       manifest.value("evidence_origin", Json()) != (cpp ? "nsight_cpp_capture" : "nsight_capture") ||
       manifest.value("backend", Json()) != "documented_nsight_cli") {
        fail(Error::NotCapture, "Inspection accepts only server capture bundles; imported/application-provided "
                                "evidence remains accessible through artifact_files/artifact_read");
    }
    require(number(manifest, "schema_version") == 1 && text(manifest, "report_path", 512) == report_path,
            "Unsupported capture manifest/report schema");
    const std::vector<std::string> required =
        cpp ? std::vector<std::string>{"derived/cpp-project.json", "raw/report.json"}
            : std::vector<std::string>{"raw/capture.ngfx-capture", "raw/exports/metadata.raw", "raw/report.json"};
    for(const auto& path : required) {
        require(std::find(capture.info.required_outputs.begin(), capture.info.required_outputs.end(), path) !=
                    capture.info.required_outputs.end(),
                "Capture manifest omits a required output");
    }
    std::size_t offset = 0;
    do {
        const auto page = store.files(id, offset, 100);
        for(const auto& file : page.files) {
            capture.files.emplace(file.path, file.bytes);
        }
        if(!page.next_offset) {
            break;
        }
        offset = *page.next_offset;
    } while(true);
    inventoried(capture, std::string(report_path), report_limit);
    capture.report = parse_report(store.read(id, std::string(report_path), report_limit));
    const auto& report = capture.report;
    for(const auto* field : {"schema_version", "project_version", "evidence_origin", "backend", "job",
                             "readable_capture", "cleanup_confirmed", "worker_outcome"}) {
        require(member(report, field) == member(manifest, field),
                std::string("Capture report and manifest disagree on ") + field);
    }
    require(number(report, "schema_version") == 1 && member(report, "readable_capture") == !cpp &&
                member(report, "cleanup_confirmed") == true && member(report, "worker_outcome") == "succeeded",
            "Capture report does not record successful capture and cleanup");
    (void)text(report, "project_version", 64);
    const auto& job = member(report, "job");
    require(text(job, "capture_id", 39) == id && number(job, "attempt") > 0,
            "Capture report job identity differs from its bundle");
    (void)text(job, "job_id", 256);
    const auto& nsight = member(report, "nsight");
    require(member(nsight, "interface_ready") == true, "Capture report lacks a verified tool interface");
    capture.producer = Json::object();
    for(const auto* name : {"capture", "replay", "cli"}) {
        if(!cpp && std::string_view(name) == "cli")
            continue;
        const auto& observed = member(nsight, name);
        const auto& retained = member(member(manifest, "nsight"), name);
        Json producer{{"path", text(observed, "path")},
                      {"version", text(observed, "version", 64)},
                      {"build", text(observed, "build", 64)}};
        for(const auto* field : {"path", "version", "build"}) {
            require(producer.at(field) == member(retained, field), "Manifest/report producer observations differ");
        }
        require(member(observed, "version_valid") == true && member(observed, "help_valid") == true,
                "Capture report has invalid tool observations");
        const auto profile = std::find_if(profiles.begin(), profiles.end(), [&](const ProducerProfile& candidate) {
            return producer.at("version") == candidate.cli_version && producer.at("build") == candidate.build;
        });
        if(profile == profiles.end() || (capture.profile && capture.profile != &*profile)) {
            fail(Error::UnsupportedProducer,
                 "Capture and replay must match one observed profile: Nsight 2026.3.1.0 build 38722833 or "
                 "2026.2.0.0 build 37991608; retain sample exports and validate a new profile before using another "
                 "producer");
        }
        capture.profile = &*profile;
        capture.producer[std::string(name) + "_tool"] = std::move(producer);
    }
    if(cpp) {
        for(const auto* field : {"capture_settings", "generated_cpp_project"})
            require(member(report, field) == member(manifest, field), "C++ capture manifest/report disagreement");
        require(member(report, "generated_cpp_project") == true &&
                    text(member(report, "capture_settings"), "format", 16) == "cpp" &&
                    text(report, "cpp_project_index", 512) == "derived/cpp-project.json",
                "Report does not identify a completed C++ capture");
        inventoried(capture, "derived/cpp-project.json", report_limit);
        capture.cpp_index = parse_report(store.read(id, "derived/cpp-project.json", report_limit));
        const auto& index = capture.cpp_index;
        require(number(index, "schema_version") == 1 && text(index, "evidence_origin", 64) == "nsight_generated_cpp" &&
                    member(index, "job") == job,
                "C++ index origin/schema/identity differs from report");
        const auto directory = text(index, "project_directory", 450);
        const auto clean_path = [](const std::string& value) {
            const std::filesystem::path path(value);
            if(path.is_absolute() || path.generic_string() != value ||
               path.lexically_normal().generic_string() != value)
                return false;
            for(const auto& component : path)
                if(component == "." || component == ".." || component.empty())
                    return false;
            return true;
        };
        require(clean_path(directory) && directory.starts_with("raw/cpp/"), "C++ project directory is invalid");
        for(const auto& [field, filename] :
            std::array<std::pair<const char*, const char*>, 3>{{{"metadata_path", "metadata.json"},
                                                                {"database_path", "data.bin"},
                                                                {"screenshot_path", "screenshot.bmp"}}}) {
            const auto path = text(index, field, 512);
            require(path == directory + "/" + filename && inventoried(capture, path, SIZE_MAX) > 0,
                    "C++ project evidence path is invalid or empty");
        }
        const auto& sources = member(index, "source_files");
        require(sources.is_array() && !sources.empty() && sources.size() <= 256, "C++ source inventory exceeds bounds");
        std::set<std::string> unique;
        for(const auto& value : sources) {
            require(value.is_string(), "C++ source path must be a string");
            const auto path = value.get<std::string>();
            require(path.size() <= 512 && clean_path(path) && path.find('\0') == std::string::npos &&
                        std::filesystem::path(path).parent_path() == directory && path.ends_with(".cpp") &&
                        unique.insert(path).second && inventoried(capture, path, SIZE_MAX) > 0,
                    "C++ source path must uniquely name an inventoried direct project source file");
        }
        std::set<std::string> published_sources;
        for(const auto& [path, unused] : capture.files) {
            (void)unused;
            if(std::filesystem::path(path).parent_path() == directory && path.ends_with(".cpp"))
                published_sources.insert(path);
        }
        require(unique == published_sources && unique.contains(directory + "/CommandList00.cpp") &&
                    unique.contains(directory + "/Resources00.cpp"),
                "C++ source index omits published project sources");
        const auto& operation = member(report, "capture");
        const auto path = text(index, "metadata_path", 512);
        require(member(operation, "outcome") == "success" && text(operation, "output", 512) == path &&
                    text(operation, "executable") == text(capture.producer.at("cli_tool"), "path") &&
                    number(operation, "output_bytes") == inventoried(capture, path, 1024U * 1024U),
                "C++ capture operation does not match retained metadata/producer");
        successful_process(operation);
        try {
            capture.cpp_metadata = parse_nsight_cpp_metadata(store.read(id, path, 1024U * 1024U));
        } catch(const NsightEvidenceError& error) {
            fail(Error::InvalidExport, "C++ metadata failed validation: " + std::string(error.what()));
        }
        const auto& metadata = capture.cpp_metadata;
        if(metadata.nsight_version != capture.profile->metadata_version ||
           std::to_string(metadata.build_id) != capture.profile->build || metadata.primary_api != "vulkan")
            fail(Error::UnsupportedProducer,
                 "C++ metadata must match the retained tool version/build and Vulkan profile");
        require(text(index, "nsight_version", 64) == metadata.nsight_version &&
                    number(index, "nsight_version_build_id") == metadata.build_id &&
                    text(index, "primary_api", 32) == metadata.primary_api &&
                    text(index, "primary_gpu") == metadata.primary_gpu &&
                    member(index, "has_unsupported_operation") == metadata.has_unsupported_operation &&
                    std::filesystem::path(directory).filename() == metadata.project_filename,
                "C++ index and same-bundle metadata disagree");
        return capture;
    }
    const auto& operation = member(report, "capture");
    require(member(operation, "outcome") == "success" && text(operation, "output", 512) == "raw/capture.ngfx-capture" &&
                text(operation, "executable") == text(capture.producer.at("capture_tool"), "path"),
            "Capture operation does not match its retained output/producer");
    successful_process(operation);
    const auto capture_file = capture.files.find("raw/capture.ngfx-capture");
    require(capture_file != capture.files.end() && capture_file->second > 0 &&
                number(operation, "output_bytes") == capture_file->second,
            "Capture file is absent, empty, or disagrees with its reported size");
    capture.metadata = parse_export(store, capture, "metadata", std::string(metadata_path), parse_nsight_metadata);
    if(capture.metadata.nsight_version != capture.profile->metadata_version ||
       capture.metadata.nsight_version_build_id != capture.profile->build || capture.metadata.primary_api != "Vulkan") {
        fail(Error::UnsupportedProducer,
             "Same-bundle metadata must identify the same observed Vulkan version/build profile as capture and "
             "replay; inspect raw/exports/metadata.raw before validating a new producer/profile");
    }
    capture.producer["metadata_nsight_version"] = *capture.metadata.nsight_version;
    capture.producer["metadata_nsight_version_build_id"] = *capture.metadata.nsight_version_build_id;
    return capture;
}

template <typename T>
Json optional(const std::optional<T>& value) {
    return value ? Json(*value) : Json(nullptr);
}

Json metadata_json(const NsightMetadata& value) {
    Json result{{"metadata_version", value.metadata_version},
                {"uuid", optional(value.uuid)},
                {"nsight_version", optional(value.nsight_version)},
                {"nsight_version_build_id", optional(value.nsight_version_build_id)},
                {"primary_api", optional(value.primary_api)},
                {"primary_gpu", optional(value.primary_gpu)},
                {"driver_vendor", optional(value.driver_vendor)},
                {"driver_version", optional(value.driver_version)},
                {"os_information", optional(value.os_information)},
                {"process_name", optional(value.process_name)},
                {"request_time", optional(value.request_time)},
                {"captured_frame", optional(value.captured_frame)},
                {"resolution", optional(value.resolution)},
                {"non_portable", optional(value.non_portable)},
                {"d3d12_core_version", optional(value.d3d12_core_version)},
                {"has_unsupported_operation", optional(value.has_unsupported_operation)},
                {"graphics_apis", optional(value.graphics_apis)},
                {"graphics_features", optional(value.graphics_features)},
                {"_metadata_collection_", nullptr}};
    if(value.collection) {
        result["_metadata_collection_"] = {{"info", optional(value.collection->info)},
                                           {"warnings", optional(value.collection->warnings)}};
    }
    return result;
}

Json base_result(const Capture& capture, const std::string& kind, const std::string& path) {
    return {{"capture_id", capture.info.summary.id},
            {"capture_uuid", optional(capture.metadata.uuid)},
            {"evidence_origin", "nsight_export"},
            {"schema_profile", capture.profile->name},
            {"metadata_version", capture.metadata.metadata_version},
            {"producer", capture.producer},
            {"source", {{"kind", kind}, {"path", path}, {"bytes", capture.files.at(path)}}},
            {"metadata_source", metadata_path},
            {"report_source", report_path},
            {"unavailable_from_these_exports",
             {"api_arguments", "event_object_associations", "pipeline_state", "descriptor_bindings",
              "shader_bytes_and_source", "event_shader_associations", "resource_contents", "pass_relationships"}}};
}

// The transport also includes an escaped JSON text fallback. Bound both forms
// and reserve the full request-line budget for its echoed ID plus framing.
bool fits_response(const Json& result) {
    const auto serialized = result.dump();
    return serialized.size() + Json(serialized).dump().size() + 65536 + 1024 <= InspectionService::maximum_result_bytes;
}

void page_arguments(std::size_t offset, std::size_t limit) {
    if(offset > NsightEvidenceLimits::maximum_records || limit == 0 ||
       limit > InspectionService::maximum_page_records) {
        throw std::invalid_argument("Inspection offset must be 0..100000 and limit must be 1..100");
    }
}

template <typename T, typename Function>
Json page_result(Json result, const char* key, const std::vector<T>& records, std::size_t offset, std::size_t limit,
                 Function&& encode) {
    result["offset"] = offset;
    result["total"] = records.size();
    result["next_offset"] = nullptr;
    result[key] = Json::array();
    const auto end = std::min(records.size(), offset + limit);
    for(auto index = offset; index < end; ++index) {
        result[key].push_back(encode(records[index]));
        const auto next = index + 1;
        result["next_offset"] = next < records.size() ? Json(next) : Json(nullptr);
        if(result.dump().size() > InspectionService::maximum_page_bytes || !fits_response(result)) {
            result[key].erase(result[key].end() - 1);
            if(index == offset) {
                fail(Error::LimitExceeded, "One inventory record exceeds the inspection response budget; retrieve "
                                           "the raw export with artifact_files/artifact_read");
            }
            result["next_offset"] = index;
            break;
        }
    }
    if(!fits_response(result)) {
        fail(Error::LimitExceeded,
             "Inspection provenance exceeds the response budget; use artifact_info and raw files");
    }
    return result;
}
} // namespace

std::string_view inspection_error_name(InspectionErrorCode code) {
    switch(code) {
        case Error::NotCapture:
            return "not_capture";
        case Error::IncompleteCapture:
            return "incomplete_capture";
        case Error::InvalidReport:
            return "invalid_capture_report";
        case Error::ExportUnavailable:
            return "export_unavailable";
        case Error::UnsupportedProducer:
            return "unsupported_producer";
        case Error::InvalidExport:
            return "invalid_export";
        case Error::LimitExceeded:
            return "inspection_limit";
    }
    return "unknown";
}

InspectionError::InspectionError(InspectionErrorCode code, std::string message) :
    std::runtime_error(std::move(message)), code_(code) {}
InspectionErrorCode InspectionError::code() const noexcept {
    return code_;
}

InspectionService::InspectionService(ArtifactStore& artifacts) : artifacts_(artifacts) {}

Json InspectionService::metadata(const std::string& capture_id) const {
    auto capture = load_capture(artifacts_, capture_id);
    auto result = base_result(capture, "metadata", std::string(metadata_path));
    result["metadata"] = metadata_json(capture.metadata);
    if(!fits_response(result)) {
        fail(Error::LimitExceeded, "Capture metadata exceeds the response budget; retrieve raw/exports/metadata.raw");
    }
    return result;
}

Json InspectionService::events(const std::string& capture_id, std::size_t offset, std::size_t limit) const {
    page_arguments(offset, limit);
    auto capture = load_capture(artifacts_, capture_id);
    const std::string path = "raw/exports/functions.raw";
    const auto records = parse_export(artifacts_, capture, "functions", path, parse_nsight_functions);
    return page_result(base_result(capture, "functions", path), "events", records, offset, limit,
                       [](const auto& event) {
                           return Json{{"event_index", event.event_index},
                                       {"function_name", event.function_name},
                                       {"thread_index", event.thread_index},
                                       {"sequence_id", optional(event.sequence_id)},
                                       {"indirect_index", optional(event.indirect_index)}};
                       });
}

Json InspectionService::objects(const std::string& capture_id, std::size_t offset, std::size_t limit) const {
    page_arguments(offset, limit);
    auto capture = load_capture(artifacts_, capture_id);
    const std::string path = "raw/exports/objects.raw";
    const auto records = parse_export(artifacts_, capture, "objects", path, parse_nsight_objects);
    return page_result(base_result(capture, "objects", path), "objects", records, offset, limit,
                       [](const auto& object) {
                           return Json{{"uid", object.uid},
                                       {"api", object.api},
                                       {"object_name", object.object_name},
                                       {"type_name", object.type_name},
                                       {"access_flags", object.access_flags}};
                       });
}
namespace {
Json cpp_base_result(const Capture& capture) {
    return {{"capture_id", capture.info.summary.id},
            {"evidence_origin", "nsight_generated_cpp"},
            {"schema_profile", std::string(capture.profile->name) + "-cpp"},
            {"producer", capture.producer},
            {"index_source", "derived/cpp-project.json"},
            {"metadata_source", capture.cpp_index.at("metadata_path")},
            {"report_source", report_path},
            {"has_unsupported_operation", capture.cpp_metadata.has_unsupported_operation}};
}
Json source_identity(const std::string& path, const std::string& content) {
    return {{"path", path},
            {"bytes", content.size()},
            {"sha256", sha256(std::as_bytes(std::span(content.data(), content.size())))}};
}
} // namespace

Json InspectionService::cpp_source(const std::string& capture_id, const std::string& source_path,
                                   std::size_t start_line, std::size_t max_lines) const {
    if(start_line == 0 || start_line > 4U * 1024U * 1024U || max_lines == 0 || max_lines > 200)
        throw std::invalid_argument("Source start_line must be 1..4194304 and max_lines 1..200");
    auto capture = load_capture(artifacts_, capture_id, true);
    const auto& sources = capture.cpp_index.at("source_files");
    if(std::find(sources.begin(), sources.end(), source_path) == sources.end())
        fail(Error::ExportUnavailable, "source_path must name a file in derived/cpp-project.json source_files");
    inventoried(capture, source_path, 4U * 1024U * 1024U);
    const auto content = artifacts_.read(capture_id, source_path, 4U * 1024U * 1024U);
    if(content.find('\0') != std::string::npos)
        fail(Error::InvalidExport, "Generated source contains a NUL byte");
    try {
        (void)Json(content).dump();
    } catch(const Json::exception&) {
        fail(Error::InvalidExport, "Generated source contains invalid UTF-8");
    }
    auto result = cpp_base_result(capture);
    result["source"] = source_identity(source_path, content);
    result["start_line"] = start_line;
    result["next_line"] = nullptr;
    result["lines"] = Json::array();
    std::vector<std::string_view> lines;
    for(std::size_t start = 0; start < content.size();) {
        const auto end = content.find('\n', start);
        const auto count = (end == std::string::npos ? content.size() : end) - start;
        lines.emplace_back(content.data() + start, count);
        start += count + 1;
    }
    result["total_lines"] = lines.size();
    for(auto line = start_line; line <= lines.size() && line - start_line < max_lines; ++line) {
        result["lines"].push_back({{"number", line}, {"text", lines[line - 1]}});
        result["next_line"] = line < lines.size() ? Json(line + 1) : Json(nullptr);
        if(result.dump().size() > maximum_page_bytes || !fits_response(result)) {
            result["lines"].erase(result["lines"].end() - 1);
            if(line == start_line)
                fail(Error::LimitExceeded, "One generated source line exceeds the response budget; use artifact_read");
            result["next_line"] = line;
            break;
        }
    }
    return result;
}

Json InspectionService::cpp_draws(const std::string& capture_id, const std::string& section, std::size_t offset,
                                  std::size_t limit) const {
    page_arguments(offset, limit);
    if(section != "draws" && section != "unsupported_recordings" && section != "unsupported_objects")
        throw std::invalid_argument("C++ section must be draws, unsupported_recordings, or unsupported_objects");
    auto capture = load_capture(artifacts_, capture_id, true);
    if(capture.cpp_metadata.has_unsupported_operation)
        fail(Error::ExportUnavailable,
             "Capture reports unsupported operations; use capture_cpp_source to inspect raw evidence");
    std::vector<CppSource> sources;
    auto identities = Json::array();
    std::size_t bytes = 0;
    for(const auto& value : capture.cpp_index.at("source_files")) {
        const auto path = value.get<std::string>();
        const auto name = std::filesystem::path(path).filename().string();
        if(!name.starts_with("CommandList") && !name.starts_with("Resources"))
            continue;
        bytes += inventoried(capture, path, 4U * 1024U * 1024U);
        if(sources.size() >= 64 || bytes > 16U * 1024U * 1024U)
            fail(Error::LimitExceeded, "Generated command/resource sources exceed the aggregate inspection budget");
        auto content = artifacts_.read(capture_id, path, 4U * 1024U * 1024U);
        identities.push_back(source_identity(path, content));
        sources.push_back({path, std::move(content)});
    }
    const auto parsed = inspect_cpp_draws(sources);
    auto result = cpp_base_result(capture);
    result["section"] = section;
    result["source_files"] = std::move(identities);
    result["association_scope"] = parsed.at("association_scope");
    for(const auto* key : {"draws", "unsupported_recordings", "unsupported_objects"}) {
        result[std::string(key) + "_total"] = parsed.at(key).size();
        result[key] = Json::array();
    }
    const auto records = parsed.at(section).get<std::vector<Json>>();
    return page_result(std::move(result), section.c_str(), records, offset, limit,
                       [](const Json& value) { return value; });
}
} // namespace ngm
