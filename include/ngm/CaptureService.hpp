#pragma once

#include "ngm/Artifacts.hpp"
#include "ngm/Jobs.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
enum class CaptureFormat { Graphics, Cpp };
enum class CaptureDelimiter { Present, GraphicsCaptureApi };
std::string_view capture_delimiter_name(CaptureDelimiter delimiter);
CaptureDelimiter parse_capture_delimiter(std::string_view name);

struct CaptureServiceOptions {
    ArtifactOptions artifacts;
    std::optional<std::filesystem::path> nsight_root;
    // Explicit desktop/runtime inputs. HOME and per-user XDG/temp directories
    // are replaced inside the attempt. XDG_DATA_DIRS is preserved, with the XDG
    // specification's defaults supplied when unset/empty for driver discovery.
    std::map<std::string, std::string> environment;
};

struct CaptureRequest {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    CaptureFormat format = CaptureFormat::Graphics;
    std::uint64_t capture_frame = 2;
    // Generate C++ Capture's distinct --wait-frames control, not a graphics
    // capture delimiter ordinal. SDK delimiters are unavailable in this mode.
    std::uint64_t cpp_wait_frames = 2;
    CaptureDelimiter delimiter = CaptureDelimiter::Present;
    std::chrono::milliseconds timeout{120000};
    bool pin = false;
    // Optional source-controlled fixture/app convention: append this option and
    // a fresh raw/application path. Empty leaves the supplied argv unchanged.
    std::string application_output_option;
    // Supplemental caller-provided provenance is labelled separately from
    // executable hashes and tool observations made by this service.
    nlohmann::json application_provenance = nlohmann::json::object();
};

struct CaptureSubmission {
    JobIdentity identity;
    std::string artifact_id;
};

// Composes the independently tested storage, coordinator, and documented CLI
// adapter. All jobs use one local-GPU reservation. Construction claims the
// artifact root; callers should construct lazily for discovery-only sessions.
class CaptureService {
public:
    explicit CaptureService(CaptureServiceOptions options);
    ~CaptureService();
    CaptureService(const CaptureService&) = delete;
    CaptureService& operator=(const CaptureService&) = delete;

    CaptureSubmission capture(CaptureRequest request);
    std::optional<JobSnapshot> status(const std::string& job_id) const;
    std::optional<JobSnapshot> wait(const std::string& job_id, std::chrono::milliseconds timeout) const;
    JobCancelResult cancel(const std::string& job_id);
    JobShutdownReport shutdown(std::chrono::milliseconds timeout);
    ArtifactStore& artifacts() noexcept;

private:
    CaptureServiceOptions options_;
    ArtifactStore artifacts_;
    // Destroy/join jobs before their artifact store and configuration.
    JobCoordinator jobs_;
};

// Forward session locations and XDG_DATA_DIRS, not authentication contents.
// Other variables must be explicitly supplied by the caller of the core service.
std::map<std::string, std::string> capture_environment();
void to_json(nlohmann::json& json, const JobIdentity& value);
void to_json(nlohmann::json& json, const JobSnapshot& value);
void to_json(nlohmann::json& json, const CaptureSubmission& value);
} // namespace ngm
