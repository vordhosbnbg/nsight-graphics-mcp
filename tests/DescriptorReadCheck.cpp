#include "Check.hpp"
#include "DescriptorReadFixture.hpp"
#include "ngm/DescriptorRead.hpp"
#include "ngm/File.hpp"
#include "ngm/Inspection.hpp"
#include <fstream>
#include <unistd.h>

namespace {
using namespace ngm::check::descriptor_read;
using ngm::check::require;
namespace fs = std::filesystem;
void write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << bytes;
    file.close();
    require(file.good(), "write test-only descriptor input");
}
struct Scratch {
    fs::path path;
    Scratch() {
        std::string pattern = "/tmp/ngm-descriptor-read-XXXXXX";
        require(mkdtemp(pattern.data()) != nullptr, "test scratch directory");
        path = pattern;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
ngm::DescriptorReadRequest request(ngm::ArtifactStore& store, const std::string& mode, const fs::path& executable) {
    auto writer = store.begin({{"test_only", true}}, {"raw/project/data.bin"}, true);
    const auto qualified = profile();
    Json paths = Json::object();
    for(const auto& [name, hash] : qualified.at("original_files").items()) {
        (void)hash;
        write(writer.raw_directory() / "project" / name, helper(name));
        paths[name] = "raw/project/" + name;
    }
    write(writer.raw_directory() / "project/data.bin", mode);
    write(writer.raw_directory() / "project/data.bin.rec", "records");
    store.publish_success(writer);
    ngm::DescriptorReadRequest result;
    result.capture_id = writer.id();
    result.project_directory = "raw/project";
    result.reference = {{"handle", 7},
                        {"write_count", 1},
                        {"output_bytes", 256},
                        {"copy_count", 0},
                        {"conditional", false},
                        {"readable", true},
                        {"update_ref", std::string(64, 'f')}};
    result.registrations = registrations();
    result.helper_profile = qualified;
    result.helper_paths = paths;
    result.expected_inputs = {{"data.bin", {{"bytes", mode.size()}, {"sha256", digest(mode)}}},
                              {"data.bin.rec", {{"bytes", 7}, {"sha256", digest("records")}}}};
    result.worker = executable;
    result.pin = true;
    return result;
}
std::string rejected(ngm::ArtifactStore& store, const ngm::DescriptorReadRequest& req) {
    std::string id;
    try {
        (void)ngm::read_cpp_descriptor_writes(store, req);
    } catch(const ngm::InspectionError& error) {
        const std::string message = error.what();
        const auto at = message.find("; evidence artifact ");
        require(at != std::string::npos, "failed attempt has an evidence artifact");
        id = message.substr(at + 20);
    }
    require(!id.empty(), "invalid descriptor worker result was accepted");
    const auto info = store.inspect(id);
    require(info.summary.status == "failed" && info.summary.pinned && !info.summary.quarantined,
            "failed descriptor attempt is published and pinned after cleanup");
    require(Json::parse(store.read(id, "raw/descriptor-read.json")).at("status") == "failed",
            "failure report retained");
    return id;
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "worker standin executable required");
        Scratch scratch;
        const auto executable = fs::absolute(argv[1]);
        std::string retained;
        {
            ngm::ArtifactStore store({.root = scratch.path / "store", .max_bytes = 0});
            for(const auto* mode : {"ok", "array", "images", "whole", "null"}) {
                const auto result = ngm::read_cpp_descriptor_writes(store, request(store, mode, executable));
                const auto& row = result.at("writes").at(0);
                require(row.at("dst_set").at("symbol") == "VkDescriptorSet_uid_29" &&
                            row.at("dst_set").at("serialized_id") == 100,
                        "source symbol is mapped by typed registration");
                if(std::string_view(mode) == "images")
                    require(row.at("images").size() == 2 &&
                                row.at("images").at(0).at("image_view").at("symbol") == "VkImageView_uid_71",
                            "image arrays resolve source object evidence");
                else if(std::string_view(mode) == "null")
                    require(row.at("buffers").at(0).at("buffer").is_null(), "null resource stays null");
                else
                    require(row.at("buffers").at(0).at("buffer").at("symbol") == "VkBuffer_uid_33",
                            "buffer source identity resolved independently of numeric UID suffix");
                retained = result.at("artifact_id");
                require(store.inspect(retained).summary.pinned &&
                            Json::parse(store.read(retained, "raw/descriptor-read.json")).at("status") == "succeeded",
                        "successful attempt retains its result and pin");
            }
            for(const auto* mode : {"wrong-profile",
                                    "wrong-handle",
                                    "wrong-count",
                                    "wrong-capacity",
                                    "wrong-used",
                                    "wrong-size",
                                    "wrong-hash",
                                    "wrong-alignment",
                                    "negative",
                                    "float",
                                    "extra-key",
                                    "unknown-id",
                                    "wrong-type",
                                    "null-destination",
                                    "unknown-descriptor",
                                    "empty-range",
                                    "overflow-range",
                                    "overflow-array",
                                    "wrong-payload",
                                    "extra-row-key",
                                    "diagnostics",
                                    "duplicate",
                                    "truncate",
                                    "extra-data",
                                    "oversized",
                                    "exit",
                                    "mutate",
                                    "hang"}) {
                auto req = request(store, mode, executable);
                if(std::string_view(mode) == "hang")
                    req.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                const auto id = rejected(store, req);
                const auto report = Json::parse(store.read(id, "raw/descriptor-read.json"));
                require(report.at("read_process").at("cleanup_confirmed") == true,
                        "malformed response, process failure and deadline confirm owned cleanup");
                require(store.read(id, "raw/input/data.bin") == (std::string_view(mode) == "mutate" ? "changed" : mode),
                        "failed attempt preserves exact available inputs");
            }
            auto bad_profile = request(store, "ok", executable);
            bad_profile.helper_profile["patch_sha256"] = std::string(64, '0');
            const auto id = rejected(store, bad_profile);
            require(!Json::parse(store.read(id, "raw/descriptor-read.json")).contains("read_process"),
                    "profile mismatch refuses before payload processing");
            auto bad_source = request(store, "ok", executable);
            bad_source.expected_inputs["data.bin"]["sha256"] = std::string(64, '0');
            rejected(store, bad_source);
            auto bad_registration = request(store, "ok", executable);
            bad_registration.registrations.push_back(bad_registration.registrations[0]);
            rejected(store, bad_registration);
            auto bad_helper = request(store, "ok", executable);
            bad_helper.helper_profile["original_files"]["VulkanStructHydrator.h"] = std::string(64, '0');
            rejected(store, bad_helper);
        }
        std::string quota_source;
        std::uint64_t baseline = 0;
        ngm::DescriptorReadRequest quota_request;
        {
            ngm::ArtifactStore setup({.root = scratch.path / "quota", .max_bytes = 0});
            quota_request = request(setup, "ok", executable);
            quota_source = quota_request.capture_id;
            baseline = setup.usage().total_bytes;
        }
        {
            ngm::ArtifactStore limited({.root = scratch.path / "quota", .max_bytes = baseline + 2048});
            const auto failed = rejected(limited, quota_request);
            const auto report = Json::parse(limited.read(failed, "raw/descriptor-read.json"));
            require(report.at("read_process").at("exit_code") == 0 && report.at("status") == "failed" &&
                        limited.read(failed, "raw/input/data.bin") == "ok" &&
                        limited.inspect(quota_source).summary.pinned && limited.usage().quota_exceeded,
                    "quota refusal after extraction preserves failed evidence, exact inputs and source pin");
        }
        ngm::ArtifactStore restarted({.root = scratch.path / "store", .max_bytes = 0});
        require(
            restarted.inspect(retained).summary.pinned &&
                Json::parse(restarted.read(retained, "raw/descriptor-read.json")).at("result").at("writes").size() == 1,
            "decoded source write and persistent pin survive restart");
    });
}
