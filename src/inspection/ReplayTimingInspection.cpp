#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/ProfileSettings.hpp"
#include "ngm/ReplayTiming.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <span>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Error = InspectionErrorCode;
namespace fs = std::filesystem;
constexpr std::size_t json_bound = 2U * 1024U * 1024U;
constexpr std::size_t hash_budget = 512U * 1024U * 1024U;
constexpr const char* profile_name = "nsight-2026.3.1-build-38722833-replay-marker-call-v1";
[[noreturn]] void fail(Error code, const std::string& message) {
    throw InspectionError(code, message);
}
void require(bool value, const std::string& message) {
    if(!value)
        fail(Error::InvalidReport, message + "; inspect this analysis's raw/report.json and child indexes");
}
std::string text(const Json& object, const char* key, std::size_t maximum = 4096) {
    const auto& value = object.at(key);
    require(value.is_string(), std::string("Expected text: ") + key);
    const auto result = value.get<std::string>();
    require(!result.empty() && result.size() <= maximum && result.find('\0') == std::string::npos,
            std::string("Text exceeds bounds: ") + key);
    return result;
}
std::uint64_t number(const Json& object, const char* key) {
    const auto& value = object.at(key);
    require(value.is_number_integer() && (value.is_number_unsigned() || value.get<std::int64_t>() >= 0),
            std::string("Expected unsigned counter: ") + key);
    return value.get<std::uint64_t>();
}
std::string hash(std::string_view bytes) {
    return sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
}
std::string digest(const Json& object, const char* key) {
    const auto value = text(object, key, 64);
    require(value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos,
            "Invalid SHA-256 identity");
    return value;
}
Json parse(std::string_view bytes) {
    require(bytes.find('\0') == std::string_view::npos, "JSON contains a NUL byte");
    std::vector<std::set<std::string>> keys;
    return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        require(depth < 32, "JSON exceeds the nesting limit");
        if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start)
            keys.emplace_back();
        if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end)
            keys.pop_back();
        if(event == Json::parse_event_t::key)
            require(keys.back().insert(value.get<std::string>()).second, "Duplicate JSON key");
        return true;
    });
}
std::string relative_path(const std::string& value, const std::string& prefix) {
    const fs::path path(value);
    require(!value.empty() && value.size() <= 512 && value.starts_with(prefix) && !path.is_absolute() &&
                path.lexically_normal().generic_string() == value,
            "Invalid same-bundle path");
    for(const auto& part : path)
        require(part != ".." && part != ".", "Invalid evidence path component");
    return value;
}
void successful(const Json& operation) {
    const auto& p = operation.at("process");
    require(operation.at("outcome") == "success" && operation.at("launched") == true && p.at("exit_code") == 0 &&
                p.at("signal").is_null() && p.at("timed_out") == false && p.at("cancelled") == false &&
                p.at("cleanup_confirmed") == true && p.at("error") == "",
            "Operation lacks successful owned cleanup");
}
Json metadata_identity(const NsightMetadata& m) {
    require(m.nsight_version == "2026.3.1" && m.nsight_version_build_id == "38722833" && m.primary_api == "Vulkan" &&
                m.has_unsupported_operation != true,
            "Input metadata is outside the qualified Vulkan producer");
    Json result{{"metadata_version", m.metadata_version}};
    const auto add = [&](const char* key, const auto& v) { result[key] = v ? Json(*v) : Json(nullptr); };
    add("uuid", m.uuid);
    add("nsight_version", m.nsight_version);
    add("build", m.nsight_version_build_id);
    add("api", m.primary_api);
    add("gpu", m.primary_gpu);
    add("driver", m.driver_version);
    add("process", m.process_name);
    add("request_time", m.request_time);
    add("captured_frame", m.captured_frame);
    add("captured_frames", m.captured_frames);
    add("resolution", m.resolution);
    return result;
}
struct Budget {
    std::size_t bytes = 0;
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    void take(std::uint64_t n) {
        if(n > hash_budget - bytes)
            fail(Error::LimitExceeded, "Timing group exceeds the 512 MiB read/fingerprint budget");
        bytes += static_cast<std::size_t>(n);
        if(std::chrono::steady_clock::now() >= deadline)
            fail(Error::LimitExceeded, "Timing group exceeds its ten-second inspection deadline");
    }
};
struct Analysis {
    ArtifactLease lease;
    ArtifactInfo info;
    std::map<std::string, std::uint64_t> files;
    Json report, index, cpp, profile, public_info, parsed;
    std::uint64_t interval = 0, count = 0;
};
std::uint64_t size(const Analysis& a, const std::string& path, std::size_t bound) {
    const auto found = a.files.find(path);
    if(found == a.files.end())
        fail(Error::ExportUnavailable, "Timing evidence is absent from the published inventory: " + path);
    if(found->second > bound)
        fail(Error::LimitExceeded, "Timing evidence exceeds its byte bound: " + path);
    return found->second;
}
std::string read(ArtifactStore& store, const Analysis& a, const std::string& path, std::size_t bound, Budget& budget) {
    budget.take(size(a, path, bound));
    return store.read(a.info.summary.id, path, bound);
}
void fingerprint(ArtifactStore& store, const Analysis& a, const std::string& path, const std::string& expected,
                 std::size_t bound, Budget& budget) {
    budget.take(size(a, path, bound));
    require(store.fingerprint(a.info.summary.id, path, bound, budget.deadline) == expected,
            "Retained input changed: " + path);
}
std::string reference(ArtifactStore& store, const Analysis& a, const Json& ref, const std::string& prefix,
                      Budget& budget) {
    require(ref.is_object() && ref.size() == 2, "Invalid hashed reference shape");
    const auto path = relative_path(text(ref, "path", 512), prefix);
    const auto bytes = read(store, a, path, ProfileEvidenceLimits::bytes, budget);
    require(hash(bytes) == digest(ref, "sha256"), "Hashed evidence differs from its index: " + path);
    return bytes;
}
Analysis open(ArtifactStore& store, const std::string& id, Budget& budget) {
    Analysis a;
    a.lease = store.lease(id);
    a.info = store.inspect(id);
    const auto& manifest = a.info.provenance;
    if(manifest.value("evidence_origin", Json()) != "nsight_replay_timing")
        fail(Error::NotCapture, "Timing queries require native combined analysis bundles; imports/captures/live "
                                "profiles are separate evidence");
    if(a.info.summary.status != "complete" || a.info.summary.quarantined)
        fail(Error::IncompleteCapture,
             "Timing analysis is not complete with confirmed cleanup; inspect job_status/artifact_info");
    for(std::size_t offset = 0;;) {
        const auto page = store.files(id, offset, 100);
        for(const auto& f : page.files)
            require(a.files.emplace(f.path, f.bytes).second, "Duplicate inventory path");
        require(a.files.size() <= 4096, "Analysis inventory exceeds bounds");
        if(!page.next_offset)
            break;
        offset = *page.next_offset;
    }
    require(a.files.size() == a.info.file_count, "Analysis file count differs from its inventory");
    const auto report_bytes = read(store, a, "raw/report.json", json_bound, budget);
    const auto master_bytes = read(store, a, "derived/timing-analysis.json", 262144, budget);
    a.report = parse(report_bytes);
    a.index = parse(master_bytes);
    const auto& report = a.report;
    const auto& index = a.index;
    require(report.at("schema_version") == 1 && index.at("schema_version") == 1 && manifest.at("schema_version") == 1 &&
                manifest.at("report_path") == "raw/report.json" && report.at("backend") == "documented_nsight_cli" &&
                manifest.at("backend") == "documented_nsight_cli" &&
                report.at("evidence_origin") == "nsight_replay_timing" &&
                index.at("evidence_origin") == "nsight_replay_timing",
            "Invalid timing origin/schema/backend");
    require(report.at("job") == index.at("job") && report.at("job") == manifest.at("job") &&
                report.at("job").is_object() && report.at("job").size() == 3 &&
                text(report.at("job"), "capture_id", 39) == id && number(report.at("job"), "attempt") > 0,
            "Timing job/index/manifest identity mismatch");
    (void)text(report.at("job"), "job_id", 256);
    require(text(report, "project_version", 64) == text(manifest, "project_version", 64),
            "Product provenance mismatch");
    for(const auto* key : {"timing_collected", "generated_cpp_project", "profiled", "cleanup_confirmed"})
        require(report.at(key) == true && manifest.at(key) == true,
                "Timing worker has incomplete collection or cleanup");
    require(report.at("worker_outcome") == "succeeded" && manifest.at("worker_outcome") == "succeeded" &&
                report.at("replayed") == false && manifest.at("replayed") == false && report.at("error") == "" &&
                report.at("analysis_index") == "derived/timing-analysis.json" &&
                manifest.at("analysis_index") == "derived/timing-analysis.json",
            "Timing completion contract differs");
    for(const auto* path : {"raw/input/capture.ngfx-capture", "raw/input/metadata.raw", "raw/input/report.json",
                            "raw/exports/metadata.raw", "raw/report.json", "derived/timing-analysis.json",
                            "derived/cpp-project.json", "derived/replay-profile.json"})
        require(std::find(a.info.required_outputs.begin(), a.info.required_outputs.end(), path) !=
                    a.info.required_outputs.end(),
                "Timing manifest omits a required output");
    for(const auto* key : {"source", "requested", "replay_target"}) {
        const auto* report_key = std::string_view(key) == "requested" ? "timing_settings" : key;
        require(index.at(key) == report.at(report_key), "Timing master/report provenance mismatch");
        if(std::string_view(key) != "replay_target")
            require(report.at(report_key) == manifest.at(report_key), "Timing manifest/report provenance mismatch");
    }
    const auto& requested = index.at("requested");
    a.count = number(requested, "source_interval_count");
    a.interval = number(requested, "interval_index");
    require(requested.size() == 13 && (a.count == 1 || a.count == 3) && a.interval < a.count &&
                number(requested, "cpp_loop_count") == 20 && number(requested, "profile_loop_count") == 60 &&
                number(requested, "cpp_wait_frames") == std::max(std::uint64_t{2}, a.count) + a.interval &&
                requested.at("time_every_action") == true && requested.at("hes_enabled") == false &&
                requested.at("gpu_clocks") == "unaltered" && requested.at("metric_set") == "Throughput Metrics" &&
                requested.at("trigger") == Json(a.interval == 0 ? "replay_pass_start" : "replay_frame_ordinal") &&
                number(requested, "timeout_ms") >= 1 && number(requested, "timeout_ms") <= 600000,
            "Timing settings are outside the qualified 20/60-loop, one/three-interval contract");
    ProfileSettings settings;
    settings.architecture = text(requested, "architecture", 64);
    settings.duration_ms = number(requested, "duration_ms");
    settings.start_after = a.interval;
    settings.limit = 1;
    settings.time_every_action = true;
    settings.hes_enabled = false;
    try {
        validate_profile_settings(settings);
    } catch(const std::invalid_argument&) {
        fail(Error::InvalidReport, "Retained timing settings are invalid");
    }
    const auto& nsight = report.at("nsight");
    require(nsight.at("interface_ready") == true, "Timing tools were not qualified");
    for(const auto* name : {"cli", "capture", "replay"}) {
        const auto& tool = nsight.at(name);
        require(tool.at("version_valid") == true && tool.at("help_valid") == true &&
                    tool.at("version") == "2026.3.1.0" && tool.at("build") == "38722833",
                "Timing producer is not the qualified matching release");
        for(const auto* key : {"path", "version", "build"})
            require(tool.at(key) == manifest.at("nsight").at(name).at(key), "Retained tool observations disagree");
    }
    const auto& source = index.at("source");
    const auto capture_hash = digest(source, "original_capture_sha256");
    require(digest(source, "observed_capture_sha256") == capture_hash, "Timing input did not match capture-time bytes");
    fingerprint(store, a, "raw/input/capture.ngfx-capture", capture_hash, ArtifactStore::maximum_fingerprint_bytes,
                budget);
    const auto source_report_bytes = read(store, a, "raw/input/report.json", json_bound, budget);
    const auto original_metadata =
        read(store, a, "raw/input/metadata.raw", NsightEvidenceLimits::maximum_bytes, budget);
    require(hash(source_report_bytes) == digest(source, "report_sha256") &&
                hash(original_metadata) == digest(source, "metadata_sha256"),
            "Timing source provenance copy changed");
    const auto original = parse(source_report_bytes);
    const auto m = parse_nsight_metadata(original_metadata);
    require(original.at("evidence_origin") == "nsight_capture" && original.at("backend") == "documented_nsight_cli" &&
                original.at("job") == source.at("job") &&
                original.at("job").at("capture_id") == source.at("capture_id") &&
                original.at("readable_capture") == true && original.at("cleanup_confirmed") == true &&
                original.at("worker_outcome") == "succeeded" && original.at("capture").at("sha256") == capture_hash,
            "Source is not a capture-time-hashed successful native Graphics Capture");
    const auto& capture_settings = original.at("capture_settings");
    const auto first = number(capture_settings, "capture_frame");
    require(capture_settings.at("delimiter") == "present" && number(capture_settings, "frame_count") == a.count &&
                first >= 2 && first <= UINT64_MAX - (a.count - 1),
            "Source interval settings disagree");
    successful(original.at("capture"));
    require(original.at("capture").at("executable") == source.at("capture_tool").at("path") &&
                original.at("capture").at("output") == "raw/capture.ngfx-capture" &&
                number(original.at("capture"), "output_bytes") ==
                    size(a, "raw/input/capture.ngfx-capture", ArtifactStore::maximum_fingerprint_bytes),
            "Source capture operation differs from its input snapshot");
    require((a.count == 1 && m.captured_frame == std::to_string(first) && !m.captured_frames) ||
                (a.count == 3 && !m.captured_frame &&
                 m.captured_frames == std::to_string(first) + " - " + std::to_string(first + 2)),
            "Source metadata has an unqualified interval shape");
    const auto fresh =
        parse_nsight_metadata(read(store, a, "raw/exports/metadata.raw", NsightEvidenceLimits::maximum_bytes, budget));
    require(metadata_identity(m) == metadata_identity(fresh), "Fresh input header differs from its retained source");
    for(const auto* key : {"path", "version", "build"})
        require(source.at("capture_tool").at(key) == original.at("nsight").at("capture").at(key),
                "Capture tool provenance differs");
    require(source.at("capture_tool").at("version") == "2026.3.1.0" &&
                source.at("capture_tool").at("build") == "38722833",
            "Source producer is unsupported");
    successful(report.at("metadata_validation"));
    require(report.at("metadata_validation").at("executable") == nsight.at("replay").at("path") &&
                report.at("metadata_validation").at("output") == "raw/exports/metadata.raw" &&
                number(report.at("metadata_validation"), "output_bytes") ==
                    size(a, "raw/exports/metadata.raw", NsightEvidenceLimits::maximum_bytes),
            "Fresh header operation differs");
    const auto target = text(index.at("replay_target"), "path");
    const auto target_hash = digest(index.at("replay_target"), "sha256_before");
    require(target_hash == digest(index.at("replay_target"), "sha256_after") &&
                target == (fs::path(text(nsight.at("replay"), "path")).parent_path() / "ngfx-replay.bin").string(),
            "Replay target identities disagree");
    require(index.at("cpp_index").at("path") == "derived/cpp-project.json" &&
                index.at("profile_index").at("path") == "derived/replay-profile.json",
            "Timing child index paths differ");
    a.cpp = parse(reference(store, a, index.at("cpp_index"), "derived/", budget));
    a.profile = parse(reference(store, a, index.at("profile_index"), "derived/", budget));
    const auto& cpp = a.cpp;
    const auto& profile = a.profile;
    require(cpp.at("schema_version") == 1 && profile.at("schema_version") == 1 && cpp.at("job") == index.at("job") &&
                profile.at("job") == index.at("job") && cpp.at("evidence_origin") == "nsight_generated_cpp" &&
                profile.at("evidence_origin") == "nsight_replay_gpu_trace" && profile.at("requested") == requested,
            "Timing child index identity/origin mismatch");
    const auto project = relative_path(text(cpp, "project_directory", 512), "raw/cpp/");
    const auto cpp_metadata_path = project + "/metadata.json";
    require(cpp.at("metadata_path") == cpp_metadata_path && cpp.at("database_path") == project + "/data.bin",
            "C++ project references differ");
    const auto& files = cpp.at("recorded_state_inputs").at("files");
    require(cpp.at("recorded_state_inputs").at("schema_version") == 1 && files.is_array() && files.size() <= 260 &&
                cpp.at("source_files").is_array() && cpp.at("source_files").size() <= 256,
            "C++ fingerprints exceed bounds");
    std::set<std::string> expected;
    std::map<std::string, Json> identities;
    for(const auto& path : cpp.at("source_files"))
        require(expected.insert(relative_path(path.get<std::string>(), project + '/')).second, "Duplicate C++ source");
    for(const auto* name : {"metadata.json", "Resources.h", "data.bin", "data.bin.rec"})
        require(expected.insert(project + '/' + name).second, "Duplicate C++ input");
    for(const auto& identity : files) {
        const auto path = relative_path(text(identity, "path", 512), project + '/');
        require(identity.size() == 3 && expected.erase(path) == 1 &&
                    number(identity, "bytes") == size(a, path, ArtifactStore::maximum_fingerprint_bytes),
                "C++ input inventory differs");
        fingerprint(store, a, path, digest(identity, "sha256"), ArtifactStore::maximum_fingerprint_bytes, budget);
        identities.emplace(path, identity);
    }
    require(expected.empty(), "C++ fingerprint set omits required inputs");
    const auto cm =
        parse_nsight_cpp_metadata(read(store, a, cpp_metadata_path, NsightEvidenceLimits::maximum_bytes, budget));
    require(cm.nsight_version == "2026.3.1" && cm.build_id == 38722833 && cm.primary_api == "vulkan" &&
                !cm.has_unsupported_operation && cpp.at("nsight_version") == cm.nsight_version &&
                cpp.at("nsight_version_build_id") == cm.build_id && cpp.at("primary_api") == cm.primary_api &&
                cpp.at("primary_gpu") == cm.primary_gpu && cpp.at("has_unsupported_operation") == false,
            "C++ metadata/index producer differs or reports unsupported operations");
    const auto repro_bytes = reference(store, a, profile.at("reproduction"), "raw/profile/", budget);
    const auto repro = parse_profile_reproduction(repro_bytes);
    require(repro.product_version == "2026.3.1.0 (build 38722833) (public-release)", "Unqualified trace producer");
    require(profile.at("producer") == repro.product_version && profile.at("gpu") == repro.gpu &&
                profile.at("driver") == repro.driver && profile.at("settings") == Json(repro.settings),
            "Replay index differs from its raw reproduction export");
    require(repro.gpu == cm.primary_gpu, "C++ and trace GPU identities differ");
    require(repro.process_executable == target && profile.at("process_executable") == target,
            "Replay reproduction target differs");
    require(repro.replay_capture && profile.at("replay_capture") == *repro.replay_capture,
            "Replay reproduction input differs");
    const fs::path input(*repro.replay_capture);
    const auto directory = input.parent_path();
    const auto staging = directory.parent_path().parent_path();
    require(input.is_absolute() && input.lexically_normal() == input && input.filename() == "capture.ngfx-capture" &&
                directory.filename() == "input" && directory.parent_path().filename() == "raw" &&
                staging.filename() == id && staging.parent_path().filename() == "staging",
            "Replay reproduction input is not this job's snapshot");
    const auto& exported = repro.settings;
    require(exported.at("API") == "Vulkan" &&
                exported.at("Start After") ==
                    (a.interval == 0 ? "Replay Pass Start" : std::to_string(a.interval) + " Frames") &&
                exported.at("Limited To") == "1 Frames" &&
                exported.at("Max Duration ") == std::to_string(settings.duration_ms) + " ms" &&
                exported.at("GPU Clocks") == "Unaltered" && exported.at("Metric Set") == "Throughput Metrics" &&
                exported.at("Multi-Pass Metrics") == "Disabled" && exported.at("Time Every Action") == "Yes",
            "Exported replay collection settings differ");
    const auto cli = text(nsight.at("cli"), "path");
    successful(report.at("cpp_capture"));
    successful(report.at("profile"));
    const std::vector<std::string> cpp_args{"--activity=Generate C++ Capture",
                                            "--platform=Linux (x86_64)",
                                            "--no-timeout",
                                            "--env=NSIGHT_SUGGEST_GRAPHICS_CAPTURE=0",
                                            "--exe=" + target,
                                            "--dir=" + directory.string(),
                                            "--output-dir=" + (staging / "raw/cpp").string(),
                                            "--wait-frames=" +
                                                std::to_string(std::max(std::uint64_t{2}, a.count) + a.interval),
                                            "--args=--loop-count=20 --present-app --verbose " + input.string()};
    const std::vector<std::string> profile_args{"--activity=GPU Trace Profiler",
                                                "--platform=Linux (x86_64)",
                                                "--exe=" + target,
                                                "--dir=" + directory.string(),
                                                "--output-dir=" + (staging / "raw/profile").string(),
                                                a.interval == 0 ? "--start-with-ngfx-sdk"
                                                                : "--start-after-frames=" + std::to_string(a.interval),
                                                "--limit-to-frames=1",
                                                "--max-duration-ms=" + std::to_string(settings.duration_ms),
                                                "--architecture=" + settings.architecture,
                                                "--metric-set-name=Throughput Metrics",
                                                "--set-gpu-clocks=unaltered",
                                                "--collect-screenshot=0",
                                                "--auto-export",
                                                "--time-every-action",
                                                "--hes-enabled=0",
                                                "--args=--loop-count=60 --present-app --verbose " + input.string()};
    require(report.at("cpp_capture").at("arguments") == cpp_args &&
                report.at("profile").at("arguments") == profile_args &&
                report.at("cpp_capture").at("executable") == cli && report.at("profile").at("executable") == cli &&
                report.at("cpp_capture").at("output") == cpp_metadata_path &&
                report.at("profile").at("output") == "raw/profile" &&
                number(report.at("cpp_capture"), "output_bytes") ==
                    size(a, cpp_metadata_path, NsightEvidenceLimits::maximum_bytes),
            "Analysis invocations differ from retained settings");
    require(profile.at("tables").is_object() && profile.at("tables").size() == 4,
            "Profile must retain its four observed exports");
    const auto table_parent = fs::path(text(profile.at("reproduction"), "path", 512)).parent_path();
    require(table_parent.filename() == "BASE_UNLOCKED" && table_parent.parent_path() == "raw/profile",
            "Unqualified profile export layout");
    std::map<std::string, std::string> tables;
    for(const auto& [key, name] : std::map<std::string, std::string>{{"frame_duration", "FRAME.xls"},
                                                                     {"frame_metrics", "GPUTRACE_FRAME.xls"},
                                                                     {"event_durations", "D3DPERF_EVENTS.xls"},
                                                                     {"regime_metrics", "GPUTRACE_REGIMES.xls"}}) {
        require(profile.at("tables").at(key).at("path") == (table_parent / name).generic_string(),
                "Profile table layout differs");
        tables[key] = reference(store, a, profile.at("tables").at(key), "raw/profile/", budget);
    }
    const auto trace_path = relative_path(text(profile.at("trace"), "path", 512), "raw/profile/");
    fingerprint(store, a, trace_path, digest(profile.at("trace"), "sha256"), 128U * 1024U * 1024U, budget);
    std::vector<CppSource> sources;
    Json source_files = Json::array();
    std::size_t source_bytes = 0;
    for(const auto& path_value : cpp.at("source_files")) {
        const auto path = path_value.get<std::string>(), name = fs::path(path).filename().string();
        if(!name.starts_with("CommandList") && !name.starts_with("Resources") && !name.starts_with("Frame"))
            continue;
        source_bytes += size(a, path, 4U * 1024U * 1024U);
        if(sources.size() >= 64 || source_bytes > 16U * 1024U * 1024U)
            fail(Error::LimitExceeded, "Timing syntax exceeds its 64-file/16 MiB budget");
        auto bytes = read(store, a, path, 4U * 1024U * 1024U, budget);
        require(hash(bytes) == digest(identities.at(path), "sha256"), "C++ source changed while inspecting");
        source_files.push_back(identities.at(path));
        sources.push_back({path, std::move(bytes)});
    }
    a.parsed = correlate_replay_draws(
        sources, parse_profile_table(tables.at("event_durations"), ProfileTableKind::EventDurations),
        parse_profile_table(tables.at("regime_metrics"), ProfileTableKind::RegimeMetrics,
                            ProfileTablePolicy::Nsight2026_3RegimeNan));
    budget.take(0);
    a.public_info = {{"analysis_id", id},
                     {"interval_index", a.interval},
                     {"collection_version", report.at("project_version")},
                     {"producer", repro.product_version},
                     {"gpu", repro.gpu},
                     {"driver", repro.driver},
                     {"replay_target_sha256", target_hash},
                     {"report", {{"path", "raw/report.json"}, {"sha256", hash(report_bytes)}}},
                     {"master", {{"path", "derived/timing-analysis.json"}, {"sha256", hash(master_bytes)}}},
                     {"cpp_index", index.at("cpp_index")},
                     {"profile_index", index.at("profile_index")},
                     {"source_inputs_sha256", hash(cpp.at("recorded_state_inputs").dump())},
                     {"source_files", std::move(source_files)},
                     {"tables",
                      {{"event_durations", profile.at("tables").at("event_durations")},
                       {"regime_metrics", profile.at("tables").at("regime_metrics")}}},
                     {"setup_sources", a.parsed.at("setup_sources")},
                     {"profile_hierarchy", a.parsed.at("profile_hierarchy")},
                     {"unsupported", a.parsed.at("unsupported")}};
    return a;
}
} // namespace
Json ReplayTimingInspection::draws(const std::vector<std::string>& ids, std::size_t offset, std::size_t limit) const {
    if((ids.size() != 1 && ids.size() != 3) || std::set<std::string>(ids.begin(), ids.end()).size() != ids.size() ||
       limit < 1 || limit > 100 || offset > 30000)
        throw std::invalid_argument("Provide 1 or 3 distinct analysis_ids, offset up to 30000 and limit 1..100");
    try {
        Budget budget;
        std::vector<Analysis> analyses;
        for(const auto& id : ids)
            analyses.push_back(open(artifacts_, id, budget));
        std::sort(analyses.begin(), analyses.end(),
                  [](const auto& a, const auto& b) { return a.interval < b.interval; });
        const auto& first = analyses.front();
        require(ids.size() == first.count,
                "Incomplete source interval group: provide every retained interval analysis");
        auto common = first.index.at("requested");
        for(const auto* key : {"interval_index", "cpp_wait_frames", "trigger", "timeout_ms"})
            common.erase(key);
        Json group = Json::array();
        std::map<std::string, std::size_t> source_paths, profile_paths;
        bool complete_interpretation = true;
        for(std::size_t i = 0; i < analyses.size(); ++i) {
            const auto& a = analyses[i];
            auto settings = a.index.at("requested");
            for(const auto* key : {"interval_index", "cpp_wait_frames", "trigger", "timeout_ms"})
                settings.erase(key);
            require(a.count == first.count && a.interval == i && a.index.at("source") == first.index.at("source") &&
                        settings == common && a.index.at("replay_target") == first.index.at("replay_target") &&
                        a.profile.at("gpu") == first.profile.at("gpu") &&
                        a.profile.at("driver") == first.profile.at("driver") &&
                        a.profile.at("producer") == first.profile.at("producer"),
                    "Mixed, duplicate or incomplete source/producer/target/GPU/settings group");
            group.push_back(
                {{"analysis_id", a.info.summary.id}, {"master_sha256", a.public_info.at("master").at("sha256")}});
            for(const auto& [path, count] : a.parsed.at("source_paths").items())
                source_paths[path] += count.get<std::size_t>();
            for(const auto& [path, count] : a.parsed.at("profile_paths").items())
                profile_paths[path] += count.get<std::size_t>();
            complete_interpretation = complete_interpretation && a.parsed.at("unsupported").empty() &&
                                      a.parsed.at("profile_paths_qualified") == true;
        }
        const auto group_hash = hash(group.dump());
        Json values = Json::array(), public_analyses = Json::array();
        std::size_t available = 0, unavailable = 0, unsupported = 0;
        for(auto& a : analyses) {
            public_analyses.push_back(a.public_info);
            unsupported += !a.parsed.at("unsupported").empty() || a.parsed.at("profile_paths_qualified") != true;
            for(auto value : a.parsed.at("draws")) {
                std::string path;
                for(const auto& component : value.at("marker_path")) {
                    if(!path.empty())
                        path += '/';
                    path += component.get<std::string>();
                }
                if(value.at("status") == "available" && !complete_interpretation) {
                    value["status"] = "unavailable";
                    value["duration_ms"] = nullptr;
                    value["trace"] = nullptr;
                    value["reason"] =
                        "An interval's source or trace hierarchy is unsupported; cross-interval identity is unproved";
                }
                if(value.at("status") == "available" && (source_paths.at(path) != 1 || profile_paths.at(path) != 1)) {
                    value["status"] = "unavailable";
                    value["duration_ms"] = nullptr;
                    value["trace"] = nullptr;
                    value["reason"] = "The literal path repeats across retained source intervals; original interval "
                                      "correspondence is ambiguous";
                }
                value["analysis_id"] = a.info.summary.id;
                value["interval_index"] = a.interval;
                value["timing_ref"] = hash(Json{
                    {"profile", profile_name},
                    {"group_sha256", group_hash},
                    {"analysis_id", a.info.summary.id},
                    {"source_inputs_sha256", a.public_info.at("source_inputs_sha256")},
                    {"selection", value.at("selection")},
                    {"marker_path",
                     value.at("marker_path")}}.dump());
                value.at("status") == "available" ? ++available : ++unavailable;
                values.push_back(std::move(value));
            }
        }
        Json page = Json::array();
        std::size_t bytes = 0;
        for(auto at = offset; at < values.size() && page.size() < limit; ++at) {
            const auto n = values[at].dump().size();
            if(bytes + n > InspectionService::maximum_page_bytes) {
                if(page.empty())
                    fail(Error::LimitExceeded, "One timing record exceeds the page budget");
                break;
            }
            bytes += n;
            page.push_back(std::move(values[at]));
        }
        const auto next = offset + page.size();
        Json result{{"evidence_origin", "nsight_replay_timing_correlation"},
                    {"correlation_profile", profile_name},
                    {"source_capture_id", first.index.at("source").at("capture_id")},
                    {"source_capture_sha256", first.index.at("source").at("original_capture_sha256")},
                    {"source_interval_count", first.count},
                    {"group_sha256", group_hash},
                    {"analyses", std::move(public_analyses)},
                    {"selection_domain", "analysis_generated_cpp_submission"},
                    {"unit", "ms"},
                    {"original_capture_event_ids_available", false},
                    {"live_performance_available", false},
                    {"physical_counter_interpretation_available", false},
                    {"scope",
                     "Unique same-input literal marker ranges containing one submitted source draw. Selection IDs "
                     "belong to the generated C++ snapshot; original event IDs remain unknown. State packets, replay "
                     "and instrumentation remain included. Intervals were profiled in separate replay processes; no "
                     "original live performance or physical counter scaling is inferred."},
                    {"coverage",
                     {{"available", available}, {"unavailable", unavailable}, {"unsupported_analyses", unsupported}}},
                    {"total", values.size()},
                    {"offset", offset},
                    {"limit", limit},
                    {"next_offset", next < values.size() ? Json(next) : Json(nullptr)},
                    {"draws", std::move(page)}};
        if(result.dump().size() > InspectionService::maximum_result_bytes)
            fail(Error::LimitExceeded, "Timing response exceeds the result byte budget");
        return result;
    } catch(const Json::exception& e) {
        fail(Error::InvalidReport,
             "Malformed timing provenance: " + std::string(e.what()).substr(0, 512) + "; inspect raw/index evidence");
    } catch(const NsightEvidenceError& e) {
        fail(Error::InvalidExport, std::string("Invalid timing export: ") + e.what());
    } catch(const std::out_of_range&) {
        fail(Error::InvalidReport, "Timing evidence refers to a missing field or identity");
    }
}
} // namespace ngm
