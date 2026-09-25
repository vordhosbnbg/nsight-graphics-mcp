#include "Check.hpp"
#include "ngm/Version.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <fastmcpp/client/client.hpp>
#include <fastmcpp/client/transports.hpp>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using ngm::check::require;
using Json = nlohmann::json;
namespace fs = std::filesystem;
using namespace std::chrono_literals;
const std::string token(64, 'a');
std::string read(const fs::path& path) {
    std::ifstream f(path);
    return {std::istreambuf_iterator<char>(f), {}};
}
struct Scratch {
    fs::path path;
    Scratch() {
        std::string value = (fs::temp_directory_path() / "ngm-http-XXXXXX").string();
        require(mkdtemp(value.data()), "scratch");
        path = value;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
struct Server {
    pid_t pid = -1;
    int port = 0;
    fs::path output, error;
    Server(const std::string& exe, const fs::path& root, const std::vector<std::string>& extra = {}) {
        fs::create_directory(root);
        output = root / "stdout";
        error = root / "stderr";
        std::vector<std::string> args{
            exe, "--transport", "http", "--http-token-file", (root.parent_path() / "token").string(), "--http-port",
            "0"};
        args.insert(args.end(), extra.begin(), extra.end());
        pid = fork();
        require(pid >= 0, "fork HTTP server");
        if(pid == 0) {
            int out = open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            int err = open(error.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
            dup2(out, STDOUT_FILENO);
            dup2(err, STDERR_FILENO);
            close(out);
            close(err);
            int in = open("/dev/null", O_RDONLY);
            dup2(in, STDIN_FILENO);
            close(in);
            clearenv();
            setenv("PATH", "/usr/bin:/bin", 1);
            setenv("DISPLAY", ":standin", 1);
            std::vector<char*> raw;
            for(auto& a : args)
                raw.push_back(a.data());
            raw.push_back(nullptr);
            execv(exe.c_str(), raw.data());
            _exit(127);
        }
    }
    ~Server() {
        if(pid > 0) {
            kill(pid, SIGTERM);
            for(int i = 0; i < 1000; ++i) {
                if(waitpid(pid, nullptr, WNOHANG) == pid)
                    return;
                std::this_thread::sleep_for(10ms);
            }
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, 0);
        }
    }
    void ready() {
        for(int i = 0; i < 500; ++i) {
            auto log = read(error);
            auto at = log.find("HTTP listening http://127.0.0.1:");
            if(at != std::string::npos) {
                port = std::stoi(log.substr(at + std::string_view("HTTP listening http://127.0.0.1:").size()));
                return;
            }
            std::this_thread::sleep_for(10ms);
        }
        throw std::runtime_error("HTTP startup failed: " + read(error));
    }
    int finish(int signal = SIGTERM) {
        if(signal)
            require(kill(pid, signal) == 0, "signal owned server");
        for(int i = 0; i < 1000; ++i) {
            int status{};
            if(waitpid(pid, &status, WNOHANG) == pid) {
                pid = -1;
                require(WIFEXITED(status), "server exits normally: " + read(error));
                return WEXITSTATUS(status);
            }
            std::this_thread::sleep_for(10ms);
        }
        throw std::runtime_error("HTTP shutdown exceeded 10 seconds: " + read(error));
    }
};
struct Client {
    int port;
    std::string session, version = "2025-11-25";
    unsigned id = 0;
    explicit Client(int p) : port(p) {}
    httplib::Headers headers() const {
        httplib::Headers h{{"Authorization", "Bearer " + token}, {"Accept", "application/json, text/event-stream"}};
        if(!session.empty()) {
            h.emplace("MCP-Session-Id", session);
            h.emplace("MCP-Protocol-Version", version);
        }
        return h;
    }
    httplib::Response post(const std::string& body, httplib::Headers h, const std::string& type = "application/json") {
        httplib::Client c("127.0.0.1", port);
        c.set_read_timeout(10);
        auto r = c.Post("/mcp", h, body, type);
        require(bool(r), "HTTP response");
        return *r;
    }
    Json rpc(const std::string& method, const Json& params = Json::object()) {
        auto r = post(Json{{"jsonrpc", "2.0"}, {"id", ++id}, {"method", method}, {"params", params}}.dump(), headers());
        require(r.status == 200, "RPC HTTP status: " + r.body);
        return Json::parse(r.body);
    }
    void initialize(const std::string& protocol = "2025-11-25") {
        version = protocol;
        auto r = post(Json{{"jsonrpc", "2.0"},
                           {"id", ++id},
                           {"method", "initialize"},
                           {"params",
                            {{"protocolVersion", protocol},
                             {"capabilities", Json::object()},
                             {"clientInfo", {{"name", "http-check"}, {"version", "1"}}}}}}
                          .dump(),
                      headers());
        require(r.status == 200, "initialize HTTP response");
        session = r.get_header_value("MCP-Session-Id");
        require(session.size() == 48, "random session id");
        auto j = Json::parse(r.body);
        require(j["result"]["serverInfo"]["version"] == ngm::project_version(), "product version");
        version = j["result"]["protocolVersion"];
        auto n = post(R"({"jsonrpc":"2.0","method":"notifications/initialized"})", headers());
        require(n.status == 202 && n.body.empty(), "notification HTTP 202");
    }
    Json tool(const std::string& name, const Json& args = Json::object()) {
        auto r = rpc("tools/call", {{"name", name}, {"arguments", args}});
        require(r.contains("result") && !r["result"].value("isError", false), "tool succeeds: " + r.dump());
        return r["result"]["structuredContent"];
    }
    void remove() {
        httplib::Client c("127.0.0.1", port);
        auto r = c.Delete("/mcp", headers());
        require(r && r->status == 204, "delete session");
    }
};
int raw_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    require(fd >= 0, "socket");
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<uint16_t>(port));
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(connect(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0, "connect raw socket");
    return fd;
}
void gone(const fs::path& record) {
    auto pid = std::stoi(read(record));
    errno = 0;
    require(kill(pid, 0) < 0 && errno == ESRCH, "owned target cleaned up");
}
Json await_job(Client& c, const std::string& id) {
    for(int i = 0; i < 300; ++i) {
        auto j = c.tool("job_status", {{"job_id", id}});
        if(j["state"] != "queued" && j["state"] != "running" && !j["worker_running"].get<bool>() &&
           !j["finalization_pending"].get<bool>())
            return j;
        std::this_thread::sleep_for(20ms);
    }
    throw std::runtime_error("job deadline");
}
void wait_record(const fs::path& p) {
    for(int i = 0; i < 500; ++i) {
        if(fs::exists(p) && !read(p).empty())
            return;
        std::this_thread::sleep_for(10ms);
    }
    throw std::runtime_error("target not started");
}
} // namespace
int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    return ngm::check::run([&] {
        require(argc == 4, "server standin mcp-check arguments");
        Scratch scratch;
        {
            std::ofstream f(scratch.path / "token");
            f << token << '\n';
        }
        fs::permissions(scratch.path / "token", fs::perms::owner_read | fs::perms::owner_write);
        const auto installation = scratch.path / "nsight";
        fs::create_directory(installation);
        for(auto name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
            fs::copy_file(argv[2], installation / name);
            fs::permissions(installation / name, fs::perms::owner_all);
        }
        const auto store = scratch.path / "artifacts";
        Server server(argv[1], scratch.path / "first",
                      {"--artifact-root", store.string(), "--nsight-root", installation.string()});
        server.ready();
        Client c(server.port);
        const std::string ping = R"({"jsonrpc":"2.0","id":1,"method":"ping"})";
        require(c.post(ping, {}).status == 401, "authentication required");
        auto bad = c.headers();
        bad.erase("Authorization");
        bad.emplace("Authorization", "Bearer wrong");
        require(c.post(ping, bad).status == 401, "wrong token denied");
        bad = c.headers();
        bad.emplace("Origin", "https://evil.invalid");
        require(c.post(ping, bad).status == 403, "Origin refused");
        bad = c.headers();
        bad.emplace("Host", "evil.invalid");
        require(c.post(ping, bad).status == 403, "Host refused");
        require(c.post(ping, c.headers()).status == 400, "session required before tools");
        c.initialize("2025-06-18");
        Client other(server.port);
        other.initialize();
        require(c.tool("capabilities")["server"]["protocol_version"] == "2025-06-18", "independent protocol version A");
        require(other.tool("capabilities")["server"]["protocol_version"] == "2025-11-25",
                "independent protocol version B");
        require(!fs::exists(store), "discovery does not open store");
        require(c.rpc("tools/list")["result"]["tools"].size() == 25, "same implemented tools");
        bad = c.headers();
        bad.erase("Accept");
        bad.emplace("Accept", "application/json;q=1");
        bad.emplace("Accept", "text/event-stream; q=0.5");
        require(c.post(ping, bad, "Application/JSON;charset=UTF-8").status == 200, "valid HTTP media syntax");
        bad = c.headers();
        bad.erase("Accept");
        bad.emplace("Accept", "application/json;q=0, text/event-stream");
        require(c.post(ping, bad).status == 406, "unacceptable JSON representation");
        bad = c.headers();
        bad.erase("MCP-Protocol-Version");
        bad.emplace("MCP-Protocol-Version", "2099-01-01");
        require(c.post(ping, bad).status == 400, "unknown header version");
        require(c.post("{bad", c.headers()).status == 400, "malformed JSON");
        require(c.post(std::string(65537, ' '), c.headers()).status == 413, "body cap");
        require(c.post(R"({"jsonrpc":"2.0","id":1,"result":{}})", c.headers()).status == 400,
                "unsolicited JSON-RPC response rejected");
        {
            httplib::Client client("127.0.0.1", server.port);
            auto r = client.Get("/mcp", c.headers());
            require(r && r->status == 405, "GET explicitly has no SSE");
        }
        // Real pinned fastmcpp HTTP client, independent of this test's raw framing.
        {
            auto transport = std::make_unique<fastmcpp::client::StreamableHttpTransport>(
                "http://127.0.0.1:" + std::to_string(server.port), "/mcp",
                std::unordered_map<std::string, std::string>{{"Authorization", "Bearer " + token}});
            fastmcpp::client::Client sdk(std::move(transport));
            try {
                sdk.initialize();
            } catch(const fastmcpp::TransportHttpError& e) {
                throw std::runtime_error(std::string(e.what()) + ": " + e.body());
            }
            require(sdk.list_tools().size() == 25, "supported client tool discovery");
            // Preserve JSON Schema union types: the pinned client's optional
            // high-level result coercion assumes string-only schema types.
            const auto result = sdk.call("tools/call", {{"name", "capabilities"}, {"arguments", Json::object()}});
            require(result.at("structuredContent").at("server").at("transport") == "streamable_http",
                    "supported client raw-result invocation");
        }
        const auto capture = [&](Client& client, const fs::path& record, bool wait) {
            auto args = Json::array({"--mcp-target", "--record", record.string()});
            if(wait)
                args.push_back("--wait");
            return client.tool("capture", {{"executable", argv[3]},
                                           {"working_directory", scratch.path.string()},
                                           {"arguments", args},
                                           {"timeout_ms", 20000},
                                           {"pin", true}});
        };
        auto complete = capture(c, scratch.path / "first.pid", false);
        auto job = await_job(other, complete["identity"]["job_id"]);
        require(job["state"] == "succeeded" && job["cleanup_confirmed"].get<bool>(), "cross-client capture lifecycle");
        gone(scratch.path / "first.pid");
        auto waiting = capture(c, scratch.path / "wait.pid", true);
        wait_record(scratch.path / "wait.pid");
        c.remove();
        std::this_thread::sleep_for(100ms);
        require(other.tool("job_status", {{"job_id", waiting["identity"]["job_id"]}})["state"] == "running",
                "session deletion does not cancel owned job");
        require(c.post(ping, c.headers()).status == 404, "deleted session rejected");
        other.tool("job_cancel", {{"job_id", waiting["identity"]["job_id"]}});
        job = await_job(other, waiting["identity"]["job_id"]);
        require(job["state"] == "cancelled" && job["cleanup_confirmed"].get<bool>(),
                "another trusted client cancels job");
        gone(scratch.path / "wait.pid");
        auto shutdown = capture(other, scratch.path / "shutdown.pid", true);
        (void)shutdown;
        wait_record(scratch.path / "shutdown.pid");
        // Unterminated header must be rejected before an unbounded allocation.
        {
            int fd = raw_socket(server.port);
            std::string attack = "GET /mcp HTTP/1.1\r\nX-Fill: " + std::string(18000, 'x');
            send(fd, attack.data(), attack.size(), MSG_NOSIGNAL);
            timeval limit{2, 0};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
            char b[4096];
            auto n = recv(fd, b, sizeof(b), 0);
            require(n >= 0, "oversized unfinished header does not wait indefinitely");
            close(fd);
        }
        // A live slow partial connection cannot delay SIGTERM cleanup.
        int partial = raw_socket(server.port);
        const std::string prefix = "POST /mcp HTTP/1.1\r\nX-Partial: ";
        send(partial, prefix.data(), prefix.size(), MSG_NOSIGNAL);
        int body_socket = raw_socket(server.port);
        const std::string body_request =
            "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(server.port) +
            "\r\nContent-Length: 100\r\nExpect: 100-continue\r\nContent-Type: application/json\r\n\r\n";
        send(body_socket, body_request.data(), body_request.size(), MSG_NOSIGNAL);
        timeval body_timeout{2, 0};
        setsockopt(body_socket, SOL_SOCKET, SO_RCVTIMEO, &body_timeout, sizeof(body_timeout));
        char response[256];
        auto received = recv(body_socket, response, sizeof(response), 0);
        require(received > 0 &&
                    std::string(response, static_cast<std::size_t>(received)).find("100 Continue") != std::string::npos,
                "server acknowledged headers and is waiting for body before shutdown");
        auto begin = std::chrono::steady_clock::now();
        require(server.finish() == 0, "SIGTERM clean exit");
        require(std::chrono::steady_clock::now() - begin < 5s, "partial connection shutdown bounded");
        close(partial);
        close(body_socket);
        gone(scratch.path / "shutdown.pid");
        require(read(server.output).empty(), "HTTP diagnostics never use stdout");
        Server restart(argv[1], scratch.path / "restart", {"--artifact-root", store.string()});
        restart.ready();
        Client after(restart.port);
        after.initialize();
        auto info = after.tool("artifact_info", {{"artifact_id", complete["artifact_id"]}});
        require(info["pinned"].get<bool>() && info["status"] == "complete",
                "pinned evidence survives persistent server restart");
        require(restart.finish(SIGINT) == 0, "SIGINT clean exit");
        fs::permissions(scratch.path / "token", fs::perms::owner_read | fs::perms::group_read);
        Server insecure(argv[1], scratch.path / "insecure");
        require(insecure.finish(0) != 0, "insecure token refused before listening");
        require(read(insecure.error).find("private regular file") != std::string::npos, "actionable token error");
    });
}
