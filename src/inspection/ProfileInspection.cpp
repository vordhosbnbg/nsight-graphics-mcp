#include "ngm/ProfileInspection.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/ProfileEvidence.hpp"
#include "ngm/ProfileSettings.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>
#include <span>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Error = InspectionErrorCode;
void require(bool value, const char* message) {
    if(!value)
        throw InspectionError(Error::InvalidReport, message);
}
Json parse(std::string_view text) {
    std::vector<std::set<std::string>> keys;
    return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
        require(depth <= 16, "Profile JSON exceeds nesting bound");
        if(event == Json::parse_event_t::object_start || event == Json::parse_event_t::array_start)
            keys.emplace_back();
        if(event == Json::parse_event_t::object_end || event == Json::parse_event_t::array_end)
            keys.pop_back();
        if(event == Json::parse_event_t::key)
            require(keys.back().insert(value.get<std::string>()).second, "Duplicate profile JSON key");
        return true;
    });
}
std::string hash(std::string_view bytes) {
    return sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
}
std::filesystem::path reference_path(const Json& ref) {
    require(ref.is_object() && ref.size() == 2, "Invalid profile reference shape");
    const auto path = ref.at("path").get<std::string>();
    const auto expected = ref.at("sha256").get<std::string>();
    const std::filesystem::path parsed(path);
    require(path.size() <= 512 && path.starts_with("raw/profile/") &&
                parsed.lexically_normal().generic_string() == path && expected.size() == 64 &&
                std::all_of(expected.begin(), expected.end(),
                            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }),
            "Invalid same-profile file reference");
    for(const auto& component : parsed)
        require(component != ".." && component != ".", "Invalid profile reference component");
    return parsed;
}
std::string reference(ArtifactStore& artifacts, const std::string& id, const Json& ref) {
    const auto path = reference_path(ref);
    const auto text = artifacts.read(id, path.generic_string(), ProfileEvidenceLimits::bytes);
    require(hash(text) == ref.at("sha256").get<std::string>(), "Profile export hash differs from retained index");
    return text;
}
void inventory(ArtifactStore& artifacts, const std::string& id, const Json& index) {
    std::map<std::string, std::uint64_t> files;
    std::size_t offset = 0;
    do {
        const auto page = artifacts.files(id, offset, 100);
        for(const auto& file : page.files)
            files.emplace(file.path, file.bytes);
        require(files.size() <= 4096, "Profile file inventory exceeds bound");
        if(!page.next_offset)
            break;
        offset = *page.next_offset;
    } while(true);
    const auto check = [&](const Json& ref, std::uint64_t bound) {
        const auto path = reference_path(ref);
        const auto found = files.find(path.generic_string());
        require(found != files.end() && found->second > 0 && found->second <= bound,
                "Profile reference is absent or outside file size bounds");
        return path;
    };
    const auto trace = check(index.at("trace"), 1024ULL * 1024 * 1024);
    require(trace.extension() == ".ngfx-gputrace", "Invalid profile trace extension");
    const auto repro = check(index.at("reproduction"), ProfileEvidenceLimits::bytes);
    require(repro.filename() == "REPRO_INFO.xls", "Invalid profile reproduction filename");
    require(index.at("tables").is_object() && index.at("tables").size() == 4,
            "Profile must inventory four observed tables");
    for(const auto& [key, name] : std::map<std::string, std::string>{{"frame_duration", "FRAME.xls"},
                                                                     {"frame_metrics", "GPUTRACE_FRAME.xls"},
                                                                     {"event_durations", "D3DPERF_EVENTS.xls"},
                                                                     {"regime_metrics", "GPUTRACE_REGIMES.xls"}}) {
        const auto path = check(index.at("tables").at(key), ProfileEvidenceLimits::bytes);
        require(path.parent_path() == repro.parent_path() && path.filename() == name,
                "Profile table reference differs from observed export layout");
    }
}
struct OpenProfile {
    ArtifactLease lease;
    Json index;
    Json report_reference;
    Json report;
};
OpenProfile open(ArtifactStore& artifacts, const std::string& id) {
    OpenProfile result{artifacts.lease(id), {}, {}, {}};
    const auto info = artifacts.inspect(id);
    if(info.provenance.value("evidence_origin", "") != "nsight_gpu_trace")
        throw InspectionError(Error::NotCapture, "Artifact is not a service-produced GPU Trace");
    if(info.summary.status != "complete" || info.summary.quarantined)
        throw InspectionError(Error::IncompleteCapture, "Profile is not complete; inspect its job/report");
    const auto report_bytes = artifacts.read(id, "raw/report.json", 2 * 1024 * 1024);
    result.report_reference = {{"path", "raw/report.json"}, {"sha256", hash(report_bytes)}};
    result.report = parse(report_bytes);
    const auto& report = result.report;
    auto& index = result.index;
    index = parse(artifacts.read(id, "derived/profile.json", 256 * 1024));
    require(index.at("schema_version") == 1 && report.at("schema_version") == 1 &&
                index.at("evidence_origin") == "nsight_gpu_trace" &&
                report.at("evidence_origin") == "nsight_gpu_trace" && index.at("job") == report.at("job") &&
                index.at("job") == info.provenance.at("job") && index.at("job").at("capture_id") == id,
            "Profile index/report/manifest identity mismatch");
    require(report.at("profiled") == true && report.at("worker_outcome") == "succeeded" &&
                report.at("cleanup_confirmed") == true && info.provenance.at("profiled") == true &&
                info.provenance.at("cleanup_confirmed") == true &&
                info.provenance.at("worker_outcome") == "succeeded" &&
                report.at("profile_index") == "derived/profile.json",
            "Profile worker did not complete successfully");
    require(info.provenance.at("schema_version") == 1 && info.provenance.at("report_path") == "raw/report.json" &&
                info.provenance.at("backend") == "documented_nsight_cli" &&
                report.at("backend") == "documented_nsight_cli" &&
                report.at("project_version") == info.provenance.at("project_version") &&
                report.at("project_version").is_string() &&
                report.at("project_version").get<std::string>().size() <= 64 &&
                report.at("nsight").at("interface_ready") == true,
            "Invalid profile manifest or tool interface qualification");
    for(const auto* required : {"derived/profile.json", "raw/report.json"})
        require(std::find(info.required_outputs.begin(), info.required_outputs.end(), required) !=
                    info.required_outputs.end(),
                "Profile manifest omits a required output");
    const auto& job = index.at("job");
    require(job.is_object() && job.size() == 3 && job.at("job_id").is_string() &&
                !job.at("job_id").get<std::string>().empty() && job.at("job_id").get<std::string>().size() <= 256 &&
                job.at("attempt").is_number_unsigned() && job.at("attempt").get<std::uint64_t>() > 0,
            "Invalid profile job identity");
    const auto& operation = report.at("profile");
    const auto& process = operation.at("process");
    require(operation.at("outcome") == "success" && operation.at("launched") == true && process.at("exit_code") == 0 &&
                process.at("signal").is_null() && process.at("timed_out") == false &&
                process.at("cancelled") == false && process.at("cleanup_confirmed") == true &&
                process.at("error") == "" && operation.at("output") == "raw/profile",
            "Profile operation was not successful");
    const auto& cli = report.at("nsight").at("cli");
    require(cli.at("version_valid") == true && cli.at("help_valid") == true &&
                operation.at("executable") == cli.at("path") &&
                cli.at("version") == info.provenance.at("nsight").at("cli").at("version") &&
                cli.at("build") == info.provenance.at("nsight").at("cli").at("build"),
            "Profile producer observations disagree");
    for(const auto* tool : {"cli", "capture", "replay"}) {
        const auto& observed = report.at("nsight").at(tool);
        const auto& retained = info.provenance.at("nsight").at(tool);
        require(observed.at("version_valid") == true && observed.at("help_valid") == true &&
                    observed.at("version") == cli.at("version") && observed.at("build") == cli.at("build"),
                "Profile tool observations are not a matching valid set");
        for(const auto* field : {"path", "version", "build"})
            require(observed.at(field) == retained.at(field), "Profile tool manifest/report mismatch");
    }
    inventory(artifacts, id, index);
    const auto repro = parse_profile_reproduction(reference(artifacts, id, index.at("reproduction")));
    require(repro.product_version == cli.at("version").get<std::string>() + " (build " +
                                         cli.at("build").get<std::string>() + ") (public-release)" &&
                index.at("producer") == repro.product_version && index.at("gpu") == repro.gpu &&
                index.at("driver") == repro.driver && index.at("settings") == Json(repro.settings),
            "Profile reproduction/index producer mismatch");
    const auto& requested = index.at("requested");
    require(requested == report.at("capture_settings") && requested == info.provenance.at("capture_settings") &&
                requested.at("format") == "profile" && requested.at("gpu_clocks") == "unaltered" &&
                requested.at("multi_pass") == false,
            "Profile settings differ across retained records");
    require(requested.is_object() && requested.size() == 10 && requested.at("timeout_ms").is_number_integer() &&
                requested.at("timeout_ms") >= 1 && requested.at("timeout_ms") <= 600000,
            "Invalid profile settings shape or deadline");
    for(const auto* field : {"start_after", "limit", "duration_ms"})
        require(requested.at(field).is_number_unsigned(), "Profile counters must be unsigned");
    ProfileSettings settings{requested.at("delimiter"),   requested.at("start_after"),  requested.at("limit"),
                             requested.at("duration_ms"), requested.at("architecture"), requested.at("metric_set")};
    try {
        validate_profile_settings(settings);
    } catch(const std::invalid_argument&) {
        throw InspectionError(Error::InvalidReport, "Invalid retained profile settings");
    }
    const auto& app = report.at("application");
    for(const auto* field : {"executable", "sha256_before_launch", "sha256_after_launch", "identity_scope"})
        require(app.at(field) == info.provenance.at("application").at(field), "Profile application provenance differs");
    std::vector<std::string> expected{"--activity=GPU Trace Profiler",
                                      "--platform=Linux (x86_64)",
                                      "--exe=" + app.at("executable").get<std::string>(),
                                      "--dir=" + app.at("working_directory").get<std::string>(),
                                      "--output-dir=" + (result.lease.directory() / "raw/profile").string(),
                                      "--start-after-" + settings.delimiter + "=" +
                                          std::to_string(settings.start_after),
                                      "--limit-to-" + settings.delimiter + "=" + std::to_string(settings.limit),
                                      "--max-duration-ms=" + std::to_string(settings.duration_ms),
                                      "--architecture=" + settings.architecture,
                                      "--metric-set-name=" + settings.metric_set,
                                      "--set-gpu-clocks=unaltered",
                                      "--collect-screenshot=0",
                                      "--auto-export"};
    // Publication renames staging directories; validate the recorded launch path
    // by its suffix rather than treating today's retained path as the launch path.
    const auto arguments = operation.at("arguments").get<std::vector<std::string>>();
    require(arguments.size() >= 13 && arguments.size() <= 14 && arguments[4].starts_with("--output-dir=/") &&
                arguments[4].ends_with("/raw/profile"),
            "Invalid profile invocation output path");
    expected[4] = arguments[4];
    std::string joined;
    for(const auto& argument : app.at("arguments").get<std::vector<std::string>>()) {
        if(!joined.empty())
            joined += ' ';
        joined += argument;
    }
    if(!joined.empty())
        expected.push_back("--args=" + joined);
    require(arguments == expected, "Profile invocation differs from retained settings");
    const std::string suffix = settings.delimiter == "frames" ? " Frames" : " Submits";
    require(repro.settings.at("Start After") == std::to_string(settings.start_after) + suffix &&
                repro.settings.at("Limited To") == std::to_string(settings.limit) + suffix &&
                repro.settings.at("Max Duration ") == std::to_string(settings.duration_ms) + " ms" &&
                repro.settings.at("GPU Clocks") == "Unaltered" &&
                repro.settings.at("Multi-Pass Metrics") == "Disabled" &&
                repro.settings.at("Metric Set") == settings.metric_set,
            "Requested and exported collection settings differ");
    require(index.at("tables").is_object() && index.at("tables").size() == 4,
            "Profile must inventory four observed tables");
    return result;
}
template <typename Function>
Json guarded(Function function) {
    try {
        return function();
    } catch(const Json::exception&) {
        throw InspectionError(Error::InvalidReport, "Malformed profile report/index");
    } catch(const NsightEvidenceError& error) {
        throw InspectionError(Error::InvalidExport, error.what());
    } catch(const std::out_of_range&) {
        throw InspectionError(Error::InvalidReport, "Missing profile collection setting");
    }
}
ProfileTableKind table_kind(const std::string& name) {
    const std::map<std::string, ProfileTableKind> kinds{{"frame_duration", ProfileTableKind::FrameDuration},
                                                        {"frame_metrics", ProfileTableKind::FrameMetrics},
                                                        {"event_durations", ProfileTableKind::EventDurations},
                                                        {"regime_metrics", ProfileTableKind::RegimeMetrics}};
    const auto found = kinds.find(name);
    if(found == kinds.end())
        throw std::invalid_argument("Unknown profile table");
    return found->second;
}
Json statistics(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    const double median = values.size() % 2 ? values[middle] : std::midpoint(values[middle - 1], values[middle]);
    // Divide before summing in wider precision; the convex mean stays within
    // the finite input range, including opposing near-DBL_MAX values.
    long double mean = 0;
    for(const auto value : values)
        mean += static_cast<long double>(value) / values.size();
    const double bounded_mean = static_cast<double>(
        std::clamp(mean, static_cast<long double>(values.front()), static_cast<long double>(values.back())));
    return {{"count", values.size()},
            {"minimum", values.front()},
            {"maximum", values.back()},
            {"median", median},
            {"mean", bounded_mean}};
}
Json finite_number(long double value) {
    if(!std::isfinite(value) || std::abs(value) > std::numeric_limits<double>::max())
        return nullptr;
    return static_cast<double>(value);
}
} // namespace

Json ProfileInspection::metadata(const std::string& id) const {
    return guarded([&] {
        const auto profile = open(artifacts_, id);
        const auto& index = profile.index;
        return Json{{"profile_id", id},
                    {"producer", index.at("producer")},
                    {"gpu", index.at("gpu")},
                    {"driver", index.at("driver")},
                    {"settings", index.at("settings")},
                    {"requested", index.at("requested")},
                    {"trace", index.at("trace")},
                    {"reproduction", index.at("reproduction")},
                    {"tables", index.at("tables")},
                    {"scope", "Retained live-target exports; application correctness, repeated-run equivalence and "
                              "physical metric scaling are not inferred."}};
    });
}

Json ProfileInspection::metrics(const std::string& id, const std::string& name, std::size_t offset, std::size_t limit,
                                std::size_t column_offset, std::size_t column_limit) const {
    if(offset > ProfileEvidenceLimits::rows || limit == 0 || limit > 100 ||
       column_offset > ProfileEvidenceLimits::columns || column_limit == 0 || column_limit > 64)
        throw std::invalid_argument(
            "Profile page bounds: offset 0..4096, limit 1..100, column_offset 0..4096, column_limit 1..64");
    const auto kind = table_kind(name);
    return guarded([&] {
        const auto profile = open(artifacts_, id);
        const auto& ref = profile.index.at("tables").at(name);
        const auto table = parse_profile_table(reference(artifacts_, id, ref), kind);
        const auto width = table.rows.front().values.size();
        const auto end_column = std::min(width, column_offset + column_limit);
        Json rows = Json::array();
        std::size_t bytes = 0, next = std::min(offset, table.rows.size());
        for(; next < table.rows.size() && rows.size() < limit; ++next) {
            const auto& row = table.rows[next];
            Json values = Json::array();
            for(auto column = column_offset; column < end_column; ++column) {
                values.push_back({{"column_index", column},
                                  {"column_name", table.columns.empty() ? Json(nullptr) : Json(table.columns[column])},
                                  {"text", row.values[column].text},
                                  {"value", row.values[column].value},
                                  {"unit", kind == ProfileTableKind::EventDurations ? Json("ms") : Json(nullptr)}});
            }
            Json item{{"row_index", next}, {"label", row.label}, {"values", std::move(values)}};
            const auto size = item.dump().size();
            if(bytes + size > 240 * 1024) {
                if(rows.empty())
                    throw InspectionError(Error::LimitExceeded,
                                          "Profile row exceeds page budget; request fewer columns");
                break;
            }
            bytes += size;
            rows.push_back(std::move(item));
        }
        return Json{
            {"profile_id", id},
            {"table", name},
            {"source", ref},
            {"total_rows", table.rows.size()},
            {"total_columns", width},
            {"offset", offset},
            {"column_offset", column_offset},
            {"rows", std::move(rows)},
            {"next_offset", next < table.rows.size() ? Json(next) : Json(nullptr)},
            {"next_column_offset", end_column < width ? Json(end_column) : Json(nullptr)},
            {"scope",
             "Zero-based export positions only; repeated labels/columns are preserved. Null unit means physical "
             "scaling is unresolved, not dimensionless. No frame/sample/statistic mapping is inferred."}};
    });
}

Json ProfileInspection::compare(const ProfileComparison& request) const {
    if(request.baseline.size() < 2 || request.baseline.size() > 8 || request.candidate.size() < 2 ||
       request.candidate.size() > 8 || request.label.empty() || request.label.size() > 4096 ||
       request.column_index >= ProfileEvidenceLimits::columns || request.workload_policy.empty() ||
       request.workload_policy.size() > 2048 || request.warmup_policy.empty() || request.warmup_policy.size() > 2048)
        throw std::invalid_argument("Comparison requires 2..8 distinct profiles per group, an exact label/column "
                                    "and bounded workload/warmup declarations");
    const auto kind = table_kind(request.table);
    std::set<std::string> ids(request.baseline.begin(), request.baseline.end());
    ids.insert(request.candidate.begin(), request.candidate.end());
    if(ids.size() != request.baseline.size() + request.candidate.size())
        throw std::invalid_argument("Comparison profiles must be distinct across both groups");
    return guarded([&] {
        // Keep every lease until the entire comparison completes. A prune can
        // race acquisition and fail the request, but cannot remove an open run.
        std::map<std::string, OpenProfile> profiles;
        std::set<std::string> jobs;
        Json common, column_name;
        for(const auto& id : ids) {
            auto profile = open(artifacts_, id);
            require(jobs.insert(profile.index.at("job").at("job_id").get<std::string>()).second,
                    "Comparison requires distinct collection jobs");
            Json settings = profile.index.at("requested");
            settings.erase("timeout_ms"); // Process deadline is not a collection setting.
            Json observed{{"producer", profile.index.at("producer")},
                          {"gpu", profile.index.at("gpu")},
                          {"driver", profile.index.at("driver")},
                          {"settings", profile.index.at("settings")},
                          {"collection", std::move(settings)}};
            if(common.is_null())
                common = std::move(observed);
            else if(common != observed)
                throw std::invalid_argument("Profile producer, GPU, driver and collection settings must match");
            profiles.emplace(id, std::move(profile));
        }
        bool first_column = true;
        const auto group = [&](const std::vector<std::string>& members) {
            Json runs = Json::array();
            std::vector<double> medians;
            for(const auto& id : members) {
                const auto& profile = profiles.at(id);
                const auto& ref = profile.index.at("tables").at(request.table);
                const auto table = parse_profile_table(reference(artifacts_, id, ref), kind);
                if(request.column_index >= table.rows.front().values.size())
                    throw std::invalid_argument("Selected column is absent in a comparison profile");
                const Json name = table.columns.empty() ? Json(nullptr) : Json(table.columns[request.column_index]);
                if(first_column) {
                    column_name = name;
                    first_column = false;
                } else if(column_name != name)
                    throw std::invalid_argument("Selected column names differ across profiles");
                std::vector<double> values;
                for(const auto& row : table.rows)
                    if(row.label == request.label)
                        values.push_back(row.values[request.column_index].value);
                if(values.empty())
                    throw std::invalid_argument("Selected exact row label is absent in a comparison profile");
                auto stats = statistics(std::move(values));
                medians.push_back(stats.at("median"));
                runs.push_back({{"profile_id", id},
                                {"job", profile.index.at("job")},
                                {"source", ref},
                                {"report", profile.report_reference},
                                {"project_version", profile.report.at("project_version")},
                                {"within_trace", std::move(stats)}});
            }
            return Json{{"runs", std::move(runs)}, {"run_medians", statistics(std::move(medians))}};
        };
        auto baseline = group(request.baseline), candidate = group(request.candidate);
        const auto& a = baseline.at("run_medians");
        const auto& b = candidate.at("run_medians");
        const double am = a.at("median"), bm = b.at("median");
        const std::string ordering = b.at("maximum").get<double>() < a.at("minimum").get<double>() ? "candidate_lower"
                                     : b.at("minimum").get<double>() > a.at("maximum").get<double>()
                                         ? "candidate_higher"
                                         : "overlap_or_touch";
        return Json{
            {"table", request.table},
            {"label", request.label},
            {"column_index", request.column_index},
            {"column_name", column_name},
            {"unit", kind == ProfileTableKind::EventDurations ? Json("ms") : Json(nullptr)},
            {"common", std::move(common)},
            {"declarations",
             {{"origin", "caller_unverified"},
              {"workload_policy", request.workload_policy},
              {"warmup_policy", request.warmup_policy}}},
            {"baseline", std::move(baseline)},
            {"candidate", std::move(candidate)},
            {"median_difference", finite_number(static_cast<long double>(bm) - am)},
            {"candidate_over_baseline", am == 0 ? Json(nullptr) : finite_number(static_cast<long double>(bm) / am)},
            {"run_median_range_order", ordering},
            {"scope", "Each fresh trace contributes one median of exact-label rows at the selected column. "
                      "Rows are not independent runs; positional columns have no inferred statistic meaning. "
                      "Ranges are descriptive, not confidence intervals or proof of improvement. Null unit "
                      "means unresolved scaling. Null arithmetic means zero denominator or overflow. Reports "
                      "retain launch/build provenance; equivalent inputs, correctness, warmup sufficiency "
                      "and clock stability require separate evidence. Live-target collection, no replay."}};
    });
}
} // namespace ngm
