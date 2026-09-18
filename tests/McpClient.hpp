#pragma once

#include "Check.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ngm::check::mcp {
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

// CPU regressions keep their short deadlines; hardware workflows select longer
// response and EOF-cleanup bounds without changing those regressions.
struct ClientTimeouts {
    std::chrono::milliseconds response{5000};
    std::chrono::milliseconds shutdown{5000};
};

inline void close_fd(int& descriptor) {
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

class Client {
public:
    Client(const std::string& executable, const std::filesystem::path& path,
           const std::vector<std::string>& arguments = {}, bool display = false,
           const std::vector<std::string>& extra_environment = {}, ClientTimeouts timeouts = {}) : timeouts_(timeouts) {
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
        environment.insert(environment.end(), extra_environment.begin(), extra_environment.end());
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
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    ~Client() {
        // A failed assertion can leave an active capture. Give the actual
        // server its EOF cleanup path before resorting to killing the server.
        close_input();
        const auto deadline = Clock::now() + timeouts_.shutdown;
        while(pid_ > 0 && Clock::now() < deadline) {
            const auto waited = waitpid(pid_, nullptr, WNOHANG);
            if(waited == pid_ || (waited < 0 && errno != EINTR)) {
                pid_ = -1;
                break;
            }
            std::this_thread::sleep_for(1ms);
        }
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
        const auto deadline = Clock::now() + timeouts_.response;
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
        const auto deadline = Clock::now() + timeouts_.shutdown;
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
                require(destination.size() < 2 * 1024 * 1024, "bounded protocol and diagnostic output");
            } else {
                require(errno == EINTR, "read server output");
            }
        }
    }
    ClientTimeouts timeouts_;
    Pipe input_;
    Pipe output_;
    Pipe error_;
    pid_t pid_ = -1;
    std::string stdout_;
    std::string stderr_;
};

} // namespace ngm::check::mcp
