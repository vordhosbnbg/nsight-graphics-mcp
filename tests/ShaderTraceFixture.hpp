#pragma once

#include "Check.hpp"
#include "ngm/Hash.hpp"
#include "ngm/ShaderTrace.hpp"

#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace ngm::check::shader_trace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
struct Scratch {
    fs::path path{};
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-shader-trace-XXXXXX").string();
        const auto directory = mkdtemp(pattern.data());
        require(directory != nullptr, "allocate shader trace scratch");
        path = directory;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
inline void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << text;
}
inline void copy_executable(const fs::path& source, const fs::path& target) {
    fs::create_directories(target.parent_path());
    fs::copy_file(source, target);
    fs::permissions(target, fs::perms::owner_all);
}
inline fs::path installation(const fs::path& standin, const fs::path& root) {
    const auto result = root / "installation";
    for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay"})
        copy_executable(standin, result / "host/linux-desktop-nomad-x64" / name);
    return result;
}
inline std::string module() {
    std::string result(20, '\0');
    result[0] = '\x03';
    result[1] = '\x02';
    result[2] = '\x23';
    result[3] = '\x07';
    return result; // CPU-only header-shaped bytes, never a qualified GPU shader.
}
inline Json payload(const std::string& shader = module()) {
    ShaderTraceBufferHeader header{.capacity = 4, .attempted_records = 4, .pixel_x = 3, .pixel_y = 2};
    std::array<ShaderTraceBufferRecord, 4> records{
        ShaderTraceBufferRecord{{10, 0, 3, 2}, {0, UINT32_MAX, 0, 4}, {0, 0x80000000, 0x7f800000, 0x7fc12345}},
        ShaderTraceBufferRecord{{10, 1, 3, 2}, {0, 3, 1, 2}, {UINT32_MAX, 17, 0, 0}},
        ShaderTraceBufferRecord{{11, 0, 3, 2}, {1, 4, 2, 1}, {UINT32_MAX, 0, 0, 0}},
        ShaderTraceBufferRecord{{11, 0, 3, 2}, {1, 4, 0, 4}, {0x3f800000, 0, 0, 0}}};
    return make_shader_trace(header, records,
                             {{"path", "shaders/probe.spv"},
                              {"sha256", sha256(std::as_bytes(std::span(shader)))},
                              {"entry_point", "main"},
                              {"stage", "fragment"}},
                             8, 6,
                             {{{"id", 0}, {"name", "float_probe"}, {"type", "float32"}, {"components", 4}},
                              {{"id", 1}, {"name", "uint_probe"}, {"type", "uint32"}, {"components", 2}},
                              {{"id", 2}, {"name", "int_probe"}, {"type", "int32"}, {"components", 1}}});
}
inline int produce(int argc, char** argv) {
    require(argc == 5 && std::string_view(argv[1]) == "--produce" && std::string_view(argv[3]) == "--output",
            "synthetic trace producer arguments");
    const fs::path root = argv[4];
    const std::string mode = argv[2];
    if(mode == "failure")
        return 9;
    fs::create_directories(root);
    if(mode == "missing")
        return 0;
    auto shader = module();
    if(mode == "not-spirv")
        shader = std::string(20, 'x');
    auto value = payload(shader);
    if(mode == "bad-hash")
        value["shader"]["sha256"] = std::string(64, 'a');
    if(mode == "bad-count")
        value["attempted_records"] = 100;
    write(root / "shaders/probe.spv", shader);
    write(root / "shader-trace.json", value.dump());
    if(mode == "slow")
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    return 0;
}
} // namespace ngm::check::shader_trace
