#pragma once
#include "ngm/Artifacts.hpp"
#include "ngm/CppEvidence.hpp"
#include "ngm/ProfileEvidence.hpp"

namespace ngm {
// Pure bounded interpretation. The caller must separately validate producer,
// same-input collection, complete bundle identities and source fingerprints.
// Multi-interval callers must also establish path uniqueness across all source
// intervals; source_paths/profile_paths are internal aids, not public joins.
// A range may include state packets; this never measures an isolated shader or
// infers original Graphics Capture event IDs, live performance or counter units.
nlohmann::json correlate_replay_draws(const std::vector<CppSource>& sources, const ProfileTable& events,
                                      const ProfileTable& regimes);
class ReplayTimingInspection {
public:
    explicit ReplayTimingInspection(ArtifactStore& artifacts) : artifacts_(artifacts) {}
    nlohmann::json draws(const std::vector<std::string>& analysis_ids, std::size_t offset = 0,
                         std::size_t limit = 50) const;

private:
    ArtifactStore& artifacts_;
};
} // namespace ngm
