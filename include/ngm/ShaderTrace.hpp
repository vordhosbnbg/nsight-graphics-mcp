#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string_view>

namespace ngm {
inline constexpr std::size_t shader_trace_maximum_bytes = 1024U * 1024U;
inline constexpr std::uint32_t shader_trace_maximum_records = 4096;
inline constexpr std::string_view shader_trace_origin = "application_reported_instrumented_shader_trace";

// Optional application-side std430 buffer contract. The application owns feature
// admission, descriptor binding, bounded writes, synchronization and readback.
// These structures do not instrument shaders or attest that their claims ran.
struct ShaderTraceBufferHeader {
    std::uint32_t capacity = 0;
    std::uint32_t attempted_records = 0;
    std::uint32_t dropped_records = 0;
    std::uint32_t pixel_x = 0;
    std::uint32_t pixel_y = 0;
    std::uint32_t frame = 0;
    std::uint32_t draw = 0;
    std::uint32_t reserved = 0;
};

struct alignas(16) ShaderTraceBufferRecord {
    // Application frame/draw and integer framebuffer pixel coordinates.
    std::array<std::uint32_t, 4> invocation{};
    // Sample, primitive (UINT32_MAX means unavailable), probe ID, components.
    std::array<std::uint32_t, 4> selector{};
    std::array<std::uint32_t, 4> bits{};
};
static_assert(sizeof(ShaderTraceBufferHeader) == 32);
static_assert(sizeof(ShaderTraceBufferRecord) == 48);

struct ShaderTraceSelection {
    std::optional<std::uint32_t> frame{};
    std::optional<std::uint32_t> draw{};
    std::optional<std::uint32_t> pixel_x{};
    std::optional<std::uint32_t> pixel_y{};
    std::optional<std::uint32_t> sample{};
    std::optional<std::uint32_t> probe{};
};

// Strict, bounded admission. Unknown keys, duplicate keys, unsupported versions,
// inconsistent counts and malformed values throw invalid_argument. Raw uint32
// payloads preserve all float32 bits, including nonfinite values and signed zero.
nlohmann::json parse_shader_trace(std::string_view text);
nlohmann::json shader_trace_page(const nlohmann::json& trace, const ShaderTraceSelection& selection = {},
                                 std::size_t offset = 0, std::size_t limit = 50);

// Producer convenience for the optional buffer contract above. Call only after
// completed GPU-to-host synchronization/invalidation, then atomically publish
// the returned payload as shader-trace.json under the application output root.
nlohmann::json make_shader_trace(const ShaderTraceBufferHeader& header,
                                 std::span<const ShaderTraceBufferRecord> records, nlohmann::json shader,
                                 std::uint32_t width, std::uint32_t height, nlohmann::json probes);
} // namespace ngm
