#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <span>
#include <stop_token>
#include <string>

namespace ngm {
// SHA-256 content identities for retained build and evidence files.
std::string sha256(std::span<const std::byte> bytes);
std::string sha256_file(const std::filesystem::path& path);
// For application paths that can change while queued: do not follow the final
// symlink or block opening a FIFO. Read only the opened regular file's observed
// size, checking cancellation/deadline between chunks and detecting changes.
std::string sha256_regular_file(const std::filesystem::path& path, std::chrono::steady_clock::time_point deadline,
                                std::stop_token stop = {});
} // namespace ngm
