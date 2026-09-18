#include "Server.hpp"
#include "ngm/Version.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <httplib.h>
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <sys/random.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace ngm {
namespace {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
std::string read_token(const std::filesystem::path& path) {
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if(fd < 0)
        throw std::runtime_error("Cannot open HTTP token file");
    struct Close {
        int fd;
        ~Close() {
            close(fd);
        }
    } closer{fd};
    struct stat st{};
    if(fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0077) || st.st_nlink != 1 ||
       st.st_size < 32 || st.st_size > 129)
        throw std::runtime_error(
            "HTTP token file must be a private regular file owned by this user, with 32..128 token characters");
    std::array<char, 130> buffer{};
    const auto size = read(fd, buffer.data(), buffer.size());
    if(size != st.st_size)
        throw std::runtime_error("HTTP token file changed or could not be read");
    std::string token(buffer.data(), static_cast<std::size_t>(size));
    if(!token.empty() && token.back() == '\n')
        token.pop_back();
    if(token.size() < 32 || token.size() > 128 ||
       token.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos)
        throw std::runtime_error("HTTP token must contain 32..128 ASCII letters, digits, hyphens or underscores");
    return token;
}
bool equal_token(const std::string& actual, const std::string& expected) {
    if(actual.size() != expected.size())
        return false;
    unsigned difference = 0;
    for(std::size_t i = 0; i < actual.size(); ++i)
        difference |= static_cast<unsigned char>(actual[i]) ^ static_cast<unsigned char>(expected[i]);
    return difference == 0;
}
std::string session_id() {
    std::array<unsigned char, 24> bytes{};
    std::size_t done = 0;
    while(done < bytes.size()) {
        auto n = getrandom(bytes.data() + done, bytes.size() - done, 0);
        if(n < 0 && errno == EINTR)
            continue;
        if(n <= 0)
            throw std::runtime_error("Could not generate HTTP session identity");
        done += static_cast<std::size_t>(n);
    }
    std::string out;
    for(auto b : bytes) {
        out += "0123456789abcdef"[b >> 4];
        out += "0123456789abcdef"[b & 15];
    }
    return out;
}
std::string normalized(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    if(first == std::string::npos)
        return {};
    value = value.substr(first, value.find_last_not_of(" \t") - first + 1);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
bool media_type(const std::string& value, std::string_view type, bool accept) {
    auto separator = value.find(';');
    if(normalized(value.substr(0, separator)) != type)
        return false;
    while(separator != std::string::npos) {
        auto end = value.find(';', separator + 1);
        auto param = normalized(value.substr(separator + 1, end == std::string::npos ? end : end - separator - 1));
        auto equals = param.find('=');
        if(equals == std::string::npos)
            return false;
        auto name = normalized(param.substr(0, equals));
        auto data = normalized(param.substr(equals + 1));
        if(accept && name == "q") {
            double q = 0;
            auto parsed = std::from_chars(data.data(), data.data() + data.size(), q);
            if(parsed.ec != std::errc{} || parsed.ptr != data.data() + data.size() || !(q > 0 && q <= 1))
                return false;
        } else if(name == "charset" && data != "utf-8" && data != "\"utf-8\"")
            return false;
        separator = end;
    }
    return true;
}
bool accepts(const httplib::Request& request, std::string_view type) {
    for(std::size_t index = 0; index < request.get_header_value_count("Accept"); ++index) {
        const auto header = request.get_header_value("Accept", index);
        std::size_t start = 0;
        while(start < header.size()) {
            auto end = header.find(',', start);
            if(media_type(header.substr(start, end == std::string::npos ? end : end - start), type, true))
                return true;
            if(end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    return false;
}
void fail(httplib::Response& response, int status, const char* message) {
    response.status = status;
    response.set_content(Json{{"error", message}}.dump(), "application/json");
}
// Bound wire bytes before cpp-httplib grows its request/header/chunk strings.
class BoundedRequestStream : public httplib::Stream {
public:
    explicit BoundedRequestStream(httplib::Stream& inner) : inner_(inner) {}
    bool is_readable() const override {
        return bytes_ < 131072 && inner_.is_readable();
    }
    bool is_writable() const override {
        return inner_.is_writable();
    }
    ssize_t read(char* data, std::size_t size) override {
        if(bytes_ >= 131072)
            return -1;
        const auto n = inner_.read(data, std::min(size, 131073 - bytes_));
        if(n <= 0)
            return n;
        bytes_ += static_cast<std::size_t>(n);
        if(bytes_ > 131072)
            return -1;
        for(ssize_t i = 0; i < n && !headers_done_; ++i) {
            if(++header_bytes_ > 16384)
                return -1;
            tail_ = (tail_ << 8) | static_cast<unsigned char>(data[i]);
            if(tail_ == 0x0d0a0d0aU)
                headers_done_ = true;
        }
        return n;
    }
    ssize_t write(const char* data, std::size_t size) override {
        return inner_.write(data, size);
    }
    void get_remote_ip_and_port(std::string& ip, int& port) const override {
        inner_.get_remote_ip_and_port(ip, port);
    }
    void get_local_ip_and_port(std::string& ip, int& port) const override {
        inner_.get_local_ip_and_port(ip, port);
    }
    socket_t socket() const override {
        return inner_.socket();
    }

private:
    httplib::Stream& inner_;
    std::size_t bytes_ = 0, header_bytes_ = 0;
    std::uint32_t tail_ = 0;
    bool headers_done_ = false;
};
// The pinned cpp-httplib stop() closes its listener only. Track the sockets
// owned by its virtual connection hook so shutdown also interrupts partial reads.
class BoundedHttpServer : public httplib::Server {
public:
    void expire_connections(bool stop_all = false) {
        std::lock_guard lock(socket_mutex_);
        stopping_ = stopping_ || stop_all;
        for(const auto& [socket, started] : sockets_)
            if(stopping_ || Clock::now() - started >= std::chrono::seconds(30))
                ::shutdown(socket, SHUT_RDWR);
    }

private:
    std::mutex socket_mutex_;
    std::map<socket_t, Clock::time_point> sockets_;
    bool stopping_ = false;
    bool process_and_close_socket(socket_t socket) override {
        {
            std::lock_guard lock(socket_mutex_);
            if(stopping_) {
                ::shutdown(socket, SHUT_RDWR);
                ::close(socket);
                return false;
            }
            sockets_.emplace(socket, Clock::now());
        }
        // This is the pinned library's thin connection adapter, with explicit
        // ownership around it; request parsing/routing remains in cpp-httplib.
        bool result = false;
        try {
            result = httplib::detail::process_server_socket(
                svr_sock_, socket, keep_alive_max_count_, keep_alive_timeout_sec_, read_timeout_sec_,
                read_timeout_usec_, write_timeout_sec_, write_timeout_usec_,
                [this](httplib::Stream& stream, bool closing, bool& closed) {
                    BoundedRequestStream bounded(stream);
                    return process_request(bounded, closing, closed, nullptr);
                });
        } catch(...) { /* The connection is closed; no exception escapes a worker. */
        }
        std::lock_guard lock(socket_mutex_);
        sockets_.erase(socket);
        ::shutdown(socket, SHUT_RDWR);
        ::close(socket);
        return result;
    }
};
struct SignalMask {
    sigset_t watched{}, previous{};
    SignalMask() {
        sigemptyset(&watched);
        sigaddset(&watched, SIGTERM);
        sigaddset(&watched, SIGINT);
        if(pthread_sigmask(SIG_BLOCK, &watched, &previous))
            throw std::runtime_error("Cannot block shutdown signals");
    }
    ~SignalMask() {
        pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    }
};
} // namespace

int serve_http(const ServerOptions& options) {
    // Block shutdown signals before creating any worker or tool threads.
    SignalMask signals;
    std::signal(SIGPIPE, SIG_IGN);
    const auto authorization = "Bearer " + read_token(options.http_token_file);
    ProtocolService service(options);
    BoundedHttpServer server;
    server.new_task_queue = [] { return new httplib::ThreadPool(4, 16); };
    server.set_payload_max_length(65536);
    server.set_read_timeout(5);
    server.set_write_timeout(5);
    server.set_keep_alive_timeout(1);
    server.set_keep_alive_max_count(16);
    const int port = options.http_port == 0 ? server.bind_to_any_port("127.0.0.1")
                                            : (server.bind_to_port("127.0.0.1", static_cast<int>(options.http_port))
                                                   ? static_cast<int>(options.http_port)
                                                   : -1);
    if(port < 0)
        throw std::runtime_error("Could not bind HTTP listener on 127.0.0.1");
    const auto host = "127.0.0.1:" + std::to_string(port);
    const auto localhost = "localhost:" + std::to_string(port);
    server.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Cache-Control", "no-store");
        if(req.get_header_value_count("Host") != 1 ||
           (req.get_header_value("Host") != host && req.get_header_value("Host") != localhost) ||
           req.has_header("Origin")) {
            fail(res, 403, "Host not allowed or browser Origin unsupported");
            return httplib::Server::HandlerResponse::Handled;
        }
        if(req.get_header_value_count("Authorization") != 1 ||
           !equal_token(req.get_header_value("Authorization"), authorization)) {
            res.set_header("WWW-Authenticate", "Bearer");
            fail(res, 401, "Bearer authentication required");
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    struct Session {
        RpcSession rpc;
        Clock::time_point used = Clock::now();
    };
    std::map<std::string, Session> sessions;
    std::mutex mutex;
    const auto endpoint = [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard lock(mutex);
        const auto now = Clock::now();
        std::erase_if(sessions, [&](const auto& pair) { return now - pair.second.used > std::chrono::minutes(30); });
        for(const auto* name : {"MCP-Session-Id", "MCP-Protocol-Version"}) {
            if(req.get_header_value_count(name) > 1) {
                fail(res, 400, "Duplicate MCP header");
                return;
            }
        }
        const auto id = req.get_header_value("MCP-Session-Id");
        const auto version = req.get_header_value("MCP-Protocol-Version");
        if(req.has_header("MCP-Protocol-Version") && version != "2025-06-18" && version != "2025-11-25") {
            fail(res, 400, "Unsupported MCP protocol version");
            return;
        }
        auto current = sessions.find(id);
        if(req.has_header("MCP-Session-Id") && current == sessions.end()) {
            fail(res, 404, "Unknown or expired MCP session");
            return;
        }
        if(current != sessions.end()) {
            if(!version.empty() && version != current->second.rpc.protocol_version) {
                fail(res, 400, "MCP protocol version differs from session");
                return;
            }
            current->second.used = now;
        }
        if(req.method == "GET") {
            res.status = 405;
            res.set_header("Allow", "POST, DELETE");
            return;
        }
        if(req.method == "DELETE") {
            if(current == sessions.end()) {
                fail(res, 400, "MCP session required");
                return;
            }
            sessions.erase(current);
            res.status = 204;
            return;
        }
        // The pinned fastmcpp client emits Content-Type twice through httplib.
        // Accept equivalent JSON declarations, but reject any conflicting value.
        if(!req.has_header("Content-Type")) {
            fail(res, 415, "Content-Type application/json required");
            return;
        }
        for(std::size_t i = 0; i < req.get_header_value_count("Content-Type"); ++i) {
            if(!media_type(req.get_header_value("Content-Type", i), "application/json", false)) {
                fail(res, 415, "Content-Type application/json required");
                return;
            }
        }
        if(!accepts(req, "application/json") || !accepts(req, "text/event-stream")) {
            fail(res, 406, "Accept must include application/json and text/event-stream");
            return;
        }
        auto parsed = parse_rpc(req.body);
        if(parsed.error) {
            res.status = 400;
            res.set_content(parsed.error->dump(), "application/json");
            return;
        }
        const auto& request = *parsed.request;
        const bool initialize = request.is_object() && request.contains("method") && request["method"] == "initialize";
        Session fresh;
        if(current == sessions.end() && !initialize) {
            fail(res, 400, "Initialize without a session header first");
            return;
        }
        if(current == sessions.end() && sessions.size() >= 128) {
            fail(res, 503, "MCP session limit reached; delete an unused session or wait for expiry");
            return;
        }
        auto& session = current == sessions.end() ? fresh : current->second;
        auto reply = service.respond(request, session.rpc);
        if(!reply) {
            res.status = 202;
            return;
        }
        if(current == sessions.end() && session.rpc.phase == RpcSession::Phase::Initialized) {
            std::string new_id;
            do {
                new_id = session_id();
            } while(sessions.contains(new_id));
            sessions.emplace(new_id, std::move(fresh));
            res.set_header("MCP-Session-Id", new_id);
        }
        if(reply->contains("error") && (*reply)["error"]["code"] == -32600)
            res.status = 400;
        auto body = reply->dump();
        if(body.size() > 1024 * 1024) {
            fail(res, 500, "MCP response exceeds transport limit");
            return;
        }
        res.set_content(body, "application/json");
    };
    server.Post("/mcp", endpoint);
    server.Get("/mcp", endpoint);
    server.Delete("/mcp", endpoint);
    std::atomic<bool> finished = false;
    bool listen_ok = false;
    std::thread listener([&] {
        listen_ok = server.listen_after_bind();
        finished.store(true);
    });
    while(!server.is_running() && !finished.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(!finished.load())
        std::cerr << "nsight-graphics-mcp " << project_version() << ": HTTP listening http://" << host << "/mcp\n";
    while(!finished.load()) {
        server.expire_connections();
        const timespec timeout{0, 100000000};
        int signal = sigtimedwait(&signals.watched, nullptr, &timeout);
        if(signal == SIGTERM || signal == SIGINT)
            break;
        if(signal < 0 && errno != EAGAIN && errno != EINTR)
            break;
    }
    server.stop();
    server.expire_connections(true);
    listener.join();
    const bool clean = service.shutdown();
    if(!clean)
        std::cerr << "nsight-graphics-mcp: HTTP shutdown did not confirm owned-process cleanup\n";
    return listen_ok && clean ? 0 : 1;
}
} // namespace ngm
