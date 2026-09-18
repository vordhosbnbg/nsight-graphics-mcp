#include "Server.hpp"
#include "ngm/Version.hpp"

#include <charconv>
#include <filesystem>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {
std::uint64_t unsigned_option(std::string_view name, std::string_view value, std::uint64_t maximum) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result > maximum) {
        throw std::invalid_argument(std::string(name) + " requires a nonnegative integer within range");
    }
    return result;
}

ngm::ServerOptions parse_options(int argc, char** argv) {
    ngm::ServerOptions options;
    std::set<std::string_view> seen;
    for(int index = 1; index < argc; index += 2) {
        const std::string_view name = argv[index];
        if(index + 1 == argc || !seen.insert(name).second) {
            throw std::invalid_argument("Every option requires one value and may be supplied only once; use --help");
        }
        const std::string_view value = argv[index + 1];
        if(name == "--transport") {
            if(value != "stdio" && value != "http")
                throw std::invalid_argument("--transport requires stdio or http");
            options.http = value == "http";
        } else if(name == "--http-port") {
            options.http_port = static_cast<unsigned>(unsigned_option(name, value, 65535));
        } else if(name == "--http-token-file") {
            if(!std::filesystem::path(value).is_absolute() || value.size() > 4096)
                throw std::invalid_argument("--http-token-file requires an absolute path");
            options.http_token_file = value;
        } else if(name == "--nsight-root") {
            const std::filesystem::path path(value);
            std::error_code error;
            if(!path.is_absolute() || !std::filesystem::is_directory(path, error) || error) {
                throw std::invalid_argument("--nsight-root requires an existing absolute directory");
            }
            options.nsight_root = std::filesystem::canonical(path);
        } else if(name == "--resource-worker-2026-3" || name == "--resource-worker-2026-2") {
            const std::filesystem::path path(value);
            std::error_code error;
            if(!path.is_absolute() || value.size() > 4096 || !std::filesystem::is_regular_file(path, error) || error ||
               access(path.c_str(), X_OK) != 0)
                throw std::invalid_argument(std::string(name) + " requires an existing absolute executable file");
            auto& worker = name == "--resource-worker-2026-3" ? options.resource_workers.nsight_2026_3
                                                              : options.resource_workers.nsight_2026_2;
            worker = std::filesystem::canonical(path);
        } else if(name == "--artifact-root") {
            const std::filesystem::path path(value);
            if(!path.is_absolute() || value.size() > 4096 || path.lexically_normal() == path.root_path()) {
                throw std::invalid_argument("--artifact-root requires an absolute directory below the filesystem root");
            }
            options.artifacts.root = path.lexically_normal();
        } else if(name == "--artifact-max-bytes") {
            options.artifacts.max_bytes = unsigned_option(name, value, std::numeric_limits<std::uint64_t>::max());
        } else if(name == "--artifact-max-age-seconds") {
            options.artifacts.max_age = std::chrono::seconds(unsigned_option(
                name, value, static_cast<std::uint64_t>(std::numeric_limits<std::chrono::seconds::rep>::max())));
        } else {
            throw std::invalid_argument("Unsupported option " + std::string(name) + "; use --help");
        }
    }
    if(options.artifacts.root.empty() &&
       (seen.contains("--artifact-max-bytes") || seen.contains("--artifact-max-age-seconds"))) {
        throw std::invalid_argument("Artifact retention options require --artifact-root ABS_PATH");
    }
    if(options.http && options.http_token_file.empty())
        throw std::invalid_argument("HTTP requires --http-token-file ABS_PATH");
    if(!options.http && (seen.contains("--http-port") || seen.contains("--http-token-file")))
        throw std::invalid_argument("HTTP options require --transport http");
    return options;
}
} // namespace

int main(int argc, char** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "nsight-graphics-mcp " << ngm::project_version() << '\n';
        return 0;
    }
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: nsight-graphics-mcp [OPTIONS] | --version | --help\n"
                     "Serve MCP over stdio (default) or loopback HTTP. Diagnostics use stderr.\n"
                     "  --transport stdio|http           Select transport; default stdio.\n"
                     "  --http-port N                    HTTP port; default 18080, 0 selects a free port.\n"
                     "  --http-token-file ABS_PATH       Required private bearer-token file for HTTP.\n"
                     "  --nsight-root ABS_PATH           Select an existing Nsight installation.\n"
                     "  --artifact-root ABS_PATH         Enable capture/job/artifact workflow tools.\n"
                     "  --artifact-max-bytes N           Retention limit; default 2147483648, 0 disables.\n"
                     "  --artifact-max-age-seconds N     Completed age limit; default 2592000, 0 disables.\n"
                     "  --resource-worker-2026-3 ABS_PATH Qualified reader executable for Nsight 2026.3.\n"
                     "  --resource-worker-2026-2 ABS_PATH Qualified reader executable for Nsight 2026.2.\n"
                     "Artifact storage is opened lazily on a workflow call. Discovery runs no Nsight commands.\n"
                     "Capture launches a fresh application; EOF cancels and cleans up owned jobs.\n";
        return 0;
    }
    ngm::ServerOptions options;
    try {
        options = parse_options(argc, argv);
    } catch(const std::exception& error) {
        std::cerr << "nsight-graphics-mcp: unsupported arguments: " << error.what() << '\n';
        return 2;
    }
    try {
        return options.http ? ngm::serve_http(options) : ngm::serve_stdio(options);
    } catch(const std::exception& error) {
        std::cerr << "nsight-graphics-mcp: " << error.what() << '\n';
        return 1;
    }
}
