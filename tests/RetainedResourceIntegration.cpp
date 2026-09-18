#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <csignal>
#include <fstream>
#include <iostream>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
using ngm::check::require;
void save(const fs::path& path, const Json& value) {
    std::ofstream file(path);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << value.dump(2) << '\n';
}
std::string decode(const std::string& hex) {
    require(hex.size() % 2 == 0, "even hexadecimal output");
    const std::string digits = "0123456789abcdef";
    std::string bytes;
    for(std::size_t i = 0; i < hex.size(); i += 2) {
        auto a = digits.find(hex[i]), b = digits.find(hex[i + 1]);
        require(a != std::string::npos && b != std::string::npos, "canonical hexadecimal output");
        bytes += static_cast<char>((a << 4) | b);
    }
    return bytes;
}
std::string digest(std::string_view bytes) {
    return ngm::sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
}
} // namespace
int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    if(argc != 7) {
        std::cerr << "SERVER STORE CASES_JSON RUN_ROOT WORKER_2026_3 WORKER_2026_2 required\n";
        return 2;
    }
    return ngm::check::run([&] {
        const auto server = fs::canonical(argv[1]), store = fs::canonical(argv[2]);
        const auto cases = Json::parse(ngm::read_regular_file(fs::canonical(argv[3]), 1024 * 1024));
        require(cases.is_array() && !cases.empty() && cases.size() <= 64, "explicit bounded case list");
        const auto root = fs::absolute(argv[4]);
        fs::create_directories(root);
        auto pattern = (root / "resource-inspection-XXXXXX").string();
        require(mkdtemp(pattern.data()), "isolated run directory");
        const fs::path run(pattern);
        for(const auto* file : {"RetainedResourceIntegration.cpp", "McpClient.hpp", "Check.hpp"})
            fs::copy_file(fs::path(__FILE__).parent_path() / file, run / file);
        save(run / "cases.json", cases);
        Json report{{"status", "running"},
                    {"project_version", ngm::project_version()},
                    {"store", store.string()},
                    {"server_sha256", ngm::sha256_file(server)},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"cases", Json::array()},
                    {"evidence_origin", "retained_resource_mcp_validation"},
                    {"scope", "Retained source references and serialized bytes; no new capture or GPU execution"}};
        report["workers"] = {
            {"2026_3", {{"path", fs::canonical(argv[5]).string()}, {"sha256", ngm::sha256_file(argv[5])}}},
            {"2026_2", {{"path", fs::canonical(argv[6]).string()}, {"sha256", ngm::sha256_file(argv[6])}}}};
        std::ofstream transcript(run / "transcript.ndjson");
        transcript.exceptions(std::ios::failbit | std::ios::badbit);
        std::cout << run << '\n' << std::flush;
        try {
            for(unsigned session = 0; session < 2; ++session) {
                ngm::check::mcp::Client client(server.string(), "/nonexistent",
                                               {"--artifact-root", store.string(), "--artifact-max-bytes", "0",
                                                "--artifact-max-age-seconds", "0", "--resource-worker-2026-3",
                                                fs::canonical(argv[5]).string(), "--resource-worker-2026-2",
                                                fs::canonical(argv[6]).string()},
                                               false, {}, {std::chrono::seconds(30), std::chrono::seconds(30)});
                int sequence = 1;
                const auto request = [&](const char* method, const Json& params, bool expect_error = false) {
                    transcript << Json{{"session", session},
                                       {"direction", "request"},
                                       {"id", sequence},
                                       {"method", method},
                                       {"params", params}}
                                      .dump()
                               << '\n';
                    const auto reply = client.request(sequence++, method, params);
                    transcript << Json{{"session", session}, {"direction", "response"}, {"message", reply}}.dump()
                               << '\n';
                    transcript.flush();
                    require(reply.contains("result") && reply.at("result").value("isError", false) == expect_error,
                            "MCP request succeeds: " + reply.dump());
                    return reply.at("result");
                };
                const auto call = [&](const char* tool, const Json& args) {
                    const auto result = request("tools/call", {{"name", tool}, {"arguments", args}});
                    require(result.dump().size() + 66560 < 1024U * 1024U, "bounded MCP response");
                    require(Json::parse(result.at("content")[0].at("text").get<std::string>()) ==
                                result.at("structuredContent"),
                            "structured/text parity");
                    return result.at("structuredContent");
                };
                const auto init =
                    request("initialize",
                            {{"protocolVersion", "2025-11-25"},
                             {"capabilities", Json::object()},
                             {"clientInfo",
                              {{"name", "ngm-retained-resource-integration"}, {"version", ngm::project_version()}}}});
                require(init.at("serverInfo").at("version") == ngm::project_version(), "server identity");
                client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
                const auto tools = request("tools/list", Json::object());
                require(tools.at("tools").size() == 22, "implemented tool count");
                if(session == 0)
                    save(run / "tools.json", tools);
                for(std::size_t n = 0; n < cases.size(); ++n) {
                    const auto& item = cases[n];
                    const auto id = item.at("capture_id").get<std::string>();
                    require(call("artifact_info", {{"artifact_id", id}}).at("pinned") == true, "source capture pinned");
                    if(item.contains("expected_error")) {
                        const auto failure =
                            request("tools/call",
                                    {{"name", "capture_cpp_resources"}, {"arguments", {{"capture_id", id}}}}, true);
                        require(failure.at("content")[0].at("text").get<std::string>().starts_with(
                                    item.at("expected_error").get<std::string>() + ":"),
                                "expected producer-boundary refusal");
                        if(session == 0)
                            report["cases"].push_back(
                                {{"input", item}, {"refusal", failure}, {"reads", Json::array()}});
                        save(run / "report.json", report);
                        continue;
                    }
                    const auto first = call("capture_cpp_resources", {{"capture_id", id}, {"limit", 7}});
                    const auto profile = item.at("profile").get<std::string>();
                    require(profile == "2026_3" || profile == "2026_2", "known oracle producer profile");
                    const auto version = profile == "2026_3" ? "2026.3.1.0" : "2026.2.0.0";
                    const auto build = profile == "2026_3" ? "38722833" : "37991608";
                    const auto helper = profile == "2026_3" ? "nsight-2026.3.1-build-38722833-linux-x86_64"
                                                            : "nsight-2026.2.0-build-37991608-linux-x86_64";
                    for(const auto* tool : {"capture_tool", "replay_tool", "cli_tool"})
                        require(first.at("producer").at(tool).at("version") == version &&
                                    first.at("producer").at(tool).at("build") == build,
                                "exact oracle producer tuple");
                    const auto index = Json::parse(
                        ngm::read_regular_file(store / "bundles" / id / "derived/cpp-project.json", 1024 * 1024));
                    const auto source_project =
                        store / "bundles" / id / index.at("project_directory").get<std::string>();
                    Json source_inputs = Json::object();
                    for(const auto* file : {"data.bin", "data.bin.rec"})
                        source_inputs[file] = {{"bytes", fs::file_size(source_project / file)},
                                               {"sha256", ngm::sha256_file(source_project / file)}};
                    if(session == 1) {
                        require(first == report.at("cases")[n].at("first_page"),
                                "resource IDs/listing stable after restart");
                        for(const auto& read : report.at("cases")[n].at("reads")) {
                            const auto artifact = read.at("artifact_id");
                            require(call("artifact_info", {{"artifact_id", artifact}}).at("pinned") == true,
                                    "read pin survives restart");
                            const auto retained = Json::parse(ngm::read_regular_file(
                                store / "bundles" / artifact.get<std::string>() / "raw/resource-read.json",
                                1024 * 1024));
                            require(retained.at("result").at("data") == read.at("data"), "retained exact read result");
                        }
                        continue;
                    }
                    Json result{
                        {"input", item}, {"first_page", first}, {"resources", Json::array()}, {"reads", Json::array()}};
                    auto page = first;
                    for(;;) {
                        for(const auto& row : page.at("resources"))
                            result["resources"].push_back(row);
                        if(page.at("next_offset").is_null())
                            break;
                        page = call("capture_cpp_resources",
                                    {{"capture_id", id}, {"offset", page.at("next_offset")}, {"limit", 7}});
                    }
                    require(result.at("resources").size() == first.at("resources_total"),
                            "complete listing pagination");
                    result["unsupported"] = Json::array();
                    std::size_t coverage_offset = 0;
                    for(;;) {
                        const auto coverage = call("capture_cpp_resources", {{"capture_id", id},
                                                                             {"section", "unsupported"},
                                                                             {"offset", coverage_offset},
                                                                             {"limit", 7}});
                        for(const auto& row : coverage.at("unsupported"))
                            result["unsupported"].push_back(row);
                        if(coverage.at("next_offset").is_null())
                            break;
                        coverage_offset = coverage.at("next_offset").get<std::size_t>();
                    }
                    require(result.at("unsupported").size() == first.at("unsupported_total"),
                            "complete unsupported pagination");
                    for(const auto& identity : first.at("source_files")) {
                        const auto path = store / "bundles" / id / identity.at("path").get<std::string>();
                        require(ngm::sha256_file(path) == identity.at("sha256").get<std::string>(),
                                "listed source hash");
                    }
                    for(const auto& oracle : item.at("resources")) {
                        const auto& rows = result.at("resources");
                        const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) {
                            return row.at("handle") == oracle.at("handle") && row.at("readable") == true;
                        });
                        require(found != rows.end(), "oracle handle has readable source reference");
                        const auto total = oracle.at("bytes").get<std::size_t>();
                        require(total > 0 && total <= 131072, "bounded qualification oracle");
                        const auto read = [&](std::size_t offset, std::size_t length) {
                            auto value = call("capture_cpp_resource", {{"capture_id", id},
                                                                       {"resource_ref", found->at("resource_ref")},
                                                                       {"offset", offset},
                                                                       {"length", length},
                                                                       {"pin", true}});
                            const auto bytes = decode(value.at("data").template get<std::string>());
                            require(value.at("total_bytes") == total && value.at("returned_bytes") == bytes.size() &&
                                        digest(bytes) == value.at("sha256").template get<std::string>(),
                                    "response bytes/hash");
                            require(value.at("reference") == *found, "read bound to listed reference");
                            require(value.at("inputs") == source_inputs, "exact original source database identities");
                            require(value.at("producer") == first.at("producer") &&
                                        value.at("helper_profile") == helper &&
                                        value.at("worker_sha256") == report.at("workers").at(profile).at("sha256"),
                                    "exact read producer, helper and configured executable");
                            const auto bundle = store / "bundles" / value.at("artifact_id").template get<std::string>();
                            for(const auto* file : {"data.bin", "data.bin.rec"})
                                require(ngm::sha256_file(bundle / "raw/input" / file) ==
                                            value.at("inputs").at(file).at("sha256").template get<std::string>(),
                                        "retained snapshot hash");
                            result["reads"].push_back(value);
                            return bytes;
                        };
                        auto whole = read(0, std::min<std::size_t>(65536, total));
                        if(total > 65536)
                            whole += read(65536, total - 65536);
                        if(oracle.contains("sha256"))
                            require(digest(whole) == oracle.at("sha256").get<std::string>(),
                                    "independent resource byte oracle");
                        else
                            require(oracle.at("validation") == "range_consistency", "explicit non-oracle scope");
                        const auto half = std::max<std::size_t>(1, total / 2);
                        const auto first_half = read(0, half),
                                   second_half = read(half, std::max<std::size_t>(1, total - half));
                        require(first_half + second_half == whole, "paged reconstruction");
                        require(read(total, 1).empty(), "end-offset empty result");
                    }
                    report["cases"].push_back(std::move(result));
                    save(run / "report.json", report);
                }
                require(client.finish() == 0, "normal server shutdown");
                std::ofstream(run / ("stderr-" + std::to_string(session) + ".log")) << client.diagnostics();
            }
            report["status"] = "pass";
            save(run / "report.json", report);
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
            save(run / "report.json", report);
            throw;
        }
    });
}
