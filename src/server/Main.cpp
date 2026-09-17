#include "Server.hpp"
#include "ngm/Version.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

int main(int argc, char** argv) {
    if(argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "nsight-graphics-mcp " << ngm::project_version() << '\n';
        return 0;
    }
    if(argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: nsight-graphics-mcp [--nsight-root PATH] | --version | --help\n"
                     "Serve MCP on stdin/stdout until EOF. Diagnostics use stderr.\n"
                     "--nsight-root selects an absolute Nsight installation directory for discovery.\n"
                     "The capabilities tool reports prerequisites; capture and inspection are not implemented.\n";
        return 0;
    }
    std::optional<std::filesystem::path> nsight_root;
    if(argc == 3 && std::string_view(argv[1]) == "--nsight-root") {
        std::filesystem::path path(argv[2]);
        std::error_code error;
        if(!path.is_absolute() || !std::filesystem::is_directory(path, error) || error) {
            std::cerr << "nsight-graphics-mcp: --nsight-root requires an existing absolute directory.\n";
            return 2;
        }
        nsight_root = std::filesystem::canonical(path, error);
        if(error) {
            std::cerr << "nsight-graphics-mcp: cannot resolve --nsight-root: " << error.message() << '\n';
            return 2;
        }
    } else if(argc != 1) {
        std::cerr << "nsight-graphics-mcp: unsupported arguments; use --help.\n";
        return 2;
    }
    try {
        return ngm::serve_stdio(nsight_root);
    } catch(const std::exception& error) {
        std::cerr << "nsight-graphics-mcp: " << error.what() << '\n';
        return 1;
    }
}
