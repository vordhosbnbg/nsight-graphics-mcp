#include "CaptureValidation.hpp"
#include "Check.hpp"

#include <array>
#include <optional>

int main() {
    return ngm::check::run([] {
        using ngm::check::capture::classify_job;
        using ngm::check::capture::JobDisposition;
        using ngm::check::capture::valid_export_size;
        using ngm::check::require;

        const std::array<std::optional<bool>, 3> readiness{std::nullopt, false, true};
        for(const auto* terminal : {"timed_out", "cancelled"}) {
            for(const auto ready : readiness) {
                require(classify_job(terminal, ready) == JobDisposition::failed,
                        "timeout/cancellation remains a failure regardless of discovery readiness");
            }
        }
        require(classify_job("failed", false) == JobDisposition::unsupported,
                "explicit interface unavailability remains distinct from execution failure");
        require(classify_job("failed", true) == JobDisposition::failed &&
                    classify_job("failed", std::nullopt) == JobDisposition::failed,
                "failed capture or missing discovery evidence does not claim unsupported tooling");
        require(classify_job("succeeded", true) == JobDisposition::inspect,
                "successful job with ready matching tools proceeds to evidence inspection");
        require(classify_job("succeeded", false) == JobDisposition::failed &&
                    classify_job("succeeded", std::nullopt) == JobDisposition::failed,
                "contradictory or missing discovery evidence cannot pass a successful job");
        for(const auto* state : {"queued", "running", "unknown"}) {
            for(const auto ready : readiness) {
                require(classify_job(state, ready) == JobDisposition::failed,
                        "nonterminal or unknown job state cannot establish unsupported or successful capture");
            }
        }

        require(valid_export_size("logs", 0), "a successful empty logs file is valid evidence of no messages");
        require(valid_export_size("logs", 1), "nonempty log exports remain valid");
        for(const auto* kind : {"metadata", "functions", "objects", "screenshot"}) {
            require(!valid_export_size(kind, 0), "other exports cannot silently accept missing content");
            require(valid_export_size(kind, 1), "nonempty known exports proceed to their remaining checks");
        }
        require(!valid_export_size("unknown", 0) && !valid_export_size("unknown", 1),
                "unknown export kinds cannot use the empty-log exception");
    });
}
