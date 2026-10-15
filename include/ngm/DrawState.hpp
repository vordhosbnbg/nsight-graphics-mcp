#pragma once

#include "ngm/CppEvidence.hpp"

namespace ngm {
// Pure interpretation of submitted, generated replay API state. This does not
// execute replay code, read serialized payloads, or observe GPU event state.
// The adapter binds each occurrence to a capture and retained input hashes.
nlohmann::json inspect_draw_executions(const std::vector<CppSource>& sources);
} // namespace ngm
