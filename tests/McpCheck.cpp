#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/Version.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using ngm::check::mcp::Client;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

struct Scratch {
    std::filesystem::path path;
    Scratch() {
        auto pattern = (std::filesystem::temp_directory_path() / "ngm-mcp-XXXXXX").string();
        char* result = mkdtemp(pattern.data());
        require(result != nullptr, "create isolated MCP check directory");
        path = result;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

Json initialize(Client& client, std::string_view protocol = "2025-11-25") {
    const auto response = client.request(1, "initialize",
                                         {{"protocolVersion", protocol},
                                          {"capabilities", Json::object()},
                                          {"clientInfo", {{"name", "ngm-mcp-check"}, {"version", "1"}}}});
    require(response.contains("result"), "initialize succeeds");
    const auto& result = response["result"];
    require(result["serverInfo"]["name"] == "nsight-graphics-mcp", "MCP server name");
    require(result["serverInfo"]["version"] == ngm::project_version(), "identity matches authoritative version");
    require(result["capabilities"] == Json{{"tools", Json::object()}}, "advertise only implemented MCP features");
    client.send({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    return result;
}

void expect_error(const Json& response, int code) {
    require(response.contains("error") && response["error"]["code"] == code, "structured error code");
    require(response["error"]["message"].is_string() && !response["error"]["message"].get<std::string>().empty(),
            "actionable error message");
}

void expect_tool_error(const Json& response, std::string_view expected = {}) {
    require(response.contains("result") && !response.contains("error"), "tool input error is a tool result");
    const auto& result = response["result"];
    require(result["isError"] == true, "invalid tool input sets isError");
    require(result["content"].size() == 1 && result["content"][0]["type"] == "text", "tool error text content");
    const auto message = result["content"][0]["text"].get<std::string>();
    require(!message.empty() && message.find(expected) != std::string::npos, "actionable tool error: " + message);
    require(!result.contains("structuredContent"), "input error does not masquerade as a capabilities report");
}

Json query_capabilities(Client& client) {
    const auto response = client.request(3, "tools/call", {{"name", "capabilities"}, {"arguments", Json::object()}});
    require(response.contains("result"), "capability query succeeds");
    const auto& result = response["result"];
    require(result.contains("structuredContent") && result["structuredContent"].is_object(), "structured result");
    require(result["content"].size() == 1 && result["content"][0]["type"] == "text", "text fallback");
    const auto report = result["structuredContent"];
    require(Json::parse(result["content"][0]["text"].get<std::string>()) == report, "text and structured result agree");
    require(report["server"]["version"] == ngm::project_version(), "capabilities use project version");
    require(report["implemented_tools"] ==
                Json::array({"capabilities", "capture", "job_status", "job_cancel", "artifact_list", "artifact_info",
                             "artifact_files", "artifact_read", "artifact_pin", "artifact_usage", "artifact_prune",
                             "artifact_import", "capture_metadata", "capture_events", "capture_objects"}),
            "exactly the implemented tools advertised");
    for(const auto* name : {"profiling", "fixture_via_mcp"}) {
        const auto& operation = report["operations"][name];
        require(operation["available"] == false && operation["status"] == "not_implemented",
                "pending typed integrations remain unavailable");
    }
    const auto storage = report["prerequisites"]["artifact_store"]["configured"].get<bool>();
    require(report["operations"]["inspection"]["available"] == storage &&
                report["operations"]["inspection"]["status"] ==
                    (storage ? "retained_exports_only" : "missing_prerequisites"),
            "inventory inspection requires retained storage, not live tool or desktop observations");
    require(report["prerequisites"]["nsight"]["compatibility"] == "not_verified", "paths do not prove support");
    require(report["prerequisites"]["nsight"]["version"].is_null(), "no invented Nsight version");
    require(report["prerequisites"]["gpu"]["status"] == "not_probed", "no GPU claim from filenames");
    return report;
}

void protocol_check(const std::string& server, const Scratch& scratch) {
    Client client(server, scratch.path);
    expect_error(client.request(0, "tools/list"), -32002);
    require(initialize(client)["protocolVersion"] == "2025-11-25", "current protocol negotiation");
    const auto listing = client.request(2, "tools/list")["result"]["tools"];
    require(listing.size() == 15, "discover exactly the implemented tools");
    for(const auto& tool : listing) {
        require(tool["inputSchema"]["additionalProperties"] == false, "closed input schema advertised");
        require(tool.contains("outputSchema"), "structured output schema advertised");
        if(tool["name"] == "capabilities") {
            require(tool["annotations"]["readOnlyHint"] == true, "read-only capability annotation");
        }
    }
    auto report = query_capabilities(client);
    require(report["server"]["protocol_version"] == "2025-11-25", "capabilities report actual negotiation");
    for(const auto& executable : report["prerequisites"]["nsight"]["executables"]) {
        require(executable["executable_found"] == false && executable["path"].is_null(), "missing tools explicit");
    }
    require(report["prerequisites"]["desktop"]["display_environment_present"] == false, "missing desktop hint");
    require(client.request(4, "tools/call", {{"name", "capabilities"}}).contains("result"), "arguments optional");
    for(const auto& arguments : {Json(nullptr), Json("bad"), Json::array(), Json(7)}) {
        expect_error(client.request(5, "tools/call", {{"name", "capabilities"}, {"arguments", arguments}}), -32602);
    }
    expect_tool_error(
        client.request(5, "tools/call", {{"name", "capabilities"}, {"arguments", {{"unexpected", true}}}}));
    expect_error(client.request(6, "tools/call", {{"name", "missing"}}), -32602);
    expect_error(client.request(7, "tools/call", {{"name", 7}}), -32602);
    expect_error(client.request(8, "tools/call", {{"name", "capabilities"}, {"extra", true}}), -32602);
    expect_error(client.request(9, "tools/call", Json::array()), -32602);
    expect_error(client.request(10, "unimplemented/method"), -32601);
    expect_error(client.request(10, "capabilities"), -32601);
    expect_error(client.request(11, "initialize", {{"protocolVersion", 7}}), -32602);

    client.send({{"jsonrpc", "2.0"}, {"method", "notifications/unknown"}});
    client.send({{"jsonrpc", "2.0"}, {"method", "tools/call"}, {"params", Json::array()}});
    require(client.request(12, "ping")["result"].is_object(), "notifications have no response");

    client.write("{malformed JSON\n");
    auto error = client.response();
    expect_error(error, -32700);
    require(error["id"].is_null(), "parse error null id");
    for(const auto& request : {Json::array(), Json(9), Json{{"jsonrpc", "2.0"}, {"id", 1}},
                               Json{{"jsonrpc", "1.0"}, {"id", 2}, {"method", "ping"}},
                               Json{{"jsonrpc", "2.0"}, {"id", true}, {"method", "ping"}},
                               Json{{"jsonrpc", "2.0"}, {"id", nullptr}, {"method", "ping"}},
                               Json{{"jsonrpc", "2.0"}, {"id", 1.5}, {"method", "ping"}}}) {
        client.send(request);
        expect_error(client.response(), -32600);
    }
    client.write(std::string(65537, ' ') + '\n');
    expect_error(client.response(), -32600);
    require(client.request(13, "ping").contains("result"), "recover after malformed/oversized requests");
    client.send({{"jsonrpc", "2.0"}, {"id", "string-id"}, {"method", "ping"}});
    require(client.response()["id"] == "string-id", "string request ids");
    client.write(R"({"jsonrpc":"2.0","id":14,"method":"ping"})");
    client.close_input();
    require(client.response()["id"] == 14, "complete final request processed without newline at EOF");
    require(client.finish() == 0, "normal EOF exits zero");
    require(client.diagnostics().find("negotiated MCP 2025-11-25") != std::string::npos, "negotiation on stderr");
    require(client.diagnostics().find("stdio closed") != std::string::npos, "shutdown on stderr");
}

void malformed_input_check(const std::string& server, const Scratch& scratch) {
    Client client(server, scratch.path);
    // A NUL after a valid initialize previously let the valid prefix execute.
    // Prove rejection happens before mutation by checking that tools remain
    // unavailable and a subsequent valid initialize still succeeds.
    std::string invalid_initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":)"
                                     R"({"protocolVersion":"2025-11-25","capabilities":{},)"
                                     R"("clientInfo":{"name":"nul-check","version":"1"}}})";
    invalid_initialize += '\0';
    invalid_initialize += "trailing non-JSON bytes\n";
    client.write(invalid_initialize);
    auto response = client.response();
    expect_error(response, -32700);
    require(response["id"].is_null(), "raw NUL rejected before interpreting a request id");
    expect_error(client.request(2, "tools/list"), -32002);
    initialize(client);

    const std::string prefix = R"({"jsonrpc":"2.0","id":3,"method":"ping","params":{"ignored":)";
    // Root and params account for two container levels. The complete request
    // stays below the line-size limit, including the crash regression at 16k.
    client.write(prefix + std::string(62, '[') + '0' + std::string(62, ']') + "}}\n");
    require(client.response()["result"].is_object(), "64-container request accepted");
    for(const auto depth : {63, 16384}) {
        client.write(prefix + std::string(depth, '[') + '0' + std::string(depth, ']') + "}}\n");
        response = client.response();
        expect_error(response, -32600);
        require(response["id"].is_null(), "overdeep request rejected during parsing");
        require(client.request(4, "ping").contains("result"), "healthy request after rejected nested arrays");
    }
    std::string objects = prefix;
    for(int depth = 0; depth < 500; ++depth) {
        objects += R"({"child":)";
    }
    client.write(objects + '0' + std::string(500, '}') + "}}\n");
    expect_error(client.response(), -32600);
    require(client.request(5, "ping").contains("result"), "healthy request after rejected nested objects");

    std::string invalid_call = R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"capabilities"}})";
    invalid_call += '\0';
    invalid_call += "\n";
    client.write(invalid_call);
    expect_error(client.response(), -32700);
    query_capabilities(client);
    require(client.finish() == 0, "malformed input does not corrupt EOF shutdown");
}

void discovery_check(const std::string& server, const Scratch& scratch) {
    const auto root = scratch.path / "Nsight installation with spaces";
    const auto tools = root / "host/linux-desktop-nomad-x64";
    std::filesystem::create_directories(tools);
    for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
        const auto path = tools / name;
        std::ofstream(path) << "Only a discovery fixture; must never be executed.\n";
        std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    }
    // A non-executable file is not an executable observation.
    std::filesystem::permissions(tools / "ngfx-replay", std::filesystem::perms::owner_read);
    const auto unopened = scratch.path / "discovery-must-not-create-this";
    Client explicit_root(server, scratch.path, {"--nsight-root", root.string(), "--artifact-root", unopened.string()},
                         true);
    initialize(explicit_root);
    const auto report = query_capabilities(explicit_root);
    require(report["prerequisites"]["nsight"]["discovery_source"] == "explicit_root", "explicit installation");
    const auto executables = report["prerequisites"]["nsight"]["executables"];
    require(executables[0]["path"] == (tools / "ngfx").string(), "explicit executable path including spaces");
    require(executables[1]["executable_found"] == true && executables[2]["executable_found"] == false,
            "discovery distinguishes executable and non-executable files");
    require(report["prerequisites"]["desktop"]["display_environment_present"] == true &&
                report["prerequisites"]["desktop"]["connection"] == "not_verified",
            "display variable is not a working connection claim");
    require(report["prerequisites"]["artifact_store"]["configured"] == true, "configured artifact root observed");
    require(report["operations"]["capture"]["available"] == false, "missing replay blocks capture readiness");
    require(explicit_root.finish() == 0, "explicit-root server EOF");
    require(!std::filesystem::exists(unopened), "discovery-only session never creates the artifact root");

    const auto search = scratch.path / "search-bin";
    std::filesystem::create_directory(search);
    std::filesystem::create_symlink(tools / "ngfx", search / "ngfx");
    Client path_discovery(server, search);
    require(initialize(path_discovery, "2025-06-18")["protocolVersion"] == "2025-06-18", "older protocol negotiation");
    const auto older_report = query_capabilities(path_discovery);
    require(older_report["server"]["protocol_version"] == "2025-06-18", "older connection reports its negotiation");
    const auto adjacent = older_report["prerequisites"]["nsight"]["executables"];
    require(adjacent[1]["path"] == (tools / "ngfx-capture").string(), "find adjacent tools through ngfx symlink");
    expect_tool_error(
        path_discovery.request(4, "tools/call", {{"name", "capabilities"}, {"arguments", {{"unexpected", true}}}}));
    expect_error(path_discovery.request(5, "tools/call", {{"name", "capabilities"}, {"arguments", Json::array()}}),
                 -32602);
    require(path_discovery.finish() == 0, "PATH server EOF");

    Client missing_override(server, search, {"--nsight-root", search.string()});
    initialize(missing_override);
    const auto selected = query_capabilities(missing_override)["prerequisites"]["nsight"]["executables"];
    require(selected[1]["executable_found"] == false, "explicit root does not mix PATH/adjacent installations");
    require(missing_override.finish() == 0, "incomplete installation still permits capability query");
    Client bad_root(server, scratch.path, {"--nsight-root", "relative/path"});
    require(bad_root.finish() == 2, "relative root rejected");
    require(bad_root.diagnostics().find("absolute directory") != std::string::npos, "root error on stderr");
}

Json call(Client& client, const char* name, Json arguments = Json::object()) {
    return client.request(42, "tools/call", {{"name", name}, {"arguments", std::move(arguments)}});
}
Json successful_call(Client& client, const char* name, Json arguments = Json::object()) {
    const auto response = call(client, name, std::move(arguments));
    if(std::string_view(name).starts_with("capture_")) {
        require(response.dump().size() <= 1024U * 1024U,
                "inspection protocol reply stays within 1 MiB including fallback");
    }
    require(response.contains("result") && !response["result"].value("isError", false),
            std::string(name) + " succeeds: " + response.dump());
    const auto& result = response["result"];
    require(result.contains("structuredContent") && result["structuredContent"].is_object(),
            "structured workflow result");
    require(Json::parse(result["content"][0]["text"].get<std::string>()) == result["structuredContent"],
            "workflow text and structured results agree");
    return result["structuredContent"];
}
Json await_job(Client& client, const std::string& id) {
    const auto deadline = Clock::now() + 10s;
    while(Clock::now() < deadline) {
        auto result = successful_call(client, "job_status", {{"job_id", id}});
        const auto state = result.at("state").get<std::string>();
        if(state != "running" && state != "queued" && result.at("worker_running") == false &&
           result.at("finalization_pending") == false) {
            return result;
        }
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("job did not terminate before test deadline");
}
void await_record(const std::filesystem::path& path) {
    const auto deadline = Clock::now() + 5s;
    while(Clock::now() < deadline) {
        std::error_code error;
        if(std::filesystem::is_regular_file(path, error) && std::filesystem::file_size(path, error) > 0 && !error) {
            return;
        }
        std::this_thread::sleep_for(5ms);
    }
    throw std::runtime_error("capture target did not publish its PID before test deadline");
}
pid_t recorded_pid(const std::filesystem::path& path) {
    pid_t result = 0;
    std::ifstream(path) >> result;
    require(result > 0, "target PID recorded");
    return result;
}
void require_gone(pid_t process) {
    require(kill(process, 0) < 0 && errno == ESRCH, "owned target process is gone");
}

void invalid_workflow_check(const std::string& server, const Scratch& scratch) {
    const auto root = scratch.path / "invalid-inputs-must-not-create-this";
    Client client(server, scratch.path, {"--artifact-root", root.string()});
    initialize(client);
    const std::string id = "bundle-" + std::string(32, '0');
    for(const auto* name :
        {"capture", "job_status", "job_cancel", "artifact_info", "artifact_files", "artifact_read", "artifact_pin",
         "artifact_import", "capture_metadata", "capture_events", "capture_objects"}) {
        expect_tool_error(call(client, name));
    }
    for(const auto* name : {"capture", "job_status", "job_cancel", "artifact_list", "artifact_info", "artifact_files",
                            "artifact_read", "artifact_pin", "artifact_usage", "artifact_prune", "artifact_import",
                            "capture_metadata", "capture_events", "capture_objects"}) {
        expect_tool_error(call(client, name, {{"unknown", true}}));
    }
    for(const auto& invalid : {Json(-1), Json(0), Json(101), Json(1.5), Json("10"), Json(true)}) {
        expect_tool_error(call(client, "artifact_list", {{"limit", invalid}}));
    }
    expect_tool_error(call(client, "job_status", {{"job_id", "../job-x"}}));
    expect_tool_error(call(client, "artifact_info", {{"artifact_id", "not-a-bundle"}}));
    expect_tool_error(call(client, "artifact_pin", {{"artifact_id", id}, {"pinned", 1}}));
    expect_tool_error(call(client, "artifact_files", {{"artifact_id", id}, {"offset", 4097}}));
    for(const auto* tool : {"capture_metadata", "capture_events", "capture_objects"}) {
        expect_tool_error(call(client, tool, {{"capture_id", "invalid"}}));
    }
    for(const auto& invalid : {Json(0), Json(101), Json(-1), Json(1.5), Json("1")}) {
        expect_tool_error(call(client, "capture_events", {{"capture_id", id}, {"limit", invalid}}));
    }
    expect_tool_error(call(client, "capture_objects", {{"capture_id", id}, {"offset", 100001}}));
    for(const auto* path : {"../outside", "/absolute", "raw/../outside", "raw//file", "raw/./file", "manifest.json"}) {
        expect_tool_error(call(client, "artifact_read", {{"artifact_id", id}, {"path", path}}));
    }
    expect_tool_error(call(client, "artifact_read", {{"artifact_id", id}, {"path", "raw/a"}, {"max_bytes", 65537}}));
    expect_tool_error(call(client, "artifact_read", {{"artifact_id", id}, {"path", std::string("raw/a\0b", 7)}}));
    expect_tool_error(call(client, "artifact_import", {{"source", "relative/path"}}));
    expect_tool_error(
        call(client, "artifact_import", {{"source", scratch.path.string()}, {"required_outputs", {"../file"}}}));
    const Json valid_capture{{"executable", server}, {"working_directory", scratch.path.string()}};
    const Json invalid_capture_fields{{"capture_frame", 1},        {"delimiter", "automatic"},
                                      {"timeout_ms", 600001},      {"pin", 1},
                                      {"arguments", "wrong-type"}, {"application_provenance", Json::object()}};
    for(const auto& [name, value] : invalid_capture_fields.items()) {
        auto arguments = valid_capture;
        arguments[name] = value;
        expect_tool_error(call(client, "capture", arguments));
    }
    for(const auto& argv :
        {Json::array({7}), Json::array({std::string("a\0b", 3)}), Json::array({std::string(4097, 'x')})}) {
        auto arguments = valid_capture;
        arguments["arguments"] = argv;
        expect_tool_error(call(client, "capture", arguments));
    }
    auto bad_option = valid_capture;
    bad_option["application_output_option"] = "--two options";
    expect_tool_error(call(client, "capture", bad_option));
    expect_tool_error(
        call(client, "capture", {{"executable", "relative"}, {"working_directory", scratch.path.string()}}));
    expect_tool_error(
        call(client, "capture",
             {{"executable", "/nonexistent-ngm-executable"}, {"working_directory", scratch.path.string()}}));
    require(client.finish() == 0, "invalid workflow calls do not damage the protocol");
    require(!std::filesystem::exists(root), "invalid inputs are checked before lazy storage initialization");

    Client disabled(server, scratch.path);
    initialize(disabled);
    expect_tool_error(call(disabled, "artifact_usage"), "--artifact-root");
    expect_tool_error(call(disabled, "capture", valid_capture), "--artifact-root");
    expect_tool_error(call(disabled, "capture_metadata", {{"capture_id", id}}), "--artifact-root");
    require(disabled.finish() == 0, "unconfigured workflow errors preserve EOF");

    for(const auto& arguments : std::vector<std::vector<std::string>>{
            {"--artifact-root", "relative"},
            {"--artifact-root", "/"},
            {"--artifact-root"},
            {"--artifact-max-bytes", "1"},
            {"--artifact-root", root.string(), "--artifact-max-bytes", "-1"},
            {"--artifact-root", root.string(), "--artifact-max-age-seconds", "18446744073709551616"},
            {"--artifact-root", root.string(), "--artifact-max-bytes", "3x"},
            {"--artifact-root", root.string(), "--artifact-root", root.string()}}) {
        Client invalid(server, scratch.path, arguments);
        require(invalid.finish() == 2 && !invalid.diagnostics().empty(), "invalid CLI configuration fails on stderr");
    }
    require(!std::filesystem::exists(root), "invalid startup arguments do not create storage");
}

Json snapshot_tree(const std::filesystem::path& root) {
    Json result = Json::object();
    result["."] = {{"kind", "directory"}, {"mtime", std::filesystem::last_write_time(root).time_since_epoch().count()}};
    for(const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        const auto name = entry.path().lexically_relative(root).generic_string();
        const auto status = entry.symlink_status();
        if(std::filesystem::is_symlink(status)) {
            result[name] = {{"kind", "symlink"}, {"target", std::filesystem::read_symlink(entry.path()).string()}};
        } else if(std::filesystem::is_directory(status)) {
            result[name] = {{"kind", "directory"}, {"mtime", entry.last_write_time().time_since_epoch().count()}};
        } else {
            require(std::filesystem::is_regular_file(status),
                    "import regression fixture contains only ordinary entries");
            std::ifstream stream(entry.path());
            const std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
            result[name] = {{"kind", "file"},
                            {"contents", contents},
                            {"mtime", entry.last_write_time().time_since_epoch().count()}};
        }
    }
    return result;
}

void import_overlap_check(const std::string& server, const Scratch& scratch) {
    const auto group = scratch.path / "import-overlap";
    const auto source = group / "source";
    const auto nested = source / "nested";
    std::filesystem::create_directories(nested);
    std::filesystem::create_directory(group / "empty");
    std::ofstream(source / "evidence.txt") << "original source bytes\n";
    std::ofstream(nested / "evidence.txt") << "original nested source bytes\n";
    const auto source_alias = group / "source-alias";
    const auto store_alias = group / "store-alias";
    std::filesystem::create_directory_symlink(source, source_alias);
    std::filesystem::create_directory_symlink(source, store_alias);
    const auto before = snapshot_tree(group);
    const std::vector<std::pair<std::filesystem::path, std::filesystem::path>> overlaps{
        {source / "new-parent/new-store", source},
        {group / "empty", group / "empty"},
        {source, source},
        {source, nested},
        {source / "new-store", source_alias},
        {store_alias / "new-store", source},
        {store_alias / "new-store", source_alias},
        {store_alias, nested},
        {source / "new-store", source / "nested/.."}};
    for(const auto& [root, input] : overlaps) {
        Client client(server, scratch.path, {"--artifact-root", root.string()});
        initialize(client);
        expect_tool_error(call(client, "artifact_import", {{"source", input.string()}}),
                          "must not contain or reside within the artifact store");
        require(client.finish() == 0, "rejected overlapping import shuts down normally");
        require(snapshot_tree(group) == before,
                "rejected import cannot create store directories or change source data/metadata");
    }

    // Component containment must not mistake a shared filename prefix for an
    // ancestor. This valid import still leaves the complete source unchanged.
    const auto source_before = snapshot_tree(source);
    Client disjoint(server, scratch.path, {"--artifact-root", (group / "source-managed").string()});
    initialize(disjoint);
    const auto result = successful_call(disjoint, "artifact_import", {{"source", source.string()}});
    require(result["status"] == "complete", "common filename prefix is not directory containment");
    require(disjoint.finish() == 0, "disjoint import shuts down normally");
    require(snapshot_tree(source) == source_before, "successful disjoint import preserves source tree");
}

void bounded_job_error_check(const std::string& server, const std::string& target, const Scratch& scratch) {
    auto root = scratch.path / "long-root";
    while(root.string().size() + 241 < 4000) {
        root /= std::string(240, 'd');
    }
    root /= std::string(4000 - root.string().size() - 1, 'd');
    require(root.string().size() == 4000, "long-path regression stays inside the accepted root limit");
    Client client(
        server, scratch.path,
        {"--artifact-root", root.string(), "--nsight-root", (scratch.path / "standin-installation").string()});
    initialize(client);
    const auto submission =
        successful_call(client, "capture", {{"executable", target}, {"working_directory", scratch.path.string()}});
    const auto result = await_job(client, submission["identity"]["job_id"]);
    require(result["state"] == "failed" && result["cleanup_confirmed"] == true,
            "discovery path limit fails without leaving owned processes");
    const auto error = result["error"].get<std::string>();
    require(!error.empty() && error.size() <= 4096, "long job diagnostic obeys its advertised output bound");
    const auto info = successful_call(client, "artifact_info", {{"artifact_id", submission["artifact_id"]}});
    require(info["status"] == "failed" && info["quarantined"] == false,
            "cleanup-confirmed long-path failure publishes ordinary failed evidence");
    require(successful_call(client, "artifact_pin",
                            {{"artifact_id", submission["artifact_id"]}, {"pinned", false}})["pinned"] == false,
            "cleanup-confirmed long-path evidence can be unpinned");
    require(client.finish() == 0, "long-path failure does not break MCP shutdown");
}

void workflow_check(const std::string& server, const std::string& standin, const std::string& target,
                    const Scratch& scratch) {
    const auto installation = scratch.path / "standin-installation";
    std::filesystem::create_directories(installation);
    for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
        std::filesystem::copy_file(standin, installation / name);
        std::filesystem::permissions(installation / name, std::filesystem::perms::owner_all);
    }
    const auto root = scratch.path / "managed";
    const std::vector<std::string> options{"--nsight-root",
                                           installation.string(),
                                           "--artifact-root",
                                           root.string(),
                                           "--artifact-max-bytes",
                                           "0",
                                           "--artifact-max-age-seconds",
                                           "0"};
    Client client(server, scratch.path, options, true, {"XDG_DATA_DIRS=/opt/ngm-test-driver/share:/usr/share"});
    initialize(client);
    const auto capabilities = query_capabilities(client);
    require(capabilities["operations"]["capture"]["available"] == true &&
                capabilities["operations"]["capture"]["status"] == "prerequisites_observed",
            "observed prerequisites enable submission without claiming verified compatibility");
    require(!std::filesystem::exists(root), "handshake and capabilities do not initialize storage");

    const auto capture_arguments = [&](const std::filesystem::path& record, bool wait = false) {
        auto argv = Json::array({"--mcp-target", "--record", record.string()});
        if(wait) {
            argv.push_back("--wait");
        }
        return Json{{"executable", target},
                    {"arguments", argv},
                    {"working_directory", scratch.path.string()},
                    {"timeout_ms", 10000},
                    {"capture_frame", 2}};
    };
    const auto record1 = scratch.path / "target1.pid";
    auto first_arguments = capture_arguments(record1);
    first_arguments["pin"] = true;
    const auto first = successful_call(client, "capture", first_arguments);
    const auto first_job = first["identity"]["job_id"].get<std::string>();
    const auto first_id = first["artifact_id"].get<std::string>();
    require(first["identity"]["capture_id"] == first_id, "submission scopes job identity to its artifact");
    auto result = await_job(client, first_job);
    require(result["state"] == "succeeded" && result["cleanup_confirmed"] == true,
            "stand-in capture and replay finish with confirmed cleanup: " + result.dump());
    require_gone(recorded_pid(record1));
    require(successful_call(client, "job_cancel", {{"job_id", first_job}})["result"] == "already_terminal",
            "terminal cancellation is idempotent");
    const auto first_info = successful_call(client, "artifact_info", {{"artifact_id", first_id}});
    require(first_info["status"] == "complete" && first_info["pinned"] == true, "completed capture explicitly pinned");
    const auto report_text =
        successful_call(client, "artifact_read", {{"artifact_id", first_id}, {"path", "raw/report.json"}});
    const auto report = Json::parse(report_text["text"].get<std::string>());
    require(report["environment"]["XDG_DATA_DIRS"] == "/opt/ngm-test-driver/share:/usr/share",
            "MCP forwards the caller's explicit system data search path to capture");
    require(report["evidence_origin"] == "nsight_capture" && report["backend"] == "documented_nsight_cli",
            "capture report labels its evidence origin");
    require(report["caller_provided_application_provenance"].empty(), "MCP does not inject expected diagnoses");
    const auto metadata = successful_call(client, "capture_metadata", {{"capture_id", first_id}});
    require(metadata["capture_id"] == first_id && metadata["metadata"]["metadata_version"] == 1 &&
                metadata["producer"]["capture_tool"]["version"] == "2026.3.1.0" &&
                metadata["producer"]["metadata_nsight_version"] == "2026.3.1" &&
                metadata["unavailable_from_these_exports"].size() == 8,
            "typed inspection preserves capture scope, observed producer strings, and state limits");
    require(metadata.dump().find("synthetic-private") == std::string::npos &&
                !metadata["metadata"].contains("process_environment") &&
                !metadata["metadata"].contains("process_command_line"),
            "ordinary inspection omits process environment and command line");
    const auto events = successful_call(client, "capture_events", {{"capture_id", first_id}, {"limit", 1}});
    require(events["events"].size() == 1 && events["total"] == 3 && events["next_offset"] == 1 &&
                events["events"][0]["event_index"] == 10,
            "event inventory uses export-order offsets rather than noncontiguous IDs");
    const auto later_events = successful_call(client, "capture_events", {{"capture_id", first_id}, {"offset", 1}});
    require(later_events["events"].size() == 2 && later_events["next_offset"].is_null() &&
                later_events["events"][0]["sequence_id"].is_null(),
            "event pagination and optional IDs are explicit");
    const auto objects = successful_call(client, "capture_objects", {{"capture_id", first_id}});
    require(objects["objects"].size() == 1 && objects["objects"][0]["uid"] == 7 &&
                objects["objects"][0]["access_flags"] == 32 && objects["next_offset"].is_null(),
            "capture-scoped object inventory retains opaque access values");
    const auto capture_stdout = report["capture"]["stdout"].get<std::string>();
    require(successful_call(client, "artifact_read", {{"artifact_id", first_id}, {"path", capture_stdout}})["text"]
                    .get<std::string>()
                    .find("mcp target stdout") != std::string::npos,
            "target stdout is retained as an artifact log");
    const auto files = successful_call(client, "artifact_files", {{"artifact_id", first_id}, {"limit", 1}});
    require(files["files"].size() == 1 && files["next_offset"] == 1, "file inventory pagination");
    require(!successful_call(client, "artifact_files", {{"artifact_id", first_id}, {"offset", 1}})["files"].empty(),
            "file inventory subsequent page");

    const auto record2 = scratch.path / "target2.pid";
    auto sdk_arguments = capture_arguments(record2);
    sdk_arguments["delimiter"] = "graphics_capture_api";
    const auto second = successful_call(client, "capture", sdk_arguments);
    require(await_job(client, second["identity"]["job_id"])["state"] == "succeeded", "second capture succeeds");
    const auto sdk_report =
        Json::parse(successful_call(client, "artifact_read",
                                    {{"artifact_id", second["artifact_id"]}, {"path", "raw/report.json"}})["text"]
                        .get<std::string>());
    require(sdk_report["capture_settings"]["delimiter"] == "graphics_capture_api" &&
                sdk_report["sdk"]["status"] == "application_control_requested",
            "MCP preserves requested SDK delimiter without claiming observed application instrumentation");
    const auto sdk_command = sdk_report["capture"]["arguments"].get<std::vector<std::string>>();
    require(std::find(sdk_command.begin(), sdk_command.end(), "--delimiter-graphics-capture-api") !=
                    sdk_command.end() &&
                std::find(sdk_command.begin(), sdk_command.end(), "--delimiter-present") == sdk_command.end(),
            "SDK delimiter reaches the real capture subprocess boundary through MCP");
    require(recorded_pid(record1) != recorded_pid(record2), "each capture launches a fresh process");
    const auto page = successful_call(client, "artifact_list", {{"limit", 1}});
    require(page["artifacts"].size() == 1 && page["next_after"].is_string(), "artifact list pagination");
    require(!successful_call(client, "artifact_list", {{"after_id", page["next_after"]}})["artifacts"].empty(),
            "artifact list subsequent page");
    require(successful_call(client, "artifact_usage")["completed_bytes"].get<std::uint64_t>() > 0,
            "storage usage recorded");
    expect_tool_error(call(client, "job_status", {{"job_id", "job-unknown"}}), "Unknown job ID");
    expect_tool_error(call(client, "job_cancel", {{"job_id", "job-unknown"}}), "Unknown job ID");

    const auto waiting_record = scratch.path / "waiting.pid";
    const auto waiting = successful_call(client, "capture", capture_arguments(waiting_record, true));
    await_record(waiting_record);
    expect_tool_error(call(client, "capture_metadata", {{"capture_id", waiting["artifact_id"]}}), "not published");
    const auto queued_record = scratch.path / "queued-must-not-launch.pid";
    const auto queued = successful_call(client, "capture", capture_arguments(queued_record));
    require(successful_call(client, "job_status", {{"job_id", queued["identity"]["job_id"]}})["state"] == "queued",
            "same-GPU capture serializes behind active work");
    successful_call(client, "job_cancel", {{"job_id", queued["identity"]["job_id"]}});
    require(await_job(client, queued["identity"]["job_id"])["state"] == "cancelled", "queued cancellation");
    require(!std::filesystem::exists(queued_record), "queued cancellation never launches its application");
    const auto queued_info = successful_call(client, "artifact_info", {{"artifact_id", queued["artifact_id"]}});
    require(queued_info["status"] == "failed" && queued_info["quarantined"] == false,
            "queued cancellation finalizes failed evidence after poll completion");
    expect_tool_error(call(client, "capture_events", {{"capture_id", queued["artifact_id"]}}), "incomplete_capture");
    successful_call(client, "job_cancel", {{"job_id", waiting["identity"]["job_id"]}});
    result = await_job(client, waiting["identity"]["job_id"]);
    require(result["state"] == "cancelled" && result["cleanup_confirmed"] == true && result["gpu_reserved"] == false,
            "running cancellation releases GPU after owned-process cleanup");
    require_gone(recorded_pid(waiting_record));

    const auto timeout_record = scratch.path / "timeout.pid";
    auto timeout_arguments = capture_arguments(timeout_record, true);
    timeout_arguments["timeout_ms"] = 1200;
    const auto timeout = successful_call(client, "capture", timeout_arguments);
    await_record(timeout_record);
    result = await_job(client, timeout["identity"]["job_id"]);
    require(result["state"] == "timed_out" && result["cleanup_confirmed"] == true, "deadline cleanup through MCP");
    require_gone(recorded_pid(timeout_record));

    const auto source = scratch.path / "import-source";
    std::filesystem::create_directory(source);
    std::ofstream(source / "note.txt") << "retained investigation evidence\n";
    std::ofstream(source / "large.txt") << std::string(65537, 'x');
    {
        std::ofstream binary(source / "binary.bin", std::ios::binary);
        binary.write("a\0b", 3);
    }
    {
        std::ofstream invalid(source / "invalid.bin", std::ios::binary);
        invalid.put(static_cast<char>(0xff));
    }
    const auto imported =
        successful_call(client, "artifact_import", {{"source", source.string()}, {"required_outputs", {"note.txt"}}});
    const auto imported_id = imported["id"].get<std::string>();
    require(imported["pinned"] == true && imported["provenance"]["evidence_origin"] == "caller_provided_import",
            "import defaults to a durable pin with distinct evidence origin");
    expect_tool_error(call(client, "capture_metadata", {{"capture_id", imported_id}}), "not_capture");
    const auto text =
        successful_call(client, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/note.txt"}});
    require(text["status"] == "text" && text["text"] == "retained investigation evidence\n",
            "bounded text evidence retrieval");
    const auto large =
        successful_call(client, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/large.txt"}});
    require(large["status"] == "reference_only" && large["text"].is_null() && large["bytes"] == 65537 &&
                std::filesystem::is_regular_file(large["local_path"].get<std::string>()),
            "large text stays on disk and returns a usable file reference");
    expect_tool_error(
        call(client, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/binary.bin"}}), "NUL");
    expect_tool_error(
        call(client, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/invalid.bin"}}), "UTF-8");
    require(client.request(77, "ping").contains("result"), "binary evidence rejection preserves protocol stdout");
    expect_tool_error(
        call(client, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/missing.txt"}}),
        "inventory");

    const auto eof_record = scratch.path / "eof.pid";
    const auto eof = successful_call(client, "capture", capture_arguments(eof_record, true));
    await_record(eof_record);
    require(client.finish() == 0, "EOF waits for cancellation and owned-process cleanup");
    require_gone(recorded_pid(eof_record));
    require(client.diagnostics().find("mcp target stdout") == std::string::npos &&
                client.diagnostics().find("stand-in diagnostic") == std::string::npos,
            "child diagnostics are artifact logs, not server protocol/stderr traffic");

    Client restarted(
        server, scratch.path,
        {"--artifact-root", root.string(), "--artifact-max-bytes", "1", "--artifact-max-age-seconds", "0"});
    initialize(restarted);
    require(successful_call(restarted, "artifact_info", {{"artifact_id", imported_id}})["pinned"] == true,
            "import pin survives server restart");
    require(successful_call(restarted, "artifact_info", {{"artifact_id", first_id}})["pinned"] == true,
            "capture pin survives server restart");
    require(successful_call(restarted, "capture_events", {{"capture_id", first_id}})["total"] == 3,
            "retained inspection works after restart with no Nsight installation or desktop configured");
    const auto eof_info = successful_call(restarted, "artifact_info", {{"artifact_id", eof["artifact_id"]}});
    require(eof_info["status"] == "failed" && eof_info["quarantined"] == false,
            "EOF publishes cleanup-confirmed failed evidence");
    expect_tool_error(call(restarted, "job_status", {{"job_id", first_job}}), "Unknown job ID");
    successful_call(restarted, "artifact_pin", {{"artifact_id", imported_id}, {"pinned", false}});
    const auto prune = successful_call(restarted, "artifact_prune");
    require(prune["expired_count"].get<std::uint64_t>() > 0 && prune["usage"]["quota_exceeded"] == true,
            "prune reports protected data preventing the configured quota");
    require(successful_call(restarted, "artifact_info", {{"artifact_id", imported_id}})["status"] == "expired",
            "unpin permits whole-bundle pruning with an expiration explanation");
    require(successful_call(restarted, "artifact_info", {{"artifact_id", first_id}})["status"] == "complete",
            "quota pressure cannot delete a pinned baseline");
    expect_tool_error(
        call(restarted, "artifact_read", {{"artifact_id", imported_id}, {"path", "raw/imported/note.txt"}}), "expired");
    require(std::filesystem::is_regular_file(source / "note.txt"),
            "pruning imported evidence never changes its source");
    require(restarted.finish() == 0, "artifact-only restart shutdown");
}

void inspection_workflow_check(const std::string& server, const std::string& target, const Scratch& scratch) {
    const auto installation = scratch.path / "standin-installation";
    const auto exports = installation / "inspection-exports";
    std::filesystem::create_directory(exports);
    const auto set_export = [&](const char* name, const std::string& text) {
        std::ofstream output(exports / (std::string(name) + ".raw"), std::ios::binary);
        output << text;
        output.close();
        require(output.good(), "write process-boundary inspection fixture");
    };
    Client client(server, scratch.path,
                  {"--nsight-root", installation.string(), "--artifact-root",
                   (scratch.path / "inspection-managed").string(), "--artifact-max-bytes", "0",
                   "--artifact-max-age-seconds", "0"});
    initialize(client);
    std::size_t invocation = 0;
    const auto capture = [&] {
        const auto record = scratch.path / ("inspection-target-" + std::to_string(++invocation) + ".pid");
        const auto submission = successful_call(client, "capture",
                                                {{"executable", target},
                                                 {"arguments", {"--mcp-target", "--record", record.string()}},
                                                 {"working_directory", scratch.path.string()},
                                                 {"timeout_ms", 10000}});
        require(await_job(client, submission["identity"]["job_id"])["state"] == "succeeded",
                "raw metadata readability remains separate from typed inspection schema validity");
        require_gone(recorded_pid(record));
        return submission["artifact_id"].get<std::string>();
    };

    Json functions = Json::array();
    for(std::size_t index = 0; index < 80; ++index) {
        functions.push_back(
            {{"event_index", index * 3 + 1000}, {"function_name", std::string(8000, '"')}, {"thread_index", 0}});
        if(index % 2 == 0) {
            functions.back()["indirect_index"] = 0;
        }
    }
    set_export("functions", functions.dump());
    const auto large = capture();
    std::size_t count = 0;
    do {
        const auto page =
            successful_call(client, "capture_events", {{"capture_id", large}, {"offset", count}, {"limit", 100}});
        require(page["total"] == 80 && page["events"].size() < 80 && !page["events"].empty() &&
                    page.dump().size() <= 256U * 1024U,
                "MCP byte pagination stops before the row limit while preserving the total");
        for(const auto& event : page["events"]) {
            require(event["event_index"] == count * 3 + 1000, "MCP pages contain each observed event exactly once");
            require(count % 2 == 0 ? event.at("indirect_index") == 0 : event.at("indirect_index").is_null(),
                    "MCP preserves an observed indirect index or explicit absence without inference");
            ++count;
        }
        if(page["next_offset"].is_null()) {
            break;
        }
        require(page["next_offset"] == count, "MCP continuation reflects consumed rows");
    } while(true);
    require(count == 80, "MCP pages retrieve the complete large inventoried file");
    require(successful_call(client, "artifact_read",
                            {{"artifact_id", large}, {"path", "raw/exports/functions.raw"}})["status"] ==
                "reference_only",
            "typed inspection's extended core read cap does not raise MCP artifact_read's 64 KiB cap");

    set_export("functions", "not valid JSON");
    set_export("metadata", R"({"metadata_version":2})");
    const auto wrong_metadata = capture();
    expect_tool_error(call(client, "capture_events", {{"capture_id", wrong_metadata}}),
                      "metadata export unsupported_version");
    expect_tool_error(call(client, "capture_objects", {{"capture_id", wrong_metadata}}),
                      "metadata export unsupported_version");
    std::filesystem::remove(exports / "metadata.raw");
    const auto malformed = capture();
    require(successful_call(client, "capture_metadata", {{"capture_id", malformed}})["metadata_version"] == 1,
            "malformed optional inventory does not prevent validated capture metadata");
    expect_tool_error(call(client, "capture_events", {{"capture_id", malformed}}), "functions export malformed_json");
    std::filesystem::remove(exports / "functions.raw");
    set_export("objects", "{not JSON");
    const auto missing_objects = capture();
    expect_tool_error(call(client, "capture_objects", {{"capture_id", missing_objects}}), "export_unavailable");
    require(successful_call(client, "capture_events", {{"capture_id", missing_objects}})["total"] == 3,
            "a failed optional export leaves other validated inventory queries available");
    require(client.finish() == 0, "inspection errors and byte pagination preserve clean protocol shutdown");
    std::filesystem::remove_all(exports);
}

int target_main(int argc, char** argv) {
    std::filesystem::path record;
    bool wait = false;
    for(int index = 2; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if(argument == "--record" && index + 1 < argc) {
            record = argv[++index];
        } else if(argument == "--wait") {
            wait = true;
        } else {
            return 83;
        }
    }
    if(record.empty()) {
        return 84;
    }
    {
        std::ofstream stream(record);
        stream << getpid() << '\n';
    }
    std::cout << "mcp target stdout\n" << std::flush;
    std::cerr << "mcp target stderr\n" << std::flush;
    if(wait) {
        std::signal(SIGTERM, SIG_IGN);
        while(true) {
            std::this_thread::sleep_for(50ms);
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if(argc > 1 && std::string_view(argv[1]) == "--mcp-target") {
        return target_main(argc, argv);
    }
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 3,
                "usage: ngm_mcp_check /absolute/path/to/nsight-graphics-mcp /absolute/path/to/nsight-standin");
        const Scratch scratch;
        protocol_check(argv[1], scratch);
        malformed_input_check(argv[1], scratch);
        discovery_check(argv[1], scratch);
        invalid_workflow_check(argv[1], scratch);
        import_overlap_check(argv[1], scratch);
        workflow_check(argv[1], argv[2], std::filesystem::canonical(argv[0]).string(), scratch);
        inspection_workflow_check(argv[1], std::filesystem::canonical(argv[0]).string(), scratch);
        bounded_job_error_check(argv[1], std::filesystem::canonical(argv[0]).string(), scratch);
        for(const auto* requested : {"2024-11-05", "2025-03-26", "2099-01-01"}) {
            Client unsupported(argv[1], scratch.path);
            require(initialize(unsupported, requested)["protocolVersion"] == "2025-11-25",
                    "unimplemented/unknown revisions fall back to latest supported revision");
            require(query_capabilities(unsupported)["server"]["protocol_version"] == "2025-11-25",
                    "capabilities report the supported fallback revision");
            unsupported.send(Json::array({{{"jsonrpc", "2.0"}, {"id", 4}, {"method", "ping"}}}));
            expect_error(unsupported.response(), -32600);
            require(unsupported.finish() == 0, "fallback server EOF");
        }
        Client partial(argv[1], scratch.path);
        partial.write("{incomplete");
        partial.close_input();
        expect_error(partial.response(), -32700);
        require(partial.finish() == 0, "incomplete final JSON produces error then clean EOF");
        Client empty(argv[1], scratch.path);
        require(empty.finish() == 0, "empty stream clean EOF");
    });
}
