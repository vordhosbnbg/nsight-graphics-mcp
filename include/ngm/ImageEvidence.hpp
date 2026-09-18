#pragma once

#include "ngm/Artifacts.hpp"
#include "ngm/Image.hpp"

namespace ngm {
struct ImageReference {
    std::string artifact_id;
    std::string path;
};

// Compares caller-selected retained images. Holds both usage leases through the
// operation. Pixel agreement does not establish equivalent workloads or a fix.
nlohmann::json compare_artifact_images(ArtifactStore& store, const ImageReference& reference,
                                       const ImageReference& candidate, std::uint8_t tolerance);
struct ImagePreview {
    nlohmann::json metadata;
    std::string png;
};
ImagePreview preview_artifact_image(ArtifactStore& store, const ImageReference& source,
                                    std::optional<ImageRegion> region = std::nullopt, std::uint32_t max_edge = 384);
} // namespace ngm
