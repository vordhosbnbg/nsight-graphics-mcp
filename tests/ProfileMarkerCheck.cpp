#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/ProfileMarkers.hpp"

#include <filesystem>
#include <limits>

namespace {
using ngm::check::require;
using Status = ngm::ProfileMarkerStatus;
using Kind = ngm::ProfileTableKind;
template <class F>
void invalid(F operation) {
    try {
        operation();
    } catch(const std::invalid_argument&) {
        return;
    }
    require(false, "absent marker selection is an explicit argument error");
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "marker fixture directory required");
        const std::filesystem::path root(argv[1]);
        const auto events = ngm::parse_profile_table(ngm::read_regular_file(root / "D3DPERF_EVENTS.xls.tsv", 16384),
                                                     Kind::EventDurations);
        const auto regimes = ngm::parse_profile_table(ngm::read_regular_file(root / "GPUTRACE_REGIMES.xls.tsv", 16384),
                                                      Kind::RegimeMetrics);
        for(const auto empty_events : {false, true}) {
            auto e = events;
            auto r = regimes;
            if(empty_events)
                e.rows.clear();
            else
                r.rows.clear();
            const auto empty = ngm::correlate_profile_markers(e, r);
            require(empty.status == Status::Unavailable && empty.rows.empty() &&
                        empty.reason.find("Producer exported no") == 0,
                    "header-only data reports missing producer rows without discarding raw frame metrics");
        }
        const auto first = ngm::correlate_profile_marker(events, regimes, 4);
        require(first.status == Status::Available &&
                    first.path == std::vector<std::string>{"frame.5", "scene.raster", "timing.batch", "branch.a",
                                                           "unique.first"} &&
                    first.regime_row == 4 && first.event_matches == 1 && first.regime_matches == 1,
                "actual single-frame export has a unique exact marker path");
        for(const auto row : {5u, 6u}) {
            const auto repeated = ngm::correlate_profile_marker(events, regimes, row);
            require(repeated.status == Status::Unavailable && repeated.event_matches == 2 &&
                        repeated.regime_matches == 2 && !repeated.regime_row,
                    "same-path siblings cannot be joined by row order");
        }
        require(ngm::correlate_profile_marker(events, regimes, 8).status == Status::Available,
                "same leaf name in a different branch remains a distinct path");
        require(ngm::correlate_profile_marker(events, regimes, 9).status == Status::Available,
                "a multi-draw marker is a valid range; the parser supplies no draw membership");
        invalid([&] { ngm::correlate_profile_marker(events, regimes, events.rows.size()); });
        {
            auto duplicate = regimes;
            duplicate.rows.push_back(duplicate.rows[4]);
            const auto result = ngm::correlate_profile_marker(events, duplicate, 4);
            require(result.status == Status::Unavailable && result.event_matches == 1 && result.regime_matches == 2,
                    "duplicate regime path refuses association even when timing name is unique");
        }
        for(const auto label :
            {"    bad-indent", "                skipped-level", "slash/name", "trailing ", "\xCE\xBB"}) {
            auto changed = events;
            changed.rows[0].label = label;
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Unsupported,
                    "unsupported spellings and malformed hierarchy never retain a qualified path");
        }
        {
            auto changed = events;
            // A label with eight original leading spaces could look like an
            // extra indentation level. Its literal flat export disproves that
            // interpretation, including for other rows in the affected tree.
            changed.rows[5].label.insert(0, 8, ' ');
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Unavailable,
                    "cross-export anchors prevent accepting an unconfirmed hierarchy");
        }
        {
            auto missing = regimes;
            missing.rows.erase(missing.rows.begin() + 4);
            require(ngm::correlate_profile_marker(events, missing, 4).status == Status::Unavailable,
                    "missing regime anchor is explicit unavailable coverage");
            missing.rows.clear();
            require(ngm::correlate_profile_marker(events, missing, 4).status == Status::Unavailable,
                    "valid header-only regime data supplies no correlation");
        }
        for(const auto value : {-1.0, std::numeric_limits<double>::infinity()}) {
            auto changed = events;
            changed.rows[4].values[0].value = value;
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Unsupported,
                    "invalid typed timing values cannot qualify a range");
        }
        {
            auto changed = events;
            changed.rows[4].values[0].value = 0;
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Available,
                    "finite exported zero is retained rather than confused with missing coverage");
            changed.columns.push_back("time_ms");
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Unsupported,
                    "identical time columns do not establish sample identity");
        }
        {
            auto changed = events;
            changed.rows[4].label =
                std::string(32, ' ') + std::string(ngm::ProfileMarkerLimits::component_bytes + 1, 'x');
            require(ngm::correlate_profile_marker(changed, regimes, 4).status == Status::Unsupported,
                    "component limit bounds interpretation and output");
        }
    });
}
