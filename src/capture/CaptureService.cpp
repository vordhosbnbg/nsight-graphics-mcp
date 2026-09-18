#include "ngm/CaptureService.hpp"

#include "ngm/Hash.hpp"
#include "ngm/Nsight.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace ngm {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

void save(const fs::path& path, const Json& value) {
    std::ofstream output(path.string() + ".tmp", std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
    output.close();
    fs::rename(path.string() + ".tmp", path);
}

Json process_json(const ProcessResult& process) {
    return {{"exit_code", process.exit_code ? Json(*process.exit_code) : Json(nullptr)},
            {"signal", process.signal ? Json(*process.signal) : Json(nullptr)},
            {"timed_out", process.timed_out},
            {"cancelled", process.cancelled},
            {"cleanup_confirmed", process.cleanup_confirmed},
            {"error", process.error}};
}

std::string relative_evidence(const fs::path& path, const fs::path& root) {
    return path.empty() ? std::string() : path.lexically_relative(root).generic_string();
}

Json observation_json(const NsightToolObservation& tool, const fs::path& root) {
    return {{"name", tool.name},
            {"path", tool.path ? Json(tool.path->string()) : Json(nullptr)},
            {"version", tool.version},
            {"build", tool.build},
            {"version_valid", tool.version_valid},
            {"help_valid", tool.help_valid},
            {"discovery_exit_one", tool.accepted_discovery_exit_one},
            {"version_process", process_json(tool.version_process)},
            {"help_process", process_json(tool.help_process)},
            {"version_stdout", relative_evidence(tool.version_stdout, root)},
            {"version_stderr", relative_evidence(tool.version_stderr, root)},
            {"help_stdout", relative_evidence(tool.help_stdout, root)},
            {"help_stderr", relative_evidence(tool.help_stderr, root)},
            {"problem", tool.problem}};
}

Json operation_json(const NsightOperationResult& result, const fs::path& root) {
    return {{"outcome", nsight_outcome_name(result.outcome)},
            {"process", process_json(result.process)},
            {"launched", result.launched},
            {"executable", result.executable.string()},
            {"arguments", result.arguments},
            {"stdout", relative_evidence(result.stdout_path, root)},
            {"stderr", relative_evidence(result.stderr_path, root)},
            {"output", relative_evidence(result.output_file, root)},
            {"output_bytes", result.output_bytes},
            {"message", result.message}};
}

std::map<std::string, std::string> isolated_environment(const CaptureServiceOptions& options, const fs::path& root) {
    auto environment = options.environment;
    // Nsight adds its layer directory to this search path. Materialize the XDG
    // defaults first so that injection does not hide system Vulkan drivers when
    // the caller supplied no value. Preserve explicit installation search paths.
    if(environment["XDG_DATA_DIRS"].empty()) {
        environment["XDG_DATA_DIRS"] = "/usr/local/share:/usr/share";
    }
    for(const auto& [name, relative] : std::map<std::string, std::string>{{"HOME", "home"},
                                                                          {"XDG_CONFIG_HOME", "config"},
                                                                          {"XDG_CACHE_HOME", "cache"},
                                                                          {"XDG_DATA_HOME", "data"},
                                                                          {"XDG_STATE_HOME", "state"},
                                                                          {"TMPDIR", "tmp"}}) {
        const auto directory = root / "raw/config" / relative;
        fs::create_directories(directory);
        environment[name] = directory.string();
    }
    // Nsight owns its documented injection environment. In particular, do not
    // carry over the fixture-only runner's implicit-layer disabling setting.
    environment.erase("VK_LOADER_LAYERS_DISABLE");
    return environment;
}

std::chrono::milliseconds remaining(const JobContext& context) {
    return std::max(std::chrono::milliseconds(1),
                    std::chrono::duration_cast<std::chrono::milliseconds>(context.deadline - JobClock::now()));
}

bool stopped(const JobContext& context) {
    return context.stop.stop_requested() || JobClock::now() >= context.deadline;
}

Json manifest_provenance(const Json& report) {
    // Full argv, caller provenance, and operation observations are retained in
    // raw/report.json. Their growth must not exhaust the compact manifest API.
    Json result{{"report_path", "raw/report.json"}};
    for(const auto* key :
        {"schema_version", "project_version", "evidence_origin", "backend", "job", "capture_settings", "sdk",
         "readable_capture", "application_identity_observed", "cleanup_confirmed", "worker_outcome"}) {
        if(report.contains(key)) {
            result[key] = report.at(key);
        }
    }
    if(report.contains("application")) {
        const auto& app = report.at("application");
        for(const auto* key : {"executable", "sha256_before_launch", "sha256_after_launch", "identity_scope"}) {
            if(app.contains(key)) {
                result["application"][key] = app.at(key);
            }
        }
    }
    if(report.contains("nsight")) {
        for(const auto* tool : {"cli", "capture", "replay"}) {
            const auto& observation = report.at("nsight").at(tool);
            for(const auto* key : {"path", "version", "build"}) {
                result["nsight"][tool][key] = observation.at(key);
            }
        }
    }
    return result;
}

std::string failure_reason(const std::string& message) {
    auto result = Json::parse(Json(message).dump(-1, ' ', false, Json::error_handler_t::replace)).get<std::string>();
    constexpr std::string_view suffix = " [truncated]";
    if(result.size() > 4096) {
        auto end = 4096 - suffix.size();
        while(end && (static_cast<unsigned char>(result[end]) & 0xc0U) == 0x80U) {
            --end;
        }
        result.resize(end);
        result += suffix;
    }
    return result.empty() ? "Capture failed without a diagnostic" : result;
}

struct Attempt {
    ArtifactStore* artifacts = nullptr;
    ArtifactWriter writer;
    Json provenance;
    bool started = false;

    ~Attempt() {
        if(!started && artifacts && !writer.id().empty()) {
            // Queued cancellation/deadline and rejected submission never own
            // a process. Retain a normal failed attempt, not a cleanup quarantine.
            try {
                provenance["worker_outcome"] = "not_started";
                provenance["cleanup_confirmed"] = true;
                save(writer.raw_directory() / "report.json", provenance);
                artifacts->update_provenance(writer, manifest_provenance(provenance));
                artifacts->publish_failure(
                    writer, "Capture did not start: cancelled, expired, rejected, or shut down while queued");
            } catch(...) {
                // Storage failure leaves the writer's conservative recovery path.
            }
        }
    }
};

JobCompletion execute_capture(const CaptureServiceOptions& options, ArtifactStore& artifacts,
                              const std::shared_ptr<Attempt>& attempt, const CaptureRequest& request,
                              const JobContext& context) {
    attempt->started = true;
    JobCompletion completion;
    completion.identity = context.identity;
    completion.cleanup_confirmed = true;
    completion.artifact_ids = {attempt->writer.id()};
    auto& report = attempt->provenance;
    report["job"] = context.identity;
    const auto root = attempt->writer.directory();
    bool readable_capture = false;
    bool inspected_application = false;
    try {
        const auto environment = isolated_environment(options, root);
        report["environment"] = environment;
        report["application"]["sha256_before_launch"] =
            sha256_regular_file(request.executable, context.deadline, context.stop);
        report["application"]["identity_scope"] = "path contents before and after launch; running image unverified";
        inspected_application = true;
        fs::create_directories(root / "raw/logs/discovery");
        fs::create_directories(root / "raw/exports");
        save(root / "raw/report.json", report);
        NsightInspectionOptions inspection;
        inspection.installation_root = options.nsight_root;
        inspection.context = {root / "raw/logs/discovery", environment, remaining(context)};
        completion.cleanup_confirmed = false;
        const auto installation = inspect_nsight(inspection, context.stop);
        completion.cleanup_confirmed = installation.cleanup_confirmed;
        report["nsight"] = {{"interface_ready", installation.interface_ready},
                            {"problem", installation.problem},
                            {"cli", observation_json(installation.cli, root)},
                            {"capture", observation_json(installation.capture, root)},
                            {"replay", observation_json(installation.replay, root)}};
        if(!completion.cleanup_confirmed) {
            throw std::runtime_error("Nsight discovery left owned-process cleanup unconfirmed");
        }
        if(!installation.interface_ready) {
            throw std::runtime_error("Nsight interface unavailable: " + installation.problem);
        }
        if(stopped(context)) {
            throw std::runtime_error("Capture stopped before application launch");
        }
        NsightCaptureOptions capture;
        fs::create_directories(root / "raw/logs/capture");
        capture.context = {root / "raw/logs/capture", environment, remaining(context)};
        capture.executable = request.executable;
        capture.arguments = request.arguments;
        capture.working_directory = request.working_directory;
        capture.capture_file = root / "raw/capture.ngfx-capture";
        capture.capture_frame = request.capture_frame;
        completion.cleanup_confirmed = false;
        const auto captured = run_nsight_capture(installation, capture, context.stop);
        completion.cleanup_confirmed = !captured.launched || captured.process.cleanup_confirmed;
        report["capture"] = operation_json(captured, root);
        if(captured.outcome != NsightOutcome::Success) {
            throw std::runtime_error("Capture failed: " + captured.message);
        }
        report["exports"] = Json::array();
        for(const auto kind : {NsightExportKind::Metadata, NsightExportKind::Functions, NsightExportKind::Objects,
                               NsightExportKind::Logs, NsightExportKind::Screenshot}) {
            if(stopped(context)) {
                throw std::runtime_error("Capture stopped before exports completed");
            }
            const std::string name(nsight_export_name(kind));
            fs::create_directories(root / "raw/logs" / name);
            NsightExportOptions export_options;
            export_options.context = {root / "raw/logs" / name, environment, remaining(context)};
            export_options.capture_file = capture.capture_file;
            export_options.capture_tool_version = installation.capture.version;
            export_options.capture_tool_build = installation.capture.build;
            export_options.kind = kind;
            export_options.output_file =
                root / "raw/exports" / (name + (kind == NsightExportKind::Screenshot ? ".png" : ".raw"));
            completion.cleanup_confirmed = false;
            const auto exported = export_nsight_capture(installation, export_options, context.stop);
            completion.cleanup_confirmed = !exported.launched || exported.process.cleanup_confirmed;
            auto evidence = operation_json(exported, root);
            evidence["kind"] = name;
            evidence["evidence_origin"] = "nsight_export";
            report["exports"].push_back(std::move(evidence));
            if(!completion.cleanup_confirmed) {
                throw std::runtime_error("Export left owned-process cleanup unconfirmed");
            }
            if(kind == NsightExportKind::Metadata) {
                if(exported.outcome != NsightOutcome::Success) {
                    throw std::runtime_error("Saved capture could not be read by matching replay tools: " +
                                             exported.message);
                }
                readable_capture = true;
            }
            // Optional exports retain their individual outcomes. A readable
            // capture does not assert that every inspection category exists.
        }
        report["application"]["sha256_after_launch"] =
            sha256_regular_file(request.executable, context.deadline, context.stop);
        if(report["application"]["sha256_before_launch"] != report["application"]["sha256_after_launch"]) {
            throw std::runtime_error("Application executable changed during capture; rebuild and submit a fresh job");
        }
        completion.outcome = JobOutcome::succeeded;
    } catch(const std::exception& error) {
        completion.error = error.what();
        completion.outcome = JobOutcome::failed;
    }
    if(stopped(context)) {
        completion.outcome = JobClock::now() >= context.deadline ? JobOutcome::timed_out : JobOutcome::cancelled;
        if(completion.error.empty()) {
            completion.error = "Capture stopped during its deadline/cancellation boundary";
        }
    }
    report["readable_capture"] = readable_capture;
    report["application_identity_observed"] = inspected_application;
    report["cleanup_confirmed"] = completion.cleanup_confirmed;
    report["worker_outcome"] = completion.outcome == JobOutcome::succeeded ? "succeeded" : "failed";
    report["error"] = completion.error;
    // These are worker observations. The coordinator alone decides the final
    // job outcome when cancellation/deadline and completion race.
    try {
        save(root / "raw/report.json", report);
        artifacts.update_provenance(attempt->writer, manifest_provenance(report));
        if(!completion.cleanup_confirmed) {
            artifacts.quarantine(attempt->writer, failure_reason(completion.error));
        } else if(completion.outcome == JobOutcome::succeeded) {
            artifacts.publish_success(attempt->writer);
        } else {
            artifacts.publish_failure(attempt->writer, failure_reason(completion.error));
        }
    } catch(const std::exception& error) {
        completion.outcome = JobOutcome::failed;
        completion.error += std::string("; evidence publication: ") + error.what();
        // A quota failure before publication owns no live process and must not
        // become a permanent cleanup quarantine. A post-rename durability error
        // may already have published the bundle: preserve its immutable files.
        if(completion.cleanup_confirmed) {
            try {
                const auto current = artifacts.inspect(attempt->writer.id()).summary;
                if(current.status == "staging" || current.quarantined) {
                    report["worker_outcome"] = "failed";
                    report["error"] = completion.error;
                    try {
                        save(attempt->writer.raw_directory() / "report.json", report);
                    } catch(const std::exception& saving) {
                        // A long/failed output path may prevent saving the raw
                        // report while descriptor-relative store metadata still
                        // works. Retain any available files as a normal failure.
                        completion.error += std::string("; saving failure report: ") + saving.what();
                    }
                    artifacts.update_provenance(attempt->writer, manifest_provenance(report));
                    artifacts.publish_failure(attempt->writer, failure_reason(completion.error));
                }
            } catch(const std::exception& failure) {
                completion.error += std::string("; retaining failed evidence: ") + failure.what();
            }
        }
    }
    return completion;
}
} // namespace

CaptureService::CaptureService(CaptureServiceOptions options) :
    options_(std::move(options)), artifacts_(options_.artifacts) {}

CaptureService::~CaptureService() {
    jobs_.shutdown(std::chrono::seconds(5));
}

CaptureSubmission CaptureService::capture(CaptureRequest request) {
    if(request.timeout.count() < 1 || request.timeout > std::chrono::minutes(10) || request.capture_frame < 2 ||
       request.arguments.size() > 256 || !request.application_provenance.is_object() ||
       request.application_provenance.dump().size() > 65536) {
        throw std::invalid_argument(
            "Capture requires timeout 1..600000 ms, frame >= 2, at most 256 arguments, and bounded provenance");
    }
    if(!request.executable.is_absolute() || !request.working_directory.is_absolute()) {
        throw std::invalid_argument("Application executable and working_directory must be absolute paths");
    }
    if(request.executable.string().find('\0') != std::string::npos ||
       request.working_directory.string().find('\0') != std::string::npos ||
       request.application_output_option.size() > 64 ||
       request.application_output_option.find('\0') != std::string::npos) {
        throw std::invalid_argument(
            "Capture paths/options must not contain NUL bytes; output option is limited to 64 bytes");
    }
    for(const auto& argument : request.arguments) {
        if(argument.size() > 4096 || argument.find('\0') != std::string::npos) {
            throw std::invalid_argument("Each application argument must be at most 4096 bytes without NUL bytes");
        }
    }
    request.executable = fs::canonical(request.executable);
    request.working_directory = fs::canonical(request.working_directory);
    if(!fs::is_regular_file(request.executable) || access(request.executable.c_str(), X_OK) != 0 ||
       !fs::is_directory(request.working_directory)) {
        throw std::invalid_argument("Application requires a regular executable file and existing working directory");
    }
    artifacts_.prune();
    Json provenance{{"schema_version", 1},
                    {"project_version", project_version()},
                    {"evidence_origin", "nsight_capture"},
                    {"backend", "documented_nsight_cli"},
                    {"application",
                     {{"executable", request.executable.string()},
                      {"arguments", request.arguments},
                      {"working_directory", request.working_directory.string()}}},
                    {"caller_provided_application_provenance", request.application_provenance},
                    {"capture_settings",
                     {{"capture_frame", request.capture_frame},
                      {"frame_count", 1},
                      {"delimiter", "present"},
                      {"timeout_ms", request.timeout.count()}}},
                    {"sdk", {{"status", "not_used"}}}};
    auto attempt = std::make_shared<Attempt>();
    attempt->artifacts = &artifacts_;
    attempt->provenance = provenance;
    attempt->writer =
        artifacts_.begin(manifest_provenance(provenance),
                         {"raw/capture.ngfx-capture", "raw/exports/metadata.raw", "raw/report.json"}, request.pin);
    const auto artifact_id = attempt->writer.id();
    if(!request.application_output_option.empty()) {
        request.arguments.push_back(request.application_output_option);
        request.arguments.push_back((attempt->writer.raw_directory() / "application").string());
        attempt->provenance["application"]["arguments"] = request.arguments;
        attempt->provenance["application"]["output_option"] = request.application_output_option;
        attempt->provenance["application"]["output_path"] = "raw/application";
        attempt->provenance["application"]["output_evidence_origin"] = "application_provided";
    }
    JobRequest job;
    job.capture_id = artifact_id;
    job.deadline = JobClock::now() + request.timeout;
    job.worker = [this, attempt, request = std::move(request)](const JobContext& context) {
        return execute_capture(options_, artifacts_, attempt, request, context);
    };
    return {jobs_.submit(std::move(job)), artifact_id};
}

std::optional<JobSnapshot> CaptureService::status(const std::string& job_id) const {
    return jobs_.snapshot(job_id);
}
std::optional<JobSnapshot> CaptureService::wait(const std::string& job_id, std::chrono::milliseconds timeout) const {
    return jobs_.wait(job_id, timeout);
}
JobCancelResult CaptureService::cancel(const std::string& job_id) {
    return jobs_.cancel(job_id);
}
JobShutdownReport CaptureService::shutdown(std::chrono::milliseconds timeout) {
    return jobs_.shutdown(timeout);
}
ArtifactStore& CaptureService::artifacts() noexcept {
    return artifacts_;
}

std::map<std::string, std::string> capture_environment() {
    std::map<std::string, std::string> result{{"PATH", "/usr/bin:/bin"}, {"LANG", "C"}, {"LC_ALL", "C"}};
    for(const auto* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE",
                            "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS", "XDG_DATA_DIRS"}) {
        if(const auto* value = std::getenv(name); value && *value) {
            result[name] = value;
        }
    }
    return result;
}

void to_json(Json& json, const JobIdentity& value) {
    json = {{"job_id", value.job_id}, {"capture_id", value.capture_id}, {"attempt", value.attempt}};
}
void to_json(Json& json, const JobSnapshot& value) {
    json = {{"identity", value.identity},
            {"gpu_key", value.gpu_key},
            {"state", job_state_name(value.state)},
            {"stop_reason", value.stop_reason ? Json(job_stop_reason_name(*value.stop_reason)) : Json(nullptr)},
            {"worker_running", value.worker_running},
            {"finalization_pending", value.finalization_pending},
            {"cleanup_confirmed", value.cleanup_confirmed},
            {"gpu_reserved", value.gpu_reserved},
            {"error", value.error},
            {"artifact_ids", value.artifact_ids}};
    const auto endpoint = value.finished_at.value_or(JobClock::now());
    json["elapsed_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(endpoint - value.submitted_at).count();
}
void to_json(Json& json, const CaptureSubmission& value) {
    json = {{"identity", value.identity}, {"artifact_id", value.artifact_id}};
}
} // namespace ngm
