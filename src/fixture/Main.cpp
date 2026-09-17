#include "ngm/Bootstrap.hpp"

#include <cstddef>

int main(int argc, char** argv) {
    return ngm::bootstrap({argv + 1, static_cast<std::size_t>(argc - 1)}, "ngm-vulkan-fixture",
                          "Rendering is not implemented yet (R-005). Only build/version queries are available.");
}
