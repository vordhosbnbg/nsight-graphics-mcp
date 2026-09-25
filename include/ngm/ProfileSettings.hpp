#pragma once

#include <cstdint>
#include <string>

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
    bool operator==(const ProfileSettings&) const = default;
};
void validate_profile_settings(const ProfileSettings& settings);
} // namespace ngm
