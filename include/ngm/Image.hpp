#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
struct Image {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgb;
};

// Bounded P6 RGB8 images; no implicit color-space conversion.
Image read_ppm(const std::filesystem::path& path);
// Bounded P6, opaque RGB/RGBA8 PNG, or BMP BITMAPINFOHEADER / BI_RGB 24/32-bit input. Produces top-down
// RGB8 without resizing/color conversion; BMP's unused fourth byte is ignored.
Image decode_image(std::string_view encoded);
// RGB8 PNG without transfer-function/color-space metadata or conversion.
std::string encode_png(const Image& image);
struct ImageRegion {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};
// Crop, then nearest-neighbor downsample to max_edge (1..384), never upscale.
Image preview_image(const Image& image, ImageRegion region, std::uint32_t max_edge);
struct ImageDifference {
    std::uint64_t differing_pixels = 0;
    std::uint8_t max_channel_difference = 0;
    double mean_absolute_channel_difference = 0;
};
ImageDifference compare_images(const Image& before, const Image& after, std::uint8_t tolerance);
} // namespace ngm
