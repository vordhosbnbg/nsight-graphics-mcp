#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/ProfileInspection.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {
using ngm::check::require;
using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace std::chrono_literals;
struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-compare-XXXXXX").string();
        const auto p = mkdtemp(pattern.data());
        require(p != nullptr, "scratch directory");
        path = p;
    }
    ~Scratch() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};
template <typename F>
void rejected(F operation) {
    try {
        operation();
    } catch(const std::exception&) {
        return;
    }
    require(false, "invalid comparison is refused");
}
Json call(ngm::check::mcp::Client& client, const char* name, const Json& args) {
    const auto response = client.request(3, "tools/call", {{"name", name}, {"arguments", args}});
    require(response.contains("result") && !response.at("result").value("isError", false), response.dump());
    const auto& result = response.at("result");
    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) == result.at("structuredContent"),
            "matching text/structured results");
    return result.at("structuredContent");
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "stand-in and server paths");
        Scratch scratch;
        const auto installation = scratch.path / "installation";
        for(const auto name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
            const auto output = installation / "host/linux-desktop-nomad-x64" / name;
            fs::create_directories(output.parent_path());
            fs::copy_file(fs::absolute(argv[1]), output);
            fs::permissions(output, fs::perms::owner_all);
        }
        const auto target = scratch.path / "target with spaces";
        fs::copy_file(fs::absolute(argv[1]), target);
        fs::permissions(target, fs::perms::owner_all);
        ngm::CaptureServiceOptions options;
        options.nsight_root = installation;
        options.artifacts.root = scratch.path / "store";
        options.environment = {{"NGM_TARGET_RECORD", (scratch.path / "targets.txt").string()}};
        ngm::CaptureRequest request;
        request.format = ngm::CaptureFormat::Profile;
        request.executable = target;
        request.working_directory = scratch.path;
        request.profile.architecture = "Ampere GA10x";
        request.timeout = 5s;
        request.pin = true;
        auto produce = [&](ngm::CaptureService& service, const std::string& numbers,
                           std::vector<std::string> arguments = {}) {
            auto run = request;
            run.arguments = std::move(arguments);
            run.arguments.push_back("--profile-values=" + numbers);
            const auto submitted = service.capture(run);
            const auto status = service.wait(submitted.identity.job_id, 10s);
            require(status && status->state == ngm::JobState::succeeded && status->cleanup_confirmed,
                    "fresh stand-in profile succeeds");
            return submitted.artifact_id;
        };
        ngm::ProfileComparison spec;
        spec.table = "regime_metrics";
        spec.label = "synthetic.work";
        spec.workload_policy = "CPU synthetic equal-workload declaration, not real GPU evidence";
        spec.warmup_policy = "30 skipped submits is a declared policy, not proof of steady state";
        Json expected;
        {
            ngm::CaptureService service(options);
            spec.baseline = {produce(service, "10,20"), produce(service, "10,20,30,40")};
            // Different job deadlines are intentionally not different collection settings.
            request.timeout = 6s;
            spec.candidate = {produce(service, "1,2,3"), produce(service, "2,4")};
            ngm::ProfileInspection inspect(service.artifacts());
            expected = inspect.compare(spec);
            require(expected["baseline"]["run_medians"]["median"] == 20 &&
                        expected["baseline"]["run_medians"]["mean"] == 20 &&
                        expected["baseline"]["run_medians"]["count"] == 2 &&
                        expected["baseline"]["runs"][1]["within_trace"]["count"] == 4 &&
                        expected["candidate"]["run_medians"]["median"] == 2.5 &&
                        expected["median_difference"] == -17.5 && expected["candidate_over_baseline"] == 0.125 &&
                        expected["run_median_range_order"] == "candidate_lower" && expected["unit"].is_null(),
                    "one median per trace with equal run weights, not pooled row weights");
            auto reversed = spec;
            std::swap(reversed.baseline, reversed.candidate);
            require(inspect.compare(reversed)["run_median_range_order"] == "candidate_higher", "direction is numeric");
            for(const auto& extra :
                {std::vector<std::string>{"--profile-gpu=other"}, std::vector<std::string>{"--profile-driver=other"},
                 std::vector<std::string>{"--profile-column=other"},
                 std::vector<std::string>{"--profile-other-label"}}) {
                auto different = spec;
                different.candidate[0] = produce(service, "1", extra);
                rejected([&] { inspect.compare(different); });
            }
            ++request.profile.start_after;
            auto different = spec;
            different.candidate[0] = produce(service, "1");
            rejected([&] { inspect.compare(different); });
            --request.profile.start_after;
            for(int invalid = 0; invalid < 9; ++invalid) {
                auto bad = spec;
                switch(invalid) {
                    case 0:
                        bad.candidate[0] = bad.baseline[0];
                        break;
                    case 1:
                        bad.baseline[1] = bad.baseline[0];
                        break;
                    case 2:
                        bad.baseline.pop_back();
                        break;
                    case 3:
                        bad.baseline.resize(9, bad.baseline[0]);
                        break;
                    case 4:
                        bad.column_index = 1;
                        break;
                    case 5:
                        bad.label = "missing";
                        break;
                    case 6:
                        bad.table = "unknown";
                        break;
                    case 7:
                        bad.warmup_policy.clear();
                        break;
                    case 8:
                        bad.workload_policy.assign(2049, 'x');
                        break;
                }
                rejected([&] { inspect.compare(bad); });
            }
            auto times = spec;
            times.table = "event_durations";
            const auto durations = inspect.compare(times);
            require(durations["unit"] == "ms" && durations["run_median_range_order"] == "overlap_or_touch" &&
                        std::abs(durations["baseline"]["run_medians"]["median"].get<double>() - 0.15) < 1e-15,
                    "explicit duration units, duplicate labels reduced within trace");
            auto headerless = spec;
            headerless.table = "frame_metrics";
            headerless.label = "synthetic.metric";
            const auto frame_metric = inspect.compare(headerless);
            require(frame_metric["column_name"].is_null() && frame_metric["unit"].is_null() &&
                        frame_metric["baseline"]["run_medians"]["median"] == 1,
                    "headerless positions have no inferred name or scaling");
            headerless.table = "frame_duration";
            headerless.label = "GPU frame time";
            require(inspect.compare(headerless)["baseline"]["run_medians"]["median"] == 0.5,
                    "headerless frame duration remains an unscaled number");
            auto missing = spec;
            missing.label = "absent after all leases were acquired";
            rejected([&] { inspect.compare(missing); });
            for(const auto& group : {spec.baseline, spec.candidate})
                for(const auto& id : group)
                    require(!service.artifacts().inspect(id).summary.in_use,
                            "mid-comparison exception releases every acquired lease");
            auto extremes = spec;
            extremes.baseline = {produce(service, "-1.7e308,-1.7e308"), produce(service, "-1.7e308")};
            extremes.candidate = {produce(service, "1.7e308,1.7e308"), produce(service, "1.7e308")};
            const auto huge = inspect.compare(extremes);
            require(huge["median_difference"].is_null() && huge["candidate_over_baseline"] == -1 &&
                        huge["candidate"]["run_medians"]["mean"] == 1.7e308,
                    "finite extreme means/medians; overflowing difference is explicit null");
            extremes.baseline = {produce(service, "-1.7e308,1.7e308"), produce(service, "0")};
            const auto zero = inspect.compare(extremes);
            require(zero["baseline"]["run_medians"]["median"] == 0 && zero["candidate_over_baseline"].is_null(),
                    "opposing extreme midpoint and zero denominator");
            extremes.baseline = {produce(service, "1e-300"), produce(service, "1e-300")};
            require(inspect.compare(extremes)["candidate_over_baseline"].is_null(),
                    "overflowing ratio is explicit null");
            const auto info = service.artifacts().inspect(spec.baseline[0]);
            require(!info.summary.in_use, "comparison releases all usage leases");
        }
        {
            auto old = options;
            old.environment["NGM_PROFILE_OLD_RELEASE"] = "1";
            ngm::CaptureService service(old);
            auto mixed = spec;
            mixed.candidate[0] = produce(service, "1");
            require(ngm::ProfileInspection(service.artifacts()).metadata(mixed.candidate[0])["producer"] ==
                        "2026.2.0.0 (build 37991608) (public-release)",
                    "second qualified synthetic producer");
            rejected([&] { ngm::ProfileInspection(service.artifacts()).compare(mixed); });
        }
        // Actual MCP invocation against retained evidence, with no installed backend supplied.
        ngm::check::mcp::Client client(fs::absolute(argv[2]), scratch.path,
                                       {"--artifact-root", options.artifacts.root.string()});
        require(client
                    .request(1, "initialize",
                             {{"protocolVersion", "2025-06-18"},
                              {"capabilities", Json::object()},
                              {"clientInfo", {{"name", "profile-comparison-check"}, {"version", "1"}}}})
                    .contains("result"),
                "initialize");
        client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        Json args{{"baseline", spec.baseline},
                  {"candidate", spec.candidate},
                  {"table", spec.table},
                  {"label", spec.label},
                  {"column_index", 0},
                  {"workload_policy", spec.workload_policy},
                  {"warmup_policy", spec.warmup_policy}};
        require(call(client, "profile_compare", args) == expected, "offline MCP comparison survives restart exactly");
        args["baseline"] = Json::array({spec.baseline[0]});
        require(client.request(4, "tools/call",
                               {{"name", "profile_compare"}, {"arguments", args}})["result"]["isError"] == true,
                "MCP rejects insufficient repeats");
        require(client.finish() == 0, "protocol-only stdout and clean shutdown");
    });
}
