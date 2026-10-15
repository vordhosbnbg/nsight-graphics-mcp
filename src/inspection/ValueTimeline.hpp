#pragma once

#include "CppReplay.hpp"
#include <chrono>
#include <functional>
#include <span>

namespace ngm::detail {
struct ValueShader {
    std::vector<std::byte> bytes;
    nlohmann::json read;
};
// Internal, source-only composition. Descriptor rows must be complete results
// from the authenticated DescriptorRead parent; the callback must use the
// authenticated ResourceRead parent. The caller also fingerprints the exported
// replay/helper semantics. No MCP caller may supply these claims or bytes.
// All frame effects, including effects after a selected submission, are checked.
// Any unsupported effect discards every plan. The result remains source evidence
// until a separately qualified captured-state interface establishes its scope.
nlohmann::json
value_draw_plans(const CppReplay&, const std::string& capture_id, const nlohmann::json& captured_database_inputs,
                 const std::vector<nlohmann::json>& descriptor_reads,
                 const std::function<ValueShader(const nlohmann::json&)>& load_shader,
                 std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max());
} // namespace ngm::detail
