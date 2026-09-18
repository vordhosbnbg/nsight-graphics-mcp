#include "ngm/Image.hpp"
#include "ngm/File.hpp"
#include <lodepng.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ngm {
namespace {
std::string token(std::istream& input, std::size_t& header_bytes) {
    const auto take = [&] {
        if(++header_bytes > 4096) {
            throw std::runtime_error("PPM header exceeds 4096 bytes");
        }
        return input.get();
    };
    std::string value;
    while(input) {
        const auto c = input.peek();
        if(c == '#') {
            std::string comment;
            // Bound comments as well as pixel data in untrusted exports.
            while(input && take() != '\n') {
                if(comment.size() == 4096) {
                    throw std::runtime_error("PPM comment exceeds limit");
                }
                comment.push_back(' ');
            }
        } else if(c != EOF && std::isspace(static_cast<unsigned char>(c))) {
            take();
        } else {
            break;
        }
    }
    while(input && input.peek() != EOF && !std::isspace(static_cast<unsigned char>(input.peek()))) {
        if(value.size() >= 32) {
            throw std::runtime_error("PPM header token exceeds limit");
        }
        value.push_back(static_cast<char>(take()));
    }
    if(value.empty()) {
        throw std::runtime_error("Incomplete PPM header");
    }
    return value;
}

std::uint32_t dimension(const std::string& value) {
    std::uint32_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if(error != std::errc{} || end != value.data() + value.size() || result == 0 || result > 4096) {
        throw std::runtime_error("PPM dimensions must be between 1 and 4096");
    }
    return result;
}

void validate(const Image& image) {
    if(image.width == 0 || image.height == 0 || image.width > 4096 || image.height > 4096 ||
       image.rgb.size() != static_cast<std::size_t>(image.width) * image.height * 3) {
        throw std::runtime_error("Invalid RGB8 image dimensions or storage");
    }
}
Image decode_png(std::string_view encoded) {
    const auto* data = reinterpret_cast<const unsigned char*>(encoded.data());
    lodepng::State state;
    unsigned width = 0, height = 0;
    const auto header_error = lodepng_inspect(&width, &height, &state, data, encoded.size());
    if(header_error) {
        throw std::runtime_error(std::string("Invalid PNG header: ") + lodepng_error_text(header_error));
    }
    if(width == 0 || height == 0 || width > 4096 || height > 4096 || state.info_png.color.bitdepth != 8 ||
       (state.info_png.color.colortype != LCT_RGB && state.info_png.color.colortype != LCT_RGBA)) {
        throw std::runtime_error("PNG requires RGB/RGBA8 and dimensions between 1 and 4096");
    }
    // Validate all chunk boundaries/CRCs, including ignored ancillary chunks.
    // Do not silently render the first frame of an animation or accept trailing data.
    std::size_t offset = 8, chunks = 0;
    bool ended = false;
    while(offset < encoded.size()) {
        if(++chunks > 4096 || encoded.size() - offset < 12) {
            throw std::runtime_error("PNG chunk count or length exceeds limit");
        }
        const auto length = lodepng_chunk_length(data + offset);
        if(length > encoded.size() - offset - 12 || lodepng_chunk_check_crc(data + offset)) {
            throw std::runtime_error("Invalid PNG chunk length or CRC");
        }
        const auto type = encoded.substr(offset + 4, 4);
        if(type == "acTL" || type == "fcTL" || type == "fdAT") {
            throw std::runtime_error("Animated PNG is unsupported");
        }
        offset += static_cast<std::size_t>(length) + 12;
        if(type == "IEND") {
            ended = length == 0 && offset == encoded.size();
            break;
        }
    }
    if(!ended) {
        throw std::runtime_error("PNG requires a final empty IEND without trailing data");
    }
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
    state.decoder.zlibsettings.max_output_size = static_cast<std::size_t>(width) * height * 8 + 4096;
    std::vector<unsigned char> rgba;
    const auto error = lodepng::decode(rgba, width, height, state, data, encoded.size());
    if(error) {
        throw std::runtime_error(std::string("Invalid PNG pixels: ") + lodepng_error_text(error));
    }
    Image result{width, height, {}};
    result.rgb.reserve(static_cast<std::size_t>(width) * height * 3);
    for(std::size_t pixel = 0; pixel < rgba.size(); pixel += 4) {
        if(rgba[pixel + 3] != 255) {
            throw std::runtime_error("PNG alpha must be opaque; no implicit background compositing");
        }
        result.rgb.insert(result.rgb.end(), rgba.begin() + pixel, rgba.begin() + pixel + 3);
    }
    validate(result);
    return result;
}
Image decode_ppm(std::string_view encoded) {
    std::istringstream input{std::string(encoded)};
    std::size_t header_bytes = 0;
    if(token(input, header_bytes) != "P6") {
        throw std::runtime_error("Expected a P6 image");
    }
    Image result;
    result.width = dimension(token(input, header_bytes));
    result.height = dimension(token(input, header_bytes));
    if(token(input, header_bytes) != "255") {
        throw std::runtime_error("PPM must use 8-bit RGB channels");
    }
    const auto separator = input.get();
    if(separator == EOF || !std::isspace(static_cast<unsigned char>(separator))) {
        throw std::runtime_error("Missing PPM pixel separator");
    }
    // P6 specifies one whitespace delimiter; do not consume pixel bytes that
    // happen to be whitespace (including CR/LF channel values).
    result.rgb.resize(static_cast<std::size_t>(result.width) * result.height * 3);
    input.read(reinterpret_cast<char*>(result.rgb.data()), static_cast<std::streamsize>(result.rgb.size()));
    if(input.gcount() != static_cast<std::streamsize>(result.rgb.size()) || input.peek() != EOF) {
        throw std::runtime_error("PPM pixel length does not match its dimensions");
    }
    return result;
}

Image decode_bmp(std::string_view encoded) {
    const auto u16 = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(encoded[offset])) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(encoded[offset + 1])) << 8);
    };
    const auto u32 = [&](std::size_t offset) { return u16(offset) | (u16(offset + 2) << 16); };
    if(encoded.size() < 54 || !encoded.starts_with("BM")) {
        throw std::runtime_error("Incomplete BMP file header");
    }
    // Explicit observed export profile; do not guess masks, palettes or color profiles.
    if(u32(2) != encoded.size() || u16(6) != 0 || u16(8) != 0 || u32(10) != 54 || u32(14) != 40 || u16(26) != 1 ||
       (u16(28) != 24 && u16(28) != 32) || u32(30) != 0 || u32(46) != 0 || u32(50) != 0) {
        throw std::runtime_error("Unsupported BMP layout: require 40-byte BI_RGB 24/32-bit header without palette");
    }
    const auto width = std::bit_cast<std::int32_t>(u32(18));
    const auto signed_height = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(u32(22)));
    const auto height = signed_height < 0 ? -signed_height : signed_height;
    if(width < 1 || width > 4096 || height < 1 || height > 4096) {
        throw std::runtime_error("BMP dimensions must be between 1 and 4096");
    }
    const std::size_t bytes_per_pixel = u16(28) / 8;
    const std::size_t stride = (static_cast<std::size_t>(width) * bytes_per_pixel + 3) & ~std::size_t{3};
    const auto pixel_bytes = stride * static_cast<std::size_t>(height);
    if(encoded.size() != 54 + pixel_bytes || (u32(34) != 0 && u32(34) != pixel_bytes)) {
        throw std::runtime_error("BMP pixel length does not match its dimensions");
    }
    Image result{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
    result.rgb.resize(static_cast<std::size_t>(width) * height * 3);
    for(std::size_t y = 0; y < result.height; ++y) {
        const auto source_y = signed_height < 0 ? y : result.height - 1 - y;
        for(std::size_t x = 0; x < result.width; ++x) {
            const auto source = 54 + source_y * stride + x * bytes_per_pixel;
            const auto destination = (y * result.width + x) * 3;
            for(std::size_t channel = 0; channel < 3; ++channel) {
                result.rgb[destination + channel] = static_cast<unsigned char>(encoded[source + 2 - channel]);
            }
        }
    }
    return result;
}
} // namespace

Image read_ppm(const std::filesystem::path& path) {
    return decode_ppm(read_regular_file(path, 4096ULL * 4096 * 3 + 4096));
}

Image decode_image(std::string_view encoded) {
    if(encoded.size() > 64U * 1024U * 1024U) {
        throw std::runtime_error("Encoded image exceeds 64 MiB analysis limit");
    }
    if(encoded.starts_with("P6")) {
        return decode_ppm(encoded);
    }
    if(encoded.starts_with("BM")) {
        return decode_bmp(encoded);
    }
    if(encoded.starts_with(std::string_view("\x89PNG\r\n\x1a\n", 8))) {
        return decode_png(encoded);
    }
    throw std::runtime_error("Unsupported image format; expected P6 RGB8, opaque RGB/RGBA8 PNG or uncompressed BMP");
}

std::string encode_png(const Image& image) {
    validate(image);
    lodepng::State state;
    state.info_raw.colortype = state.info_png.color.colortype = LCT_RGB;
    state.info_raw.bitdepth = state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 0;
    state.encoder.filter_strategy = LFS_ZERO;
    // Stored deflate blocks provide deterministic size bounds for MCP previews.
    state.encoder.zlibsettings.btype = 0;
    std::vector<unsigned char> output;
    const auto error = lodepng::encode(output, image.rgb, image.width, image.height, state);
    if(error) {
        throw std::runtime_error(std::string("PNG encoding failed: ") + lodepng_error_text(error));
    }
    return {reinterpret_cast<const char*>(output.data()), output.size()};
}

Image preview_image(const Image& image, ImageRegion region, std::uint32_t max_edge) {
    validate(image);
    if(max_edge == 0 || max_edge > 384 || region.width == 0 || region.height == 0 || region.x >= image.width ||
       region.y >= image.height || region.width > image.width - region.x || region.height > image.height - region.y) {
        throw std::invalid_argument("Preview requires an in-bounds nonempty region and max_edge between 1 and 384");
    }
    const auto divisor = std::max({region.width, region.height, max_edge});
    Image output{std::max(1U, region.width * max_edge / divisor), std::max(1U, region.height * max_edge / divisor), {}};
    output.rgb.resize(static_cast<std::size_t>(output.width) * output.height * 3);
    for(std::uint32_t y = 0; y < output.height; ++y) {
        for(std::uint32_t x = 0; x < output.width; ++x) {
            const auto source = (static_cast<std::size_t>(region.y + y * region.height / output.height) * image.width +
                                 region.x + x * region.width / output.width) *
                                3;
            const auto destination = (static_cast<std::size_t>(y) * output.width + x) * 3;
            std::copy_n(image.rgb.begin() + source, 3, output.rgb.begin() + destination);
        }
    }
    return output;
}

ImageDifference compare_images(const Image& before, const Image& after, std::uint8_t tolerance) {
    validate(before);
    validate(after);
    if(before.width != after.width || before.height != after.height) {
        throw std::runtime_error("Image dimensions differ");
    }
    ImageDifference result;
    std::uint64_t total = 0;
    for(std::size_t pixel = 0; pixel < before.rgb.size(); pixel += 3) {
        bool different = false;
        for(std::size_t channel = 0; channel < 3; ++channel) {
            const auto delta = static_cast<std::uint8_t>(
                std::abs(static_cast<int>(before.rgb[pixel + channel]) - after.rgb[pixel + channel]));
            total += delta;
            result.max_channel_difference = std::max(result.max_channel_difference, delta);
            different |= delta > tolerance;
        }
        result.differing_pixels += different;
    }
    result.mean_absolute_channel_difference = static_cast<double>(total) / before.rgb.size();
    return result;
}
} // namespace ngm
