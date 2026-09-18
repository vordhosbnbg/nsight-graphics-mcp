#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <span>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
void save(const fs::path& path, const Json& value) {
    std::ofstream output(path);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
}
} // namespace
int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    if(argc != 5) {
        std::cerr << "SERVER ARTIFACT_ROOT CASES_JSON RUN_ROOT required\n";
        return 2;
    }
    try {
        const auto server = fs::canonical(argv[1]), store = fs::canonical(argv[2]);
        const auto cases = Json::parse(ngm::read_regular_file(fs::canonical(argv[3]), 65536));
        require(cases.is_array() && !cases.empty() && cases.size() <= 64, "1..64 explicit retained C++ captures");
        const auto root = fs::absolute(argv[4]);
        fs::create_directories(root);
        auto pattern = (root / "cpp-inspection-XXXXXX").string();
        require(mkdtemp(pattern.data()), "allocate isolated retained C++ validation output");
        const fs::path run(pattern);
        for(const auto* file : {"RetainedCppIntegration.cpp", "McpClient.hpp", "Check.hpp"})
            fs::copy_file(fs::path(__FILE__).parent_path() / file, run / file);
        save(run / "cases.json", cases);
        Json report{{"status", "running"},
                    {"project_version", ngm::project_version()},
                    {"evidence_origin", "retained_cpp_mcp_validation"},
                    {"store", store.string()},
                    {"server_sha256", ngm::sha256_file(server)},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"cases", Json::array()},
                    {"scope", "Retained generated source only; no new capture, GPU replay or resource extraction"}};
        std::ofstream transcript(run / "transcript.ndjson");
        transcript.exceptions(std::ios::badbit | std::ios::failbit);
        std::cout << run << '\n' << std::flush;
        try {
            for(unsigned session = 0; session < 2; ++session) {
                ngm::check::mcp::Client client(server.string(), "/nonexistent", {"--artifact-root", store.string()},
                                               false, {}, {std::chrono::seconds(30), std::chrono::seconds(30)});
                std::int64_t sequence = 1;
                const auto request = [&](const char* method, const Json& params) {
                    transcript << Json{{"session", session},
                                       {"direction", "request"},
                                       {"id", sequence},
                                       {"method", method},
                                       {"params", params}}
                                      .dump()
                               << '\n';
                    auto reply = client.request(sequence++, method, params);
                    transcript << Json{{"session", session}, {"direction", "response"}, {"message", reply}}.dump()
                               << '\n';
                    transcript.flush();
                    require(reply.contains("result") && !reply.at("result").value("isError", false),
                            "MCP request succeeds: " + reply.dump());
                    return reply.at("result");
                };
                const auto call = [&](const char* name, const Json& args) {
                    const auto value = request("tools/call", {{"name", name}, {"arguments", args}});
                    require(value.contains("structuredContent") && value.dump().size() + 66560 < 1024U * 1024U,
                            "bounded structured MCP response");
                    require(Json::parse(value.at("content").at(0).at("text").get<std::string>()) ==
                                value.at("structuredContent"),
                            "text fallback matches structured evidence");
                    return value.at("structuredContent");
                };
                const auto initialized = request(
                    "initialize",
                    {{"protocolVersion", "2025-11-25"},
                     {"capabilities", Json::object()},
                     {"clientInfo", {{"name", "ngm-retained-cpp-integration"}, {"version", ngm::project_version()}}}});
                require(initialized.at("serverInfo").at("version") == ngm::project_version(),
                        "server version matches harness");
                client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
                const auto listing = request("tools/list", Json::object());
                require(listing.at("tools").size() == 20, "twenty implemented tools");
                if(session == 0)
                    save(run / "tools.json", listing);
                for(std::size_t n = 0; n < cases.size(); ++n) {
                    const auto& item = cases[n];
                    const auto id = item.at("capture_id").get<std::string>();
                    require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true,
                            "capture explicitly pinned");
                    const auto page = call("capture_cpp_draws", {{"capture_id", id}, {"limit", 1}});
                    require(page.at("draws_total") == item.at("expected_draws") &&
                                page.at("unsupported_recordings_total") == 0 &&
                                page.at("unsupported_objects_total") == 0 &&
                                page.at("producer").at("cli_tool").at("version") == item.at("tool_version"),
                            "qualified draw count, coverage and producer");
                    if(session == 1) {
                        require(page == report.at("cases").at(n).at("first_page"),
                                "retained first page stable after restart");
                        continue;
                    }
                    Json result{{"input", item},
                                {"first_page", page},
                                {"draws", Json::array()},
                                {"source_excerpts", Json::array()}};
                    Json current = page;
                    do {
                        for(const auto& draw : current.at("draws")) {
                            require(draw.at("association_status") == "resolved_source_relationship" &&
                                        !draw.at("pipeline").at("stages").empty(),
                                    "draw resolves explicit pipeline and shader stages");
                            result["draws"].push_back(draw);
                            const auto& span = draw.at("source");
                            const auto excerpt = call("capture_cpp_source", {{"capture_id", id},
                                                                             {"source_path", span.at("path")},
                                                                             {"start_line", span.at("start_line")},
                                                                             {"max_lines", 2}});
                            const auto source_path = store / "bundles" / id / span.at("path").get<std::string>();
                            require(ngm::sha256_file(source_path) ==
                                        excerpt.at("source").at("sha256").get<std::string>(),
                                    "source hash identifies actual retained bytes");
                            require(excerpt.at("lines").at(0).at("text").get<std::string>().find(
                                        draw.at("function").get<std::string>()) != std::string::npos,
                                    "source location identifies literal draw");
                            result["source_excerpts"].push_back(excerpt);
                        }
                        if(current.at("next_offset").is_null())
                            break;
                        current = call("capture_cpp_draws",
                                       {{"capture_id", id}, {"offset", current.at("next_offset")}, {"limit", 1}});
                    } while(true);
                    require(result.at("draws").size() == item.at("expected_draws"), "complete draw pagination");
                    for(const auto* section : {"unsupported_recordings", "unsupported_objects"}) {
                        const auto coverage = call("capture_cpp_draws", {{"capture_id", id}, {"section", section}});
                        require(coverage.at("total") == 0 && coverage.at(section).empty(),
                                "empty qualified coverage pages");
                    }
                    for(const auto& identity : page.at("source_files")) {
                        const auto path = store / "bundles" / id / identity.at("path").get<std::string>();
                        require(ngm::sha256_file(path) == identity.at("sha256").get<std::string>() &&
                                    fs::file_size(path) == identity.at("bytes"),
                                "all contributing source identities match files");
                    }
                    report["cases"].push_back(std::move(result));
                    save(run / "report.json", report);
                }
                require(client.finish() == 0, "clean EOF shutdown");
                std::ofstream(run / ("server-" + std::to_string(session) + ".stderr.txt")) << client.diagnostics();
            }
            report["status"] = "pass";
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
        }
        save(run / "report.json", report);
        std::cout << report.at("status") << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
