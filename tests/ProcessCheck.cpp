#include "Check.hpp"
#include "ngm/Process.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <pthread.h>
#include <signal.h>
#include <sstream>
#include <stop_token>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using ngm::check::require;
using namespace std::chrono_literals;

pthread_mutex_t atfork_mutex = PTHREAD_MUTEX_INITIALIZER;
std::atomic<int> atfork_phase{0};

// Reproduces a caller hook that completes normally while another caller thread
// acquires the mutex before fork. That mutex is permanently locked in the child.
void prepare_atfork() {
    pthread_mutex_lock(&atfork_mutex);
    pthread_mutex_unlock(&atfork_mutex);
    atfork_phase.store(1);
    while(atfork_phase.load() != 2) {
        std::this_thread::yield();
    }
}

class Scratch {
public:
    Scratch() {
        auto pattern = (std::filesystem::temp_directory_path() / "ngm process check XXXXXX").string();
        const auto directory = mkdtemp(pattern.data());
        require(directory != nullptr, "create unique scratch directory");
        path = directory;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

ngm::ProcessOptions options_for(const std::filesystem::path& executable, const std::filesystem::path& root,
                                const std::string& name) {
    const auto directory = root / name;
    std::filesystem::create_directories(directory);
    ngm::ProcessOptions options;
    options.executable = executable;
    options.working_directory = directory;
    options.stdout_path = directory / "stdout log.txt";
    options.stderr_path = directory / "stderr log.txt";
    options.timeout = 3s;
    options.terminate_grace = 30ms;
    return options;
}

void require_success(const ngm::ProcessResult& result) {
    require(result.error.empty(), "no supervisor error: " + result.error);
    require(result.exit_code == 0, "target exited successfully");
    require(!result.signal && !result.timed_out && !result.cancelled, "successful exit is unambiguous");
    require(result.cleanup_confirmed, "all owned processes were reaped");
}

// Run in a separate executable invocation because pthread_atfork hooks cannot be
// unregistered and must not affect the rest of this check's process launches.
int atfork_probe(const std::filesystem::path& executable, const std::filesystem::path& scratch) {
    return ngm::check::run([&] {
        auto options = options_for(executable, scratch, "inner atfork launch");
        options.arguments = {"exit", "0"};
        options.timeout = 200ms;
        require(pthread_atfork(prepare_atfork, nullptr, nullptr) == 0, "register inherited atfork hook");
        std::jthread holder([](std::stop_token stop) {
            while(atfork_phase.load() != 1 && !stop.stop_requested()) {
                std::this_thread::yield();
            }
            if(stop.stop_requested()) {
                return;
            }
            pthread_mutex_lock(&atfork_mutex);
            atfork_phase.store(2);
            std::this_thread::sleep_for(100ms);
            pthread_mutex_unlock(&atfork_mutex);
        });
        const auto result = ngm::run_process(options);
        holder.request_stop();
        holder.join();
        require(atfork_phase.load() == 2, "caller hook and contending caller thread actually ran");
        require_success(result);
    });
}

void require_pids_gone(const std::filesystem::path& path) {
    std::ifstream stream(path);
    pid_t child = -1;
    int count = 0;
    while(stream >> child) {
        require(child > 1, "stand-in recorded a valid process identity");
        errno = 0;
        require(kill(child, 0) == -1 && errno == ESRCH, "recorded owned process is gone, including zombies");
        ++count;
    }
    require(count == 3, "target, child, and grandchild actually launched");
}

ngm::ProcessResult cancel_when_ready(const ngm::ProcessOptions& options) {
    std::stop_source stop;
    auto task = std::async(std::launch::async, [&] { return ngm::run_process(options, stop.get_token()); });
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    bool ready = false;
    while(std::chrono::steady_clock::now() < deadline) {
        if(read_file(options.stdout_path).find("ready\n") != std::string::npos) {
            ready = true;
            break;
        }
        if(task.wait_for(0ms) == std::future_status::ready) {
            break;
        }
        std::this_thread::sleep_for(5ms);
    }
    stop.request_stop();
    const auto result = task.get();
    require(ready, "stand-in reached its real execution boundary before cancellation: " + result.error);
    return result;
}

// A separate direct child is deliberately outside the runner's owned subtree.
class UnrelatedChild {
public:
    UnrelatedChild() : pid_(fork()) {
        require(pid_ >= 0, "launch unrelated sentinel");
        if(pid_ == 0) {
            std::this_thread::sleep_for(10s);
            _exit(0);
        }
    }
    ~UnrelatedChild() {
        if(pid_ > 0) {
            kill(pid_, SIGKILL);
            while(waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
    }
    void require_alive() const {
        siginfo_t information{};
        require(waitid(P_PID, static_cast<id_t>(pid_), &information, WEXITED | WNOHANG | WNOWAIT) == 0 &&
                    information.si_pid == 0,
                "unrelated process remains alive and remains the caller's child");
    }

private:
    pid_t pid_;
};
} // namespace

int main(int argc, char** argv) {
    if(argc == 4 && std::string_view(argv[1]) == "--atfork-probe") {
        return atfork_probe(argv[2], argv[3]);
    }
    return ngm::check::run([&] {
        require(argc == 2, "usage: ProcessCheck <stand-in executable>");
        Scratch scratch;
        const auto executable = scratch.path / "stand in executable";
        std::filesystem::copy_file(std::filesystem::absolute(argv[1]), executable);
        std::filesystem::permissions(executable, std::filesystem::perms::owner_all);
        auto options = options_for(executable, scratch.path, "working directory with spaces");
        options.arguments = {"echo",       "two words", "", "'quoted'", "$(touch should-not-exist); `id` $HOME *",
                             "line\nbreak"};
        options.environment = {{"NGM_PROCESS_VALUE", "explicit value with spaces = $HOME"}};
        const char* previous = std::getenv("NGM_PROCESS_PARENT_SECRET");
        const std::string saved_secret = previous == nullptr ? "" : previous;
        setenv("NGM_PROCESS_PARENT_SECRET", "must not leak", 1);
        const auto echoed = ngm::run_process(options);
        if(previous == nullptr) {
            unsetenv("NGM_PROCESS_PARENT_SECRET");
        } else {
            setenv("NGM_PROCESS_PARENT_SECRET", saved_secret.c_str(), 1);
        }
        require_success(echoed);
        std::string expected = "cwd=" + options.working_directory.string() +
                               "\nenv=explicit value with spaces = $HOME\nsecret=<unset>\nstdin_eof=1\n";
        for(std::size_t index = 1; index < options.arguments.size(); ++index) {
            expected += std::to_string(options.arguments[index].size()) + ':' + options.arguments[index] + '\n';
        }
        require(read_file(options.stdout_path) == expected, "argv, explicit environment, cwd, and stdin preserved");
        require(read_file(options.stderr_path) == "standin stderr\n", "stderr is separate from stdout");
        require(!std::filesystem::exists(options.working_directory / "should-not-exist"), "arguments were not a shell");

        const int inherited = open((scratch.path / "inherited.txt").c_str(), O_WRONLY | O_CREAT, 0600);
        require(inherited >= 0, "create deliberately inheritable descriptor");
        auto fd_options = options_for(executable, scratch.path, "descriptor isolation");
        fd_options.arguments = {"check-fd", std::to_string(inherited)};
        const auto fd_result = ngm::run_process(fd_options);
        close(inherited);
        require_success(fd_result);

        for(const int exit : {0, 23, 127}) {
            auto early = options_for(executable, scratch.path, "exit " + std::to_string(exit));
            early.arguments = {"exit", std::to_string(exit)};
            const auto result = ngm::run_process(early);
            require(result.exit_code == exit && result.error.empty() && result.cleanup_confirmed,
                    "application exits, including 127, are not exec failures");
            require(read_file(early.stdout_path) == "early stdout\n" &&
                        read_file(early.stderr_path) == "early stderr\n",
                    "early-exit logs are retained");
        }
        auto signalled = options_for(executable, scratch.path, "signal exit");
        signalled.arguments = {"signal"};
        const auto signal_result = ngm::run_process(signalled);
        require(signal_result.signal == SIGUSR1 && !signal_result.exit_code && signal_result.cleanup_confirmed &&
                    signal_result.error.empty(),
                "signal exit is typed separately");
        auto traced = options_for(executable, scratch.path, "ptrace stop");
        traced.arguments = {"trace-stop"};
        traced.timeout = 120ms;
        const auto trace_started = std::chrono::steady_clock::now();
        const auto trace_result = ngm::run_process(traced);
        require(trace_result.timed_out && !trace_result.cancelled && !trace_result.exit_code &&
                    trace_result.signal == SIGKILL && trace_result.cleanup_confirmed && trace_result.error.empty(),
                "ptrace stop remains live until timeout and real SIGKILL exit: " + read_file(traced.stderr_path));
        require(std::chrono::steady_clock::now() - trace_started >= traced.timeout &&
                    std::chrono::steady_clock::now() - trace_started < 2s,
                "tracing stop neither completes early nor defeats bounded cleanup");
        const auto trace_output = read_file(traced.stdout_path);
        require(trace_output.starts_with("pid=") && trace_output.ends_with("\nready\n"), "traced target actually ran");
        const auto traced_pid = static_cast<pid_t>(std::stol(trace_output.substr(4)));
        errno = 0;
        require(kill(traced_pid, 0) == -1 && errno == ESRCH, "traced target was actually reaped");

        auto hooks = options_for(std::filesystem::absolute(argv[0]), scratch.path, "atfork regression");
        hooks.arguments = {"--atfork-probe", executable.string(), hooks.working_directory.string()};
        hooks.timeout = 5s;
        const auto hook_result = ngm::run_process(hooks);
        require(hook_result.exit_code == 0, "inherited atfork hook regression: " + read_file(hooks.stderr_path));
        require_success(hook_result);

        for(const std::string kind : {"valid", "malformed", "missing"}) {
            auto exported = options_for(executable, scratch.path, kind + " output");
            const auto output = exported.working_directory / "export.json";
            exported.arguments = {"output", output.string(), kind};
            require_success(ngm::run_process(exported));
            if(kind == "missing") {
                require(!std::filesystem::exists(output), "successful process can omit expected evidence");
            } else {
                require(read_file(output) == (kind == "valid" ? "{\"ok\":true}\n" : "{ malformed export\n"),
                        "raw output, including malformed evidence, reaches the real filesystem");
            }
        }

        auto invalid = options_for(scratch.path / "missing executable", scratch.path, "failed exec");
        for(int attempt = 0; attempt < 8; ++attempt) {
            const auto result = ngm::run_process(invalid);
            require(!result.exit_code && result.error.find("execute target") != std::string::npos &&
                        result.cleanup_confirmed,
                    "failed exec is reported without racing with early exit");
        }
        invalid.executable = executable;
        invalid.working_directory /= "does not exist";
        const auto cwd_result = ngm::run_process(invalid);
        require(cwd_result.error.find("working directory") != std::string::npos && cwd_result.cleanup_confirmed,
                "invalid working directory is actionable");
        invalid = options_for(executable, scratch.path, "invalid output");
        invalid.stdout_path = invalid.working_directory / "missing parent" / "out.txt";
        require(ngm::run_process(invalid).error.find("stdout log") != std::string::npos, "missing log parent fails");
        invalid.stdout_path = invalid.stderr_path;
        require(ngm::run_process(invalid).error.find("distinct") != std::string::npos, "shared log inode is rejected");
        invalid = options_for(executable, scratch.path, "invalid argument");
        invalid.arguments = {std::string("bad\0argument", 12)};
        require(ngm::run_process(invalid).error.find("NUL") != std::string::npos, "embedded argument NUL is rejected");

        auto sleeping = options_for(executable, scratch.path, "timeout");
        sleeping.arguments = {"sleep", "10000"};
        sleeping.timeout = 120ms;
        const auto started = std::chrono::steady_clock::now();
        const auto timed = ngm::run_process(sleeping);
        require(timed.timed_out && !timed.cancelled && timed.cleanup_confirmed && timed.signal == SIGTERM &&
                    timed.error.empty(),
                "deadline terminates and reaps a real target");
        require(std::chrono::steady_clock::now() - started < 2s, "timeout cleanup is bounded");
        sleeping = options_for(executable, scratch.path, "cancel");
        sleeping.arguments = {"sleep", "10000"};
        const auto cancelled = cancel_when_ready(sleeping);
        require(cancelled.cancelled && !cancelled.timed_out && cancelled.cleanup_confirmed && cancelled.error.empty(),
                "stop token cancels an executing target");
        auto concurrent = options_for(executable, scratch.path, "concurrent survivor");
        concurrent.arguments = {"sleep", "300"};
        auto survivor = std::async(std::launch::async, [&] { return ngm::run_process(concurrent); });
        sleeping = options_for(executable, scratch.path, "concurrent cancellation");
        sleeping.arguments = {"sleep", "10000"};
        const auto concurrent_cancelled = cancel_when_ready(sleeping);
        const auto concurrent_survived = survivor.get();
        require(concurrent_cancelled.cancelled && concurrent_cancelled.cleanup_confirmed &&
                    concurrent_cancelled.error.empty(),
                "one concurrent invocation can be cancelled cleanly");
        require_success(concurrent_survived);
        require(read_file(concurrent.stdout_path) == "ready\n", "other concurrent invocation retains its own output");
        std::stop_source stopped;
        stopped.request_stop();
        auto unlaunched = options_for(executable, scratch.path, "already cancelled");
        const auto pre_cancelled = ngm::run_process(unlaunched, stopped.get_token());
        require(pre_cancelled.cancelled && pre_cancelled.cleanup_confirmed && !pre_cancelled.exit_code &&
                    !std::filesystem::exists(unlaunched.stdout_path),
                "a pre-cancelled launch creates no target or logs");

        UnrelatedChild unrelated;
        for(const std::string leader : {"exit", "wait"}) {
            for(const std::string group : {"same-group", "escape"}) {
                auto children = options_for(executable, scratch.path, leader + " descendants " + group);
                const auto pids = children.working_directory / "pids.txt";
                children.arguments = {"descendants", pids.string(), leader, group};
                children.timeout = 200ms;
                const auto result = ngm::run_process(children);
                require(result.cleanup_confirmed && result.error.empty(), "descendant cleanup: " + result.error);
                if(leader == "exit") {
                    require_success(result);
                } else {
                    require(result.timed_out && result.signal == SIGKILL, "stubborn process group reaches KILL");
                }
                require_pids_gone(pids);
                unrelated.require_alive();
            }
        }
        auto children = options_for(executable, scratch.path, "cancel descendants");
        const auto pids = children.working_directory / "pids.txt";
        children.arguments = {"descendants", pids.string(), "wait", "escape"};
        const auto child_cancelled = cancel_when_ready(children);
        require(child_cancelled.cancelled && child_cancelled.cleanup_confirmed && child_cancelled.signal == SIGKILL &&
                    child_cancelled.error.empty(),
                "cancellation cleans up stubborn escaped descendants");
        require_pids_gone(pids);
        unrelated.require_alive();
    });
}
