#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <span>
#include <string_view>

namespace ngm {
// Pure, bounded reflection of statically addressed uniform loads in a selected
// SPIR-V 1.6 entry point. This is not a complete SPIR-V validator, resource
// association, lifetime proof, or observation of a GPU shader invocation.
// Malformed inputs throw; unsupported shapes return unavailable with a reason.
nlohmann::json inspect_spirv_uniform_reads(std::span<const std::byte> module, std::string_view entry_point,
                                           std::uint32_t execution_model);
// Separate, conservative effect proof for the same straight-line profile.
// Only invocation-local/output stores are allowed. Unknown operations, external
// storage, pointer flow, atomics, images and extension instructions refuse.
// This does not establish resource aliases, invocation counts or GPU visibility.
nlohmann::json inspect_spirv_buffer_effects(std::span<const std::byte> module, std::string_view entry_point,
                                            std::uint32_t execution_model);
// Decode a qualified read's scalar/vector layout against bytes beginning at
// the proven descriptor base. The composing caller must separately establish
// shader/resource identity, effective range, upload order and absence of writes.
// Raw bits survive nonfinite float values; these have null numeric values.
nlohmann::json decode_spirv_uniform_read(const nlohmann::json& read, std::span<const std::byte> bytes);
} // namespace ngm
