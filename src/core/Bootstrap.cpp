#include "ngm/Bootstrap.hpp"

#include "ngm/Version.hpp"

#include <iostream>

namespace ngm {
int bootstrap(std::span<const char* const> arguments, std::string_view name, std::string_view pending) {
    if(arguments.size() == 1 && std::string_view(arguments.front()) == "--version") {
        std::cout << name << ' ' << project_version() << '\n';
        return 0;
    }
    if(arguments.size() == 1 && std::string_view(arguments.front()) == "--help") {
        std::cout << "Usage: " << name << " [--version | --help]\n" << pending << '\n';
        return 0;
    }
    if(!arguments.empty()) {
        std::cerr << name << ": unsupported arguments; use --help.\n";
        return 2;
    }
    // In particular, never write a startup banner onto future MCP stdout.
    std::cerr << name << ": " << pending << '\n';
    return 1;
}
} // namespace ngm
