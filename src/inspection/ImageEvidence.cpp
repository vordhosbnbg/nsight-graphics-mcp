#include "ngm/ImageEvidence.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Image.hpp"

#include <span>

namespace ngm {
namespace {
std::pair<Image, nlohmann::json> load_image(ArtifactStore& store, const ImageReference& source) {
    const auto encoded = store.read(source.artifact_id, source.path, 16U * 1024U * 1024U);
    auto image = decode_image(encoded);
    nlohmann::json info{{"artifact_id", source.artifact_id},
                        {"path", source.path},
                        {"artifact_status", store.inspect(source.artifact_id).summary.status},
                        {"encoded_bytes", encoded.size()},
                        {"encoded_sha256", sha256(std::as_bytes(std::span(encoded)))},
                        {"rgb_sha256", sha256(std::as_bytes(std::span(image.rgb)))}};
    return std::pair{std::move(image), std::move(info)};
}
} // namespace

nlohmann::json compare_artifact_images(ArtifactStore& store, const ImageReference& reference,
                                       const ImageReference& candidate, std::uint8_t tolerance) {
    auto reference_lease = store.lease(reference.artifact_id);
    auto candidate_lease = store.lease(candidate.artifact_id);
    const auto [before, before_info] = load_image(store, reference);
    const auto [after, after_info] = load_image(store, candidate);
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
ImagePreview preview_artifact_image(ArtifactStore& store, const ImageReference& source,
                                    std::optional<ImageRegion> region, std::uint32_t max_edge) {
    auto lease = store.lease(source.artifact_id);
    const auto [original, info] = load_image(store, source);
    const auto selected = region.value_or(ImageRegion{0, 0, original.width, original.height});
    const auto preview = preview_image(original, selected, max_edge);
    auto png = encode_png(preview);
    nlohmann::json metadata{
        {"evidence_origin", "caller_selected_artifact_image_preview"},
        {"source", info},
        {"source_width", original.width},
        {"source_height", original.height},
        {"region", {{"x", selected.x}, {"y", selected.y}, {"width", selected.width}, {"height", selected.height}}},
        {"width", preview.width},
        {"height", preview.height},
        {"max_edge", max_edge},
        {"resampled", preview.width != selected.width || preview.height != selected.height},
        {"sampling", "nearest_neighbor_top_left"},
        {"mime_type", "image/png"},
        {"png_bytes", png.size()},
        {"png_sha256", sha256(std::as_bytes(std::span(png)))},
        {"preview_rgb_sha256", sha256(std::as_bytes(std::span(preview.rgb)))},
        {"preview_scope", "Display preview of caller-selected pixels; no color correction, alpha compositing or "
                          "workload inference. Compare original images for fix verification."}};
    return {std::move(metadata), std::move(png)};
}
} // namespace ngm
