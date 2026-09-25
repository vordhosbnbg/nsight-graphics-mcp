#include "Fixture.hpp"
#include "ngm/Version.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ngm-vulkan-fixture " << ngm::project_version() << '\n';
        return 0;
    }
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: ngm-vulkan-fixture --scenario NAME --seed UINT --width UINT --height UINT\n"
                     "                          --frame UINT --output NEW_DIRECTORY [--shader-dir DIRECTORY]\n"
                     "                          [--sdk-first-boundary-frame UINT] [--compute-boundary "
                     "none|vk_frame_boundary] [--warmup UINT]\n"
                     "       ngm-vulkan-fixture [--version | --help]\n"
                     "Scenarios: reference, shader-error, binding-error, pipeline-error,\n"
                     "multipass-reference, pass-output-error, bindless-reference, resource-selection-error,\n"
                     "indirect-reference, indirect-parameter-error, combined-reference, combined-pass-error,\n"
                     "combined-resource-error, combined-indirect-error, compute-reference, compute-index-error, "
                     "compute-arithmetic-error, performance-reference, performance-underfilled.\n"
                     "Performance scenarios require --warmup 0..100 and frame >= warmup; timestamps exclude warmup.\n"
                     "All six inputs are required. Frame is zero-based (0..600); dimensions are 32..4096.\n"
                     "Compute scenarios use width*height uint32 elements (maximum 16384), no presentation, and "
                     "per-frame JSON readback.\n"
                     "Graphics uses an existing X11/Xwayland desktop through DISPLAY, a Vulkan 1.3 device,\n"
                     "and verified diagnostic GLSL/SPIR-V artifacts. Each launch presents frames 0..frame\n"
                     "and saves the selected frame as application readback in image.ppm and result.json.\n"
                     "SDK boundaries are optional, require an SDK-enabled build and Nsight injection, and start\n"
                     "before the selected zero-based application frame; at least two later frames must remain.\n";
        return 0;
    }
    try {
        const auto options = ngm::fixture::parse_arguments({argv + 1, static_cast<std::size_t>(argc - 1)});
        ngm::fixture::run(options);
        return 0;
    } catch(const std::invalid_argument& error) {
        std::cerr << "ngm-vulkan-fixture: unsupported arguments: " << error.what() << "; use --help.\n";
        return 2;
    } catch(const ngm::fixture::Unsupported& error) {
        std::cerr << "ngm-vulkan-fixture: unsupported prerequisite: " << error.what() << '\n';
        return 3;
    } catch(const std::exception& error) {
        std::cerr << "ngm-vulkan-fixture: " << error.what() << '\n';
        return 1;
    }
}
