#include "ngm/ProfileMarkers.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace ngm {
ProfileMarkerTable correlate_profile_markers(const ProfileTable& events, const ProfileTable& regimes) {
    const auto refuse = [](ProfileMarkerStatus status, const char* reason) {
        ProfileMarkerTable result;
        result.status = status;
        result.reason = reason;
        return result;
    };
    using Status = ProfileMarkerStatus;
    if(events.rows.size() > ProfileEvidenceLimits::rows || regimes.rows.size() > ProfileEvidenceLimits::rows ||
       regimes.columns.empty() || regimes.columns.size() > ProfileEvidenceLimits::columns ||
       events.label_column != "event_text" || events.columns != std::vector<std::string>{"time_ms"} ||
       regimes.label_column != "flattened_event_name")
        return refuse(Status::Unsupported,
                      "Marker association requires qualified single-column event headers and regime headers");
    std::map<std::string, std::pair<std::size_t, std::size_t>> regime_paths;
    for(std::size_t row = 0; row < regimes.rows.size(); ++row) {
        const auto& item = regimes.rows[row];
        if(item.label.empty() || item.label.size() > ProfileMarkerLimits::path_bytes ||
           item.values.size() != regimes.columns.size())
            return refuse(Status::Unsupported, "Regime rows exceed the qualified marker path/table shape");
        auto& [count, first] = regime_paths[item.label];
        if(count++ == 0)
            first = row;
    }
    ProfileMarkerTable result;
    std::vector<std::string> stack;
    std::map<std::string, std::size_t> event_paths;
    for(std::size_t row = 0; row < events.rows.size(); ++row) {
        const auto& item = events.rows[row];
        if(item.values.size() != 1 || !item.values[0].value || !std::isfinite(*item.values[0].value) ||
           *item.values[0].value < 0)
            return refuse(Status::Unsupported, "Marker durations require one finite nonnegative exported time per row");
        const auto spaces = item.label.find_first_not_of(' ');
        if(spaces == std::string::npos || spaces % 8 != 0 || spaces / 8 >= ProfileMarkerLimits::depth ||
           spaces / 8 > stack.size())
            return refuse(Status::Unsupported, "Marker indentation is outside the qualified eight-space hierarchy");
        const auto name = item.label.substr(spaces);
        if(name.empty() || name.size() > ProfileMarkerLimits::component_bytes || name.front() == ' ' ||
           name.back() == ' ' ||
           std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c > 126 || c == '/'; }))
            return refuse(Status::Unsupported,
                          "Marker components require bounded visible ASCII without slash or edge spaces");
        stack.resize(spaces / 8);
        stack.push_back(name);
        std::string flat;
        for(const auto& component : stack) {
            if(!flat.empty())
                flat += '/';
            if(flat.size() + component.size() > ProfileMarkerLimits::path_bytes)
                return refuse(Status::Unsupported, "Exported marker path exceeds the qualified byte limit");
            flat += component;
        }
        ++event_paths[flat];
        ProfileMarkerAssociation association;
        association.path = stack;
        association.flat_path = flat;
        result.rows.push_back(std::move(association));
    }
    // The second export confirms the indentation interpretation, including
    // paths affected by leading whitespace or unsupported component spellings.
    for(const auto& [path, count] : event_paths) {
        (void)count;
        if(!regime_paths.contains(path))
            return refuse(Status::Unavailable,
                          "An event path has no exact same-profile regime path; hierarchy association is unavailable");
    }
    result.status = Status::Available;
    result.reason = "Qualified single-column event/regime hierarchy; occurrence uniqueness is checked per row";
    for(auto& association : result.rows) {
        association.event_matches = event_paths.at(association.flat_path);
        const auto& [matches, first] = regime_paths.at(association.flat_path);
        association.regime_matches = matches;
        if(association.event_matches != 1 || matches != 1) {
            association.status = Status::Unavailable;
            association.reason =
                "Repeated exported marker paths lack unique occurrence keys; timing/counter association is unavailable";
        } else {
            association.status = Status::Available;
            association.reason = "Unique exact event/regime path within the qualified single-frame export; scope is an "
                                 "exported marker range";
            association.regime_row = first;
        }
    }
    return result;
}
ProfileMarkerAssociation correlate_profile_marker(const ProfileTable& events, const ProfileTable& regimes,
                                                  std::size_t row_index) {
    if(row_index >= events.rows.size())
        throw std::invalid_argument("Selected event row is absent; inspect profile_metrics event_durations first");
    const auto table = correlate_profile_markers(events, regimes);
    if(table.status == ProfileMarkerStatus::Available)
        return table.rows.at(row_index);
    ProfileMarkerAssociation result;
    result.status = table.status;
    result.reason = table.reason;
    return result;
}
} // namespace ngm
