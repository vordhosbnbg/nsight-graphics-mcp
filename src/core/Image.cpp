#include "ngm/Image.hpp"
#include "ngm/File.hpp"

#include <algorithm>
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
} // namespace

Image read_ppm(const std::filesystem::path& path) {
    std::istringstream input(read_regular_file(path, 4096ULL * 4096 * 3 + 4096));
    std::size_t header_bytes = 0;
    if(token(input, header_bytes) != "P6") {
        throw std::runtime_error("Expected a P6 image: " + path.string());
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
