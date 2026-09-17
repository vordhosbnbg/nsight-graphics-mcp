#pragma once

#include <span>
#include <string_view>

namespace ngm {
// Build-foundation entry point. Serving and rendering arrive in R-003/R-005.
int bootstrap(std::span<const char* const> arguments, std::string_view name, std::string_view pending);
} // namespace ngm
