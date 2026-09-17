#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <stop_token>
#include <string>

namespace ngm {
struct ExperimentOptions {
    std::filesystem::path fixture;
    std::filesystem::path output_root;
    std::filesystem::path shader_directory;
    std::string scenario;
    std::uint32_t seed = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frame = 0;
    std::chrono::milliseconds timeout{30000};
    bool validation = false;
};

struct ExperimentResult {
    std::filesystem::path directory;
    nlohmann::json report;
};

// Each call allocates a new directory and a fresh, owned application process.
// Errors after allocation are retained as failed reports, never as success.
ExperimentResult run_experiment(const ExperimentOptions& options, std::stop_token cancellation = {});
} // namespace ngm
