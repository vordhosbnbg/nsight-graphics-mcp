#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/Image.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 3, "expected isolated scratch and PNG fixture directories");
        const auto path = std::filesystem::path(argv[1]) / "image.ppm";
        std::filesystem::create_directories(path.parent_path());
        const auto write = [&](const std::string& text) {
            std::ofstream output(path, std::ios::binary);
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            require(output.good(), "write image test fixture");
        };
        write(std::string("P6\n# comment\n1 1\n255\n") + '\n' + '\r' + ' ');
        const auto image = ngm::read_ppm(path);
        require(image.width == 1 && image.height == 1 && image.rgb == std::vector<std::uint8_t>{10, 13, 32},
                "pixel bytes that resemble whitespace are retained");
        auto other = image;
        other.rgb = {11, 11, 35};
        const auto difference = ngm::compare_images(image, other, 2);
        require(difference.differing_pixels == 1 && difference.max_channel_difference == 3 &&
                    difference.mean_absolute_channel_difference == 2,
                "pixel tolerance and channel difference units");
        require(ngm::compare_images(image, other, 3).differing_pixels == 0, "inclusive tolerance");
        for(const auto* invalid : {"P3\n1 1\n255\n000", "P6\n999999 1\n255\n", "P6\n1 1\n65535\n000",
                                   "P6\n1 1\n255\n00", "P6\n1 1\n255\n0000"}) {
            write(invalid);
            bool rejected = false;
            try {
                ngm::read_ppm(path);
            } catch(const std::exception&) {
                rejected = true;
            }
            require(rejected, "invalid or oversized PPM rejected");
        }
        write("P6\n" + std::string(4096, ' ') + "1 1\n255\n000");
        bool rejected = false;
        try {
            ngm::read_ppm(path);
        } catch(const std::exception&) {
            rejected = true;
        }
        require(rejected, "total header size bounded including whitespace");
        // A bottom-up 1x2 24-bit BMP has four-byte rows. Nonzero padding must
        // not enter RGB pixels, and its orientation must match top-down P6.
        std::string bmp(62, '\0');
        bmp[0] = 'B';
        bmp[1] = 'M';
        bmp[2] = 62;
        bmp[10] = 54;
        bmp[14] = 40;
        bmp[18] = 1;
        bmp[22] = 2;
        bmp[26] = 1;
        bmp[28] = 24;
        bmp[54] = 6;
        bmp[55] = 5;
        bmp[56] = 4;
        bmp[57] = 99;
        bmp[58] = 3;
        bmp[59] = 2;
        bmp[60] = 1;
        bmp[61] = 88;
        const auto decoded = ngm::decode_image(bmp);
        require(decoded.width == 1 && decoded.height == 2 && decoded.rgb == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6},
                "BMP orientation, BGR channel order and row padding decoded");
        const auto ppm = std::string("P6\n1 2\n255\n") + std::string("\1\2\3\4\5\6", 6);
        require(ngm::compare_images(decoded, ngm::decode_image(ppm), 0).differing_pixels == 0,
                "different encodings compare by exact RGB pixels");
        auto top_down = bmp;
        top_down[22] = static_cast<char>(254);
        for(int i = 23; i < 26; ++i)
            top_down[i] = static_cast<char>(255);
        top_down.replace(54, 4, bmp.substr(58, 4));
        top_down.replace(58, 4, bmp.substr(54, 4));
        require(ngm::decode_image(top_down).rgb == decoded.rgb, "negative BMP height is top-down");
        auto wide = bmp;
        wide.resize(70, '\0');
        wide[2] = 70;
        wide[18] = 2;
        wide.replace(54, 8, std::string("\11\10\7\14\13\12\177\176", 8));
        wide.replace(62, 8, std::string("\3\2\1\6\5\4\175\174", 8));
        require(ngm::decode_image(wide).rgb == std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
                "multiple columns and padded rows preserve horizontal pixel addressing");
        auto rgb32 = top_down;
        rgb32[28] = 32;
        require(ngm::decode_image(rgb32).rgb == decoded.rgb, "32-bit unused byte is ignored, not alpha blended");
        const auto reject_image = [&](const std::string& bytes) {
            bool invalid = false;
            try {
                (void)ngm::decode_image(bytes);
            } catch(const std::exception&) {
                invalid = true;
            }
            require(invalid, "malformed or unsupported image rejected");
        };
        for(const auto [offset, value] :
            {std::pair{2, 61}, {6, 1}, {10, 53}, {14, 108}, {18, 0}, {26, 2}, {28, 16}, {30, 3}, {34, 1}, {46, 1}}) {
            auto invalid = bmp;
            invalid[offset] = static_cast<char>(value);
            reject_image(invalid);
        }
        for(const auto bad_height : {0U, 4097U, 0x80000000U}) {
            auto invalid = bmp;
            for(int byte = 0; byte < 4; ++byte)
                invalid[22 + byte] = static_cast<char>(bad_height >> (8 * byte));
            reject_image(invalid);
        }
        reject_image(bmp.substr(0, 53));
        reject_image(bmp.substr(0, 61));
        reject_image(bmp + 'x');
        reject_image("not an image");
        const auto fixture_bytes = [&](const char* name) {
            return ngm::read_regular_file(std::filesystem::path(argv[2]) / name, 65536);
        };
        const ngm::Image expected{2, 2, {10, 32, 255, 0, 128, 13, 1, 2, 3, 250, 100, 50}};
        for(const auto* name :
            {"rgb8.png", "rgba8.png", "rgb8-interlaced.png", "rgba8-interlaced.png", "ignored-ancillary.png"}) {
            require(ngm::compare_images(ngm::decode_image(fixture_bytes(name)), expected, 0).differing_pixels == 0,
                    "independent RGB/RGBA8 PNG fixtures preserve exact channels");
        }
        const auto encoded = ngm::encode_png(expected);
        require(ngm::decode_image(encoded).rgb == expected.rgb, "PNG encoder preserves known RGB samples");
        std::ofstream(path.parent_path() / "encoded.png", std::ios::binary) << encoded;
        for(const auto* name : {"transparent.png", "transparent-key.png", "bad-ancillary-crc.png", "gray8.png",
                                "rgb16.png", "animated.png", "inflated-too-large.png", "oversized-dimensions.png"}) {
            reject_image(fixture_bytes(name));
        }
        auto corrupted = fixture_bytes("rgb8.png");
        corrupted[corrupted.size() - 1] ^= 1;
        reject_image(corrupted);
        reject_image(encoded + "trailing");
        reject_image(encoded.substr(0, encoded.size() - 12));
        reject_image(encoded.substr(0, 30));
        require(ngm::preview_image(expected, {0, 0, 2, 2}, 384).rgb == expected.rgb,
                "preview never upscales small sources");
        require(ngm::preview_image(expected, {1, 0, 1, 2}, 384).rgb ==
                    std::vector<std::uint8_t>{0, 128, 13, 250, 100, 50},
                "region extracts requested columns without interpolation");
        const auto reduced = ngm::preview_image(expected, {0, 0, 2, 2}, 1);
        require(reduced.width == 1 && reduced.height == 1 && reduced.rgb == std::vector<std::uint8_t>{10, 32, 255},
                "nearest-neighbor preview selects the declared top-left sample");
        const ngm::Image wide_image{5, 2, std::vector<std::uint8_t>(30, 17)};
        const auto aspect = ngm::preview_image(wide_image, {0, 0, 5, 2}, 3);
        require(aspect.width == 3 && aspect.height == 1, "aspect ratio rounds down with minimum one pixel");
        ngm::Image ramp{5, 4, {}};
        for(std::uint8_t y = 0; y < 4; ++y) {
            for(std::uint8_t x = 0; x < 5; ++x) {
                ramp.rgb.insert(ramp.rgb.end(), 3, static_cast<std::uint8_t>(10 * y + x));
            }
        }
        const auto uneven = ngm::preview_image(ramp, {0, 0, 5, 4}, 3);
        require(uneven.width == 3 && uneven.height == 2 &&
                    uneven.rgb ==
                        std::vector<std::uint8_t>{0, 0, 0, 1, 1, 1, 3, 3, 3, 20, 20, 20, 21, 21, 21, 23, 23, 23},
                "nonintegral downsampling floors source coordinates on both axes");
        require(ngm::preview_image(ramp, {1, 1, 4, 3}, 2).rgb == std::vector<std::uint8_t>{11, 11, 11, 13, 13, 13},
                "downsampling selects coordinates relative to the requested crop origin");
        for(const auto region :
            {ngm::ImageRegion{0, 0, 0, 2}, {0, 0, 3, 2}, {2, 0, 1, 1}, {1, 1, 2, 2}, {0xffffffffU, 0, 1, 1}}) {
            bool invalid = false;
            try {
                (void)ngm::preview_image(expected, region, 384);
            } catch(const std::invalid_argument&) {
                invalid = true;
            }
            require(invalid, "invalid and overflowing crop rejected");
        }
        for(const auto edge : {0U, 385U, 0xffffffffU}) {
            bool invalid = false;
            try {
                (void)ngm::preview_image(expected, {0, 0, 2, 2}, edge);
            } catch(const std::invalid_argument&) {
                invalid = true;
            }
            require(invalid, "preview edge bounds enforced by core");
        }
        const ngm::Image large_image{384, 384, std::vector<std::uint8_t>(384 * 384 * 3, 127)};
        require(ngm::encode_png(large_image).size() < 512 * 1024, "maximum preview PNG fits declared byte budget");
        std::filesystem::remove(path);
        require(mkfifo(path.c_str(), 0600) == 0, "create FIFO image stand-in");
        rejected = false;
        try {
            ngm::read_ppm(path);
        } catch(const std::exception&) {
            rejected = true;
        }
        std::filesystem::remove(path);
        require(rejected, "FIFO image rejected without waiting for a writer");
    });
}
