#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>

struct VkQueue_T;

namespace ngm::fixture {
// Optional per-launch SDK control. No SDK call occurs without an explicit frame
// selector. Reports are application-provided observations, not Nsight exports.
class SdkControl {
public:
    SdkControl(std::optional<std::uint32_t> first_boundary_frame, std::filesystem::path report_path);
    void initialize_before_vulkan(nlohmann::json application_context);
    void before_frame(std::uint32_t application_frame, VkQueue_T* queue);
    const nlohmann::json& report() const noexcept;

private:
    void publish() const;
    std::optional<std::uint32_t> first_frame_;
    std::filesystem::path report_path_;
    nlohmann::json report_;
    bool initialized_ = false;
    std::uint32_t entered_ = 0;
    [[maybe_unused]] std::uint32_t completed_ = 0; // Used only when SDK calls are compiled in.
};
} // namespace ngm::fixture
