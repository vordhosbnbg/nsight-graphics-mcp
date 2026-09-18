#include "ReadOnlyDatabase.h"
#include "ResourceWorkerBuild.hpp"
#include "ngm/WorkerConfinement.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
std::uint64_t number(std::string_view value) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if(parsed.ec != std::errc() || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("Expected unsigned decimal integer");
    return result;
}
void require(bool value, const char* message) {
    if(!value)
        throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    const char* phase = "arguments";
    try {
        if(argc == 2 && std::string_view(argv[1]) == "--profile") {
            std::cout << ngm::resource_worker_build_json << '\n';
            return 0;
        }
        require(argc == 6, "Expected SNAPSHOT HANDLE OFFSET LENGTH DECLARED_BYTES (0 means unknown)");
        const std::filesystem::path root(argv[1]);
        require(root.is_absolute(), "Snapshot path must be absolute");
        const auto handle = number(argv[2]), offset = number(argv[3]), length = number(argv[4]),
                   declared = number(argv[5]);
        constexpr std::uint64_t maximum_blob = 16U * 1024U * 1024U;
        require(handle <= std::numeric_limits<std::int32_t>::max() && offset <= maximum_blob && length > 0 &&
                    length <= 65536 && declared <= maximum_blob,
                "Request exceeds worker limits");
        const auto database_path = root / "data.bin";
        const auto records_path = root / "data.bin.rec";
        phase = "confinement";
        ngm::confine_resource_worker(database_path, records_path);
        // Database construction and all payload parsing occur after confinement.
        phase = "database";
        Serialization::ReadOnlyDatabase database;
        require(database.Init(database_path.c_str()) == Serialization::ReadOnlyDatabase::InitResult::Ok,
                "Generated database initialization failed");
        const Serialization::DATABASE_HANDLE id(static_cast<std::int32_t>(handle));
        const auto total = database.GetSize(id);
        require(total > 0 && total <= maximum_blob && (declared == 0 || total == declared),
                "Missing, excessive or mismatched resource size");
        require(offset <= total, "Offset exceeds resource size");
        const auto count = std::min(length, total - offset);
        Serialization::DataScope scope(Serialization::DataScopeTracker::Instance());
        auto resource = database.Read<const char*>(id);
        require(resource.Get() != nullptr, "Generated resource read failed");
        std::cout << "{\"schema_version\":1,\"profile\":\"" << ngm::resource_worker_profile
                  << "\",\"handle\":" << handle << ",\"offset\":" << offset << ",\"total_bytes\":" << total
                  << ",\"returned_bytes\":" << count << "}\n";
        std::cout.write(resource.Get() + offset, static_cast<std::streamsize>(count));
        std::cout.flush();
        require(bool(std::cout), "Worker output failed");
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "Resource worker " << phase << ": " << error.what() << '\n';
        return 2;
    }
}
