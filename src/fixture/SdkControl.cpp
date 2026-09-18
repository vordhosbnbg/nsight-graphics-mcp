#include "SdkControl.hpp"
#include "Fixture.hpp"
#include "SdkBuild.hpp"

#if NGM_FIXTURE_HAS_NSIGHT_SDK
    #include <NGFX_GraphicsCapture_Vulkan.h>
#endif

#include <fstream>
#include <stdexcept>
#include <utility>

namespace ngm::fixture {
SdkControl::SdkControl(std::optional<std::uint32_t> first_boundary_frame, std::filesystem::path report_path) :
    first_frame_(first_boundary_frame), report_path_(std::move(report_path)),
    report_{
        {"schema_version", 1},
        {"evidence_origin", "application_sdk_control"},
        {"sdk_build", nlohmann::json::parse(sdk_build_json)},
        {"requested", first_frame_.has_value()},
        {"status", first_frame_ ? "not_initialized" : "not_requested"},
        {"first_boundary_application_frame", first_frame_ ? nlohmann::json(*first_frame_) : nlohmann::json(nullptr)},
        {"application_frame_numbering", "zero_based"},
        {"delimiter", "graphics_capture_api"},
        {"initialized_before_vulkan_instance", false},
        {"boundaries_entered", 0},
        {"boundaries_completed", 0}} {}

void SdkControl::publish() const {
    const auto temporary = report_path_.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output << report_.dump(2) << '\n';
        output.close();
    }
    std::filesystem::rename(temporary, report_path_);
}

void SdkControl::initialize_before_vulkan(nlohmann::json application_context) {
    if(!first_frame_) {
        return;
    }
    if(initialized_) {
        throw std::logic_error("SDK control must initialize once before the Vulkan instance");
    }
    report_["application_context"] = std::move(application_context);
    publish();
#if NGM_FIXTURE_HAS_NSIGHT_SDK
    NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params parameters{};
    parameters.version = NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params_VER;
    const auto result = NGFX_GraphicsCapture_InitializeActivity_Vulkan(&parameters);
    report_["initialization_result"] = static_cast<int>(result);
    initialized_ = result == NGFX_Result_Success;
    report_["initialized_before_vulkan_instance"] = initialized_;
    report_["status"] = initialized_ ? "initialized" : "unavailable";
    publish();
    if(!initialized_) {
        throw Unsupported("NGFX SDK graphics-capture initialization failed with result " +
                          std::to_string(static_cast<int>(result)) +
                          "; launch through matching ngfx-capture with Graphics Capture API delimiters");
    }
#else
    report_["status"] = "not_compiled";
    publish();
    throw Unsupported("SDK control was requested but this fixture was built without NGM_NSIGHT_SDK_ROOT");
#endif
}

void SdkControl::before_frame(std::uint32_t application_frame, VkQueue_T* queue) {
    if(!first_frame_ || application_frame < *first_frame_) {
        return;
    }
    if(!initialized_ || queue == nullptr) {
        throw std::logic_error("SDK boundary requires successful initialization and a Vulkan queue");
    }
    report_["last_boundary_application_frame"] = application_frame;
    report_["boundaries_entered"] = ++entered_;
    report_["status"] = "boundary_pending";
    report_["last_boundary_result"] = nullptr;
    // Nsight may terminate the target inside this call after saving the capture.
    // Retain entry separately; it never claims that the call returned success.
    publish();
#if NGM_FIXTURE_HAS_NSIGHT_SDK
    NGFX_FrameBoundary_Vulkan_Params parameters{};
    parameters.version = NGFX_FrameBoundary_Vulkan_Params_VER;
    parameters.queue = queue;
    parameters.outputResources = nullptr;
    parameters.numOutputResources = 0;
    const auto result = NGFX_FrameBoundary_Vulkan(&parameters);
    report_["last_boundary_result"] = static_cast<int>(result);
    report_["status"] = result == NGFX_Result_Success ? "boundary_completed" : "failed";
    if(result == NGFX_Result_Success) {
        report_["boundaries_completed"] = ++completed_;
        report_["last_completed_boundary_application_frame"] = application_frame;
    }
    publish();
    if(result != NGFX_Result_Success) {
        throw std::runtime_error("NGFX frame boundary failed with result " + std::to_string(static_cast<int>(result)));
    }
#endif
}

const nlohmann::json& SdkControl::report() const noexcept {
    return report_;
}
} // namespace ngm::fixture
