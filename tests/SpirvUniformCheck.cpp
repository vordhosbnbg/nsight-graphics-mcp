#include "Check.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/SpirvUniforms.hpp"

#include <bit>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <set>
#include <vector>

namespace {
using Json = nlohmann::json;
using Words = std::vector<std::uint32_t>;
using ngm::check::require;
Words load(const char* path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "open source-compiled SPIR-V");
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input), {}};
    require(bytes.size() % 4 == 0, "compiled input has full words");
    Words words;
    for(std::size_t at = 0; at < bytes.size(); at += 4)
        words.push_back(bytes[at] | (std::uint32_t(bytes[at + 1]) << 8) | (std::uint32_t(bytes[at + 2]) << 16) |
                        (std::uint32_t(bytes[at + 3]) << 24));
    return words;
}
std::vector<std::byte> bytes(const Words& words) {
    std::vector<std::byte> result;
    for(const auto word : words)
        for(unsigned byte = 0; byte < 4; ++byte)
            result.push_back(std::byte((word >> (byte * 8)) & 255));
    return result;
}
Json reflect(const Words& words, std::string_view entry = "main", std::uint32_t model = 4) {
    return ngm::inspect_spirv_uniform_reads(bytes(words), entry, model);
}
std::size_t
find(const Words& words, unsigned opcode, const std::function<bool(std::size_t)>& select = [](auto) { return true; }) {
    for(std::size_t at = 5; at < words.size();) {
        require(words[at] >> 16, "mutation input has full instructions");
        if((words[at] & 65535) == opcode && select(at))
            return at;
        at += words[at] >> 16;
    }
    throw std::runtime_error("missing SPIR-V mutation anchor");
}
void malformed(const Words& words) {
    bool refused = false;
    try {
        (void)reflect(words);
    } catch(const ngm::InspectionError&) {
        refused = true;
    }
    require(refused, "malformed binary refuses without producing a layout");
}
void unavailable(const Words& words) {
    const auto result = reflect(words);
    require(result.at("status") == "unavailable" && result.at("reads").empty() && result.at("reason").is_string(),
            "unsupported layout/control flow returns a reason and no inferred reads");
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "fixture and independent typed shader paths required");
        const auto original = load(argv[1]), typed = load(argv[2]);
        const auto result = reflect(original);
        require(result.at("status") == "qualified_static_uniform_loads" && result.at("reads").size() == 1 &&
                    result.at("module_sha256") == ngm::sha256(bytes(original)),
                "actual compiled fixture has one statically addressed uniform load and exact module hash");
        const auto read = result.at("reads").at(0);
        require(read.at("descriptor_set") == 0 && read.at("binding") == 0 && read.at("offset") == 0 &&
                    read.at("member_name") == "tint" && read.at("block_name") == "Palette" &&
                    read.at("layout").at("scalar") == "float32" && read.at("layout").at("lanes") == 4,
                "offset/type come from compiled SPIR-V, with debug names supplementary");
        const auto location = read.at("word_offset").get<std::size_t>();
        require((original[location] & 65535) == 61 &&
                    original[location] >> 16 == read.at("word_count").get<std::uint32_t>(),
                "word provenance points to the exact OpLoad");

        const Words float_bits{std::bit_cast<std::uint32_t>(1.25f), std::bit_cast<std::uint32_t>(-2.5f),
                               std::bit_cast<std::uint32_t>(.75f), std::bit_cast<std::uint32_t>(1.f)};
        auto decoded = ngm::decode_spirv_uniform_read(read, bytes(float_bits));
        require(decoded.at("components").at(0).at("value") == 1.25 &&
                    decoded.at("components").at(1).at("value") == -2.5,
                "float bytes decode independently supplied values");
        const Words exceptional{0x7fc00001u, 0xff800000u, 0x80000000u, 1u};
        decoded = ngm::decode_spirv_uniform_read(read, bytes(exceptional));
        const auto& components = decoded.at("components");
        require(components[0].at("classification") == "nan" && components[0].at("value").is_null() &&
                    components[0].at("bits") == 0x7fc00001u && components[1].at("classification") == "infinity" &&
                    components[1].at("value").is_null() && components[2].at("bits") == 0x80000000u &&
                    components[3].at("value").get<double>() > 0,
                "nonfinite, signed-zero and subnormal bits survive without non-JSON numbers");

        const auto all = reflect(typed);
        require(all.at("status") == "qualified_static_uniform_loads" && all.at("reads").size() == 3,
                "independently compiled multi-member float/signed/unsigned layout");
        Words payload = float_bits;
        payload.insert(payload.end(),
                       {std::bit_cast<std::uint32_t>(std::int32_t(-17)), 0x80000000u, 0xffffffffu, 0x80000000u});
        std::set<std::string> scalars;
        for(const auto& row : all.at("reads")) {
            require(row.at("descriptor_set") == 2 && row.at("binding") == 5, "nonzero binary set/binding survive");
            const auto values = ngm::decode_spirv_uniform_read(row, bytes(payload));
            const auto scalar = values.at("scalar").get<std::string>();
            scalars.insert(scalar);
            if(scalar == "int32")
                require(row.at("offset") == 16 && values.at("components")[0].at("value") == -17 &&
                            values.at("components")[1].at("value") == std::numeric_limits<std::int32_t>::min(),
                        "signed components preserve full int32 range");
            if(scalar == "uint32")
                require(row.at("offset") == 24 && values.at("components")[0].at("value") == 0xffffffffu &&
                            values.at("components")[1].at("value") == 0x80000000u,
                        "unsigned components preserve full uint32 range");
        }
        require(scalars == std::set<std::string>({"float32", "int32", "uint32"}),
                "all independently supplied scalar kinds checked");
        auto stripped = original;
        for(std::size_t at = 5; at < stripped.size();) {
            const auto opcode = stripped[at] & 65535, count = stripped[at] >> 16;
            if(opcode == 5 || opcode == 6)
                stripped.erase(stripped.begin() + at, stripped.begin() + at + count);
            else
                at += count;
        }
        const auto without_names = reflect(stripped);
        require(without_names.at("reads").size() == 1 && without_names.at("reads")[0].at("member_name").is_null() &&
                    without_names.at("reads")[0].at("layout") == read.at("layout"),
                "debug names are optional and never supply type/layout");
        require(reflect(original, "absent").at("status") == "unavailable" &&
                    reflect(original, "main", 0).at("status") == "unavailable",
                "missing or wrong-stage entry selection refuses");

        auto mutate = [&](const std::function<void(Words&)>& change, bool corrupt = false) {
            auto altered = original;
            change(altered);
            if(corrupt)
                malformed(altered);
            else
                unavailable(altered);
        };
        mutate([](auto& w) { w[0] = 0x03022307; }, true);
        mutate([](auto& w) { w[1] = 0x00010500; });
        mutate([](auto& w) { w[3] = 0; }, true);
        mutate([](auto& w) { w[3] = 65537; }, true);
        mutate([](auto& w) { w[4] = 1; }, true);
        mutate([](auto& w) { w[5] &= 65535; }, true);
        mutate([](auto& w) { w[5] = (65535u << 16) | (w[5] & 65535); }, true);
        mutate([](auto& w) { w[5] = (w[5] & 0xffff0000) | 65535; });
        mutate([](auto& w) { w.pop_back(); }, true);
        mutate([](auto& w) { w[find(w, 22) + 1] = w[find(w, 19) + 1]; }, true);
        mutate([](auto& w) { w[find(w, 22) + 2] = 64; });
        mutate([](auto& w) { w[find(w, 253)] = (1u << 16) | 252; });
        mutate([](auto& w) { w[find(w, 65) + 4] = w[3]; }, true);
        mutate([](auto& w) { w[find(w, 65) + 4] = w[find(w, 80) + 2]; });
        mutate([&](auto& w) {
            const auto at =
                find(w, 61, [&](auto at) { return w[at + 2] == read.at("load_result_id").get<unsigned>(); });
            w[at + 3] = read.at("variable_id").get<unsigned>();
        });
        mutate([](auto& w) {
            const auto at = find(w, 71, [&](auto n) { return w[n + 2] == 33; });
            w.erase(w.begin() + at, w.begin() + at + (w[at] >> 16));
        });
        mutate(
            [](auto& w) {
                const auto at = find(w, 71);
                const Words duplicate(w.begin() + at, w.begin() + at + (w[at] >> 16));
                w.insert(w.begin() + at, duplicate.begin(), duplicate.end());
            },
            true);
        mutate(
            [](auto& w) {
                const auto at = find(w, 15);
                w[at + 4] = 0x100;
            },
            true);
        auto overlap = typed;
        overlap[find(overlap, 72, [&](auto at) { return overlap[at + 2] == 1; }) + 4] = 0;
        unavailable(overlap);
        bool empty_refused = false;
        try {
            (void)ngm::inspect_spirv_uniform_reads({}, "main", 4);
        } catch(const ngm::InspectionError&) {
            empty_refused = true;
        }
        require(empty_refused, "empty binary refuses");
        for(unsigned selection = 0; selection < 3; ++selection) {
            auto altered = read;
            switch(selection) {
                case 0:
                    altered["offset"] = UINT64_MAX;
                    break;
                case 1:
                    altered["layout"]["lanes"] = 5u;
                    break;
                default:
                    altered["layout"]["scalar"] = "float64";
                    break;
            }
            bool refused = false;
            try {
                (void)ngm::decode_spirv_uniform_read(altered, bytes(float_bits));
            } catch(const ngm::InspectionError&) {
                refused = true;
            }
            require(refused, "invalid decoded range/type/extent refuses");
        }
        bool short_refused = false;
        try {
            auto short_bytes = bytes(float_bits);
            short_bytes.pop_back();
            (void)ngm::decode_spirv_uniform_read(read, short_bytes);
        } catch(const ngm::InspectionError&) {
            short_refused = true;
        }
        require(short_refused, "short uniform payload refuses");
    });
}
