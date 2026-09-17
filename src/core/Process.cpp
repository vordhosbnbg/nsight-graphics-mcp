#include "ngm/Process.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace ngm {
namespace {
constexpr std::int64_t kill_budget_ms = 2000;
constexpr int poll_ms = 10;

class Descriptor {
public:
    explicit Descriptor(int value = -1) : value_(value) {}
    ~Descriptor() {
        if(value_ >= 0) {
            close(value_);
        }
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const {
        return value_;
    }
    void reset() {
        if(value_ >= 0) {
            close(value_);
            value_ = -1;
        }
    }

private:
    int value_;
};

enum class Stage : int {
    none,
    descriptors,
    signals,
    session,
    subreaper,
    fork_target,
    process_group,
    directory,
    execute,
    wait,
    children,
    terminate,
    cleanup
};

struct LaunchError {
    Stage stage = Stage::none;
    int number = 0;
};

struct Report {
    std::uint32_t magic = 0x4e474d50;
    LaunchError failure;
    int exit_code = -1;
    int signal = 0;
    bool timed_out = false;
    bool cancelled = false;
    bool cleanup_confirmed = false;
};

struct Launch {
    const char* executable;
    char* const* arguments;
    char* const* environment;
    int directory;
    int input;
    int output;
    int error;
    int socket;
    int exec_read;
    int exec_write;
    std::array<int, 4> keep;
    std::int64_t timeout_ms;
    std::int64_t grace_ms;
};

std::int64_t monotonic_ms() {
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return static_cast<std::int64_t>(time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

void remember_failure(Report& report, Stage stage, int number) {
    if(report.failure.stage == Stage::none) {
        report.failure = {stage, number};
    }
}

[[noreturn]] void send_report(int socket, const Report& report) {
    ssize_t count;
    do {
        count = send(socket, &report, sizeof(report), MSG_NOSIGNAL);
    } while(count < 0 && errno == EINTR);
    _exit(count == sizeof(report) ? 0 : 125);
}

// Only syscall-style operations and preallocated storage are used after fork.
// In particular, do not allocate C++ strings or use iostreams in the supervisor.
bool close_except(const int* keep, std::size_t size) {
    unsigned int first = 3;
    for(std::size_t index = 0; index < size; ++index) {
        const auto next = static_cast<unsigned int>(keep[index]);
        if(first < next && syscall(SYS_close_range, first, next - 1, 0) != 0) {
            return false;
        }
        first = next + 1;
    }
    return syscall(SYS_close_range, first, UINT_MAX, 0) == 0;
}

bool reset_signals() {
    struct sigaction action{};
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);
    for(int number = 1; number < NSIG; ++number) {
        if(number != SIGKILL && number != SIGSTOP && sigaction(number, &action, nullptr) != 0 && errno != EINVAL) {
            return false;
        }
    }
    sigset_t mask;
    sigemptyset(&mask);
    return sigprocmask(SIG_SETMASK, &mask, nullptr) == 0;
}

[[noreturn]] void fail_launch(int descriptor, Stage stage) {
    const LaunchError failure{stage, errno};
    ssize_t count;
    do {
        count = write(descriptor, &failure, sizeof(failure));
    } while(count < 0 && errno == EINTR);
    _exit(127);
}

[[noreturn]] void execute(const Launch& launch) {
    close(launch.socket);
    close(launch.exec_read);
    if(setpgid(0, 0) != 0) {
        fail_launch(launch.exec_write, Stage::process_group);
    }
    if(fchdir(launch.directory) != 0) {
        fail_launch(launch.exec_write, Stage::directory);
    }
    close(launch.directory);
    execve(launch.executable, launch.arguments, launch.environment);
    fail_launch(launch.exec_write, Stage::execute);
}

bool observe(pid_t child, siginfo_t& information) {
    information = {};
    int status;
    do {
        status = waitid(P_PID, static_cast<id_t>(child), &information, WEXITED | WNOHANG | WNOWAIT | __WALL);
    } while(status != 0 && errno == EINTR);
    return status == 0;
}

bool exited(const siginfo_t& information) {
    // Linux can report ptrace stops even without WSTOPPED. A nonzero si_pid
    // therefore does not imply termination or make this PID safe to release.
    return information.si_pid != 0 && (information.si_code == CLD_EXITED || information.si_code == CLD_KILLED ||
                                       information.si_code == CLD_DUMPED);
}

void record_exit(Report& report, const siginfo_t& information) {
    if(information.si_code == CLD_EXITED) {
        report.exit_code = information.si_status;
    } else if(information.si_code == CLD_KILLED || information.si_code == CLD_DUMPED) {
        report.signal = information.si_status;
    }
}

enum class Reap { reaped, pending, failed };

Reap reap(pid_t child) {
    int information = 0;
    pid_t status;
    do {
        status = waitpid(child, &information, WNOHANG | __WALL);
    } while(status < 0 && errno == EINTR);
    if(status < 0) {
        return Reap::failed;
    }
    // waitpid also returns the PID for ptrace stops, without reaping it.
    return status == child && (WIFEXITED(information) || WIFSIGNALED(information)) ? Reap::reaped : Reap::pending;
}

// Children listed here belong exclusively to this single-threaded supervisor.
// Each live child remains unreaped while it is signaled, preventing PID reuse.
bool sweep_child(pid_t child, pid_t target, int signal, bool& live, Report& report) {
    siginfo_t information{};
    if(!observe(child, information)) {
        if(errno == ECHILD) {
            return true;
        }
        remember_failure(report, Stage::wait, errno);
        return false;
    }
    if(exited(information)) {
        if(child == target) {
            record_exit(report, information);
        } else {
            const auto reaped = reap(child);
            if(reaped == Reap::failed) {
                remember_failure(report, Stage::wait, errno);
                return false;
            }
            live = live || reaped == Reap::pending;
        }
    } else {
        live = true;
        if(kill(child, signal) != 0 && errno != ESRCH) {
            remember_failure(report, Stage::terminate, errno);
            return false;
        }
    }
    return true;
}

bool sweep_children(pid_t target, int signal, bool& live, Report& report) {
    const int descriptor = open("/proc/thread-self/children", O_RDONLY | O_CLOEXEC);
    if(descriptor < 0) {
        remember_failure(report, Stage::children, errno);
        return false;
    }
    char buffer[4096];
    unsigned int child = 0;
    bool valid = true;
    while(valid) {
        const auto count = read(descriptor, buffer, sizeof(buffer));
        if(count < 0) {
            if(errno == EINTR) {
                continue;
            }
            remember_failure(report, Stage::children, errno);
            valid = false;
            break;
        }
        if(count == 0) {
            break;
        }
        for(ssize_t index = 0; index < count; ++index) {
            const char value = buffer[index];
            if(value >= '0' && value <= '9' && child <= static_cast<unsigned int>(INT_MAX) / 10 - 1) {
                child = child * 10 + static_cast<unsigned int>(value - '0');
            } else if(value == ' ' || value == '\n') {
                if(child != 0) {
                    valid = sweep_child(static_cast<pid_t>(child), target, signal, live, report) && valid;
                    child = 0;
                }
            } else {
                remember_failure(report, Stage::children, EINVAL);
                valid = false;
                break;
            }
        }
    }
    if(child != 0 && valid) {
        valid = sweep_child(static_cast<pid_t>(child), target, signal, live, report);
    }
    close(descriptor);
    return valid;
}

pid_t fork_target() {
    // Calling libc fork here would rerun the caller's pthread_atfork handlers
    // in a child that can contain locks left held by vanished caller threads.
    // No allocator, thread runtime, or other non-async-safe code runs afterward.
#ifdef SYS_fork
    return static_cast<pid_t>(syscall(SYS_fork));
#else
    // Do not guess at clone's architecture-specific argument ordering.
    errno = ENOSYS;
    return -1;
#endif
}

[[noreturn]] void supervise(const Launch& launch) {
    Report report;
    if(dup2(launch.input, STDIN_FILENO) < 0 || dup2(launch.output, STDOUT_FILENO) < 0 ||
       dup2(launch.error, STDERR_FILENO) < 0 || !close_except(launch.keep.data(), launch.keep.size())) {
        report.failure = {Stage::descriptors, errno};
        report.cleanup_confirmed = true;
        send_report(launch.socket, report);
    }
    if(!reset_signals()) {
        report.failure = {Stage::signals, errno};
        report.cleanup_confirmed = true;
        send_report(launch.socket, report);
    }
    if(setsid() < 0 || prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0) {
        report.failure = {Stage::subreaper, errno};
        report.cleanup_confirmed = true;
        send_report(launch.socket, report);
    }
    const auto started = monotonic_ms();
    const pid_t target = fork_target();
    if(target < 0) {
        report.failure = {Stage::fork_target, errno};
        report.cleanup_confirmed = true;
        send_report(launch.socket, report);
    }
    if(target == 0) {
        execute(launch);
    }
    close(launch.exec_write);
    close(launch.directory);
    // The target also sets its group before exec; EACCES means it got there first.
    if(setpgid(target, target) != 0 && errno != EACCES) {
        remember_failure(report, Stage::process_group, errno);
    }
    bool cleaning = report.failure.stage != Stage::none;
    bool target_reaped = false;
    bool exec_open = true;
    bool sent_term = false;
    bool sent_kill = false;
    auto cleanup_started = monotonic_ms();
    while(true) {
        const auto now = monotonic_ms();
        siginfo_t information{};
        if(!target_reaped && !observe(target, information)) {
            remember_failure(report, Stage::wait, errno);
            // No longer holding the leader means group signaling is unsafe.
            target_reaped = true;
        }
        if(exited(information)) {
            record_exit(report, information);
        }
        // Observe exit before draining exec status so an exec failure cannot be
        // mistaken for an application that happened to return 127.
        if(exec_open) {
            LaunchError failure;
            const auto count = read(launch.exec_read, &failure, sizeof(failure));
            if(count == sizeof(failure)) {
                remember_failure(report, failure.stage, failure.number);
            } else if(count > 0) {
                remember_failure(report, Stage::execute, EIO);
            }
            if(count == 0 || count > 0) {
                close(launch.exec_read);
                exec_open = false;
            } else if(errno != EAGAIN && errno != EINTR) {
                remember_failure(report, Stage::execute, errno);
            }
        }
        if(!cleaning) {
            if(exited(information) || report.failure.stage != Stage::none) {
                cleaning = true;
            } else {
                char command;
                const auto count = recv(launch.socket, &command, 1, MSG_DONTWAIT);
                if(count >= 0 || (errno != EAGAIN && errno != EINTR)) {
                    report.cancelled = true; // Explicit stop or the caller disappeared.
                    cleaning = true;
                } else if(now - started >= launch.timeout_ms) {
                    report.timed_out = true;
                    cleaning = true;
                }
            }
            if(cleaning) {
                cleanup_started = now;
            }
        }
        if(cleaning) {
            const auto elapsed = now - cleanup_started;
            const int signal = elapsed >= launch.grace_ms ? SIGKILL : SIGTERM;
            if(!target_reaped && ((!sent_term && signal == SIGTERM) || (!sent_kill && signal == SIGKILL))) {
                if(kill(-target, signal) != 0 && errno != ESRCH) {
                    remember_failure(report, Stage::terminate, errno);
                }
                sent_term = sent_term || signal == SIGTERM;
                sent_kill = sent_kill || signal == SIGKILL;
            }
            bool live = false;
            const bool swept = sweep_children(target_reaped ? -1 : target, signal, live, report);
            if(!target_reaped && exited(information) && swept && !live) {
                const auto reaped = reap(target);
                target_reaped = reaped == Reap::reaped;
                if(reaped == Reap::failed) {
                    remember_failure(report, Stage::wait, errno);
                }
            }
            if(target_reaped) {
                // /proc is only an enumeration aid. ECHILD is the authoritative
                // confirmation, including children adopted during the sweep.
                siginfo_t remaining{};
                const auto state = waitid(P_ALL, 0, &remaining, WEXITED | WNOHANG | WNOWAIT | __WALL);
                if(state < 0 && errno == ECHILD) {
                    report.cleanup_confirmed = true;
                    send_report(launch.socket, report);
                }
                if(state < 0 && errno != EINTR) {
                    remember_failure(report, Stage::wait, errno);
                }
            }
            if(elapsed >= launch.grace_ms && elapsed - launch.grace_ms >= kill_budget_ms) {
                remember_failure(report, Stage::cleanup, ETIMEDOUT);
                send_report(launch.socket, report);
            }
        }
        pollfd descriptor{launch.socket, POLLIN, 0};
        // Avoid a busy loop after a stop byte or parent EOF during cleanup.
        poll(cleaning ? nullptr : &descriptor, cleaning ? 0 : 1, poll_ms);
    }
}

const char* stage_name(Stage stage) {
    switch(stage) {
        case Stage::none:
            return "none";
        case Stage::descriptors:
            return "isolate process file descriptors (requires Linux close_range)";
        case Stage::signals:
            return "reset child signals";
        case Stage::session:
            return "create owned process session";
        case Stage::subreaper:
            return "create private child subreaper/session";
        case Stage::fork_target:
            return "fork target (requires Linux SYS_fork)";
        case Stage::process_group:
            return "create owned process group";
        case Stage::directory:
            return "enter working directory";
        case Stage::execute:
            return "execute target";
        case Stage::wait:
            return "observe/reap owned child";
        case Stage::children:
            return "enumerate owned descendants through /proc";
        case Stage::terminate:
            return "signal owned process";
        case Stage::cleanup:
            return "confirm owned-process cleanup within deadline";
    }
    return "unknown process stage";
}

ProcessResult failure(std::string message, int number = 0) {
    ProcessResult result;
    result.cleanup_confirmed = true; // No process was started.
    result.error = std::move(message);
    if(number != 0) {
        result.error += ": ";
        result.error += std::strerror(number);
    }
    return result;
}

bool has_null(const std::string& value) {
    return value.find('\0') != std::string::npos;
}

// Keep internal descriptors away from stdio even when the caller closed it.
int above_stdio(int descriptor) {
    if(descriptor < 0 || descriptor > STDERR_FILENO) {
        return descriptor;
    }
    const int copy = fcntl(descriptor, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    const int error = errno;
    close(descriptor);
    errno = error;
    return copy;
}
} // namespace

ProcessResult run_process(const ProcessOptions& options, std::stop_token stop) {
    if(stop.stop_requested()) {
        ProcessResult result;
        result.cancelled = true;
        result.cleanup_confirmed = true;
        return result;
    }
    if(options.timeout.count() <= 0 || options.terminate_grace.count() < 0) {
        return failure("Process timeout must be positive and termination grace must be nonnegative");
    }
    if(options.executable.empty() || options.stdout_path.empty() || options.stderr_path.empty()) {
        return failure("Executable, stdout log path, and stderr log path are required");
    }
    struct sigaction child_action{};
    if(sigaction(SIGCHLD, nullptr, &child_action) != 0) {
        return failure("Read caller SIGCHLD disposition", errno);
    }
    if(child_action.sa_handler != SIG_DFL || (child_action.sa_flags & SA_NOCLDWAIT) != 0) {
        return failure(
            "run_process requires the default SIGCHLD disposition and exclusive ownership of its supervisor");
    }
    std::error_code error;
    const auto cwd = std::filesystem::current_path(error);
    if(error) {
        return failure("Resolve caller working directory: " + error.message());
    }
    const auto absolute = [&cwd](const std::filesystem::path& path) { return path.is_absolute() ? path : cwd / path; };
    const auto executable = absolute(options.executable).string();
    const auto working = (options.working_directory.empty() ? cwd : absolute(options.working_directory)).string();
    const auto output_path = absolute(options.stdout_path).string();
    const auto error_path = absolute(options.stderr_path).string();
    if(has_null(executable) || has_null(working) || has_null(output_path) || has_null(error_path)) {
        return failure("Process paths must not contain NUL bytes");
    }
    std::vector<std::string> argument_strings{executable};
    argument_strings.insert(argument_strings.end(), options.arguments.begin(), options.arguments.end());
    std::vector<char*> arguments;
    for(auto& value : argument_strings) {
        if(has_null(value)) {
            return failure("Process arguments must not contain NUL bytes");
        }
        arguments.push_back(value.data());
    }
    arguments.push_back(nullptr);
    std::vector<std::string> environment_strings;
    for(const auto& [key, value] : options.environment) {
        if(key.empty() || key.find('=') != std::string::npos || has_null(key) || has_null(value)) {
            return failure("Process environment contains an invalid name or NUL byte");
        }
        environment_strings.push_back(key + '=' + value);
    }
    std::vector<char*> environment;
    for(auto& value : environment_strings) {
        environment.push_back(value.data());
    }
    environment.push_back(nullptr);

    const Descriptor directory(above_stdio(open(working.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC)));
    if(directory.get() < 0) {
        return failure("Open working directory '" + working + "'", errno);
    }
    const Descriptor input(above_stdio(open("/dev/null", O_RDONLY | O_CLOEXEC)));
    if(input.get() < 0) {
        return failure("Open process stdin /dev/null", errno);
    }
    constexpr int log_flags = O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
    const Descriptor output(above_stdio(open(output_path.c_str(), log_flags, 0600)));
    if(output.get() < 0) {
        return failure("Open stdout log '" + output_path + "'", errno);
    }
    const Descriptor error_output(above_stdio(open(error_path.c_str(), log_flags, 0600)));
    if(error_output.get() < 0) {
        return failure("Open stderr log '" + error_path + "'", errno);
    }
    struct stat output_stat{}, error_stat{};
    if(fstat(output.get(), &output_stat) != 0 || fstat(error_output.get(), &error_stat) != 0) {
        return failure("Inspect process log files", errno);
    }
    if(!S_ISREG(output_stat.st_mode) || !S_ISREG(error_stat.st_mode)) {
        return failure("Process logs must be regular files");
    }
    if(output_stat.st_dev == error_stat.st_dev && output_stat.st_ino == error_stat.st_ino) {
        return failure("Process stdout and stderr logs must be distinct files");
    }
    if(ftruncate(output.get(), 0) != 0 || ftruncate(error_output.get(), 0) != 0) {
        return failure("Truncate process logs", errno);
    }
    int sockets[2];
    if(socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, sockets) != 0) {
        return failure("Create process supervisor channel", errno);
    }
    const Descriptor parent_socket(above_stdio(sockets[0]));
    Descriptor child_socket(above_stdio(sockets[1]));
    int pipes[2];
    if(pipe2(pipes, O_CLOEXEC | O_NONBLOCK) != 0) {
        return failure("Create process exec status pipe", errno);
    }
    Descriptor exec_read(above_stdio(pipes[0]));
    Descriptor exec_write(above_stdio(pipes[1]));
    if(parent_socket.get() < 0 || child_socket.get() < 0 || exec_read.get() < 0 || exec_write.get() < 0) {
        return failure("Reserve internal process descriptors", errno);
    }
    Launch launch{executable.c_str(),
                  arguments.data(),
                  environment.data(),
                  directory.get(),
                  input.get(),
                  output.get(),
                  error_output.get(),
                  child_socket.get(),
                  exec_read.get(),
                  exec_write.get(),
                  {directory.get(), child_socket.get(), exec_read.get(), exec_write.get()},
                  options.timeout.count(),
                  options.terminate_grace.count()};
    std::sort(launch.keep.begin(), launch.keep.end());
    const auto started = monotonic_ms();
    const auto supervisor = fork();
    if(supervisor < 0) {
        return failure("Fork process supervisor", errno);
    }
    if(supervisor == 0) {
        supervise(launch);
    }
    // The parent's duplicate would otherwise suppress detection of supervisor EOF.
    child_socket.reset();
    exec_read.reset();
    exec_write.reset();
    bool stop_sent = false;
    bool received = false;
    Report report;
    std::int64_t cleanup_requested = -1;
    while(true) {
        const auto count = recv(parent_socket.get(), &report, sizeof(report), MSG_DONTWAIT);
        if(count == sizeof(report) && report.magic == 0x4e474d50) {
            received = true;
        }
        siginfo_t information{};
        if(!observe(supervisor, information)) {
            received = false;
            break;
        }
        if(exited(information)) {
            if(!received) {
                const auto final_count = recv(parent_socket.get(), &report, sizeof(report), MSG_DONTWAIT);
                received = final_count == sizeof(report) && report.magic == 0x4e474d50;
            }
            const auto reaped = reap(supervisor);
            if(reaped == Reap::reaped) {
                break;
            }
            if(reaped == Reap::failed) {
                received = false;
                break;
            }
        }
        const auto now = monotonic_ms();
        if(!stop_sent && stop.stop_requested()) {
            const char command = 'C';
            send(parent_socket.get(), &command, 1, MSG_NOSIGNAL);
            stop_sent = true;
            cleanup_requested = now;
        }
        if(cleanup_requested < 0 && now - started >= options.timeout.count()) {
            cleanup_requested = now;
        }
        if(cleanup_requested >= 0 && now - cleanup_requested >= options.terminate_grace.count() &&
           now - cleanup_requested - options.terminate_grace.count() >= kill_budget_ms + 1000) {
            // The unreaped direct child pins this PID. Never guess at a target
            // group after its supervisor has failed; cleanup is unconfirmed.
            kill(supervisor, SIGKILL);
            const auto killed = monotonic_ms();
            while(monotonic_ms() - killed < 1000) {
                if(reap(supervisor) == Reap::reaped) {
                    break;
                }
                poll(nullptr, 0, poll_ms);
            }
            received = false;
            break;
        }
        pollfd descriptor{parent_socket.get(), POLLIN, 0};
        poll(&descriptor, 1, poll_ms);
    }
    ProcessResult result;
    if(!received) {
        result.error = "Process supervisor failed before reporting confirmed cleanup";
        return result;
    }
    result.exit_code = report.exit_code >= 0 ? std::optional<int>(report.exit_code) : std::nullopt;
    result.signal = report.signal != 0 ? std::optional<int>(report.signal) : std::nullopt;
    result.timed_out = report.timed_out;
    result.cancelled = report.cancelled;
    result.cleanup_confirmed = report.cleanup_confirmed;
    if(report.failure.stage != Stage::none) {
        result.error = std::string(stage_name(report.failure.stage)) + " for '" + executable +
                       "': " + std::strerror(report.failure.number);
        if(report.failure.stage == Stage::execute) {
            result.exit_code.reset(); // _exit(127) was supervisor setup, not the application.
        }
    }
    return result;
}
} // namespace ngm
