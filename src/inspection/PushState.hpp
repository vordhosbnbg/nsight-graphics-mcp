#pragma once

#include "CppReplay.hpp"
#include <array>

namespace ngm::detail {
class PushState {
public:
    PushState();
    void record(const CppReplay& replay, const ReplayCall& call, const ReplayDeclarations& values);
    nlohmann::json at_draw(const CppReplay& replay, const std::string& layout) const;

private:
    struct Write {
        std::string layout_signature;
        std::uint32_t stages = 0;
        std::size_t offset = 0, size = 0;
        std::uint64_t handle = 0, event = 0;
        nlohmann::json source, resource_source;
    };
    std::vector<Write> writes_;
    std::array<std::array<int, 256>, 6> bytes_;
    std::string unsupported_;
};
// Pure merge of already verified reader results. No filesystem/worker access;
// validate input lengths/hex and commit the field only after a complete merge.
void resolve_push_data(nlohmann::json& field, const std::map<std::string, nlohmann::json>& reads);
} // namespace ngm::detail
