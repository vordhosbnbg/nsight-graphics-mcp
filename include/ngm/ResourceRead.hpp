#pragma once

#include "ngm/Artifacts.hpp"
#include <optional>

namespace ngm {
struct ResourceWorkers {
    std::filesystem::path nsight_2026_3;
    std::filesystem::path nsight_2026_2;
    std::filesystem::path descriptor_nsight_2026_3 = {};
};
// Internal composition boundary. The inspection service supplies a qualified
// capture/reference and a compiled-in helper profile, never MCP caller claims.
// The executable is trusted operator configuration, not a tool argument.
struct ResourceReadRequest {
    std::string capture_id;
    std::string project_directory;
    nlohmann::json reference;
    nlohmann::json helper_profile;
    std::filesystem::path worker;
    std::size_t offset = 0;
    std::size_t length = 65536;
    bool pin = false;
    // A composing query can share one bounded deadline across multiple reads.
    std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt;
};
// Leases source, fingerprints helpers, snapshots data, supervises worker,
// validates its entire bounded response, and publishes success/failure evidence.
// Each attempt retains identities, output/logs and its exact snapshot inputs.
nlohmann::json read_cpp_resource(ArtifactStore& store, const ResourceReadRequest& request);
} // namespace ngm
