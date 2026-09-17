#include "Check.hpp"
#include "ngm/Image.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 2, "expected isolated scratch directory");
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
