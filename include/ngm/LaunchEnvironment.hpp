#pragma once

#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
#include <stop_token>
#include <string>

namespace ngm {
using LaunchEnvironment = std::map<std::string, std::string>;

// Public job inputs are a closed overlay, independent of the trusted service's
// complete environment. Validation observes selections, never loaded libraries.
nlohmann::json launch_environment_policy();
nlohmann::json validate_launch_environment(
    const LaunchEnvironment& overrides, const LaunchEnvironment& service_defaults = {},
    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5),
    std::stop_token stop = {});
} // namespace ngm
