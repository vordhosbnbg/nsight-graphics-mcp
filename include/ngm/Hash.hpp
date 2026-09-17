#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

namespace ngm {
// SHA-256 content identities for retained build and evidence files.
std::string sha256(std::span<const std::byte> bytes);
std::string sha256_file(const std::filesystem::path& path);
} // namespace ngm
