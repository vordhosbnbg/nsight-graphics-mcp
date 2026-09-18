#include "ngm/Hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace ngm {
namespace {
class Sha256 {
public:
    void update(std::span<const std::byte> bytes) {
        if(bytes.size() > (std::numeric_limits<std::uint64_t>::max() / 8) - size_) {
            throw std::runtime_error("Input exceeds SHA-256 length limit");
        }
        size_ += bytes.size();
        for(const auto byte : bytes) {
            block_[used_++] = std::to_integer<std::uint8_t>(byte);
            if(used_ == block_.size()) {
                transform();
                used_ = 0;
            }
        }
    }

    std::string finish() {
        block_[used_++] = 0x80;
        if(used_ > 56) {
            while(used_ < block_.size()) {
                block_[used_++] = 0;
            }
            transform();
            used_ = 0;
        }
        while(used_ < 56) {
            block_[used_++] = 0;
        }
        const auto bits = size_ * 8;
        for(unsigned index = 0; index < 8; ++index) {
            block_[63 - index] = static_cast<std::uint8_t>(bits >> (index * 8));
        }
        transform();
        constexpr char hex[] = "0123456789abcdef";
        std::string output;
        output.reserve(64);
        for(const auto word : state_) {
            for(int shift = 28; shift >= 0; shift -= 4) {
                output.push_back(hex[(word >> shift) & 15]);
            }
        }
        return output;
    }

private:
    void transform() {
        // FIPS 180-4, section 4.2.2: cube-root fractional constants.
        constexpr std::array<std::uint32_t, 64> constants = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::array<std::uint32_t, 64> words{};
        for(std::size_t i = 0; i < 16; ++i) {
            for(std::size_t byte = 0; byte < 4; ++byte) {
                words[i] = (words[i] << 8) | block_[i * 4 + byte];
            }
        }
        for(std::size_t i = 16; i < words.size(); ++i) {
            const auto a = words[i - 15];
            const auto b = words[i - 2];
            const auto s0 = std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3);
            const auto s1 = std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state_;
        for(std::size_t i = 0; i < words.size(); ++i) {
            const auto s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choose = (e & f) ^ (~e & g);
            const auto first = h + s1 + choose + constants[i] + words[i];
            const auto s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            h = g;
            g = f;
            f = e;
            e = d + first;
            d = c;
            c = b;
            b = a;
            a = first + s0 + majority;
        }
        const std::array values{a, b, c, d, e, f, g, h};
        for(std::size_t i = 0; i < state_.size(); ++i) {
            state_[i] += values[i];
        }
    }

    std::array<std::uint32_t, 8> state_{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> block_{};
    std::uint64_t size_ = 0;
    std::size_t used_ = 0;
};
} // namespace

std::string sha256(std::span<const std::byte> bytes) {
    Sha256 hash;
    hash.update(bytes);
    return hash.finish();
}

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw std::runtime_error("Cannot read file for SHA-256: " + path.string());
    }
    Sha256 hash;
    std::array<char, 65536> buffer{};
    while(input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        hash.update(std::as_bytes(std::span(buffer.data(), static_cast<std::size_t>(input.gcount()))));
    }
    if(!input.eof()) {
        throw std::runtime_error("Error reading file for SHA-256: " + path.string());
    }
    return hash.finish();
}

std::string sha256_regular_file(const std::filesystem::path& path, std::chrono::steady_clock::time_point deadline,
                                std::stop_token stop) {
    const auto check_stop = [&] {
        if(stop.stop_requested() || std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("File hashing cancelled or deadline expired: " + path.string());
        }
    };
    check_stop();
    if(path.string().find('\0') != std::string::npos) {
        throw std::invalid_argument("Hash input path must not contain NUL bytes");
    }
    struct File {
        int descriptor;
        ~File() {
            if(descriptor >= 0) {
                close(descriptor);
            }
        }
    } file{open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC)};
    struct stat before{};
    if(file.descriptor < 0 || fstat(file.descriptor, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size < 0) {
        throw std::runtime_error("Expected a readable regular file for SHA-256: " + path.string());
    }
    Sha256 hash;
    std::array<std::byte, 65536> buffer{};
    auto remaining_bytes = static_cast<std::uint64_t>(before.st_size);
    while(remaining_bytes) {
        check_stop();
        const auto requested = static_cast<std::size_t>(std::min<std::uint64_t>(remaining_bytes, buffer.size()));
        const auto count = read(file.descriptor, buffer.data(), requested);
        if(count < 0 && errno == EINTR) {
            continue;
        }
        if(count <= 0) {
            throw std::runtime_error("File changed or failed while hashing: " + path.string());
        }
        hash.update(std::span(buffer.data(), static_cast<std::size_t>(count)));
        remaining_bytes -= static_cast<std::uint64_t>(count);
    }
    check_stop();
    struct stat after{};
    if(fstat(file.descriptor, &after) != 0 || before.st_size != after.st_size ||
       before.st_mtim.tv_sec != after.st_mtim.tv_sec || before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
       before.st_ctim.tv_sec != after.st_ctim.tv_sec || before.st_ctim.tv_nsec != after.st_ctim.tv_nsec) {
        throw std::runtime_error("File changed while hashing: " + path.string());
    }
    return hash.finish();
}
} // namespace ngm
