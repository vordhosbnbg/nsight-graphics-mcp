#include "Check.hpp"
#include "CppEvidenceFixture.hpp"
#include "ngm/Inspection.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <unistd.h>

namespace {
using Json = nlohmann::json;
using ngm::check::require;
using Error = ngm::InspectionErrorCode;
const std::string project = "raw/cpp/CppCaptures/test-project";
const std::string source = project + "/CommandList00.cpp";
struct Scratch {
    std::filesystem::path path;
    Scratch() {
        std::string pattern = "/tmp/ngm-cpp-inspection-XXXXXX";
        require(mkdtemp(pattern.data()) != nullptr, "create isolated store");
        path = pattern;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
void write(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << contents;
    file.close();
    require(file.good(), "write synthetic C++ bundle");
}
using Edit = std::function<void(Json&, Json&, Json&, Json&)>;
std::string publish(ngm::ArtifactStore& store, Edit edit = {},
                    std::string commands = ngm::check::cpp::recording(ngm::check::cpp::begin + ngm::check::cpp::draw +
                                                                      ngm::check::cpp::end),
                    bool failed = false) {
    auto writer = store.begin(Json::object(), {"raw/report.json", "derived/cpp-project.json"});
    const Json job{{"job_id", "job-synthetic"}, {"capture_id", writer.id()}, {"attempt", 1}};
    Json metadata{{"nsight_version", "2026.3.1"},
                  {"nsight_version_build_id", 38722833},
                  {"metadata_version", 1},
                  {"primary_api", "vulkan"},
                  {"primary_gpu", "synthetic"},
                  {"project_filename", "test-project"},
                  {"has_unsupported_operation", false},
                  {"process_environment", "secret"}};
    Json index{{"schema_version", 1},
               {"evidence_origin", "nsight_generated_cpp"},
               {"job", job},
               {"nsight_version", "2026.3.1"},
               {"nsight_version_build_id", 38722833},
               {"primary_api", "vulkan"},
               {"primary_gpu", "synthetic"},
               {"has_unsupported_operation", false},
               {"project_directory", project},
               {"metadata_path", project + "/metadata.json"},
               {"database_path", project + "/data.bin"},
               {"screenshot_path", project + "/screenshot.bmp"},
               {"source_files", {source, project + "/Resources00.cpp"}}};
    Json report{{"schema_version", 1},
                {"project_version", "test-only"},
                {"evidence_origin", "nsight_cpp_capture"},
                {"backend", "documented_nsight_cli"},
                {"job", job},
                {"readable_capture", false},
                {"generated_cpp_project", true},
                {"cleanup_confirmed", true},
                {"worker_outcome", "succeeded"},
                {"capture_settings", {{"format", "cpp"}}},
                {"cpp_project_index", "derived/cpp-project.json"},
                {"nsight", {{"interface_ready", true}}},
                {"capture",
                 {{"outcome", "success"},
                  {"launched", true},
                  {"output", project + "/metadata.json"},
                  {"output_bytes", metadata.dump().size()},
                  {"executable", "/synthetic/cli"},
                  {"process",
                   {{"exit_code", 0},
                    {"signal", nullptr},
                    {"cleanup_confirmed", true},
                    {"cancelled", false},
                    {"timed_out", false}}}}}};
    for(const auto* tool : {"capture", "replay", "cli"})
        report["nsight"][tool] = {{"path", std::string("/synthetic/") + tool},
                                  {"version", "2026.3.1.0"},
                                  {"build", "38722833"},
                                  {"version_valid", true},
                                  {"help_valid", true}};
    Json manifest = report;
    manifest["report_path"] = "raw/report.json";
    if(edit)
        edit(report, manifest, index, metadata);
    // Metadata byte-count bookkeeping is independent of every semantic mutation.
    report["capture"]["output_bytes"] = metadata.dump().size();
    write(writer.directory() / "raw/report.json", report.dump());
    write(writer.directory() / "derived/cpp-project.json", index.dump());
    write(writer.directory() / (project + "/metadata.json"), metadata.dump());
    write(writer.directory() / (project + "/data.bin"), "synthetic, not serialized data");
    write(writer.directory() / (project + "/screenshot.bmp"), "synthetic, not image data");
    write(writer.directory() / source, commands);
    write(writer.directory() / (project + "/Resources00.cpp"), ngm::check::cpp::resources);
    store.update_provenance(writer, manifest);
    return (failed ? store.publish_failure(writer, "synthetic failure") : store.publish_success(writer)).summary.id;
}
template <typename Function>
void rejected(Error code, Function function) {
    try {
        function();
    } catch(const ngm::InspectionError& error) {
        require(error.code() == code, std::string("unexpected inspection error: ") + error.what());
        return;
    }
    throw std::runtime_error("expected rejection");
}
void bounded(const Json& result) {
    const auto text = result.dump();
    require(text.size() <= ngm::InspectionService::maximum_page_bytes &&
                text.size() + Json(text).dump().size() + 66560 <= ngm::InspectionService::maximum_result_bytes,
            "response exceeds budget");
}
} // namespace
int main() {
    return ngm::check::run([] {
        Scratch scratch;
        ngm::ArtifactStore store({scratch.path / "store"});
        ngm::InspectionService inspection(store);
        const auto id = publish(store);
        const auto page = inspection.cpp_source(id, source, 1, 2);
        require(page["lines"].size() == 2 && page["next_line"] == 3 &&
                    page["source"]["sha256"].get<std::string>().size() == 64,
                "source line/hash page");
        require(page.dump().find("secret") == std::string::npos, "process metadata leaked");
        bounded(page);
        require(inspection.cpp_source(id, source, 999)["lines"].empty(), "past end source page");
        const auto draws = inspection.cpp_draws(id);
        require(draws["draws_total"] == 1 &&
                    draws["draws"][0]["association_status"] == "resolved_source_relationship" &&
                    draws["source_files"].size() == 2,
                "draw query lacks source relationship/identities");
        bounded(draws);
        const auto resources = inspection.cpp_resources(id, "resources", 0, 1);
        bounded(resources);
        require(resources["resources"].size() == 1 && resources["resources_total"] == 1 &&
                    resources["resources"][0]["handle"] == 14,
                "resource source query");
        const auto reference = resources["resources"][0]["resource_ref"].get<std::string>();
        rejected(Error::ExportUnavailable, [&] { (void)inspection.cpp_resource(id, reference); });
        rejected(Error::ExportUnavailable, [&] { (void)inspection.cpp_resource(id, std::string(64, '0')); });
        require(inspection.cpp_resources(id, "resources", 100)["resources"].empty(), "past-end resources");
        require(inspection.cpp_draws(id, "draws", 100)["draws"].empty(), "past end draw page");
        require(inspection.cpp_draws(id, "unsupported_objects")["total"] == 0, "coverage pagination");
        require(!store.inspect(id).summary.in_use, "lease leaked after query");
        const auto old = publish(store, [](Json& r, Json& m, Json& i, Json& md) {
            for(const auto* tool : {"capture", "replay", "cli"}) {
                r["nsight"][tool]["version"] = m["nsight"][tool]["version"] = "2026.2.0.0";
                r["nsight"][tool]["build"] = m["nsight"][tool]["build"] = "37991608";
            }
            i["nsight_version"] = md["nsight_version"] = "2026.2.0";
            i["nsight_version_build_id"] = md["nsight_version_build_id"] = 37991608;
        });
        require(inspection.cpp_draws(old)["draws_total"] == 1, "second exact profile");
        require(inspection.cpp_resources(old)["resources_total"] == 1, "second resource source profile");
        for(const auto& edit :
            std::vector<Edit>{[](Json&, Json& m, Json&, Json&) { m["nsight"]["cli"]["path"] = "/other"; },
                              [](Json& r, Json&, Json&, Json&) { r["capture"]["process"]["cancelled"] = true; },
                              [](Json&, Json&, Json& i, Json&) { i["source_files"].erase(0); },
                              [](Json&, Json&, Json& i, Json&) { i["source_files"].push_back(source); },
                              [](Json&, Json&, Json& i, Json&) { i["source_files"][0] = "raw/cpp/../escape.cpp"; },
                              [](Json&, Json&, Json& i, Json&) { i["job"]["attempt"] = 2; },
                              [](Json&, Json&, Json& i, Json&) { i["primary_gpu"] = "different"; },
                              [](Json&, Json&, Json&, Json& md) { md["project_filename"] = "different"; },
                              [](Json& r, Json& m, Json&, Json&) {
                                  r["generated_cpp_project"] = m["generated_cpp_project"] = false;
                              }}) {
            const auto bad = publish(store, edit);
            rejected(Error::InvalidReport, [&] { inspection.cpp_draws(bad); });
            rejected(Error::InvalidReport, [&] { inspection.cpp_resources(bad); });
            require(!store.inspect(bad).summary.in_use, "failed-query lease leaked");
        }
        const auto mismatched =
            publish(store, [](Json&, Json&, Json&, Json& md) { md["nsight_version"] = "2026.2.0"; });
        rejected(Error::UnsupportedProducer, [&] { inspection.cpp_draws(mismatched); });
        const auto unsupported = publish(store, [](Json&, Json&, Json& i, Json& md) {
            i["has_unsupported_operation"] = md["has_unsupported_operation"] = true;
        });
        require(inspection.cpp_source(unsupported, source)["has_unsupported_operation"] == true,
                "unsupported raw source unavailable");
        rejected(Error::ExportUnavailable, [&] { inspection.cpp_draws(unsupported); });
        rejected(Error::ExportUnavailable, [&] { inspection.cpp_resources(unsupported); });
        rejected(Error::ExportUnavailable, [&] { inspection.cpp_source(id, project + "/data.bin"); });
        const auto failed = publish(store, {}, "synthetic", true);
        rejected(Error::IncompleteCapture, [&] { inspection.cpp_draws(failed); });
        const auto invalid = publish(store, {}, std::string(1, static_cast<char>(0xff)));
        rejected(Error::InvalidExport, [&] { inspection.cpp_source(invalid, source); });
        const auto nul = publish(store, {}, std::string(1, '\0'));
        rejected(Error::InvalidExport, [&] { inspection.cpp_source(nul, source); });
        const auto oversized = publish(store, {}, std::string(4U * 1024U * 1024U + 1, 'x'));
        rejected(Error::LimitExceeded, [&] { inspection.cpp_source(oversized, source); });
        const auto long_line = publish(store, {}, std::string(300000, 'x'));
        rejected(Error::LimitExceeded, [&] { inspection.cpp_source(long_line, source); });
        const auto unsupported_recording =
            publish(store, {},
                    ngm::check::cpp::recording(ngm::check::cpp::begin + "hidden_helper();\n" + ngm::check::cpp::draw +
                                               ngm::check::cpp::end));
        require(inspection.cpp_draws(unsupported_recording)["unsupported_recordings_total"] == 1,
                "unsupported coverage missing");
        const auto coverage = inspection.cpp_draws(unsupported_recording, "unsupported_recordings");
        require(coverage["unsupported_recordings"].size() == 1 && coverage["draws"].empty(), "coverage section absent");
    });
}
