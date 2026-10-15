#pragma once

#include "ngm/Artifacts.hpp"
#include <optional>

namespace ngm {
// Internal composition only. The inspector supplies capture-validated source
// identities and the compiled-in helper profile; MCP callers cannot supply them.
struct DescriptorReadRequest {
    std::string capture_id, project_directory;
    nlohmann::json reference, registrations, helper_profile, helper_paths, expected_inputs;
    std::filesystem::path worker;
    bool pin = false;
    std::optional<std::chrono::steady_clock::time_point> deadline = std::nullopt;
};
// Leases and snapshots exact inputs, verifies helper/worker identities, validates
// the entire confined response and every typed ID against source registrations,
// and retains both successful and failed attempts. Results are source writes,
// not selected-draw visibility, effective addresses or shader values.
nlohmann::json read_cpp_descriptor_writes(ArtifactStore&, const DescriptorReadRequest&);
} // namespace ngm
