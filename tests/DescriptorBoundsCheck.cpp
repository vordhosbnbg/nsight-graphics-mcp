#include "Check.hpp"
#include "ngm/DescriptorBounds.hpp"

#include <array>
#include <limits>

int main() {
    return ngm::check::run([] {
        using ngm::check::require;
        alignas(16) std::array<std::uint64_t, 16> input{}, output{};
        const auto refused = [&](auto&& operation) {
            bool failed = false;
            try {
                operation();
            } catch(const std::runtime_error&) {
                failed = true;
            }
            require(failed, "invalid hydration window operation refuses");
        };
        ngm::DescriptorBounds window(input.data(), sizeof(input), output.data(), sizeof(output));
        require(window.pointer(nullptr, input.data()) == input.data(), "top-level absolute database pointer");
        require(window.pointer(input.data(), reinterpret_cast<const std::uint64_t*>(8)) == input.data() + 1,
                "nested relative pointer uses checked arithmetic");
        window.copy(16, input.data(), 0);
        window.copy(1, input.data() + 15, sizeof(output) - 8);
        refused([&] { window.pointer(nullptr, input.data() + input.size()); });
        refused([&] { window.pointer(input.data(), reinterpret_cast<const std::uint64_t*>(1)); });
        refused([&] { window.pointer(output.data(), reinterpret_cast<const std::uint64_t*>(8)); });
        refused([&] { window.pointer(input.data(), reinterpret_cast<const std::uint64_t*>(UINTPTR_MAX)); });
        refused([&] { window.copy(2, input.data() + 15, 0); });
        refused([&] { window.copy(1, input.data(), sizeof(output) + 1); });
        refused([&] { window.copy(2, input.data(), sizeof(output) - 8); });
        refused([&] { window.copy(1, input.data(), 1); });
        refused([&] { window.copy(0, input.data(), 0); });
        refused([&] { window.copy(UINT32_MAX, input.data(), 0); });
        refused([&] { ngm::DescriptorBounds invalid(nullptr, 8, output.data(), 8); });
        refused([&] { ngm::DescriptorBounds invalid(input.data(), 8, input.data(), 8); });
        refused([&] { ngm::DescriptorBounds invalid(input.data(), sizeof(input), output.data(), 65537); });
        refused([&] { ngm::DescriptorBounds invalid(input.data(), SIZE_MAX, output.data(), 8); });
        ngm::DescriptorBounds limited(input.data(), sizeof(input), output.data(), sizeof(output));
        for(int i = 0; i < 1024; ++i)
            limited.copy(1, input.data(), 0);
        refused([&] { limited.copy(1, input.data(), 0); });
        refused([&] { ngm::descriptor_assert(false); });
    });
}
