#include "Check.hpp"
#include "ngm/Version.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

void close_fd(int& descriptor) {
    if(descriptor >= 0) {
        close(descriptor);
        descriptor = -1;
    }
}

struct Pipe {
    int descriptors[2] = {-1, -1};
    Pipe() {
        require(pipe2(descriptors, O_CLOEXEC) == 0, "create check pipe");
    }
    ~Pipe() {
        close_fd(descriptors[0]);
        close_fd(descriptors[1]);
    }
};

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

class Client {
public:
    Client(const std::string& executable, const std::filesystem::path& path,
           const std::vector<std::string>& arguments = {}, bool display = false) {
        std::vector<std::string> command{executable};
        command.insert(command.end(), arguments.begin(), arguments.end());
        std::vector<char*> argv;
        for(auto& argument : command) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);
        std::vector<std::string> environment{"PATH=" + path.string()};
        if(display) {
            environment.emplace_back("DISPLAY=:not-a-real-display");
        }
        std::vector<char*> envp;
        for(auto& variable : environment) {
            envp.push_back(variable.data());
        }
        envp.push_back(nullptr);

        posix_spawn_file_actions_t actions;
        require(posix_spawn_file_actions_init(&actions) == 0, "initialize spawn actions");
        const bool valid_actions = posix_spawn_file_actions_adddup2(&actions, input_.descriptors[0], 0) == 0 &&
                                   posix_spawn_file_actions_adddup2(&actions, output_.descriptors[1], 1) == 0 &&
                                   posix_spawn_file_actions_adddup2(&actions, error_.descriptors[1], 2) == 0;
        const auto error = valid_actions
                               ? posix_spawn(&pid_, executable.c_str(), &actions, nullptr, argv.data(), envp.data())
                               : EINVAL;
        posix_spawn_file_actions_destroy(&actions);
        require(error == 0, "spawn actual MCP server executable");
        close_fd(input_.descriptors[0]);
        close_fd(output_.descriptors[1]);
        close_fd(error_.descriptors[1]);
    }
    ~Client() {
        if(pid_ > 0) {
            kill(pid_, SIGKILL);
            while(waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
    }
    void write(std::string_view bytes) {
        while(!bytes.empty()) {
            const auto count = ::write(input_.descriptors[1], bytes.data(), bytes.size());
            if(count < 0 && errno == EINTR) {
                continue;
            }
            require(count > 0, "write MCP request to server");
            bytes.remove_prefix(static_cast<std::size_t>(count));
        }
    }
    void send(const Json& request) {
        write(request.dump() + '\n');
    }
    Json response() {
        const auto deadline = Clock::now() + 5s;
        while(stdout_.find('\n') == std::string::npos) {
            require(output_.descriptors[0] >= 0, "server exited before response: " + stderr_);
            pump(deadline);
        }
        const auto newline = stdout_.find('\n');
        const auto line = stdout_.substr(0, newline);
        stdout_.erase(0, newline + 1);
        const auto result = Json::parse(line);
        require(result.is_object() && result.value("jsonrpc", "") == "2.0", "stdout contains only JSON-RPC");
        require(result.contains("id") && (result.contains("result") != result.contains("error")),
                "response has an id and exactly one result or error");
        return result;
    }
    Json request(int id, const char* method, Json params = Json::object()) {
        send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}});
        auto result = response();
        require(result["id"] == id, "response id matches request (notifications produced no response)");
        return result;
    }
    void close_input() {
        close_fd(input_.descriptors[1]);
    }
    int finish() {
        close_input();
        const auto deadline = Clock::now() + 5s;
        while(output_.descriptors[0] >= 0 || error_.descriptors[0] >= 0) {
            pump(deadline);
        }
        require(stdout_.empty(), "EOF has no unexpected protocol output or banner");
        int status = 0;
        while(true) {
            const auto waited = waitpid(pid_, &status, WNOHANG);
            if(waited == pid_) {
                pid_ = -1;
                break;
            }
            require(waited == 0 || (waited < 0 && errno == EINTR), "wait for server process");
            require(Clock::now() < deadline, "server exits promptly on EOF");
            std::this_thread::sleep_for(1ms);
        }
        require(WIFEXITED(status), "server terminates normally");
        return WEXITSTATUS(status);
    }
    const std::string& diagnostics() const {
        return stderr_;
    }

private:
    void pump(Clock::time_point deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        require(remaining.count() > 0, "server response/shutdown deadline");
        std::array<pollfd, 2> descriptors{{{output_.descriptors[0], POLLIN, 0}, {error_.descriptors[0], POLLIN, 0}}};
        const auto result = poll(descriptors.data(), descriptors.size(), static_cast<int>(remaining.count()));
        if(result < 0 && errno == EINTR) {
            return;
        }
        require(result > 0, "server pipe deadline");
        for(std::size_t index = 0; index < descriptors.size(); ++index) {
            if(descriptors[index].revents == 0) {
                continue;
            }
            int& descriptor = index == 0 ? output_.descriptors[0] : error_.descriptors[0];
            auto& destination = index == 0 ? stdout_ : stderr_;
            std::array<char, 4096> buffer;
            const auto count = read(descriptor, buffer.data(), buffer.size());
            if(count == 0) {
                close_fd(descriptor);
            } else if(count > 0) {
                destination.append(buffer.data(), static_cast<std::size_t>(count));
                require(destination.size() < 256 * 1024, "bounded protocol and diagnostic output");
            } else {
                require(errno == EINTR, "read server output");
            }
        }
    }
    Pipe input_;
    Pipe output_;
    Pipe error_;
    pid_t pid_ = -1;
    std::string stdout_;
    std::string stderr_;
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

void expect_tool_error(const Json& response) {
    require(response.contains("result") && !response.contains("error"), "tool input error is a tool result");
    const auto& result = response["result"];
    require(result["isError"] == true, "invalid tool input sets isError");
    require(result["content"].size() == 1 && result["content"][0]["type"] == "text", "tool error text content");
    require(result["content"][0]["text"].get<std::string>().find("Remove all argument properties") != std::string::npos,
            "tool input error explains how to correct the call");
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
    require(report["implemented_tools"] == Json::array({"capabilities"}), "only available tool advertised");
    for(const auto& operation : report["operations"]) {
        require(operation["available"] == false && operation["status"] == "not_implemented",
                "pending integrations remain unavailable");
    }
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
    require(listing.size() == 1 && listing[0]["name"] == "capabilities", "discover exactly the implemented tool");
    require(listing[0]["inputSchema"]["additionalProperties"] == false, "closed input schema advertised");
    require(listing[0].contains("outputSchema"), "structured output schema advertised");
    require(listing[0]["annotations"]["readOnlyHint"] == true, "read-only tool annotation");
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
    Client explicit_root(server, scratch.path, {"--nsight-root", root.string()}, true);
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
    require(explicit_root.finish() == 0, "explicit-root server EOF");

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
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 2, "usage: ngm_mcp_check /absolute/path/to/nsight-graphics-mcp");
        const Scratch scratch;
        protocol_check(argv[1], scratch);
        malformed_input_check(argv[1], scratch);
        discovery_check(argv[1], scratch);
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
