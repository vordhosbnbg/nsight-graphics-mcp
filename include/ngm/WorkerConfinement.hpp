#pragma once

#include <filesystem>

namespace ngm {
// Irreversible, single-threaded worker-only boundary. Call after exec, before
// parsing untrusted data. Closes every inherited descriptor above stderr.
// Allows read access only to the two regular files named below, stdout/stderr
// writes, and the syscalls needed by the qualified database reader and C++ runtime.
// Launch through run_process with an empty environment, /dev/null stdin, and
// regular write-only log files for stdout/stderr. The output-size rlimit does
// not bound a pipe or terminal. stat/readlink can still observe path metadata.
// Requires Linux x86-64, Landlock ABI >= 3 and seccomp filtering. Throws on any
// setup failure; the caller MUST exit, never continue with the reader.
// The caller supplies an immutable private snapshot and bounds wall time via
// run_process. This does not authenticate data or make helper output trustworthy.
void confine_resource_worker(const std::filesystem::path& database, const std::filesystem::path& records);
} // namespace ngm
