#pragma once

#include <string>
#include <string_view>

namespace ngm::detail {
// Canonicalize only the complete, literal prototype set in the generated
// ReplayProcedures prologue. Every other byte, including the executable body,
// remains part of the source contract. This does not execute exported source.
std::string canonical_value_procedures(std::string_view source);
} // namespace ngm::detail
