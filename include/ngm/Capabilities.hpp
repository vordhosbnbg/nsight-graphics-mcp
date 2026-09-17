#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace ngm {
struct ExecutableObservation {
    std::string name;
    std::optional<std::filesystem::path> path;
};

// Observations only: finding an executable or a display environment variable
// does not establish a working capture, compatible GPU, or desktop connection.
struct PrerequisiteObservations {
    std::string discovery_source;
    std::optional<std::filesystem::path> nsight_root;
    std::array<ExecutableObservation, 3> executables;
    bool display_present = false;
    bool wayland_display_present = false;
};

PrerequisiteObservations discover_prerequisites(const std::optional<std::filesystem::path>& nsight_root);
} // namespace ngm
