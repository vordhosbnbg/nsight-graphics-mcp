#pragma once
#include "ngm/Hash.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace ngm::check::resource {
inline std::string digest(std::string_view bytes) {
    return sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
}
inline nlohmann::json profile() {
    nlohmann::json files = nlohmann::json::object();
    for(const auto* name :
        {"ReadOnlyDatabase.cpp", "ReadOnlyDatabase.h", "DataScope.cpp", "DataScope.h", "DllCommon.h"})
        files[name] = digest("test-only helper");
    return {{"profile", "cpu-test-only"}, {"files", files}};
}
inline std::string payload() {
    return std::string("\0\xff\x10\r\nABC", 8);
}
} // namespace ngm::check::resource
