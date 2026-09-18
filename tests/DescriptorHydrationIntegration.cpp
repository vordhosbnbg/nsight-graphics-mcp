#include "Check.hpp"
#include "ngm/Artifacts.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Process.hpp"
#include "ngm/Version.hpp"

#include <fstream>
#include <iostream>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
void save(const fs::path& path, const Json& value) {
    std::ofstream output(path);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
}
Json result_json(const ngm::ProcessResult& result) {
    return {{"exit_code", result.exit_code ? Json(*result.exit_code) : Json(nullptr)},
            {"signal", result.signal ? Json(*result.signal) : Json(nullptr)},
            {"timed_out", result.timed_out},
            {"cleanup_confirmed", result.cleanup_confirmed},
            {"error", result.error}};
}
Json expected_setup_writes() {
    // Harness-only expectation from the fixture's two palette descriptors and
    // scene image/sampler, associated separately within each fixed capture.
    Json palette = Json::array();
    for(unsigned slot = 0; slot < 2; ++slot)
        palette.push_back({{"dst_set", "VkDescriptorSet_uid_39"},
                           {"binding", 0},
                           {"array_element", slot},
                           {"descriptor_count", 1},
                           {"descriptor_type", "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER"},
                           {"buffers", Json::array({{{"buffer", slot == 0 ? "VkBuffer_uid_32" : "VkBuffer_uid_34"},
                                                     {"offset", 0},
                                                     {"range", 16}}})},
                           {"images", Json::array()}});
    Json image{{"image_view", "VkImageView_uid_27"}, {"sampler", "VkSampler_uid_29"}, {"image_layout", 5}};
    Json post{{"dst_set", "VkDescriptorSet_uid_40"},
              {"binding", 0},
              {"array_element", 0},
              {"descriptor_count", 1},
              {"descriptor_type", "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER"},
              {"buffers", Json::array()},
              {"images", Json::array({image})}};
    return Json::array({palette, Json::array({post})});
}
} // namespace

int main(int argc, char** argv) {
    if(argc != 3) {
        std::cerr << "ARTIFACT_ROOT RUN_ROOT required (four fixed trusted captures; no GPU execution)\n";
        return 2;
    }
    try {
        const fs::path project = NGM_SOURCE_ROOT;
        const auto cases_text =
            ngm::read_regular_file(project / "tests/fixtures/descriptor-hydration/cases.json", 65536);
        require(ngm::sha256(std::as_bytes(std::span(cases_text))) == NGM_DESCRIPTOR_CASES_SHA256,
                "Rebuild harness after changing its fixed cases");
        const auto cases = Json::parse(cases_text);
        const auto root = fs::absolute(argv[2]);
        fs::create_directories(root);
        auto pattern = (root / "descriptor-hydration-XXXXXX").string();
        require(mkdtemp(pattern.data()), "Allocate isolated qualification directory");
        const fs::path run(pattern);
        std::cout << run << '\n' << std::flush;
        ngm::ArtifactOptions options;
        options.root = fs::canonical(argv[1]);
        ngm::ArtifactStore store(options);
        Json report{{"status", "running"},
                    {"project_version", ngm::project_version()},
                    {"scope", "Four fixed trusted retained captures; generated helper contract qualification only"},
                    {"compiler", NGM_CXX_COMPILER},
                    {"compiler_sha256", ngm::sha256_file(NGM_CXX_COMPILER)},
                    {"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                    {"cases", Json::array()}};
        for(const auto* name : {"DescriptorHydrationIntegration.cpp", "DescriptorHydrationWorker.cpp", "Check.hpp"})
            fs::copy_file(project / "tests" / name, run / name);
        save(run / "cases.json", cases);
        try {
            Json dependencies = Json::object();
            for(const auto* directory : {"include/ngm", "external/json/include", "external/vulkan-headers/include"}) {
                for(const auto& entry : fs::recursive_directory_iterator(project / directory)) {
                    if(entry.is_regular_file())
                        dependencies[entry.path().lexically_relative(project).generic_string()] = {
                            {"sha256", ngm::sha256_file(entry.path())}, {"bytes", entry.file_size()}};
                }
            }
            dependencies["ngm_core_archive"] = {{"path", NGM_CORE_ARCHIVE},
                                                {"sha256", ngm::sha256_file(NGM_CORE_ARCHIVE)}};
            save(run / "build-input-identities.json", dependencies);
            ngm::ProcessOptions version;
            version.executable = NGM_CXX_COMPILER;
            version.arguments = {"--version"};
            version.stdout_path = run / "compiler-version.stdout.txt";
            version.stderr_path = run / "compiler-version.stderr.txt";
            const auto observed = ngm::run_process(version);
            save(run / "compiler-version.result.json", result_json(observed));
            require(observed.exit_code == 0 && observed.cleanup_confirmed && observed.error.empty(),
                    "Compiler version observation");
            report["expected_setup_writes"] = expected_setup_writes();
            report["build_inputs_sha256"] = ngm::sha256_file(run / "build-input-identities.json");
            for(const auto& item : cases) {
                const auto id = item.at("capture_id").get<std::string>();
                auto lease = store.lease(id);
                const auto info = store.inspect(id);
                require(info.summary.status == "complete" && info.summary.pinned && !info.summary.quarantined,
                        "Complete pinned source capture");
                const auto capture = Json::parse(store.read(id, "raw/report.json", 2 * 1024 * 1024));
                require(capture.at("generated_cpp_project") == true && capture.at("cleanup_confirmed") == true &&
                            capture.at("nsight").at("cli").at("version") == item.at("producer") &&
                            capture.at("nsight").at("cli").at("build") == item.at("build"),
                        "Exact retained producer profile");
                const auto index = Json::parse(store.read(id, "derived/cpp-project.json"));
                require(index.at("project_directory") == item.at("project"), "Exact indexed generated project");
                const auto destination = run / id;
                const auto snapshot = destination / "input";
                fs::create_directories(snapshot / "include");
                save(snapshot / "case.json", item);
                for(const auto& [name, identity] : item.at("files").items()) {
                    const auto source = lease.directory() / item.at("project").get<std::string>() / name;
                    require(fs::symlink_status(source).type() == fs::file_type::regular &&
                                fs::file_size(source) == identity.at("bytes") &&
                                ngm::sha256_file(source) == identity.at("sha256").get<std::string>(),
                            "Fixed input fingerprint before compilation: " + name);
                    fs::copy_file(source, snapshot / name);
                    require(ngm::sha256_file(snapshot / name) == identity.at("sha256").get<std::string>(),
                            "Copied input identity");
                }
                const auto binary = destination / "worker";
                ngm::ProcessOptions compile;
                compile.executable = NGM_CXX_COMPILER;
                compile.arguments = {"-std=c++20",
                                     "-g",
                                     "-fsanitize=address,undefined",
                                     "-fno-omit-frame-pointer",
                                     "-I" + (project / "include").string(),
                                     "-isystem",
                                     (project / "external/json/include").string(),
                                     "-isystem",
                                     (project / "external/vulkan-headers/include").string(),
                                     "-isystem",
                                     snapshot.string(),
                                     "-isystem",
                                     (snapshot / "include").string(),
                                     "-DNGM_DESCRIPTOR_CASE_SHA256=\"" + ngm::sha256_file(snapshot / "case.json") +
                                         "\"",
                                     (run / "DescriptorHydrationWorker.cpp").string(),
                                     (snapshot / "ReadOnlyDatabase.cpp").string(),
                                     (snapshot / "DataScope.cpp").string(),
                                     (snapshot / "VulkanStructHydrator.cpp").string(),
                                     NGM_CORE_ARCHIVE,
                                     "-pthread",
                                     "-o",
                                     binary.string()};
                compile.working_directory = destination;
                compile.stdout_path = destination / "build.stdout.txt";
                compile.stderr_path = destination / "build.stderr.txt";
                compile.environment = {{"PATH", "/usr/bin:/bin"}, {"TMPDIR", destination.string()}};
                compile.timeout = std::chrono::seconds(120);
                save(destination / "build.argv.json", compile.arguments);
                const auto built = ngm::run_process(compile);
                save(destination / "build.result.json", result_json(built));
                require(built.exit_code == 0 && built.cleanup_confirmed && built.error.empty(),
                        "Compile fixed helper worker");
                ngm::ProcessOptions execute;
                execute.executable = binary;
                execute.arguments = {snapshot.string()};
                execute.working_directory = destination;
                execute.stdout_path = destination / "worker.stdout.json";
                execute.stderr_path = destination / "worker.stderr.txt";
                execute.environment = {{"ASAN_OPTIONS", "detect_leaks=1"}, {"UBSAN_OPTIONS", "halt_on_error=1"}};
                execute.timeout = std::chrono::seconds(30);
                const auto result = ngm::run_process(execute);
                save(destination / "worker.result.json", result_json(result));
                require(result.exit_code == 0 && result.cleanup_confirmed && result.error.empty() &&
                            fs::file_size(execute.stderr_path) == 0,
                        "Fixed descriptor hydration passes without sanitizer diagnostics");
                const auto evidence = Json::parse(ngm::read_regular_file(execute.stdout_path, 1024 * 1024));
                require(evidence.at("status") == "pass" && evidence.at("capture_id").get<std::string>() == id &&
                            evidence.at("updates").size() == 2,
                        "Both observed descriptor updates");
                Json writes = Json::array();
                for(std::size_t n = 0; n < evidence.at("updates").size(); ++n) {
                    const auto& update = evidence.at("updates").at(n);
                    require(update.at("source") == item.at("updates").at(n), "Exact update callsite");
                    require(update.at("input_bytes") == item.at("updates").at(n).at("output_bytes") &&
                                update.at("output_used_bytes") == item.at("updates").at(n).at("output_bytes"),
                            "Observed input and output lengths match this fixed case");
                    writes.push_back(update.at("writes"));
                }
                require(writes == expected_setup_writes(),
                        "Exact setup fields match independently known fixture descriptors");
                // Negative case exercises identity rejection before the unchecked
                // vendor reader. Never feed corrupted bytes to that reader.
                const auto bad = destination / "altered-input";
                fs::copy(snapshot, bad, fs::copy_options::recursive);
                {
                    std::ofstream(bad / "data.bin", std::ios::binary | std::ios::app).put('\0');
                }
                execute.arguments = {bad.string()};
                execute.stdout_path = destination / "altered.stdout.txt";
                execute.stderr_path = destination / "altered.stderr.json";
                const auto rejected = ngm::run_process(execute);
                save(destination / "altered.result.json", result_json(rejected));
                const auto refusal = Json::parse(ngm::read_regular_file(execute.stderr_path, 65536));
                require(rejected.exit_code == 2 && rejected.cleanup_confirmed &&
                            refusal.at("phase") == "input_identity" &&
                            refusal.at("error").get<std::string>().find("data.bin") != std::string::npos,
                        "Changed database rejected before helper initialization");
                fs::remove_all(bad);
                report["cases"].push_back({{"capture_id", id},
                                           {"scenario", item.at("scenario")},
                                           {"worker_sha256", ngm::sha256_file(binary)},
                                           {"evidence", evidence},
                                           {"altered_input_rejected", true}});
                save(run / "report.json", report);
            }
            require(report.at("cases").size() == 4, "Complete two-release reference/fault matrix");
            for(std::size_t n = 0; n < 2; ++n) {
                for(std::size_t pair = 0; pair < 2; ++pair) {
                    const auto& reference = report.at("cases").at(pair * 2).at("evidence").at("updates").at(n);
                    const auto& fault = report.at("cases").at(pair * 2 + 1).at("evidence").at("updates").at(n);
                    require(reference.at("input_sha256") != fault.at("input_sha256") &&
                                reference.at("writes") == fault.at("writes"),
                            "Different serialized bytes preserve these typed setup fields");
                }
            }
            report["setup_fields_equal_despite_serialized_differences"] = true;
            report["status"] = "pass";
        } catch(const std::exception& error) {
            report["status"] = "fail";
            report["error"] = error.what();
        }
        save(run / "report.json", report);
        std::cout << report.at("status") << '\n';
        return report.at("status") == "pass" ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
