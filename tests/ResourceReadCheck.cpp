#include "Check.hpp"
#include "ResourceReadFixture.hpp"
#include "ngm/File.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ResourceRead.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {
using ngm::check::require;
using namespace ngm::check::resource;
namespace fs = std::filesystem;
using Json = nlohmann::json;
struct Scratch {
    fs::path path;
    Scratch() {
        std::string pattern = "/tmp/ngm-resource-read-XXXXXX";
        require(mkdtemp(pattern.data()) != nullptr, "scratch directory");
        path = pattern;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
void write(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << data;
    file.close();
    require(file.good(), "write synthetic evidence");
}
std::string capture(ngm::ArtifactStore& store, const std::string& mode) {
    auto writer = store.begin({{"test_only", true}}, {"raw/project/data.bin"}, true);
    write(writer.raw_directory() / "project/data.bin", mode);
    write(writer.raw_directory() / "project/data.bin.rec", "records");
    const auto qualified = profile();
    for(const auto& [name, hash] : qualified.at("files").items()) {
        (void)hash;
        write(writer.raw_directory() / "project" / name, "test-only helper");
    }
    store.publish_success(writer);
    return writer.id();
}
ngm::ResourceReadRequest request(const std::string& id, const fs::path& executable) {
    return {id,
            "raw/project",
            {{"handle", 7}, {"readable", true}, {"expected_bytes", 8}, {"resource_ref", std::string(64, 'a')}},
            profile(),
            executable,
            0,
            8,
            true};
}
template <class F>
void rejected(F&& fn) {
    bool failed = false;
    try {
        fn();
    } catch(const std::exception&) {
        failed = true;
    }
    require(failed, "invalid operation succeeded");
}
void snapshots(ngm::ArtifactStore& store, const fs::path& scratch) {
    std::string id;
    {
        auto source = store.begin(Json::object());
        id = source.id();
        write(source.raw_directory() / "large", std::string(17U * 1024U * 1024U, 'x'));
        write(source.raw_directory() / "empty", "");
        store.publish_success(source);
    }
    auto destination = store.begin(Json::object());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    const auto copied =
        store.snapshot_file(id, "raw/large", destination, "raw/input/data", 18U * 1024U * 1024U, deadline);
    require(copied.bytes == 17U * 1024U * 1024U, "streaming copy length");
    require(ngm::sha256_regular_file(destination.directory() / copied.path, deadline) ==
                digest(std::string(17U * 1024U * 1024U, 'x')),
            "streaming copy content");
    struct stat attrs{};
    require(stat((destination.directory() / copied.path).c_str(), &attrs) == 0 && (attrs.st_mode & 0777) == 0400,
            "snapshot not readonly");
    rejected(
        [&] { store.snapshot_file(id, "raw/large", destination, "raw/input/data", 18U * 1024U * 1024U, deadline); });
    rejected([&] { store.snapshot_file(id, "raw/large", destination, "raw/small", 100, deadline); });
    rejected(
        [&] { store.snapshot_file(id, "raw/empty", destination, "raw/late", 100, std::chrono::steady_clock::now()); });
    rejected([&] { store.snapshot_file(id, "../raw/large", destination, "raw/escape", 100, deadline); });
    fs::create_directory_symlink(scratch, destination.raw_directory() / "link");
    rejected([&] { store.snapshot_file(id, "raw/empty", destination, "raw/link/escape", 100, deadline); });
    fs::remove(destination.raw_directory() / "link");
    ngm::ArtifactStore other({.root = scratch / "other", .max_bytes = 0});
    auto foreign = other.begin(Json::object());
    rejected([&] { store.snapshot_file(id, "raw/empty", foreign, "raw/foreign", 100, deadline); });
    other.publish_failure(foreign, "test finished");
    store.publish_success(destination);
    rejected([&] { store.snapshot_file(id, "raw/empty", destination, "raw/published", 100, deadline); });
    auto tampered = store.begin(Json::object());
    auto lease = store.lease(id);
    fs::remove(lease.directory() / "raw/empty");
    fs::create_symlink(scratch / "outside", lease.directory() / "raw/empty");
    rejected([&] { store.snapshot_file(id, "raw/empty", tampered, "raw/symlink", 100, deadline); });
    fs::remove(lease.directory() / "raw/empty");
    write(lease.directory() / "raw/empty", "");
    fs::create_hard_link(lease.directory() / "raw/empty", scratch / "hardlink");
    rejected([&] { store.snapshot_file(id, "raw/empty", tampered, "raw/hardlink", 100, deadline); });
    fs::remove(scratch / "hardlink");
    store.publish_failure(tampered, "test finished");
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "standin executable required");
        Scratch scratch;
        std::string result_id;
        {
            ngm::ArtifactStore store({.root = scratch.path / "store", .max_bytes = 0});
            const auto id = capture(store, "ok");
            auto req = request(id, fs::absolute(argv[1]));
            const auto result = ngm::read_cpp_resource(store, req);
            require(result.at("data") == "00ff100d0a414243" && result.at("sha256") == digest(payload()),
                    "binary response");
            result_id = result.at("artifact_id").get<std::string>();
            require(store.inspect(result_id).summary.pinned, "result pin missing");
            require(fs::exists(store.lease(result_id).directory() / "raw/input/data.bin"),
                    "success lost input snapshot");
            require(Json::parse(store.read(result_id, "raw/resource-read.json")).at("status") == "succeeded",
                    "success report");
            req.offset = 3;
            req.length = 2;
            const auto page = ngm::read_cpp_resource(store, req);
            require(page.at("data") == "0d0a" && page.at("next_offset") == 5, "range response");
            req.offset = 8;
            const auto end = ngm::read_cpp_resource(store, req);
            require(end.at("data") == "" && end.at("next_offset").is_null(), "end range");
            req.offset = 0;
            req.reference["expected_bytes"] = nullptr;
            require(ngm::read_cpp_resource(store, req).at("total_bytes") == 8, "unknown declared size");
            for(const auto* mode : {"wrong-profile", "wrong-handle", "wrong-offset", "wrong-total", "negative", "float",
                                    "extra-key", "diagnostics", "oversized", "missing-header", "duplicate", "truncate",
                                    "extra-data", "mutate", "exit", "hang"}) {
                auto bad = request(capture(store, mode), fs::absolute(argv[1]));
                std::string failure_id;
                try {
                    (void)ngm::read_cpp_resource(store, bad);
                } catch(const ngm::InspectionError& error) {
                    const std::string message = error.what();
                    const auto marker = message.find("; evidence artifact ");
                    require(marker != std::string::npos, "failure has no retained evidence reference");
                    failure_id = message.substr(marker + 20);
                }
                require(!failure_id.empty(), std::string("accepted bad worker response: ") + mode);
                const auto info = store.inspect(failure_id);
                require(info.summary.status == "failed" && !info.summary.quarantined, "failure publication/cleanup");
                const auto report = Json::parse(store.read(failure_id, "raw/resource-read.json"));
                require(report.at("status") == "failed" && report.at("read_process").at("cleanup_confirmed") == true,
                        "failure report cleanup");
                require(store.read(failure_id, "raw/input/data.bin") ==
                            (std::string(mode) == "mutate" ? "changed" : mode),
                        "failed inputs not retained");
            }
            auto bad_helper = request(id, fs::absolute(argv[1]));
            bad_helper.helper_profile["files"]["DllCommon.h"] = std::string(64, '0');
            rejected([&] { (void)ngm::read_cpp_resource(store, bad_helper); });
            snapshots(store, scratch.path);
        }
        std::string quota_source;
        std::uint64_t baseline_bytes = 0;
        {
            ngm::ArtifactStore setup({.root = scratch.path / "quota", .max_bytes = 0});
            quota_source = capture(setup, "ok");
            baseline_bytes = setup.usage().total_bytes;
        }
        {
            ngm::ArtifactStore limited({.root = scratch.path / "quota", .max_bytes = baseline_bytes + 2048});
            std::string failure;
            try {
                (void)ngm::read_cpp_resource(limited, request(quota_source, fs::absolute(argv[1])));
            } catch(const ngm::InspectionError& error) {
                const std::string message = error.what();
                const auto at = message.find("; evidence artifact ");
                require(at != std::string::npos, "quota failure lost evidence identity");
                failure = message.substr(at + 20);
            }
            require(!failure.empty() && limited.inspect(failure).summary.status == "failed",
                    "quota publication accepted");
            const auto report = Json::parse(limited.read(failure, "raw/resource-read.json"));
            require(report.at("read_process").at("exit_code") == 0 && report.at("status") == "failed",
                    "quota failure not after successful extraction");
            require(limited.read(failure, "raw/input/data.bin") == "ok", "quota failure lost exact snapshot");
            require(limited.inspect(quota_source).summary.pinned && limited.usage().quota_exceeded,
                    "quota handling lost protected source evidence");
        }
        ngm::ArtifactStore restarted({.root = scratch.path / "store", .max_bytes = 0});
        require(restarted.inspect(result_id).summary.pinned &&
                    Json::parse(restarted.read(result_id, "raw/resource-read.json")).at("result").at("data") ==
                        "00ff100d0a414243",
                "result lost on restart");
    });
}
