#include "ResourceReadFixture.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <unistd.h>

int main(int argc, char** argv) {
    using namespace ngm::check::resource;
    if(argc == 2 && std::string_view(argv[1]) == "--profile") {
        auto value = profile();
        value["schema_version"] = 1;
        value["project_version"] = "cpu-test-only";
        std::cout << value.dump() << '\n';
        return 0;
    }
    if(argc != 6)
        return 2;
    std::ifstream input(std::string(argv[1]) + "/data.bin");
    std::string mode;
    input >> mode;
    if(mode == "hang") {
        if(fork() == 0) {
            for(;;)
                pause();
        }
        for(;;)
            pause();
    }
    if(mode == "exit") {
        std::cerr << "deliberate failure";
        return 7;
    }
    const auto offset = std::stoull(argv[3]), length = std::stoull(argv[4]);
    const auto data = payload();
    if(offset > data.size())
        return 2;
    auto bytes = data.substr(offset, length);
    nlohmann::json header{{"schema_version", 1}, {"profile", "cpu-test-only"}, {"handle", std::stoull(argv[2])},
                          {"offset", offset},    {"total_bytes", data.size()}, {"returned_bytes", bytes.size()}};
    if(mode == "wrong-profile")
        header["profile"] = "other";
    if(mode == "wrong-handle")
        header["handle"] = 8;
    if(mode == "wrong-offset")
        header["offset"] = offset + 1;
    if(mode == "wrong-total")
        header["total_bytes"] = 9;
    if(mode == "negative")
        header["returned_bytes"] = -1;
    if(mode == "float")
        header["total_bytes"] = 8.0;
    if(mode == "extra-key")
        header["extra"] = true;
    if(mode == "diagnostics")
        std::cerr << "unexpected warning";
    if(mode == "oversized") {
        std::cout << std::string(131073, 'x');
        return 0;
    }
    if(mode == "missing-header") {
        std::cout << bytes;
        return 0;
    }
    auto wire = header.dump();
    if(mode == "duplicate")
        wire.insert(1, "\"schema_version\":1,");
    if(mode == "truncate")
        bytes.pop_back();
    if(mode == "extra-data")
        bytes += 'x';
    if(mode == "mutate") {
        const auto file = std::string(argv[1]) + "/data.bin";
        std::filesystem::permissions(file, std::filesystem::perms::owner_write, std::filesystem::perm_options::add);
        std::ofstream(file) << "changed";
    }
    std::cout << wire << '\n' << bytes;
    return 0;
}
