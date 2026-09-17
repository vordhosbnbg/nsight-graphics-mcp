#include "Check.hpp"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 2, "expected one SPIR-V path");
        std::ifstream input(argv[1], std::ios::binary);
        require(input.good(), "open compiled shader");
        const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        require(bytes.size() >= 20 && bytes.size() % 4 == 0, "SPIR-V header and word alignment");
        std::vector<std::uint32_t> words;
        for(std::size_t offset = 0; offset < bytes.size(); offset += 4) {
            words.push_back(static_cast<std::uint32_t>(bytes[offset]) |
                            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
                            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
                            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24));
        }
        require(words[0] == 0x07230203, "SPIR-V magic");
        require(words[1] == 0x00010600, "Vulkan 1.3 compilation produces SPIR-V 1.6");
        bool source = false;
        bool line = false;
        for(std::size_t offset = 5; offset < words.size();) {
            const auto count = words[offset] >> 16;
            const auto opcode = words[offset] & 0xffff;
            require(count > 0 && count <= words.size() - offset, "valid SPIR-V instruction size");
            // OpSource includes source text beyond its language, version and file operands.
            source |= opcode == 3 && count > 4 && words[offset + 1] == 2 && words[offset + 2] == 460;
            line |= opcode == 8 && count == 4;
            offset += count;
        }
        require(source, "embedded GLSL source is retained");
        require(line, "source line information is retained");
    });
}
