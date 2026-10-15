#pragma once

#include "CppReplay.hpp"

namespace ngm::detail {
// Recorded graphics state for the Values grammar. Query commands and literal
// image-to-buffer copies do not change these binding/state banks. The composing
// value inspector must independently account for their memory effects and all
// host/update/lifetime operations; this function proves no executed values.
nlohmann::json value_recording_draws(const CppReplay&, const ReplayRecording&);
// Worker-free source candidates. Selection identity is separate from proving
// descriptor/upload lifetimes or reading values at the chosen occurrence.
nlohmann::json value_execution_candidates(const CppReplay&);
} // namespace ngm::detail
