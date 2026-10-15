#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ngm {
struct ProfileSettings {
    // Documented live-target start/stop controls, not application frame IDs.
    std::string delimiter = "submits";
    std::uint64_t start_after = 30;
    std::uint64_t limit = 3;
    std::uint64_t duration_ms = 1000;
    // Explicit per-architecture configuration selector. The export reports GPU
    // identity separately; it does not echo or verify this selector. Metric-set
    // selection is checked against the export. No machine-specific GPU is assumed.
    std::string architecture;
    std::string metric_set = "Throughput Metrics";
    // Null preserves the tool default. False per-action timing is verified
    // against the export; the documented CLI only has an enable switch.
    std::optional<bool> time_every_action = std::nullopt;
    // Explicit CLI selection, not proof that HES is active on the actual GPU.
    std::optional<bool> hes_enabled = std::nullopt;
    bool operator==(const ProfileSettings&) const = default;
};
void validate_profile_settings(const ProfileSettings& settings);
std::vector<std::string> profile_control_arguments(const ProfileSettings& settings);
} // namespace ngm
