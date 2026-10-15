#pragma once

#include "CppReplay.hpp"

namespace ngm::detail {
// Static captured buffer/allocation/property relationships only. This does not
// resolve uploads, synchronization, descriptor selection or GPU-written data.
// Unsupported or ambiguous definitions refuse instead of completing a graph.
nlohmann::json inspect_value_memory_bindings(const CppReplay& replay);
} // namespace ngm::detail
