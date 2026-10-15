#include "DescriptorReadFixture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>

int main(int argc, char** argv) {
    using namespace ngm::check::descriptor_read;
    if(argc == 2 && std::string_view(argv[1]) == "--profile") {
        auto value = profile();
        value["project_version"] = "cpu-only";
        std::cout << value.dump() << '\n';
        return 0;
    }
    if(argc != 5)
        return 2;
    std::string mode;
    std::ifstream(std::string(argv[1]) + "/data.bin") >> mode;
    if(mode == "exit")
        return 7;
    if(mode == "hang") {
        if(fork() == 0)
            for(;;)
                pause();
        for(;;)
            pause();
    }
    if(mode == "oversized") {
        std::cout << std::string(131073, 'x');
        return 0;
    }
    auto value = response(mode, std::stoull(argv[2]), std::stoull(argv[3]), std::stoull(argv[4]));
    auto& write = value["writes"][0];
    if(mode == "wrong-profile")
        value["profile"] = "other";
    if(mode == "wrong-handle")
        value["handle"] = 8;
    if(mode == "wrong-count")
        value["write_count"] = 2;
    if(mode == "wrong-capacity")
        value["output_capacity"] = 512;
    if(mode == "wrong-used")
        value["output_used_bytes"] = 257;
    if(mode == "wrong-size")
        value["input_bytes"] = 1;
    if(mode == "wrong-hash")
        value["input_sha256"] = "not-a-hash";
    if(mode == "wrong-alignment")
        value["source_alignment_remainder"] = 8;
    if(mode == "negative")
        value["write_count"] = -1;
    if(mode == "float")
        value["input_bytes"] = 88.0;
    if(mode == "extra-key")
        value["extra"] = true;
    if(mode == "unknown-id")
        write["buffers"][0]["buffer"]["serialized_id"] = 999;
    if(mode == "wrong-type")
        write["buffers"][0]["buffer"]["object_type"] = 14;
    if(mode == "null-destination")
        write["dst_set"] = nullptr;
    if(mode == "unknown-descriptor")
        write["descriptor_type"] = 5;
    if(mode == "empty-range")
        write["buffers"][0]["range"] = 0;
    if(mode == "overflow-range") {
        write["buffers"][0]["range"] = 2;
        write["buffers"][0]["offset"] = UINT64_MAX;
    }
    if(mode == "overflow-array") {
        write["array_element"] = UINT32_MAX;
        write["descriptor_count"] = 2;
    }
    if(mode == "wrong-payload")
        write["buffers"] = Json::array();
    if(mode == "extra-row-key")
        write["ignored"] = true;
    if(mode == "diagnostics")
        std::cerr << "unexpected worker warning\n";
    if(mode == "mutate") {
        const auto path = std::filesystem::path(argv[1]) / "data.bin";
        std::filesystem::permissions(path, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
        std::ofstream(path) << "changed";
    }
    auto wire = value.dump();
    if(mode == "duplicate")
        wire.insert(1, "\"schema_version\":1,");
    if(mode == "truncate")
        wire.pop_back();
    if(mode == "extra-data")
        wire += "{}";
    std::cout << wire << '\n';
    return 0;
}
