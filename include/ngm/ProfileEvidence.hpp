#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
struct ProfileEvidenceLimits {
    static constexpr std::size_t bytes = 16U * 1024U * 1024U;
    static constexpr std::size_t rows = 4096;
    static constexpr std::size_t columns = 4096;
    static constexpr std::size_t cells = 262144;
    static constexpr std::size_t field_bytes = 4096;
};

// The .xls files emitted by --auto-export are tab-separated UTF-8 text, not
// binary Excel workbooks. A position is an export position, not a sample/frame
// identity or min/mean/max statistic. Preserve duplicates and textual precision.
struct ProfileValue {
    std::string text;
    double value = 0;
};
struct ProfileRow {
    std::string label;
    std::vector<ProfileValue> values;
};
struct ProfileTable {
    std::string label_column;
    // Empty for headerless FRAME/GPUTRACE_FRAME. Otherwise repeated headers are
    // intentional and column identity is its zero-based position in this list.
    std::vector<std::string> columns;
    std::vector<ProfileRow> rows;
};
enum class ProfileTableKind { FrameDuration, FrameMetrics, EventDurations, RegimeMetrics };

// Pure, bounded parsers. Throw NsightEvidenceError. No file access, provenance
// authentication, unit scaling, event joins, or workload equivalence is inferred.
// Empty exports are rejected; callers must report missing evidence separately.
ProfileTable parse_profile_table(std::string_view text, ProfileTableKind kind);

struct ProfileReproduction {
    std::string product_version;
    std::string gpu;
    std::string driver;
    // Selected exported settings only. Host/process names and command lines are
    // deliberately excluded. Raw evidence belongs in the retained bundle.
    std::map<std::string, std::string> settings;
};
// Exact observed Vulkan producer versions only; extra bounded fields are ignored.
ProfileReproduction parse_profile_reproduction(std::string_view text);
} // namespace ngm
