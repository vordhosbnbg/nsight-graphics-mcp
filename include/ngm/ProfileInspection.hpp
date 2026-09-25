#pragma once
#include "ngm/Artifacts.hpp"

namespace ngm {
class ProfileInspection {
public:
    explicit ProfileInspection(ArtifactStore& artifacts) : artifacts_(artifacts) {}
    nlohmann::json metadata(const std::string& id) const;
    nlohmann::json metrics(const std::string& id, const std::string& table, std::size_t offset = 0,
                           std::size_t limit = 50, std::size_t column_offset = 0, std::size_t column_limit = 32) const;

private:
    ArtifactStore& artifacts_;
};
} // namespace ngm
