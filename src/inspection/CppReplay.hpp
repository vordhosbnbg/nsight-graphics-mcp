#pragma once

#include "ngm/CppEvidence.hpp"

#include <map>
#include <string>
#include <vector>

namespace ngm::detail {
// Bounded syntax only. This representation never executes generated code or
// hydrates serialized structures. State interpretation lives in DrawState.cpp.
struct ReplayValue {
    std::string text;
    std::string constructor;
    std::vector<ReplayValue> arguments;
    std::size_t position = 0;
    nlohmann::json source;
};
struct ReplayDeclaration {
    std::string type;
    std::vector<ReplayValue> elements;
    std::size_t end = 0;
};
using ReplayDeclarations = std::map<std::string, ReplayDeclaration>;
struct ReplayCall {
    std::string name;
    std::vector<ReplayValue> arguments;
    std::uint64_t event = 0;
    std::size_t position = 0;
    nlohmann::json source;
    // Frames use nested anonymous scopes; retain only declarations visible at
    // this statement. Recording/object calls share their containing scope.
    ReplayDeclarations visible;
};
struct ReplayObject {
    ReplayCall creation;
    ReplayDeclarations values;
    nlohmann::json source;
    nlohmann::json shader_relationship;
};
struct ReplayRecording {
    std::vector<ReplayCall> calls;
    ReplayDeclarations values;
    nlohmann::json source;
};
struct CppReplay {
    std::map<std::string, ReplayObject> objects;
    std::map<std::string, ReplayRecording> recordings;
    std::vector<ReplayCall> frame;
    nlohmann::json unsupported = nlohmann::json::array();
};
CppReplay parse_cpp_replay(const std::vector<CppSource>& sources);
} // namespace ngm::detail
