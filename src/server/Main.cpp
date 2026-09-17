#include "ngm/Bootstrap.hpp"

#include <cstddef>

int main(int argc, char** argv) {
    return ngm::bootstrap({argv + 1, static_cast<std::size_t>(argc - 1)}, "nsight-graphics-mcp",
                          "MCP serving is not implemented yet (R-003). Only build/version queries are available.");
}
