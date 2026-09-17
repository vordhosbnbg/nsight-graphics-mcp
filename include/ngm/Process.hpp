#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace ngm {
struct ProcessOptions {
    std::filesystem::path executable;
    // Arguments exclude argv[0]; no shell or PATH lookup is used.
    std::vector<std::string> arguments;
    // Empty selects the caller's current directory. Relative paths, including
    // executable and log paths, are resolved against the caller's directory.
    std::filesystem::path working_directory;
    std::filesystem::path stdout_path;
    std::filesystem::path stderr_path;
    // This is the complete environment, not additions to the caller's environment.
    // Supply required display/session variables explicitly. stdin is /dev/null.
    std::map<std::string, std::string> environment;
    std::chrono::milliseconds timeout{30000};
    std::chrono::milliseconds terminate_grace{500};
};

struct ProcessResult {
    std::optional<int> exit_code;
    std::optional<int> signal;
    bool timed_out = false;
    bool cancelled = false;
    bool cleanup_confirmed = false;
    // Setup, exec, supervision, or cleanup error. An ordinary nonzero exit has
    // an exit_code and no error; absence of an expected output is caller policy.
    std::string error;
};

// Linux synchronous primitive. Each invocation has its own session, process group
// and child-subreaper supervisor; it does not change the caller's subreaper state.
// Logs are distinct regular files, truncated on launch, with no inherited file
// descriptors reaching exec. Requires /proc, close_range (Linux 5.9+), and an
// architecture exposing SYS_fork; otherwise launch reports ENOSYS. The private
// supervisor uses the raw fork syscall to avoid rerunning inherited atfork hooks.
//
// Cleanup starts on every completion, including normal exit, and uses TERM then
// KILL, with up to two seconds after terminate_grace to confirm reaping. The group
// leader is retained until group signaling ends to prevent PID/group reuse.
// Stopped or traced children remain live until a real exit or terminating signal.
// Orphan descendants, including double-fork/setsid children, are reaped by the
// private supervisor. This is not containment of hostile applications: processes
// launched through an external service are not descendants; privilege changes
// can prevent signaling, and uninterruptible kernel waits can exceed the budget.
// Such cleanup failures must not be treated as a released GPU reservation.
// The caller must leave SIGCHLD at its default disposition and must not reap this
// function's private supervisor (e.g. with a concurrent waitpid(-1)).
ProcessResult run_process(const ProcessOptions& options, std::stop_token stop = {});
} // namespace ngm
