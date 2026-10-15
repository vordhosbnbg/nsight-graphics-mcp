#pragma once

#include "CppReplay.hpp"
#include <array>
#include <optional>

namespace ngm::detail {
// Values-policy allocations with unique source pool/layout definitions. This
// does not establish update history, live resources or selected shader access.
nlohmann::json value_descriptor_allocations(const CppReplay&);
// Recorded binding identity only. Descriptor writes and buffer contents are not
// decoded, and no GPU shader access or effective buffer address is inferred.
class DescriptorState {
public:
    void record(const CppReplay&, const ReplayCall&, const ReplayDeclarations&);
    nlohmann::json at_draw(const CppReplay&, const std::string& layout) const;

private:
    struct Layout {
        std::string name, push_signature;
        std::vector<std::string> sets;
    };
    struct Binding {
        Layout layout;
        std::string descriptor_set, set_layout;
        nlohmann::json offsets, evidence;
    };
    static bool compatible(const Layout&, const Layout&, std::size_t set);
    std::array<std::optional<Binding>, 8> sets_;
    std::string unsupported_;
};
} // namespace ngm::detail
