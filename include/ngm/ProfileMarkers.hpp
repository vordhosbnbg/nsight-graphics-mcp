#pragma once
#include "ngm/ProfileEvidence.hpp"

namespace ngm {
struct ProfileMarkerLimits {
    static constexpr std::size_t depth = 32;
    static constexpr std::size_t component_bytes = 256;
    static constexpr std::size_t path_bytes = 4096;
    static constexpr std::size_t metrics = 8;
};
enum class ProfileMarkerStatus { Available, Unavailable, Unsupported };
struct ProfileMarkerAssociation {
    ProfileMarkerStatus status = ProfileMarkerStatus::Unsupported;
    std::string reason;
    std::vector<std::string> path;
    std::string flat_path;
    std::size_t event_matches = 0, regime_matches = 0;
    std::optional<std::size_t> regime_row;
};
// Pure interpretation of validated text tables. No file access, producer or
// collection qualification, original execution IDs, draw membership, hardware
// units or physical counter normalization is supplied by this function.
ProfileMarkerAssociation correlate_profile_marker(const ProfileTable& events, const ProfileTable& regimes,
                                                  std::size_t row_index);
} // namespace ngm
