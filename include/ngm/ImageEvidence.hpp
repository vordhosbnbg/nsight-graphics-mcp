#pragma once

#include "ngm/Artifacts.hpp"

namespace ngm {
struct ImageReference {
    std::string artifact_id;
    std::string path;
};

// Compares caller-selected retained images. Holds both usage leases through the
// operation. Pixel agreement does not establish equivalent workloads or a fix.
nlohmann::json compare_artifact_images(ArtifactStore& store, const ImageReference& reference,
                                       const ImageReference& candidate, std::uint8_t tolerance);
} // namespace ngm
