#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
Json load(const fs::path& path) {
    return Json::parse(ngm::read_regular_file(path, 8U * 1024U * 1024U));
}
void save(const fs::path& path, const Json& value) {
    std::ofstream stream(path.string() + ".tmp");
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream << value.dump(2) << '\n';
    stream.close();
    fs::rename(path.string() + ".tmp", path);
}
std::vector<std::vector<std::string>> raw_table(const fs::path& source, const std::string& id, const Json& ref) {
    const fs::path relative(ref.at("path").get<std::string>());
    require(relative.generic_string().starts_with("raw/profile/") && relative.lexically_normal() == relative,
            "source-relative documented export");
    const auto bundle = source / "store/bundles" / id;
    const auto path = fs::canonical(bundle / relative);
    require(path.string().starts_with(fs::canonical(bundle).string() + "/") &&
                ngm::sha256_file(path) == ref.at("sha256").get<std::string>(),
            "independent source export identity");
    std::istringstream lines(ngm::read_regular_file(path, 16U * 1024U * 1024U));
    std::vector<std::vector<std::string>> rows;
    for(std::string line; std::getline(lines, line);) {
        if(line.ends_with('\r'))
            line.pop_back();
        std::vector<std::string> cells;
        for(std::size_t start = 0;;) {
            const auto end = line.find('\t', start);
            cells.push_back(line.substr(start, end == std::string::npos ? end : end - start));
            if(end == std::string::npos)
                break;
            start = end + 1;
        }
        rows.push_back(std::move(cells));
    }
    return rows;
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 4,
                "SERVER SOURCE_CORRELATION_RUN OUTPUT_PARENT required; this verifier performs no GPU collection");
        const auto server = fs::canonical(argv[1]), source = fs::canonical(argv[2]);
        const auto parent = fs::absolute(argv[3]);
        const auto native = load(source / "report.json"), audit = load(source / "independent-audit.json");
        require(native.at("status") == "pass" && audit.at("status") == "pass", "retained qualified native matrix");
        fs::create_directories(parent);
        auto pattern = (parent / "profile-marker-XXXXXX").string();
        require(mkdtemp(pattern.data()), "unique read-only marker validation run");
        const fs::path run(pattern);
        fs::copy_file(__FILE__, run / "ProfileMarkerIntegration.cpp");
        Json report{{"status", "running"},
                    {"reader_version", ngm::project_version()},
                    {"server_sha256", ngm::sha256_file(server)},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"source_run", source.string()},
                    {"source_version", native.at("project_version")},
                    {"source_report_sha256", ngm::sha256_file(source / "report.json")},
                    {"scope", "Actual MCP query of retained native evidence with independent export positions; no new "
                              "GPU run or private witness supplied to product"},
                    {"cases", Json::array()}};
        save(run / "report.json", report);
        std::cout << run << '\n' << std::flush;
        std::ofstream transcript(run / "transcript.ndjson");
        transcript.exceptions(std::ios::failbit | std::ios::badbit);
        const std::vector<std::string> names{"FE_A.TriageSCG.gr__cycles_active.avg.pct_of_peak_sustained_elapsed",
                                             "FE_A.TriageSCG.fe__draw_count.sum", "ngm.absent.metric"};
        try {
            for(unsigned pass = 0; pass < 2; ++pass) {
                ngm::check::mcp::Client client(server.string(), "/usr/bin:/bin",
                                               {"--artifact-root", (source / "store").string()}, false, {}, {30s, 30s});
                int sequence = 1;
                const auto request = [&](const char* method, const Json& args) {
                    transcript << Json{{"pass", pass},
                                       {"request", {{"id", sequence}, {"method", method}, {"params", args}}}}
                                      .dump()
                               << '\n';
                    const auto response = client.request(sequence++, method, args);
                    transcript << Json{{"pass", pass}, {"response", response}}.dump() << '\n';
                    transcript.flush();
                    require(response.contains("result"), "marker protocol response");
                    return response.at("result");
                };
                const auto call = [&](const char* name, const Json& args) {
                    const auto result = request("tools/call", {{"name", name}, {"arguments", args}});
                    require(!result.value("isError", false) && result.contains("structuredContent"),
                            "marker tool: " + result.dump());
                    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                                result.at("structuredContent"),
                            "marker JSON text/structured parity");
                    return result.at("structuredContent");
                };
                require(request("initialize",
                                {{"protocolVersion", "2025-06-18"},
                                 {"capabilities", Json::object()},
                                 {"clientInfo",
                                  {{"name", "ngm-profile-marker-integration"}, {"version", ngm::project_version()}}}})
                                .at("serverInfo")
                                .at("version") == ngm::project_version(),
                        "current reader identity");
                client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
                if(pass == 0) {
                    report["tools"] = request("tools/list", Json::object());
                    report["capabilities"] = call("capabilities", Json::object());
                    require(report.at("tools").at("tools").size() == 29 &&
                                report.at("capabilities").at("operations").at("profile_markers").at("available") ==
                                    true,
                            "implemented marker query is discoverable with retained storage");
                }
                for(std::size_t case_index = 0; case_index < native.at("profiles").size(); ++case_index) {
                    const auto& item = native.at("profiles")[case_index];
                    const auto id = item.at("submission").at("artifact_id").get<std::string>();
                    require(audit.at("profiles")[case_index].at("id") == id &&
                                audit.at("profiles")[case_index].at("key") == item.at("key"),
                            "independent audit belongs to this exact native profile");
                    const auto regimes = raw_table(source, id, item.at("metadata").at("tables").at("regime_metrics"));
                    const auto& tree = audit.at("profiles")[case_index].at("tree");
                    std::map<std::string, std::size_t> event_counts, regime_counts, regime_positions;
                    for(const auto& node : tree)
                        ++event_counts[node.at("path").get<std::string>()];
                    for(std::size_t row = 1; row < regimes.size(); ++row) {
                        ++regime_counts[regimes[row][0]];
                        regime_positions[regimes[row][0]] = row - 1;
                    }
                    const auto& settings = item.at("request").at("settings");
                    const bool collection = settings.at("delimiter") == "frames" && settings.at("limit") == 1 &&
                                            settings.at("time_every_action") == true;
                    Json current = Json::array();
                    for(std::size_t row = 0; row < tree.size(); ++row) {
                        const auto flat = tree[row].at("path").get<std::string>();
                        const bool unique = event_counts[flat] == 1 && regime_counts[flat] == 1;
                        const bool available = collection && unique;
                        const auto result =
                            call("profile_marker", {{"profile_id", id}, {"event_row_index", row}, {"metrics", names}});
                        require(result.at("profile_id") == id &&
                                    result.at("source_project_version") == native.at("project_version") &&
                                    result.at("coverage").at("status") == (!collection ? "unsupported"
                                                                           : unique    ? "available"
                                                                                       : "unavailable") &&
                                    result.at("selection").at("domain") == "gpu_trace_exported_marker" &&
                                    result.at("selection").at("raw_label") ==
                                        item.at("events").at("rows")[row].at("label") &&
                                    result.at("evidence").at("event_durations") ==
                                        item.at("metadata").at("tables").at("event_durations") &&
                                    result.at("evidence").at("regime_metrics") ==
                                        item.at("metadata").at("tables").at("regime_metrics"),
                                "exact capture-local source identity and independent collection/path coverage");
                        if(available) {
                            require(result.at("duration").at("value") ==
                                            item.at("events").at("rows")[row].at("values")[0].at("value") &&
                                        result.at("duration").at("text") ==
                                            item.at("events").at("rows")[row].at("values")[0].at("text") &&
                                        result.at("duration").at("unit") == "ms" &&
                                        result.at("selection").at("flat_path") == flat &&
                                        result.at("selection").at("regime_row_index") == regime_positions[flat],
                                    "exact exported duration, ms unit, unique flat path and source positions");
                            for(std::size_t n = 0; n < names.size(); ++n) {
                                const auto& actual = result.at("metrics")[n];
                                const auto found = std::find(regimes[0].begin() + 1, regimes[0].end(), names[n]);
                                if(found == regimes[0].end())
                                    require(actual.at("status") == "unavailable" && actual.at("value").is_null() &&
                                                actual.at("column_index").is_null(),
                                            "absent requested counter is not fabricated");
                                else {
                                    const auto column = static_cast<std::size_t>(found - regimes[0].begin());
                                    const auto& text = regimes[regime_positions[flat] + 1][column];
                                    require(actual.at("text") == text &&
                                                actual.at("row_index") == regime_positions[flat] &&
                                                actual.at("column_index") == column - 1,
                                            "independent raw counter text and row/column identity");
                                    if(text == "nan")
                                        require(actual.at("value").is_null(), "undefined counter retains null");
                                    else
                                        require(actual.at("value") == std::stod(text), "raw counter numeric value");
                                }
                                require(actual.at("unit").is_null() && actual.at("aggregation").is_null(),
                                        "physical counter scaling remains unresolved");
                            }
                        } else {
                            require(result.at("duration").at("value").is_null() &&
                                        !result.at("coverage").at("reason").get<std::string>().empty(),
                                    "unsupported/ambiguous duration retains an explicit reason and no value");
                            for(const auto& value : result.at("metrics"))
                                require(value.at("value").is_null() && value.at("status") == "unavailable",
                                        "unqualified counter association has no numeric result");
                        }
                        for(const auto* identity : {"frame_id", "queue_id", "submission_id", "action_id", "draw_count"})
                            require(result.at("identities").at(identity).is_null(),
                                    "original execution identities and draw membership remain unavailable");
                        require(result.at("metric_semantics").at("isolated_draw") == false,
                                "no isolated draw counter claim");
                        current.push_back(result);
                    }
                    const auto info = call("artifact_info", {{"artifact_id", id}});
                    require(info.at("pinned") == true && info.at("status") == "complete" &&
                                info.at("in_use") == false &&
                                info.at("bytes") == item.at("artifact_info").at("bytes") &&
                                info.at("file_count") == item.at("artifact_info").at("file_count"),
                            "source pin, released leases and unchanged bundle inventory");
                    if(pass == 0)
                        report["cases"].push_back({{"key", item.at("key")}, {"profile_id", id}, {"results", current}});
                    else
                        require(current == report.at("cases")[case_index].at("results"),
                                "identical range results after an offline reader restart");
                    save(run / "report.json", report);
                }
                require(client.finish() == 0, "read-only marker reader exits cleanly");
            }
            report["status"] = "pass";
            save(run / "report.json", report);
        } catch(...) {
            report["status"] = "failed";
            save(run / "report.json", report);
            throw;
        }
    });
}
