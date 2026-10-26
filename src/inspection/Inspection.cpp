#include "ngm/Inspection.hpp"

#include "CppReplay.hpp"
#include "DescriptorProfiles.hpp"
#include "PushState.hpp"
#include "ReplayValues.hpp"
#include "ResourceProfiles.hpp"
#include "ValueExecutionState.hpp"
#include "ValueSourceContract.hpp"
#include "ValueTimeline.hpp"
#include "ngm/CppEvidence.hpp"
#include "ngm/DescriptorRead.hpp"
#include "ngm/DrawState.hpp"
#include "ngm/Hash.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/SpirvUniforms.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
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
    // The qualified no-presentation 2026.3 export leaves primary_api empty.
    // Accept only its observed explicit Vulkan inventories, with the matching
    // service delimiter and invocation. Missing/null or contradictory API fields
    // remain unsupported; do not manufacture a primary_api value for callers.
    bool frame_boundary_profile = false;
    if(capture.metadata.primary_api == "" && std::string_view(capture.profile->cli_version) == "2026.3.1.0" &&
       capture.metadata.graphics_apis == NsightMetadataStringLists{{"Vulkan", {"general"}}} &&
       capture.metadata.graphics_features == NsightMetadataStringLists{{"general", {"Vulkan"}}} &&
       capture.metadata.has_unsupported_operation == false) {
        const auto settings = report.find("capture_settings");
        if(settings != report.end() && settings->is_object() &&
           settings->value("delimiter", Json()) == "vk_frame_boundary") {
            require(*settings == member(manifest, "capture_settings"),
                    "Frame-boundary capture settings differ from manifest");
            const auto& arguments = member(operation, "arguments");
            require(arguments.is_array() && arguments.size() <= 1024 &&
                        std::all_of(arguments.begin(), arguments.end(),
                                    [](const Json& argument) {
                                        return argument.is_string() &&
                                               !argument.get_ref<const std::string&>().empty() &&
                                               argument.get_ref<const std::string&>().find('\0') == std::string::npos;
                                    }),
                    "Frame-boundary capture invocation must retain string arguments");
            frame_boundary_profile =
                std::count(arguments.begin(), arguments.end(), "--delimiter-vk-frame-boundary-ext") == 1 &&
                std::count(arguments.begin(), arguments.end(), "--delimiter-present") == 0 &&
                std::count(arguments.begin(), arguments.end(), "--delimiter-graphics-capture-api") == 0;
        }
    }
    if(capture.metadata.nsight_version != capture.profile->metadata_version ||
       capture.metadata.nsight_version_build_id != capture.profile->build ||
       (capture.metadata.primary_api != "Vulkan" && !frame_boundary_profile)) {
        fail(Error::UnsupportedProducer,
             "Same-bundle metadata must identify the same observed Vulkan version/build profile as capture and "
             "replay; inspect raw/exports/metadata.raw before validating a new producer/profile");
    }
    capture.producer["api_identification"] =
        frame_boundary_profile ? "graphics_apis_for_vk_frame_boundary" : "primary_api";
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
                {"captured_frames", optional(value.captured_frames)},
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

InspectionService::InspectionService(ArtifactStore& artifacts, ResourceWorkers workers) :
    artifacts_(artifacts), workers_(std::move(workers)) {}

Json InspectionService::shader_trace(const std::string& capture_id, const ShaderTraceSelection& selection,
                                     std::size_t offset, std::size_t limit) const {
    page_arguments(offset, limit);
    auto capture = load_capture(artifacts_, capture_id);
    const auto& application = member(capture.report, "application");
    for(const auto* field : {"executable", "sha256_before_launch", "sha256_after_launch", "identity_scope",
                             "output_option", "output_path", "output_evidence_origin"})
        require(member(application, field) == member(member(capture.info.provenance, "application"), field),
                "Shader trace launch/output fields differ between report and manifest");
    require(member(application, "output_path") == "raw/application" &&
                member(application, "output_evidence_origin") == "application_provided",
            "Shader trace requires service-owned application output provenance");
    const auto before = text(application, "sha256_before_launch", 64);
    const auto after = text(application, "sha256_after_launch", 64);
    require(before.size() == 64 && before == after &&
                std::all_of(before.begin(), before.end(),
                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }) &&
                member(application, "identity_scope") ==
                    "path contents before and after launch; running image unverified",
            "Shader trace launch path-content identity is missing or inconsistent");
    constexpr auto path = "raw/application/shader-trace.json";
    inventoried(capture, path, shader_trace_maximum_bytes);
    const auto raw = artifacts_.read(capture_id, path, shader_trace_maximum_bytes);
    Json trace;
    try {
        trace = parse_shader_trace(raw);
    } catch(const std::invalid_argument& error) {
        fail(Error::InvalidExport, error.what());
    }
    const auto shader_path = "raw/application/" + trace.at("shader").at("path").get<std::string>();
    constexpr std::size_t shader_limit = 16U * 1024U * 1024U;
    const auto bytes = inventoried(capture, shader_path, shader_limit);
    require(bytes >= 20 && bytes % 4 == 0, "Reported shader is not a bounded SPIR-V module");
    const auto shader = artifacts_.read(capture_id, shader_path, shader_limit);
    if(shader[0] != '\x03' || shader[1] != '\x02' || shader[2] != '\x23' || shader[3] != '\x07' ||
       sha256(std::as_bytes(std::span(shader))) != trace.at("shader").at("sha256").get_ref<const std::string&>())
        fail(Error::InvalidExport, "Reported instrumented shader bytes/hash differ from retained SPIR-V");
    auto result = shader_trace_page(trace, selection, offset, limit);
    result["capture_id"] = capture_id;
    result["trace"] = {{"path", path}, {"bytes", raw.size()}, {"sha256", sha256(std::as_bytes(std::span(raw)))}};
    result["shader"]["retained_path"] = shader_path;
    result["shader"]["bytes"] = bytes;
    result["shader"]["retained_content_matches_reported_hash"] = true;
    result["launch"] = {{"sha256_before_launch", before},
                        {"sha256_after_launch", after},
                        {"identity_scope", application.at("identity_scope")}};
    result["scope"] = "Application-reported instrumented fragment values; retained hashes and service-owned "
                      "launch/output provenance do not establish shader execution, invocation identity, original "
                      "register values or Graphics Capture event correspondence.";
    if(!fits_response(result))
        fail(Error::LimitExceeded, "Shader trace page exceeds the response budget");
    return result;
}

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
namespace {
std::pair<Json, Json> resource_sources(ArtifactStore& store, const Capture& capture) {
    if(capture.cpp_metadata.has_unsupported_operation)
        fail(Error::ExportUnavailable, "Capture reports unsupported operations; inspect generated source directly");
    std::vector<CppSource> sources;
    Json identities = Json::array();
    std::size_t bytes = 0;
    for(const auto& value : capture.cpp_index.at("source_files")) {
        const auto path = value.get<std::string>();
        const auto name = std::filesystem::path(path).filename().string();
        if(!name.starts_with("CommandList") && !name.starts_with("Resources") && !name.starts_with("Frame"))
            continue;
        bytes += inventoried(capture, path, 4U * 1024U * 1024U);
        if(sources.size() >= 64 || bytes > 16U * 1024U * 1024U)
            fail(Error::LimitExceeded, "Resource source files exceed the aggregate budget");
        auto content = store.read(capture.info.summary.id, path, 4U * 1024U * 1024U);
        identities.push_back(source_identity(path, content));
        sources.push_back({path, std::move(content)});
    }
    return {inspect_cpp_resources(sources), identities};
}
} // namespace
namespace {
struct StateInputs {
    Json parsed, identities, resources;
};
Json capture_database_inputs(const Capture& capture, const Json& anchors) {
    const auto project = capture.cpp_index.at("project_directory").get<std::string>();
    Json result = Json::object();
    for(const auto* name : {"data.bin", "data.bin.rec"}) {
        const auto found = std::find_if(anchors.begin(), anchors.end(),
                                        [&](const Json& file) { return file.at("path") == project + '/' + name; });
        require(found != anchors.end(), "Captured database fingerprint is missing");
        result[name] = {{"bytes", found->at("bytes")}, {"sha256", found->at("sha256")}};
    }
    return result;
}
DescriptorReadRequest descriptor_request(const Capture& capture, const Json& anchors, const Json& parsed,
                                         const Json& reference, const std::filesystem::path& worker, bool pin,
                                         std::chrono::steady_clock::time_point deadline) {
    const auto expected = Json::parse(descriptor_profile_json);
    const auto project = capture.cpp_index.at("project_directory").get<std::string>();
    Json paths = Json::object();
    for(const auto& [name, digest] : expected.at("original_files").items()) {
        (void)digest;
        const auto root = project + '/' + name, alternate = project + "/include/" + name;
        const bool at_root = capture.files.contains(root);
        const bool nested = name == "VulkanStructHydrator.h" && capture.files.contains(alternate);
        if(at_root == nested)
            fail(Error::ExportUnavailable, "Descriptor helper source is absent or ambiguous: " + name);
        paths[name] = at_root ? root : alternate;
    }
    DescriptorReadRequest request;
    request.capture_id = capture.info.summary.id;
    request.project_directory = project;
    request.reference = reference;
    request.registrations = parsed.at("registrations");
    request.helper_profile = expected;
    request.helper_paths = std::move(paths);
    request.expected_inputs = capture_database_inputs(capture, anchors);
    request.worker = worker;
    request.pin = pin;
    request.deadline = deadline;
    return request;
}
struct FingerprintedCppSources {
    std::vector<CppSource> sources;
    Json anchors;
};
FingerprintedCppSources
validated_cpp_sources(ArtifactStore& store, const Capture& capture,
                      std::optional<std::chrono::steady_clock::time_point> parent_deadline = std::nullopt) {
    if(capture.cpp_metadata.has_unsupported_operation)
        fail(Error::ExportUnavailable, "Capture reports unsupported operations; inspect raw generated source");
    if(std::string_view(capture.profile->cli_version) != "2026.3.1.0")
        fail(Error::UnsupportedProducer, "Recorded draw state is qualified only for Nsight 2026.3.1.0/build 38722833");
    const auto& index = capture.cpp_index;
    if(!index.contains("recorded_state_inputs"))
        fail(Error::ExportUnavailable,
             "Capture has no capture-time state-input fingerprints; recapture with 0.4.5 or newer");
    const auto& inputs = index.at("recorded_state_inputs");
    require(number(inputs, "schema_version") == 1, "Unsupported state-input fingerprint schema");
    const auto& files = member(inputs, "files");
    require(files.is_array() && files.size() <= 260, "State-input fingerprint list exceeds bounds");
    std::set<std::string> expected;
    for(const auto& value : index.at("source_files"))
        expected.insert(value.get<std::string>());
    const auto project = index.at("project_directory").get<std::string>();
    for(const auto* name : {"metadata.json", "Resources.h", "data.bin", "data.bin.rec"})
        expected.insert(project + '/' + name);
    std::map<std::string, Json> identities;
    std::size_t total = 0;
    const auto deadline = parent_deadline.value_or(std::chrono::steady_clock::now() + std::chrono::seconds(10));
    for(const auto& file : files) {
        const auto path = text(file, "path", 512), digest = text(file, "sha256", 64);
        require(expected.erase(path) == 1 && digest.size() == 64 &&
                    digest.find_first_not_of("0123456789abcdef") == std::string::npos,
                "Missing, duplicated or invalid state-input identity");
        const auto size = inventoried(capture, path, ArtifactStore::maximum_fingerprint_bytes);
        require(number(file, "bytes") == size, "State-input size does not match inventory");
        total += size;
        if(total > 512U * 1024U * 1024U)
            fail(Error::LimitExceeded, "State inputs exceed the 512 MiB fingerprint budget");
        if(store.fingerprint(capture.info.summary.id, path, ArtifactStore::maximum_fingerprint_bytes, deadline) !=
           digest)
            fail(Error::InvalidExport,
                 "State input changed since capture: " + path + "; recapture rather than reusing execution references");
        identities.emplace(path, file);
    }
    require(expected.empty(), "State-input identities do not cover all captured inputs");
    std::vector<CppSource> sources;
    std::size_t source_bytes = 0;
    for(const auto& value : index.at("source_files")) {
        const auto path = value.get<std::string>(), name = std::filesystem::path(path).filename().string();
        if(!name.starts_with("CommandList") && !name.starts_with("Resources") && !name.starts_with("Frame"))
            continue;
        source_bytes += inventoried(capture, path, 4U * 1024U * 1024U);
        if(sources.size() >= 64 || source_bytes > 16U * 1024U * 1024U)
            fail(Error::LimitExceeded, "State syntax exceeds the 16 MiB/64-file budget");
        auto content = store.read(capture.info.summary.id, path, 4U * 1024U * 1024U);
        if(source_identity(path, content) != identities.at(path))
            fail(Error::InvalidExport, "State source changed while reading: " + path);
        sources.push_back({path, std::move(content)});
    }
    Json anchors = Json::array();
    for(const auto& [path, identity] : identities) {
        (void)path;
        anchors.push_back(identity);
    }
    return {std::move(sources), std::move(anchors)};
}
StateInputs state_sources(ArtifactStore& store, const Capture& capture) {
    auto [sources, anchors] = validated_cpp_sources(store, capture);
    auto parsed = inspect_draw_executions(sources);
    // Resource references remain opaque and can be read through the existing
    // confined reader; never turn initial bytes into post-event values here.
    auto resources = inspect_cpp_resources(sources);
    const auto input_text = anchors.dump();
    const auto input_hash = sha256(std::as_bytes(std::span(input_text.data(), input_text.size())));
    for(auto& execution : parsed["executions"]) {
        const auto identity = Json{{"capture_id", capture.info.summary.id},
                                   {"inputs_sha256", input_hash},
                                   {"selection", execution.at("selection")}}
                                  .dump();
        execution["execution_ref"] = sha256(std::as_bytes(std::span(identity.data(), identity.size())));
        auto& shaders = execution["state"]["shaders"];
        if(shaders.at("status") == "qualified_recorded_state") {
            for(auto& stage : shaders["value"]) {
                Json matches = Json::array();
                const auto& module = stage.at("module_source");
                for(const auto& resource : resources.at("resources")) {
                    const auto& location = resource.at("source");
                    if(resource.at("handle") == stage.at("resource_handle") && resource.at("readable") == true &&
                       resource.at("expected_bytes") == stage.at("declared_bytes") &&
                       location.at("path") == module.at("path") &&
                       location.at("start_line") >= module.at("start_line") &&
                       location.at("end_line") <= module.at("end_line"))
                        matches.push_back(resource.at("resource_ref"));
                }
                stage["serialized_byte_read"] = {
                    {"status", "not_read"},
                    {"sha256", nullptr},
                    {"reason",
                     matches.size() == 1
                         ? "Read this opaque shader resource through capture_cpp_resource for exact bytes/hash"
                         : "No unique readable serialized shader reference"},
                    {"resource_ref", matches.size() == 1 ? matches[0] : Json(nullptr)}};
            }
        }
        auto& push = execution["state"]["push_constants"];
        if(push.contains("coverage"))
            for(auto& range : push["coverage"]["ranges"])
                for(auto& segment : range["segments"]) {
                    if(segment.at("status") != "not_read")
                        continue;
                    const auto& write = segment.at("write");
                    const auto& location = write.at("resource_source");
                    Json matches = Json::array();
                    for(const auto& resource : resources.at("resources"))
                        if(resource.at("handle") == write.at("handle") && resource.at("macro") == write.at("macro") &&
                           resource.at("expected_bytes") == write.at("declared_bytes") &&
                           resource.at("readable") == true && resource.at("conditional") == false &&
                           resource.at("source").at("path") == location.at("path") &&
                           resource.at("source").at("start_line") == location.at("start_line") &&
                           resource.at("source").at("end_line") == location.at("end_line"))
                            matches.push_back(resource.at("resource_ref"));
                    if(matches.size() == 1)
                        segment["resource_ref"] = matches[0];
                    else {
                        segment["status"] = "unsupported";
                        segment["reason"] = "push input has no unique readable source reference";
                        push["coverage"]["input_coverage_complete"] = false;
                    }
                }
    }
    return {std::move(parsed), std::move(anchors), std::move(resources)};
}
Json state_base(const Capture& capture, const Json& parsed, Json identities) {
    auto result = cpp_base_result(capture);
    result["schema_profile"] = std::string(capture.profile->name) + "-recorded-state-v3";
    for(const auto* key : {"selection_domain", "evidence_origin", "captured_gpu_execution_correspondence",
                           "full_effective_state_coverage"})
        result[key] = parsed.at(key);
    result["input_files"] = std::move(identities);
    result["association_scope"] = "Recorded API state at draw occurrences in one generated Frame0Part00 submission "
                                  "stream; no observed GPU state or join to graphics function-inventory event IDs";
    return result;
}
} // namespace
Json InspectionService::draw_executions(const std::string& capture_id, const std::string& section, std::size_t offset,
                                        std::size_t limit) const {
    page_arguments(offset, limit);
    if(section != "executions" && section != "unsupported")
        throw std::invalid_argument("State section must be executions or unsupported");
    auto capture = load_capture(artifacts_, capture_id, true);
    auto [parsed, identities, references] = state_sources(artifacts_, capture);
    (void)references;
    auto result = state_base(capture, parsed, std::move(identities));
    result["section"] = section;
    result["executions_total"] = parsed.at("executions").size();
    result["unsupported_total"] = parsed.at("unsupported").size();
    auto rows = parsed.at(section).get<std::vector<Json>>();
    if(section == "executions")
        for(auto& row : rows)
            row.erase("state");
    return page_result(std::move(result), section.c_str(), rows, offset, limit,
                       [](const Json& value) { return value; });
}
Json InspectionService::draw_state(const std::string& capture_id, const std::string& execution_ref,
                                   bool read_push_constants, bool pin) const {
    if(pin && !read_push_constants)
        throw std::invalid_argument("pin requires read_push_constants");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    if(execution_ref.size() != 64 || execution_ref.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::invalid_argument("Execution reference must be a listed 64-character lowercase SHA-256 reference");
    auto capture = load_capture(artifacts_, capture_id, true);
    auto [parsed, identities, references] = state_sources(artifacts_, capture);
    const auto& rows = parsed.at("executions");
    const auto found = std::find_if(rows.begin(), rows.end(),
                                    [&](const auto& row) { return row.at("execution_ref") == execution_ref; });
    if(found == rows.end())
        fail(Error::ExportUnavailable,
             "Execution reference is absent, foreign or stale; list this capture's draw executions");
    auto result = state_base(capture, parsed, std::move(identities));
    result["execution"] = *found;
    auto& push = result["execution"]["state"]["push_constants"];
    if(read_push_constants && push.contains("coverage")) {
        std::map<std::string, Json> selected, reads;
        std::size_t bytes = 0;
        for(const auto& range : push.at("coverage").at("ranges"))
            for(const auto& segment : range.at("segments"))
                if(segment.at("status") == "not_read" && segment.at("resource_ref").is_string()) {
                    const auto ref = segment.at("resource_ref").get<std::string>();
                    if(selected.contains(ref))
                        continue;
                    const auto& rows_ = references.at("resources");
                    const auto reference = std::find_if(rows_.begin(), rows_.end(),
                                                        [&](const auto& row) { return row.at("resource_ref") == ref; });
                    require(reference != rows_.end(), "Push reference not present in source inventory");
                    bytes += number(*reference, "expected_bytes");
                    selected.emplace(ref, *reference);
                }
        if(selected.size() > 8 || bytes > 2048)
            fail(Error::LimitExceeded, "Push query exceeds eight reads/2048 input bytes");
        if(!selected.empty() && workers_.nsight_2026_3.empty())
            fail(Error::ExportUnavailable, "Push bytes require the matching configured 2026.3 resource worker");
        const auto profile = Json::parse(resource_profiles_json).at("2026_3");
        push["read_artifacts"] = Json::array();
        for(const auto& [ref, reference] : selected) {
            ResourceReadRequest request{capture_id,
                                        capture.cpp_index.at("project_directory").get<std::string>(),
                                        reference,
                                        profile,
                                        workers_.nsight_2026_3,
                                        0,
                                        number(reference, "expected_bytes"),
                                        pin,
                                        deadline};
            auto read = read_cpp_resource(artifacts_, request);
            for(const auto* name : {"data.bin", "data.bin.rec"}) {
                const auto path = capture.cpp_index.at("project_directory").get<std::string>() + '/' + name;
                const auto& anchors = result.at("input_files");
                const auto match = std::find_if(anchors.begin(), anchors.end(),
                                                [&](const auto& input) { return input.at("path") == path; });
                require(match != anchors.end() && match->at("sha256") == read.at("inputs").at(name).at("sha256"),
                        "Push reader snapshot differs from capture-time inputs");
            }
            require(read.at("returned_bytes") == number(reference, "expected_bytes") &&
                        read.at("total_bytes") == number(reference, "expected_bytes") &&
                        read.at("next_offset").is_null(),
                    "Push payload size does not match its recorded static input");
            push["read_artifacts"].push_back(read.at("artifact_id"));
            reads.emplace(ref, std::move(read));
        }
        detail::resolve_push_data(push, reads);
    }
    if(result.dump().size() > maximum_page_bytes || !fits_response(result))
        fail(Error::LimitExceeded, "Selected state exceeds the bounded response; inspect raw sources");
    return result;
}
Json InspectionService::cpp_resources(const std::string& capture_id, const std::string& section, std::size_t offset,
                                      std::size_t limit) const {
    page_arguments(offset, limit);
    if(section != "resources" && section != "unsupported")
        throw std::invalid_argument("Resource section must be resources or unsupported");
    auto capture = load_capture(artifacts_, capture_id, true);
    auto [parsed, identities] = resource_sources(artifacts_, capture);
    auto result = cpp_base_result(capture);
    result["source_files"] = std::move(identities);
    result["source_scope"] = "CommandList*, Resources* and Frame* indexed .cpp files; helper implementations excluded";
    result["association_scope"] = parsed.at("association_scope");
    result["section"] = section;
    for(const auto* key : {"resources", "unsupported"}) {
        result[std::string(key) + "_total"] = parsed.at(key).size();
        result[key] = Json::array();
    }
    return page_result(std::move(result), section.c_str(), parsed.at(section).get<std::vector<Json>>(), offset, limit,
                       [](const Json& value) { return value; });
}
Json InspectionService::cpp_resource(const std::string& capture_id, const std::string& resource_ref, std::size_t offset,
                                     std::size_t length, bool pin) const {
    if(resource_ref.size() != 64 || resource_ref.find_first_not_of("0123456789abcdef") != std::string::npos ||
       offset > 16U * 1024U * 1024U || length == 0 || length > 65536)
        throw std::invalid_argument("Invalid resource reference or byte range");
    auto capture = load_capture(artifacts_, capture_id, true);
    const auto [parsed, identities] = resource_sources(artifacts_, capture);
    const auto& rows = parsed.at("resources");
    const auto found =
        std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.at("resource_ref") == resource_ref; });
    if(found == rows.end())
        fail(Error::ExportUnavailable, "Resource reference is absent or source changed; list resources again");
    if(found->at("readable") != true)
        fail(Error::ExportUnavailable, "Resource has conflicting or invalid byte declarations");
    const bool newer = std::string_view(capture.profile->cli_version) == "2026.3.1.0";
    const auto profiles = Json::parse(resource_profiles_json);
    auto result =
        read_cpp_resource(artifacts_, {capture_id, capture.cpp_index.at("project_directory").get<std::string>(), *found,
                                       profiles.at(newer ? "2026_3" : "2026_2"),
                                       newer ? workers_.nsight_2026_3 : workers_.nsight_2026_2, offset, length, pin});
    result["producer"] = capture.producer;
    // Full source identities remain in the listing; the selected reference binds
    // its complete source hash and span in both the read result and retained log.
    if(!fits_response(result))
        fail(Error::LimitExceeded, "Resource response exceeds MCP budget");
    return result;
}

Json InspectionService::cpp_descriptor_writes(const std::string& capture_id, const std::string& update_ref,
                                              const std::string& section, std::size_t offset, std::size_t limit,
                                              bool pin) const {
    page_arguments(offset, limit);
    if((section != "updates" && section != "unsupported") ||
       (!update_ref.empty() && (section != "updates" || update_ref.size() != 64 ||
                                update_ref.find_first_not_of("0123456789abcdef") != std::string::npos)) ||
       (pin && update_ref.empty()))
        throw std::invalid_argument("List updates/unsupported, or supply a listed update_ref; pin requires a read");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    auto capture = load_capture(artifacts_, capture_id, true);
    auto [sources, anchors] = validated_cpp_sources(artifacts_, capture, deadline);
    auto parsed = inspect_cpp_descriptor_updates(sources);
    const auto input_text = anchors.dump();
    const auto input_hash = sha256(std::as_bytes(std::span(input_text.data(), input_text.size())));
    for(auto& row : parsed["updates"]) {
        const auto identity =
            Json{{"capture_id", capture_id}, {"inputs_sha256", input_hash}, {"source_ref", row.at("source_ref")}}
                .dump();
        row["update_ref"] = sha256(std::as_bytes(std::span(identity.data(), identity.size())));
    }
    auto result = cpp_base_result(capture);
    result["schema_profile"] = std::string(capture.profile->name) + "-descriptor-writes-v1";
    result["input_files"] = anchors;
    result["inputs_sha256"] = input_hash;
    result["association_scope"] = parsed.at("association_scope");
    result["registration_coverage_complete"] = parsed.at("registration_coverage_complete");
    result["section"] = section;
    result["read"] = nullptr;
    result["writes"] = Json::array();
    result["writes_total"] = 0;
    for(const auto* name : {"updates", "unsupported"}) {
        result[name] = Json::array();
        result[std::string(name) + "_total"] = parsed.at(name).size();
    }
    if(update_ref.empty())
        return page_result(std::move(result), section.c_str(), parsed.at(section).get<std::vector<Json>>(), offset,
                           limit, [](const Json& row) { return row; });
    const auto& updates = parsed.at("updates");
    const auto found = std::find_if(updates.begin(), updates.end(),
                                    [&](const Json& row) { return row.at("update_ref") == update_ref; });
    if(found == updates.end())
        fail(Error::ExportUnavailable,
             "Descriptor update reference is absent, foreign or stale; list this capture again");
    if(found->at("readable") != true || parsed.at("registration_coverage_complete") != true)
        fail(Error::ExportUnavailable,
             "Descriptor update or typed source registrations are unqualified; inspect unsupported forms");
    if(workers_.descriptor_nsight_2026_3.empty())
        fail(Error::ExportUnavailable, "Configure --descriptor-worker-2026-3 before decoding source descriptor writes");
    const auto request =
        descriptor_request(capture, anchors, parsed, *found, workers_.descriptor_nsight_2026_3, pin, deadline);
    auto read = read_cpp_descriptor_writes(artifacts_, request);
    const auto writes = read.at("writes").get<std::vector<Json>>();
    read.erase("writes");
    result["read"] = std::move(read);
    result["section"] = "writes";
    result["writes_total"] = writes.size();
    try {
        return page_result(result, "writes", writes, offset, limit, [](const Json& row) { return row; });
    } catch(const InspectionError& error) {
        throw InspectionError(error.code(), std::string(error.what()) + "; descriptor read artifact " +
                                                result.at("read").at("artifact_id").get<std::string>());
    }
}

Json InspectionService::draw_values(const std::string& capture_id, const std::string& execution_ref,
                                    const std::string& section, std::size_t offset, std::size_t limit, bool pin) const {
    page_arguments(offset, limit);
    if((execution_ref.empty() && (pin || (section != "executions" && section != "unsupported"))) ||
       (!execution_ref.empty() &&
        (execution_ref.size() != 64 || execution_ref.find_first_not_of("0123456789abcdef") != std::string::npos ||
         (section != "executions" && section != "values"))))
        throw std::invalid_argument("List executions/unsupported, or read a listed execution_ref; pin requires a read");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    auto capture = load_capture(artifacts_, capture_id, true);
    auto [sources, anchors] = validated_cpp_sources(artifacts_, capture, deadline);
    const auto input_text = anchors.dump();
    const auto input_hash = sha256(std::as_bytes(std::span(input_text.data(), input_text.size())));
    const auto parsed = detail::parse_cpp_replay(sources, detail::ReplayParseScope::Values);
    auto candidates = detail::value_execution_candidates(parsed);
    const auto reference_for = [&](const Json& selection) {
        const auto text = Json{
            {"interface", "ngm-cpp-draw-values-v1"},
            {"capture_id", capture_id},
            {"inputs_sha256", input_hash},
            {"selection",
             selection}}.dump();
        return sha256(std::as_bytes(std::span(text.data(), text.size())));
    };
    for(auto& row : candidates["executions"])
        row["execution_ref"] = reference_for(row.at("selection"));
    auto result = cpp_base_result(capture);
    result["evidence_origin"] = "nsight_generated_cpp_draw_values";
    result["schema_profile"] = std::string(capture.profile->name) + "-draw-values-v1";
    result["selection_domain"] = "generated_cpp_captured_submission";
    result["captured_gpu_execution_correspondence"] = false;
    result["association_scope"] =
        "Bounded generated-source uniform visibility model for one captured C++ submission; "
        "no live invocation/register observation or join to numeric graphics-inventory event IDs";
    result["input_files"] = anchors;
    result["inputs_sha256"] = input_hash;
    result["executions_total"] = candidates.at("executions").size();
    result["unsupported_total"] = candidates.at("unsupported").size();
    result["executions"] = Json::array();
    result["unsupported"] = Json::array();
    result["values"] = Json::array();
    result["values_total"] = 0;
    result["execution"] = nullptr;
    result["values_status"] = "not_read";
    result["reason"] = nullptr;
    result["artifact_id"] = nullptr;
    result["report_path"] = nullptr;
    result["read_artifacts"] = Json::array();
    result["section"] = section;
    if(execution_ref.empty())
        return page_result(std::move(result), section.c_str(), candidates.at(section).get<std::vector<Json>>(), offset,
                           limit, [](const Json& row) { return row; });
    const auto selected = std::find_if(candidates.at("executions").begin(), candidates.at("executions").end(),
                                       [&](const Json& row) { return row.at("execution_ref") == execution_ref; });
    if(selected == candidates.at("executions").end())
        fail(Error::ExportUnavailable,
             "Draw-value reference is absent, foreign or stale; list this capture's value candidates");
    if(workers_.descriptor_nsight_2026_3.empty() || workers_.nsight_2026_3.empty())
        fail(Error::ExportUnavailable,
             "Draw values require matching --descriptor-worker-2026-3 and --resource-worker-2026-3");
    const auto project = capture.cpp_index.at("project_directory").get<std::string>();
    const auto database = capture_database_inputs(capture, anchors);
    auto writer = artifacts_.begin({{"evidence_origin", "nsight_cpp_draw_value_query"},
                                    {"capture_id", capture_id},
                                    {"execution_ref", execution_ref},
                                    {"report_path", "raw/draw-values.json"}},
                                   {"raw/draw-values.json"}, pin);
    Json report{{"schema_version", 1},
                {"project_version", project_version()},
                {"evidence_origin", "nsight_cpp_draw_value_query"},
                {"capture_id", capture_id},
                {"execution_ref", execution_ref},
                {"input_files", anchors},
                {"inputs_sha256", input_hash},
                {"database_inputs", database},
                {"status", "preparing"},
                {"source_contract", Json::array()},
                {"descriptor_reads", Json::array()},
                {"resource_reads", Json::array()},
                {"raw_values", Json::array()}};
    const auto flush = [&] {
        const auto text = report.dump(2) + '\n';
        if(text.size() > 16U * 1024U * 1024U)
            fail(Error::LimitExceeded, "Draw-value retained report exceeds 16 MiB");
        std::ofstream file(writer.raw_directory() / "draw-values.json", std::ios::binary);
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file << text;
        file.close();
    };
    const auto checkpoint = [&] {
        if(std::chrono::steady_clock::now() >= deadline)
            fail(Error::LimitExceeded, "Draw-value query exceeded its shared twenty-second deadline");
    };
    std::size_t requests = 0, returned_bytes = 0;
    const auto budget = [&](std::size_t bytes = 0) {
        checkpoint();
        returned_bytes += bytes;
        if(requests > 32 || returned_bytes > 1024U * 1024U)
            fail(Error::LimitExceeded,
                 "Draw values exceed 32 parent reads or 1 MiB of returned byte/structure payloads");
    };
    try {
        flush();
        // Source semantics only. These exported helpers are fingerprinted and
        // inspected as text; none is compiled or executed by this query.
        const std::map<std::string, std::string> contract{
            {"VulkanReplay.cpp", "a4e0f9ddbb097b8bc131a55eb574e9306b74222a84ed5f67365bc3ad39a46bfa"},
            {"include/VulkanReplay.h", "74e2ea544c8552d17616d8a3800210647580d76f6fd64b076dd631f4f2df5f4c"},
            {"include/VulkanHelpers.h", "e2ae3909a03e35167d47590ed883c4281e7b4723e03c9787d3ad68bfc8045448"},
            {"ReplayProcedures.cpp", "f889fbdbc6bb134e95f67ad9942ea2b41230b3c25e6ee01f40b5ea590cb50108"},
            {"Application.cpp", "53ccbd68bfc44264e8cedd5f141b186cf5fdfbc626b55482142c68807687c5b3"},
            {"CommonReplay.cpp", "43916f87a901740ddb38a02cc02085d2c0e65e1e85cb608ebb4d21eeeb1196d7"}};
        for(const auto& [name, hash] : contract) {
            const auto path = project + '/' + name;
            (void)inventoried(capture, path, 2U * 1024U * 1024U);
            const auto raw_hash = artifacts_.fingerprint(capture_id, path, 2U * 1024U * 1024U, deadline);
            auto contract_hash = raw_hash;
            if(name == "ReplayProcedures.cpp") {
                const auto raw_text = artifacts_.read(capture_id, path, 65536);
                require(sha256(std::as_bytes(std::span(raw_text.data(), raw_text.size()))) == raw_hash,
                        "Replay procedures changed while reading the source contract");
                const auto text = detail::canonical_value_procedures(raw_text);
                contract_hash = sha256(std::as_bytes(std::span(text.data(), text.size())));
            }
            if(contract_hash != hash)
                fail(Error::ExportUnavailable, "Exported draw-value source contract is unqualified: " + name);
            report["source_contract"].push_back(
                {{"path", path},
                 {"sha256", raw_hash},
                 {"contract_sha256", contract_hash},
                 {"canonicalization", name == "ReplayProcedures.cpp" ? "literal-prototypes-v1" : "none"}});
        }
        auto descriptors = inspect_cpp_descriptor_updates(sources);
        require(descriptors.at("registration_coverage_complete") == true,
                "Draw values require complete typed source registrations");
        for(auto& ref : descriptors["updates"]) {
            const auto text =
                Json{{"capture_id", capture_id}, {"inputs_sha256", input_hash}, {"source_ref", ref.at("source_ref")}}
                    .dump();
            ref["update_ref"] = sha256(std::as_bytes(std::span(text.data(), text.size())));
        }
        std::vector<Json> updates;
        for(const auto* calls : {&parsed.initial_state, &parsed.frame})
            for(const auto& call : *calls)
                if(call.name == "VulkanHelper_UpdateDescriptorSetsFromDatabase" && call.arguments.size() == 6 &&
                   detail::number(call.arguments[1]) != 0) {
                    const Json* found = nullptr;
                    for(const auto& ref : descriptors.at("updates"))
                        if(ref.at("source") == call.source) {
                            require(!found, "Ambiguous descriptor source reference in value query");
                            found = &ref;
                        }
                    if(!found || found->at("readable") != true || detail::number(call.arguments[4]) != 0)
                        fail(Error::ExportUnavailable,
                             "Draw-value descriptor source write is outside the qualified reader profile");
                    updates.push_back(*found);
                }
        if(updates.size() > 32)
            fail(Error::LimitExceeded, "Draw-value source write count exceeds the parent-read budget");
        std::vector<Json> descriptor_reads;
        for(const auto& ref : updates) {
            ++requests;
            budget();
            report["current_operation"] = {{"kind", "descriptor_write"}, {"reference", ref}};
            flush();
            auto read = read_cpp_descriptor_writes(artifacts_, descriptor_request(capture, anchors, descriptors, ref,
                                                                                  workers_.descriptor_nsight_2026_3,
                                                                                  pin, deadline));
            budget(number(read, "output_used_bytes"));
            result["read_artifacts"].push_back(read.at("artifact_id"));
            auto receipt = read;
            receipt.erase("writes");
            report["descriptor_reads"].push_back(std::move(receipt));
            descriptor_reads.push_back(std::move(read));
        }
        const auto references = inspect_cpp_resources(sources).at("resources");
        const auto profile = Json::parse(resource_profiles_json).at("2026_3");
        const auto read_resource = [&](const Json& ref, std::size_t first, std::size_t bytes) {
            ++requests;
            budget();
            if(bytes == 0 || bytes > 65536)
                fail(Error::LimitExceeded, "A draw-value resource read exceeds 64 KiB");
            report["current_operation"] = {
                {"kind", "resource"}, {"reference", ref}, {"offset", first}, {"length", bytes}};
            flush();
            auto read = read_cpp_resource(
                artifacts_, {capture_id, project, ref, profile, workers_.nsight_2026_3, first, bytes, pin, deadline});
            require(read.at("capture_id") == capture_id && read.at("inputs") == database &&
                        read.at("offset") == first && read.at("returned_bytes") == bytes,
                    "Draw-value byte reader differs from the exact capture snapshot/range");
            budget(bytes);
            result["read_artifacts"].push_back(read.at("artifact_id"));
            auto receipt = read;
            receipt.erase("data");
            report["resource_reads"].push_back(std::move(receipt));
            return read;
        };
        const auto unhex = [](const Json& read) {
            const auto& text = read.at("data").get_ref<const std::string&>();
            require(text.size() <= 131072 && text.size() % 2 == 0, "Draw-value reader returned invalid hex extent");
            std::vector<std::byte> bytes;
            bytes.reserve(text.size() / 2);
            for(std::size_t at = 0; at < text.size(); at += 2) {
                unsigned value = 0;
                const auto [end, error] = std::from_chars(text.data() + at, text.data() + at + 2, value, 16);
                require(error == std::errc{} && end == text.data() + at + 2,
                        "Draw-value reader returned invalid hex bytes");
                bytes.push_back(std::byte(value));
            }
            require(bytes.size() == read.at("returned_bytes") && sha256(bytes) == read.at("sha256").get<std::string>(),
                    "Draw-value returned byte digest differs from its parent receipt");
            return bytes;
        };
        auto model = detail::value_draw_plans(
            parsed, capture_id, database, descriptor_reads,
            [&](const Json& stage) {
                const Json* found = nullptr;
                const auto first = number(stage.at("module_source"), "byte_offset");
                const auto size = number(stage.at("module_source"), "byte_count");
                for(const auto& ref : references) {
                    const auto& source = ref.at("source");
                    const auto at = number(source, "byte_offset");
                    if(ref.at("handle") == stage.at("resource_handle") && ref.at("readable") == true &&
                       ref.at("conditional") == false && ref.at("expected_bytes") == stage.at("declared_bytes") &&
                       source.at("path") == stage.at("module_source").at("path") && at >= first && at - first < size &&
                       number(source, "byte_count") <= size - (at - first)) {
                        require(!found, "Ambiguous source accessor for selected value shader");
                        found = &ref;
                    }
                }
                if(!found)
                    fail(Error::ExportUnavailable, "Selected value shader has no unique readable byte source");
                auto read = read_resource(*found, 0, number(stage, "declared_bytes"));
                return detail::ValueShader{unhex(read), std::move(read)};
            },
            deadline);
        checkpoint();
        result["section"] = "values";
        result["execution"] = *selected;
        result["artifact_id"] = writer.id();
        result["report_path"] = "raw/draw-values.json";
        std::vector<Json> values;
        if(model.at("status") != "qualified_source_draw_value_plans") {
            result["values_status"] = "unavailable";
            result["reason"] = model.at("reason");
            report["model_refusal"] = model;
        } else {
            const auto found =
                std::find_if(model.at("executions").begin(), model.at("executions").end(),
                             [&](const Json& row) { return reference_for(row.at("selection")) == execution_ref; });
            require(found != model.at("executions").end(),
                    "Selected source-value occurrence differs from its candidate");
            report["selection_plan"] = *found;
            std::map<std::string, Json> byte_cache;
            for(const auto& plan : found->at("plans")) {
                checkpoint();
                const auto length = number(plan, "prefix_bytes"), base = number(plan, "effective_offset");
                require(length > 0 && length <= 65536, "Selected uniform prefix exceeds bounds");
                std::vector<std::byte> bytes(length);
                std::vector<bool> covered(length, false);
                for(const auto& part : plan.at("upload").at("segments")) {
                    const auto& payload = part.at("payload");
                    const Json* ref = nullptr;
                    for(const auto& r : references)
                        if(r.at("handle") == payload.at("handle") && r.at("source") == payload.at("source") &&
                           r.at("macro") == payload.at("accessor") &&
                           r.at("expected_bytes") == payload.at("expected_bytes") && r.at("readable") == true &&
                           r.at("conditional") == false) {
                            require(!ref, "Ambiguous uniform upload byte accessor");
                            ref = &r;
                        }
                    if(!ref)
                        fail(Error::ExportUnavailable, "Latest uniform upload has no exact readable source accessor");
                    const auto first = number(part, "payload_offset"), count = number(part, "length");
                    const auto offset_ = number(part, "buffer_offset");
                    require(offset_ >= base && offset_ - base <= length && count > 0 &&
                                count <= length - (offset_ - base),
                            "Uniform assembly segment escapes its selected prefix");
                    const auto cache_key = ref->at("resource_ref").get<std::string>() + ':' + std::to_string(first) +
                                           ':' + std::to_string(count);
                    if(!byte_cache.contains(cache_key))
                        byte_cache.emplace(cache_key, read_resource(*ref, first, count));
                    const auto& read = byte_cache.at(cache_key);
                    require(read.at("total_bytes") == payload.at("required_bytes"),
                            "Uniform source byte extent differs from the modeled upload");
                    const auto data = unhex(read);
                    for(std::size_t i = 0; i < count; ++i) {
                        const auto at = offset_ - base + i;
                        require(!covered[at], "Uniform assembly segments overlap");
                        covered[at] = true;
                        bytes[at] = data.at(i);
                    }
                }
                require(std::all_of(covered.begin(), covered.end(), [](bool value) { return value; }),
                        "Uniform assembly has a byte-coverage hole");
                const auto decoded = decode_spirv_uniform_read(plan.at("uniform_read"), bytes);
                const auto& uniform = plan.at("uniform_read");
                Json row{{"stage", plan.at("shader").at("stage")},
                         {"module", plan.at("shader").at("module")},
                         {"module_sha256", plan.at("shader_effects").at("module_sha256")},
                         {"entry_point", plan.at("shader_effects").at("entry_point")},
                         {"descriptor_set", uniform.at("descriptor_set")},
                         {"binding", uniform.at("binding")},
                         {"array_element", 0},
                         {"buffer", plan.at("buffer")},
                         {"effective_offset", base},
                         {"effective_range", plan.at("effective_range")},
                         {"member_offset", uniform.at("offset")},
                         {"variable_name", uniform.at("variable_name")},
                         {"block_name", uniform.at("block_name")},
                         {"member_name", uniform.at("member_name")},
                         {"load_word_offset", uniform.at("word_offset")},
                         {"scalar", decoded.at("scalar")},
                         {"components", decoded.at("components")},
                         {"evidence",
                          {{"artifact_id", writer.id()},
                           {"report_path", "raw/draw-values.json"},
                           {"value_index", values.size()}}}};
                report["raw_values"].push_back({{"value", row}, {"source_plan", plan}});
                values.push_back(std::move(row));
            }
            result["values_status"] = values.empty() ? "not_applicable" : "qualified_source_values";
        }
        result["execution"]["value_status"] = result.at("values_status");
        result["values_total"] = values.size();
        report["status"] = "succeeded";
        report["result"] = page_result(result, "values", values, offset, limit, [](const Json& row) { return row; });
        report["parent_read_count"] = requests;
        report["returned_payload_bytes"] = returned_bytes;
        checkpoint();
        flush();
        artifacts_.publish_success(writer);
        return report.at("result");
    } catch(const std::exception& error) {
        report["status"] = "failed";
        report["error"] = std::string(error.what()).substr(0, 4096);
        try {
            flush();
            artifacts_.publish_failure(writer, "Draw-value inspection failed; inspect raw/draw-values.json");
        } catch(...) { /* An interrupted/unpublished writer remains protected. */
        }
        throw InspectionError(Error::ExportUnavailable, std::string(error.what()).substr(0, 2048) +
                                                            "; draw-value evidence artifact " + writer.id());
    }
}
} // namespace ngm
