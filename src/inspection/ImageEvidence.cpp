#include "ngm/ImageEvidence.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Image.hpp"

#include <span>

namespace ngm {
nlohmann::json compare_artifact_images(ArtifactStore& store, const ImageReference& reference,
                                       const ImageReference& candidate, std::uint8_t tolerance) {
    auto reference_lease = store.lease(reference.artifact_id);
    auto candidate_lease = store.lease(candidate.artifact_id);
    const auto load = [&](const ImageReference& source) {
        const auto encoded = store.read(source.artifact_id, source.path, 16U * 1024U * 1024U);
        auto image = decode_image(encoded);
        nlohmann::json info{{"artifact_id", source.artifact_id},
                            {"path", source.path},
                            {"artifact_status", store.inspect(source.artifact_id).summary.status},
                            {"encoded_bytes", encoded.size()},
                            {"encoded_sha256", sha256(std::as_bytes(std::span(encoded)))},
                            {"rgb_sha256", sha256(std::as_bytes(std::span(image.rgb)))}};
        return std::pair{std::move(image), std::move(info)};
    };
    const auto [before, before_info] = load(reference);
    const auto [after, after_info] = load(candidate);
    const auto difference = compare_images(before, after, tolerance);
    return {{"evidence_origin", "caller_selected_artifact_images"},
            {"reference", before_info},
            {"candidate", after_info},
            {"width", before.width},
            {"height", before.height},
            {"channel_tolerance", tolerance},
            {"differing_pixels", difference.differing_pixels},
            {"max_channel_difference", difference.max_channel_difference},
            {"mean_absolute_channel_difference", difference.mean_absolute_channel_difference},
            {"matches_within_tolerance", difference.differing_pixels == 0},
            {"units", "RGB8_channel_steps"},
            {"comparison_scope",
             "Top-down RGB8, no resize or color conversion. BMP unused fourth byte ignored. "
             "Caller selects images; workload equivalence, capture success and repair are not inferred."}};
}
} // namespace ngm
