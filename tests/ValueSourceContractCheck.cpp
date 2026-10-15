#include "Check.hpp"
#include "ReplayValues.hpp"
#include "ValueSourceContract.hpp"

#include <algorithm>
#include <array>

int main() {
    return ngm::check::run([] {
        using ngm::check::require;
        // Literal interface declarations and a hand-written body; no vendor
        // implementation is needed by this ordinary CPU regression check.
        std::array<std::string, 15> declarations{"extern void CreateResources00();",
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
        const auto source = [&] {
            std::string text = "// Hand-written prologue\n";
            for(const auto& line : declarations)
                text += line + '\n';
            return text + "\nvoid Example() {\n  SetInitialFrameState00();\n}\n";
        };
        const auto original = source();
        const auto canonical = ngm::detail::canonical_value_procedures(original);
        std::reverse(declarations.begin(), declarations.end());
        require(ngm::detail::canonical_value_procedures(source()) == canonical,
                "literal prototype permutations preserve the contract");
        auto changed = original;
        changed.replace(changed.find("  SetInitialFrameState00();"), std::string("  SetInitialFrameState00();").size(),
                        "  ResetState00();");
        require(ngm::detail::canonical_value_procedures(changed) != canonical,
                "executable body changes remain fingerprinted");
        const auto refuses = [&](const std::string& text) {
            bool refused = false;
            try {
                (void)ngm::detail::canonical_value_procedures(text);
            } catch(const ngm::detail::Unqualified&) {
                refused = true;
            }
            require(refused, "incomplete, ambiguous or executable prototypes refuse normalization");
        };
        changed = original;
        changed.erase(changed.find("extern void CreateResources00();\n"),
                      std::string("extern void CreateResources00();\n").size());
        refuses(changed);
        refuses("extern void Unknown();\n" + original);
        refuses(declarations[0] + '\n' + original);
        refuses(original + "extern void Unknown();\n");
        changed = original;
        changed.replace(changed.find("extern void CreateResources00();"),
                        std::string("extern void CreateResources00();").size(),
                        "extern void CreateResources00(int value = mutate());");
        refuses(changed);
        refuses(std::string(65537, 'x'));
        refuses(std::string(1, '\0') + original);
    });
}
