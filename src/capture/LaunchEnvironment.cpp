#include "ngm/LaunchEnvironment.hpp"

#include "ngm/File.hpp"
#include "ngm/Hash.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ngm {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::array keys{"LD_LIBRARY_PATH",          "VK_DRIVER_FILES", "VK_ICD_FILENAMES",
                          "SDL_VIDEODRIVER",          "QT_QPA_PLATFORM", "DISABLE_VK_LAYER_LUNARG_override",
                          "VK_LOADER_LAYERS_DISABLE", "VK_LOADER_DEBUG"};
void require(bool condition, const std::string& reason) {
    if(!condition)
        throw std::invalid_argument("launch_environment: " + reason);
}
std::vector<std::string> split(const std::string& text, char separator, std::size_t maximum) {
    std::vector<std::string> result;
    std::size_t start = 0;
    for(;;) {
        const auto end = text.find(separator, start);
        const auto part = text.substr(start, end == std::string::npos ? end : end - start);
        require(!part.empty(), "empty list entry");
        result.push_back(part);
        require(result.size() <= maximum, "too many list entries");
        if(end == std::string::npos)
            return result;
        start = end + 1;
    }
}
fs::path absolute_existing(const fs::path& path, bool directory) {
    require(path.is_absolute(), "paths must be absolute");
    std::error_code error;
    const auto resolved = fs::canonical(path, error);
    require(!error && (directory ? fs::is_directory(resolved, error) : fs::is_regular_file(resolved, error)) && !error,
            "expected an existing " + std::string(directory ? "directory: " : "regular file: ") + path.string());
    return resolved;
}
Json file_identity(const fs::path& path, std::chrono::steady_clock::time_point deadline, std::stop_token stop) {
    const auto resolved = absolute_existing(path, false);
    return {{"path", path.string()},
            {"canonical_path", resolved.string()},
            {"sha256", sha256_regular_file(resolved, deadline, stop)}};
}
} // namespace

Json launch_environment_policy() {
    return {{"supported_keys", keys},
            {"maximum_keys", 8},
            {"maximum_value_bytes", 4096},
            {"maximum_path_entries", 16},
            {"maximum_total_bytes", 16384},
            {"scope", "Per-job tool and target overlay; reserved HOME/XDG/temp isolation remains enforced. "
                      "Selections do not prove actual loaded libraries or backend compatibility."}};
}

Json validate_launch_environment(const LaunchEnvironment& overrides, const LaunchEnvironment& service_defaults,
                                 std::chrono::steady_clock::time_point deadline, std::stop_token stop) {
    require(!stop.stop_requested() && std::chrono::steady_clock::now() < deadline,
            "validation cancelled or deadline expired");
    require(overrides.size() <= keys.size(), "at most eight keys");
    std::size_t total = 0;
    for(const auto& [key, value] : overrides) {
        require(std::find(keys.begin(), keys.end(), key) != keys.end(), "unsupported key: " + key);
        require(!value.empty() && value.size() <= 4096 && value.find('\0') == std::string::npos,
                key + " requires 1..4096 bytes without NUL");
        total += key.size() + value.size();
    }
    require(total <= 16384, "combined names and values exceed 16 KiB");
    require(!(overrides.contains("VK_DRIVER_FILES") && overrides.contains("VK_ICD_FILENAMES")),
            "choose VK_DRIVER_FILES or VK_ICD_FILENAMES, not both");
    auto effective = service_defaults;
    for(const auto& [key, value] : overrides)
        effective[key] = value;
    // Also reject conflicting inherited selectors when the job explicitly chooses
    // a driver. The applied environment must agree with the retained selection.
    if(overrides.contains("VK_DRIVER_FILES") || overrides.contains("VK_ICD_FILENAMES"))
        require(!(effective.contains("VK_DRIVER_FILES") && effective.contains("VK_ICD_FILENAMES")),
                "selected ICD conflicts with a service-default selector");

    Json selection{{"directories", Json::array()}, {"icd_manifests", Json::array()}};
    if(overrides.contains("LD_LIBRARY_PATH")) {
        for(const auto& path : split(overrides.at("LD_LIBRARY_PATH"), ':', 16))
            selection["directories"].push_back(
                {{"path", path}, {"canonical_path", absolute_existing(path, true).string()}});
    }
    for(const auto& [key, value] : overrides) {
        if(key == "SDL_VIDEODRIVER" || key == "QT_QPA_PLATFORM")
            require(value == "wayland" || value == (key == "SDL_VIDEODRIVER" ? "x11" : "xcb"),
                    "unsupported desktop backend for " + key);
        else if(key == "DISABLE_VK_LAYER_LUNARG_override")
            require(value == "0" || value == "1", key + " requires 0 or 1");
        else if(key == "VK_LOADER_DEBUG")
            require(value == "layer", "only the qualified VK_LOADER_DEBUG=layer selector is accepted");
        else if(key == "VK_LOADER_LAYERS_DISABLE") {
            std::set<std::string> seen;
            for(const auto& layer : split(value, ',', 2))
                require((layer == "VK_LAYER_LUNARG_override" || layer == "VK_LAYER_KHRONOS_validation") &&
                            seen.insert(layer).second,
                        "layer disabling accepts only distinct qualified unwanted layer names");
        }
    }
    for(const auto* key : {"VK_DRIVER_FILES", "VK_ICD_FILENAMES"}) {
        if(!overrides.contains(key))
            continue;
        for(const auto& path : split(overrides.at(key), ':', 16)) {
            const auto manifest = absolute_existing(path, false);
            require(manifest.extension() == ".json", "ICD manifests require a .json extension");
            const auto text = read_regular_file(manifest, 1024 * 1024);
            const auto data = Json::parse(text, [](int depth, Json::parse_event_t event, Json&) {
                require(depth < 32 ||
                            (event != Json::parse_event_t::object_start && event != Json::parse_event_t::array_start),
                        "ICD manifest nesting exceeds 32 containers");
                return true;
            });
            require(data.is_object() && data.contains("file_format_version") &&
                        data.at("file_format_version").is_string() && data.contains("ICD") &&
                        data.at("ICD").is_object() && data.at("ICD").contains("library_path") &&
                        data.at("ICD").at("library_path").is_string(),
                    "ICD manifest requires file_format_version and ICD.library_path");
            const auto name = data.at("ICD").at("library_path").get<std::string>();
            require(!name.empty() && name.size() <= 4096 && name.find('\0') == std::string::npos,
                    "invalid ICD.library_path");
            const fs::path library(name);
            fs::path resolved;
            std::string resolution;
            if(library.is_absolute() || library.has_parent_path()) {
                resolved = absolute_existing(library.is_absolute() ? library : manifest.parent_path() / library, false);
                resolution = library.is_absolute() ? "absolute_manifest_path" : "manifest_relative_path";
            } else {
                // Bare names follow the loader's dynamic-library search. Verify a
                // concrete candidate without claiming it is the loaded object.
                std::vector<std::string> directories;
                if(effective.contains("LD_LIBRARY_PATH"))
                    directories = split(effective.at("LD_LIBRARY_PATH"), ':', 16);
                directories.insert(directories.end(), {"/usr/lib", "/usr/lib64", "/lib", "/lib64",
                                                       "/usr/lib/x86_64-linux-gnu", "/lib/x86_64-linux-gnu"});
                for(const auto& directory : directories) {
                    std::error_code error;
                    const auto candidate = fs::path(directory) / library;
                    if(fs::is_regular_file(candidate, error) && !error) {
                        resolved = absolute_existing(candidate, false);
                        break;
                    }
                }
                require(!resolved.empty(), "ICD library has no verified candidate; supply LD_LIBRARY_PATH: " + name);
                resolution = "search_candidate_only";
            }
            auto identity = file_identity(manifest, deadline, stop);
            require(identity.at("sha256") == sha256(std::as_bytes(std::span(text.data(), text.size()))),
                    "ICD manifest changed during validation");
            identity["selector"] = key;
            identity["requested_path"] = path;
            identity["library_path"] = name;
            identity["library_candidate"] = file_identity(resolved, deadline, stop);
            identity["library_resolution"] = resolution;
            selection["icd_manifests"].push_back(std::move(identity));
        }
    }
    return selection;
}
} // namespace ngm
