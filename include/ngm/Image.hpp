#pragma once

#include <cstdint>
#include <filesystem>
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
// Bounded P6 or BMP BITMAPINFOHEADER / BI_RGB 24/32-bit input. Produces top-down
// RGB8 without resizing/color conversion; BMP's unused fourth byte is ignored.
Image decode_image(std::string_view encoded);
struct ImageDifference {
    std::uint64_t differing_pixels = 0;
    std::uint8_t max_channel_difference = 0;
    double mean_absolute_channel_difference = 0;
};
ImageDifference compare_images(const Image& before, const Image& after, std::uint8_t tolerance);
} // namespace ngm
