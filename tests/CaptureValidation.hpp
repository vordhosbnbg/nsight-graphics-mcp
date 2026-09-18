#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace ngm::check::capture {
enum class JobDisposition { inspect, failed, unsupported };

// An unfinished discovery is not evidence of an unsupported installation when
// the owning job reached its deadline or was cancelled. Only an ordinary failed
// job with an explicit unavailable interface gets that classification.
constexpr JobDisposition classify_job(std::string_view state, std::optional<bool> interface_ready) {
    if(state == "timed_out" || state == "cancelled") {
        return JobDisposition::failed;
    }
    if(state == "failed" && interface_ready == false) {
        return JobDisposition::unsupported;
    }
    if(state == "succeeded" && interface_ready == true) {
        return JobDisposition::inspect;
    }
    return JobDisposition::failed;
}

// A successful logs export can report no messages. Its existing regular file
// is still read, inventoried and hashed by the harness. Other known export
// categories require data; an unrecognized category cannot bypass validation.
constexpr bool valid_export_size(std::string_view kind, std::size_t bytes) {
    if(kind == "logs") {
        return true;
    }
    return bytes > 0 && (kind == "metadata" || kind == "functions" || kind == "objects" || kind == "screenshot");
}
} // namespace ngm::check::capture
