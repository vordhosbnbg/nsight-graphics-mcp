#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace ngm {
// Open without following links or blocking on FIFOs/devices; verify the opened
// descriptor is a bounded regular file before reading any data.
std::string read_regular_file(const std::filesystem::path& path, std::size_t maximum_bytes);
} // namespace ngm
