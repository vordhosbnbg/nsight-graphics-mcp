#include "ValueSourceContract.hpp"

#include "ReplayValues.hpp"
#include <algorithm>
#include <array>
#include <set>

namespace ngm::detail {
std::string canonical_value_procedures(std::string_view source) {
    require(source.size() <= 65536 && source.find('\0') == std::string_view::npos,
            "Replay procedures exceed the bounded text profile");
    constexpr std::array<std::string_view, 15> declarations{
        "extern void CreateResources00();",
        "extern void CreateWinResources00();",
        "extern void OnAfterWinResourcesInit00();",
        "extern void OnAfterWinResourcesReleased00();",
        "extern void OnBeforeWinResourcesInit00();",
        "extern void ReleaseResources00();",
        "extern void ReleaseWinResources00();",
        "extern void ResetInitialFrameState00(bool firstTime, bool isAsync);",
        "extern void ResetState00();",
        "extern void ResetWinResources(bool firstTime);",
        "extern void RestoreEventRanges();",
        "extern void RunFrame0Part00(uint64_t frameIndex);",
        "extern void SetInitialFrameState00();",
        "extern void SetupWinResources();",
        "extern void TerminateEventRanges();"};
    std::set<std::string_view> seen;
    std::string result = "ngm-cpp-procedures-v1\n";
    bool body = false;
    for(std::size_t at = 0; at < source.size();) {
        const auto end = source.find('\n', at);
        const auto length = (end == std::string_view::npos ? source.size() : end) - at;
        const auto line = source.substr(at, length);
        body = body || line.find('{') != std::string_view::npos;
        if(line.starts_with("extern ")) {
            require(!body && end != std::string_view::npos, "Replay prototype is outside the literal prologue");
            require(std::find(declarations.begin(), declarations.end(), line) != declarations.end() &&
                        seen.insert(line).second,
                    "Unknown or duplicate replay prototype");
        } else
            result.append(line);
        if(end != std::string_view::npos)
            result += '\n';
        at += length + (end != std::string_view::npos ? 1 : 0);
    }
    require(seen.size() == declarations.size(), "Replay prototype set is incomplete");
    result += "\n// ngm canonical prototypes\n";
    for(const auto declaration : declarations) {
        result.append(declaration);
        result += '\n';
    }
    return result;
}
} // namespace ngm::detail
