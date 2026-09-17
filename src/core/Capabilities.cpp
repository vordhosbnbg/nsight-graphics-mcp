#include "ngm/Capabilities.hpp"

#include <cstdlib>
#include <string_view>
#include <unistd.h>

namespace ngm {
namespace {
std::optional<std::filesystem::path> executable_at(const std::filesystem::path& path) {
    std::error_code error;
    if(!std::filesystem::is_regular_file(path, error) || error || access(path.c_str(), X_OK) != 0) {
        return std::nullopt;
    }
    auto canonical = std::filesystem::canonical(path, error);
    return error ? std::nullopt : std::optional(std::move(canonical));
}

std::optional<std::filesystem::path> on_path(std::string_view name) {
    const char* value = std::getenv("PATH");
    if(value == nullptr) {
        return std::nullopt;
    }
    std::string_view remaining(value);
    while(true) {
        const auto separator = remaining.find(':');
        const auto part = remaining.substr(0, separator);
        const auto directory = part.empty() ? std::filesystem::path(".") : std::filesystem::path(part);
        if(auto path = executable_at(directory / name)) {
            return path;
        }
        if(separator == std::string_view::npos) {
            return std::nullopt;
        }
        remaining.remove_prefix(separator + 1);
    }
}

bool environment_present(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0';
}
} // namespace

PrerequisiteObservations discover_prerequisites(const std::optional<std::filesystem::path>& nsight_root) {
    PrerequisiteObservations result;
    result.executables = {{{"ngfx", {}}, {"ngfx-capture", {}}, {"ngfx-replay", {}}}};
    result.display_present = environment_present("DISPLAY");
    result.wayland_display_present = environment_present("WAYLAND_DISPLAY");
    result.nsight_root = nsight_root;
    result.discovery_source = nsight_root ? "explicit_root" : "PATH_and_adjacent_tools";
    if(nsight_root) {
        for(auto& tool : result.executables) {
            tool.path = executable_at(*nsight_root / "host/linux-desktop-nomad-x64" / tool.name);
            if(!tool.path) {
                tool.path = executable_at(*nsight_root / tool.name);
            }
        }
        return result;
    }

    result.executables[0].path = on_path("ngfx");
    for(std::size_t i = 1; i < result.executables.size(); ++i) {
        auto& tool = result.executables[i];
        if(result.executables[0].path) {
            const auto directory = result.executables[0].path->parent_path();
            tool.path = executable_at(directory / tool.name);
            if(!tool.path) {
                tool.path = executable_at(directory / "host/linux-desktop-nomad-x64" / tool.name);
            }
        }
        if(!tool.path) {
            tool.path = on_path(tool.name);
        }
    }
    return result;
}
} // namespace ngm
