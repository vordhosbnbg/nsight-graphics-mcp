#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ngm {
struct CppSource {
    std::string path;
    std::string text;
};
// Pure, bounded inspection of the qualified generated-source grammar. Results
// describe literal source relationships, never executed GPU state or resource bytes.
// Unsupported recordings/creation forms remain explicit in the returned coverage.
nlohmann::json inspect_cpp_draws(const std::vector<CppSource>& sources);
// Literal serialized-resource macro occurrences only. IDs bind a source file's
// full content identity and byte span. No execution order or target state is
// inferred; unsupported occurrences and conflicting sizes remain explicit.
nlohmann::json inspect_cpp_resources(const std::vector<CppSource>& sources);
} // namespace ngm
