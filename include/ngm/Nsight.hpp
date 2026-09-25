#pragma once

#include "ngm/Capabilities.hpp"
#include "ngm/NsightEvidence.hpp"
#include "ngm/Process.hpp"
#include "ngm/ProfileEvidence.hpp"
#include "ngm/ProfileSettings.hpp"

#include <cstdint>
#include <set>
#include <string_view>

namespace ngm {
// Every directory belongs to the caller's isolated attempt. Environment is the
// complete tool/target environment; the backend does not inherit the server's.
struct NsightRunContext {
    std::filesystem::path log_directory;
    std::map<std::string, std::string> environment;
    std::chrono::milliseconds timeout{120000};
    std::chrono::milliseconds terminate_grace{500};
};

struct NsightInspectionOptions {
    std::optional<std::filesystem::path> installation_root;
    NsightRunContext context;
};

struct NsightToolObservation {
    std::string name;
    std::optional<std::filesystem::path> path;
    std::string version;
    std::string build;
    bool version_valid = false;
    bool help_valid = false;
    bool accepted_discovery_exit_one = false;
    ProcessResult version_process;
    ProcessResult help_process;
    std::filesystem::path version_stdout;
    std::filesystem::path version_stderr;
    std::filesystem::path help_stdout;
    std::filesystem::path help_stderr;
    std::set<std::string> documented_options;
    std::string problem;
};

struct NsightInstallation {
    PrerequisiteObservations discovery;
    NsightToolObservation cli;
    NsightToolObservation capture;
    NsightToolObservation replay;
    // Missing/unattempted tools own no process. Any unconfirmed probe cleanup
    // makes this false, even if later observations otherwise look usable.
    bool cleanup_confirmed = true;
    // Matching tools, recognizable version/help, and basic documented options.
    // This is an interface observation, never a claim of GPU compatibility.
    bool interface_ready = false;
    std::string problem;
};

// Inspects only --version and --help/--help-all, retaining their exact logs. The
// ngfx discovery-only exit-one quirk requires recognizable output; capture and
// replay commands never accept exit one as success. Existing logs are rejected.
NsightInstallation inspect_nsight(const NsightInspectionOptions& options, std::stop_token stop = {});

enum class NsightDelimiter { Present, VulkanFrameBoundary, GraphicsCaptureApi };

struct NsightCaptureOptions {
    NsightRunContext context;
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    std::filesystem::path capture_file;
    // Nsight numbers delimiters from one. Current help additionally says > 1;
    // this adapter conservatively requires >= 2 and always sets the delimiter.
    std::uint64_t capture_frame = 2;
    std::uint32_t frame_count = 1;
    NsightDelimiter delimiter = NsightDelimiter::Present;
};

enum class NsightExportKind { Metadata, Functions, Objects, Logs, Screenshot };

struct NsightExportOptions {
    NsightRunContext context;
    std::filesystem::path capture_file;
    // Required retained provenance, compared exactly with the selected tools.
    std::string capture_tool_version;
    std::string capture_tool_build;
    NsightExportKind kind = NsightExportKind::Metadata;
    std::filesystem::path output_file;
    std::size_t maximum_bytes = 64U * 1024U * 1024U;
};

enum class NsightOutcome {
    Success,
    InvalidInput,
    Unavailable,
    Failed,
    InvalidOutput,
    TimedOut,
    Cancelled,
    CleanupFailed
};
std::string_view nsight_outcome_name(NsightOutcome outcome);
std::string_view nsight_export_name(NsightExportKind kind);

struct NsightOperationResult {
    NsightOutcome outcome = NsightOutcome::InvalidInput;
    ProcessResult process;
    bool launched = false;
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path stdout_path;
    std::filesystem::path stderr_path;
    std::filesystem::path output_file;
    std::uint64_t output_bytes = 0;
    std::string message;
};

struct NsightCppCaptureOptions {
    NsightRunContext context;
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    // Must not exist. ngfx creates its timestamped project below this directory.
    std::filesystem::path output_directory;
    std::uint64_t wait_frames = 2;
};

struct NsightCppCaptureResult {
    NsightOperationResult operation;
    std::filesystem::path project_directory;
    NsightCppMetadata metadata;
    // Relative to project_directory. Generated source IDs are local to this
    // capture; they must not be joined to another capture's event inventories.
    std::vector<std::filesystem::path> source_files;
};

// Documented Generate C++ Capture on the two explicitly qualified Vulkan
// producers. Uses the same safe-token argument boundary as graphics capture.
// Success checks one project, matching metadata, required regular source/data
// files, and BMP framing. It does not compile source, decode data.bin, execute
// GPU replay, or establish arbitrary resource contents at an event.
NsightCppCaptureResult run_nsight_cpp_capture(const NsightInstallation& installation,
                                              const NsightCppCaptureOptions& options, std::stop_token stop = {});

struct NsightProfileOptions {
    NsightRunContext context;
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    std::filesystem::path output_directory;
    ProfileSettings settings;
};
struct NsightProfileResult {
    NsightOperationResult operation;
    ProfileReproduction reproduction;
    std::filesystem::path trace_file;
    std::filesystem::path reproduction_file;
    std::map<std::string, std::filesystem::path> tables;
};
// GPU Trace auto-export on the two observed producer profiles. Always leaves
// clocks unaltered, disables screenshots and uses a fresh target. No permission
// changes, GPU replay, private-format parsing or inferred metric units.
NsightProfileResult run_nsight_profile(const NsightInstallation& installation, const NsightProfileOptions& options,
                                       std::stop_token stop = {});

// No shell is used. Because the documented --args string does not define its
// escaping rules, this adapter accepts only nonempty ASCII tokens composed of
// letters, digits, '_', '-', '.', '/', ':', '=', ',', '+', '@', and '%'. Other
// target arguments are rejected, never rewritten. Executable/cwd paths remain
// independent argv entries and can contain spaces. This is an adapter limit,
// not a claim that Nsight cannot represent other arguments.
NsightOperationResult run_nsight_capture(const NsightInstallation& installation, const NsightCaptureOptions& options,
                                         std::stop_token stop = {});

// Each export uses a separate ngfx-replay process. Success requires exit zero,
// confirmed cleanup, and a readable regular output within maximum_bytes. Raw
// metadata/functions must be nonempty; logs may be empty. Objects must parse as
// JSON, without assuming a schema. Screenshot bytes remain opaque until actual
// format validation is implemented. A capture file alone is not a readable-
// capture guarantee; callers must additionally obtain successful metadata.
// Existing output and log files are rejected. Input capture must remain leased.
// An export absent from the selected replayer's help returns Unavailable without
// launching it; this is a capability limit, not malformed caller input.
NsightOperationResult export_nsight_capture(const NsightInstallation& installation, const NsightExportOptions& options,
                                            std::stop_token stop = {});
} // namespace ngm
