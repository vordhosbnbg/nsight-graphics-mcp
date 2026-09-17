#pragma once

#include <filesystem>
#include <optional>

namespace ngm {
int serve_stdio(const std::optional<std::filesystem::path>& nsight_root);
} // namespace ngm
