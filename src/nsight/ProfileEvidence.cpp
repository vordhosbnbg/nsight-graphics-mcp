#include "ngm/ProfileEvidence.hpp"
#include "ngm/NsightEvidence.hpp"

#include <charconv>
#include <cmath>
#include <nlohmann/json.hpp>
#include <set>

namespace ngm {
namespace {
using Limits = ProfileEvidenceLimits;
using Error = NsightEvidenceErrorCode;
[[noreturn]] void fail(Error code, const char* message) {
    throw NsightEvidenceError(code, message);
}

std::vector<std::vector<std::string_view>> cells(std::string_view text) {
    if(text.empty())
        fail(Error::InvalidSchema, "Empty GPU Trace export");
    if(text.size() > Limits::bytes)
        fail(Error::LimitExceeded, "GPU Trace export byte limit");
    // Validate UTF-8 before accepting labels or discarded reproduction fields.
    try {
        (void)nlohmann::json(std::string(text)).dump();
    } catch(const nlohmann::json::exception&) {
        fail(Error::InvalidField, "GPU Trace export contains invalid UTF-8");
    }
    for(const unsigned char c : text) {
        if((c < 32 && c != '\t' && c != '\r' && c != '\n') || c == 127)
            fail(Error::InvalidField, "GPU Trace export contains a control character");
    }
    std::vector<std::vector<std::string_view>> result;
    std::size_t total = 0;
    while(!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if(!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if(line.empty() || line.find('\r') != line.npos)
            fail(Error::InvalidSchema, "Empty row or embedded carriage return in GPU Trace export");
        if(result.size() == Limits::rows)
            fail(Error::LimitExceeded, "GPU Trace row limit");
        std::vector<std::string_view> fields;
        while(true) {
            const auto tab = line.find('\t');
            const auto field = line.substr(0, tab);
            if(field.size() > Limits::field_bytes || fields.size() == Limits::columns || total == Limits::cells)
                fail(Error::LimitExceeded, "GPU Trace field/column/cell limit");
            fields.push_back(field);
            ++total;
            if(tab == line.npos)
                break;
            line.remove_prefix(tab + 1);
        }
        result.push_back(std::move(fields));
        if(end == text.npos)
            break;
        text.remove_prefix(end + 1);
    }
    return result;
}

ProfileValue numeric(std::string_view text) {
    double value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if(text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value))
        fail(Error::InvalidField, "GPU Trace numeric cell is missing, nonfinite, or malformed");
    return {std::string(text), value};
}
} // namespace

ProfileTable parse_profile_table(std::string_view text, ProfileTableKind kind) {
    const auto lines = cells(text);
    ProfileTable table;
    const bool header = kind == ProfileTableKind::EventDurations || kind == ProfileTableKind::RegimeMetrics;
    const auto width = lines.front().size();
    if(width < 2 || (header && lines.size() < 2))
        fail(Error::InvalidSchema, "GPU Trace table has no values");
    if(header) {
        const auto expected = kind == ProfileTableKind::EventDurations ? "event_text" : "flattened_event_name";
        if(lines.front().front() != expected)
            fail(Error::InvalidSchema, "Unexpected GPU Trace table header");
        table.label_column = expected;
        for(std::size_t column = 1; column < width; ++column) {
            const auto name = lines.front()[column];
            if(name.empty() || (kind == ProfileTableKind::EventDurations && name != "time_ms"))
                fail(Error::InvalidField, "Invalid GPU Trace value header");
            table.columns.emplace_back(name);
        }
    }
    std::set<std::string_view> metric_names;
    for(std::size_t row = header ? 1 : 0; row < lines.size(); ++row) {
        const auto& line = lines[row];
        if(line.size() != width || line.front().empty())
            fail(Error::InvalidSchema, "Ragged or unnamed GPU Trace row");
        if(kind == ProfileTableKind::FrameDuration && (lines.size() != 1 || line.front() != "GPU frame time"))
            fail(Error::InvalidSchema, "Unexpected GPU Trace frame-duration row");
        if(kind == ProfileTableKind::FrameMetrics && !metric_names.insert(line.front()).second)
            fail(Error::DuplicateIdentifier, "Duplicate GPU Trace frame metric");
        ProfileRow output{std::string(line.front()), {}};
        for(std::size_t column = 1; column < width; ++column)
            output.values.push_back(numeric(line[column]));
        table.rows.push_back(std::move(output));
    }
    return table;
}

ProfileReproduction parse_profile_reproduction(std::string_view text) {
    const auto lines = cells(text);
    if(lines.size() > 128)
        fail(Error::LimitExceeded, "GPU Trace reproduction entry limit");
    std::map<std::string_view, std::string_view> values;
    for(const auto& line : lines) {
        if(line.size() != 2 || line.front().empty())
            fail(Error::InvalidSchema, "Invalid GPU Trace reproduction entry");
        if(!values.emplace(line[0], line[1]).second)
            fail(Error::DuplicateKey, "Duplicate GPU Trace reproduction key");
    }
    const auto required = [&values](std::string_view key) {
        const auto found = values.find(key);
        if(found == values.end() || found->second.empty())
            fail(Error::MissingField, "Missing GPU Trace reproduction field");
        return std::string(found->second);
    };
    ProfileReproduction result{required("Product Version"), required("Device Name"), required("Driver Version"), {}};
    if(result.product_version != "2026.3.1.0 (build 38722833) (public-release)" &&
       result.product_version != "2026.2.0.0 (build 37991608) (public-release)")
        fail(Error::UnsupportedVersion, "Unqualified GPU Trace export producer");
    if(required("API") != "Vulkan")
        fail(Error::InvalidField, "GPU Trace export API is not Vulkan");
    for(const auto key : {"API", "Chip Name", "Start After", "Max Duration ", "Limited To", "V-Sync Mode", "GPU Clocks",
                          "Metric Set", "Real-Time Shader Profiler", "Multi-Pass Metrics", "Time Every Action"}) {
        if(const auto found = values.find(key); found != values.end())
            result.settings.emplace(found->first, found->second);
    }
    // These values bind comparisons to actual exported settings, not just the
    // caller's requested CLI arguments. Missing settings cannot qualify a run.
    for(const auto key : {"Start After", "Limited To", "GPU Clocks", "Metric Set", "Multi-Pass Metrics"})
        (void)required(key);
    return result;
}
} // namespace ngm
