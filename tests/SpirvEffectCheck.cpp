#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/SpirvUniforms.hpp"

#include <vector>

namespace {
using ngm::check::require;
std::vector<std::byte> load(const char* path) {
    const auto text = ngm::read_regular_file(path, 1024U * 1024U);
    const auto data = std::as_bytes(std::span(text));
    return {data.begin(), data.end()};
}
std::uint32_t word(const std::vector<std::byte>& bytes, std::size_t at) {
    std::uint32_t result = 0;
    for(unsigned n = 0; n < 4; ++n)
        result |= std::uint32_t(std::to_integer<unsigned char>(bytes.at(at * 4 + n))) << (n * 8);
    return result;
}
void append(std::vector<std::byte>& bytes, std::uint32_t value) {
    for(unsigned n = 0; n < 4; ++n)
        bytes.push_back(std::byte((value >> (n * 8)) & 255));
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 4, "compiled vertex, fragment and storage-writing fragment required");
        const auto vertex = load(argv[1]), fragment = load(argv[2]), writer = load(argv[3]);
        for(const auto& [module, stage] :
            std::vector<std::pair<std::vector<std::byte>, unsigned>>{{vertex, 0}, {fragment, 4}}) {
            const auto effect = ngm::inspect_spirv_buffer_effects(module, "main", stage);
            require(effect.at("status") == "qualified_no_external_buffer_writes" &&
                        effect.at("module_sha256") == ngm::sha256(module) &&
                        effect.at("local_or_output_stores").get<unsigned>() > 0,
                    "actual shader loads and local/output stores have separate bounded effect proof");
        }
        require(ngm::inspect_spirv_uniform_reads(writer, "main", 4).at("status") == "qualified_static_uniform_loads",
                "static uniform reflection alone permits an independent storage-buffer writer");
        require(ngm::inspect_spirv_buffer_effects(writer, "main", 4).at("status") == "unavailable",
                "source-compiled atomic storage writer cannot supply read-only buffer proof");
        require(ngm::inspect_spirv_buffer_effects(fragment, "absent", 4).at("status") == "unavailable" &&
                    ngm::inspect_spirv_buffer_effects(fragment, "main", 0).at("status") == "unavailable",
                "entry and stage identity are required for an effect proof");

        // Add a valid logical OpCopyMemory from the reflected uniform vec4 to
        // the vec4 output. Even this output-only copy needs explicit semantics;
        // the effect allowlist must refuse rather than skip memory operations.
        std::uint32_t output = 0, uniform_pointer = 0;
        std::size_t end = 0;
        for(std::size_t at = 5; at < fragment.size() / 4; at += word(fragment, at) >> 16) {
            const auto op = word(fragment, at) & 65535;
            if(op == 59 && word(fragment, at + 3) == 3)
                output = word(fragment, at + 2);
            if(op == 65)
                uniform_pointer = word(fragment, at + 2);
            if(op == 253)
                end = at * 4;
        }
        require(output && uniform_pointer && end, "actual compiled mutation anchors exist");
        auto copy = std::vector<std::byte>(fragment.begin(), fragment.begin() + end);
        for(const auto value : {std::uint32_t((3u << 16) | 63), output, uniform_pointer})
            append(copy, value);
        copy.insert(copy.end(), fragment.begin() + end, fragment.end());
        require(ngm::inspect_spirv_uniform_reads(copy, "main", 4).at("status") == "qualified_static_uniform_loads" &&
                    ngm::inspect_spirv_buffer_effects(copy, "main", 4).at("status") == "unavailable",
                "unmodeled memory operations never inherit a successful uniform reflection");
    });
}
