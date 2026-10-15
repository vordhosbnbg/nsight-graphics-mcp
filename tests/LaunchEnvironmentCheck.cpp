#include "Check.hpp"
#include "ngm/LaunchEnvironment.hpp"

#include <filesystem>
#include <fstream>
#include <string_view>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using ngm::check::require;
struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-launch-env-XXXXXX").string();
        require(mkdtemp(pattern.data()), "scratch directory");
        path = pattern;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
template <typename Function>
void rejected(Function&& action, std::string_view reason = {}) {
    try {
        action();
    } catch(const std::exception& error) {
        require(std::string_view(error.what()).find(reason) != std::string_view::npos,
                "specific validation refusal: " + std::string(error.what()));
        return;
    }
    require(false, "invalid launch environment refused");
}
} // namespace

int main() {
    return ngm::check::run([] {
        Scratch scratch;
        const auto libraries = scratch.path / "libraries with spaces";
        fs::create_directory(libraries);
        std::ofstream(libraries / "libtest.so") << "test library candidate, never loaded";
        const auto manifest = scratch.path / "test_icd.json";
        std::ofstream(manifest) << R"({"file_format_version":"1.0.1","ICD":{"library_path":"libtest.so"}})";
        ngm::LaunchEnvironment values{
            {"LD_LIBRARY_PATH", libraries.string()},
            {"VK_DRIVER_FILES", manifest.string()},
            {"SDL_VIDEODRIVER", "wayland"},
            {"QT_QPA_PLATFORM", "xcb"},
            {"DISABLE_VK_LAYER_LUNARG_override", "1"},
            {"VK_LOADER_LAYERS_DISABLE", "VK_LAYER_LUNARG_override,VK_LAYER_KHRONOS_validation"},
            {"VK_LOADER_DEBUG", "layer"}};
        const auto result = ngm::validate_launch_environment(values);
        require(result.at("directories")[0].at("canonical_path") == libraries.string() &&
                    result.at("icd_manifests")[0].at("library_candidate").at("canonical_path") ==
                        (libraries / "libtest.so").string() &&
                    result.at("icd_manifests")[0].at("library_resolution") == "search_candidate_only",
                "explicit library directory resolves a bare ICD name as selection evidence");
        require(result.at("icd_manifests")[0].at("sha256").get<std::string>().size() == 64 &&
                    result.at("icd_manifests")[0].at("library_candidate").at("sha256").get<std::string>().size() == 64,
                "manifest and concrete library candidate have content identities");
        const auto original = values;
        require(ngm::validate_launch_environment(values) == result && values == original,
                "validation is deterministic and preserves the requested map");
        require(ngm::validate_launch_environment({}).at("icd_manifests").empty(), "absent overlay selects no ICD");
        rejected([&] { ngm::validate_launch_environment(values, {}, std::chrono::steady_clock::now()); },
                 "deadline expired");
        std::stop_source stopped;
        stopped.request_stop();
        rejected(
            [&] {
                ngm::validate_launch_environment(values, {}, std::chrono::steady_clock::now() + std::chrono::seconds(5),
                                                 stopped.get_token());
            },
            "cancelled");
        auto legacy = values;
        legacy.erase("VK_DRIVER_FILES");
        legacy["VK_ICD_FILENAMES"] = manifest.string();
        require(ngm::validate_launch_environment(legacy).at("icd_manifests")[0].at("selector") == "VK_ICD_FILENAMES",
                "legacy ICD selector remains supported");
        auto both = values;
        both["VK_ICD_FILENAMES"] = manifest.string();
        rejected([&] { ngm::validate_launch_environment(both); }, "choose VK_DRIVER_FILES");
        rejected([&] { ngm::validate_launch_environment(values, {{"VK_ICD_FILENAMES", manifest.string()}}); },
                 "conflicts with a service-default selector");
        for(const auto* key : {"PATH", "HOME", "XDG_CONFIG_HOME", "TMPDIR", "LD_PRELOAD", "VK_INSTANCE_LAYERS",
                               "VK_LAYER_PATH", "VK_LOADER_LAYERS_ENABLE", "unrecognized"})
            rejected([&] { ngm::validate_launch_environment({{key, "value"}}); }, "unsupported key");
        for(const auto& bad : {"", ".", "relative/path", ":/usr/lib", "/usr/lib:", "/usr/lib::/lib"})
            rejected([&] { ngm::validate_launch_environment({{"LD_LIBRARY_PATH", bad}}); });
        rejected([&] { ngm::validate_launch_environment({{"LD_LIBRARY_PATH", (scratch.path / "missing").string()}}); });
        rejected([&] { ngm::validate_launch_environment({{"LD_LIBRARY_PATH", manifest.string()}}); });
        rejected([&] { ngm::validate_launch_environment({{"VK_DRIVER_FILES", libraries.string()}}); });
        for(const auto& [key, bad] : ngm::LaunchEnvironment{{"SDL_VIDEODRIVER", "dummy"},
                                                            {"QT_QPA_PLATFORM", "offscreen"},
                                                            {"DISABLE_VK_LAYER_LUNARG_override", "true"},
                                                            {"VK_LOADER_DEBUG", "all"}})
            rejected([&] { ngm::validate_launch_environment({{key, bad}}); });
        for(const auto* bad : {"*", "~implicit~", "VK_LAYER_NV_nomad", "VK_LAYER_KHRONOS_*",
                               "VK_LAYER_LUNARG_override,", "VK_LAYER_LUNARG_override,VK_LAYER_LUNARG_override"})
            rejected([&] { ngm::validate_launch_environment({{"VK_LOADER_LAYERS_DISABLE", bad}}); });
        rejected([&] { ngm::validate_launch_environment({{"SDL_VIDEODRIVER", std::string("x11\0tail", 8)}}); }, "NUL");
        rejected([&] { ngm::validate_launch_environment({{"LD_LIBRARY_PATH", std::string(4097, 'a')}}); }, "4096");
        std::string paths;
        for(unsigned i = 0; i < 17; ++i)
            paths += (i ? ":" : "") + libraries.string();
        rejected([&] { ngm::validate_launch_environment({{"LD_LIBRARY_PATH", paths}}); }, "too many list entries");
        auto excessive = values;
        for(auto& [key, value] : excessive) {
            (void)key;
            value = std::string(4096, 'x');
        }
        rejected([&] { ngm::validate_launch_environment(excessive); }, "combined names and values");
        for(const auto* text : {"not json", "{}", R"({"file_format_version":"1","ICD":{"library_path":17}})",
                                R"({"file_format_version":"1","ICD":{"library_path":"missing.so"}})"}) {
            std::ofstream(manifest) << text;
            rejected([&] { ngm::validate_launch_environment(values); });
        }
        std::ofstream(manifest)
            << R"({"file_format_version":"1.0.1","ICD":{"library_path":"libraries with spaces/libtest.so"}})";
        const auto relative = ngm::validate_launch_environment(values);
        require(relative.at("icd_manifests")[0].at("library_resolution") == "manifest_relative_path",
                "relative library names with separators resolve from the manifest");
        std::ofstream(libraries / "libtest.so", std::ios::trunc) << "changed library bytes";
        require(ngm::validate_launch_environment(values) != relative, "changed selected library invalidates identity");
        fs::create_symlink(libraries, scratch.path / "library-link");
        require(ngm::validate_launch_environment({{"LD_LIBRARY_PATH", (scratch.path / "library-link").string()}})
                        .at("directories")[0]
                        .at("canonical_path") == libraries.string(),
                "directory symlinks retain their resolved selection");
    });
}
