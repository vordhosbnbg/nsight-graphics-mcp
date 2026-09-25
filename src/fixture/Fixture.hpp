#pragma once

#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>

namespace ngm::fixture {
struct Options {
    std::string scenario;
    std::uint32_t seed = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frame = 0;
    std::filesystem::path output;
    std::filesystem::path shader_directory;
    std::optional<std::uint32_t> sdk_first_boundary_frame;
    std::optional<std::uint32_t> warmup;
    bool compute_frame_boundary = false;
};

class Unsupported : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

Options parse_arguments(std::span<const char* const> arguments);
void run(const Options& options);
// Uses verified retained shader files; observations remain application evidence.
void run_compute(const Options& options, nlohmann::json& result);
} // namespace ngm::fixture
