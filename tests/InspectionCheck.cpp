#include "Check.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/NsightEvidence.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using Error = ngm::InspectionErrorCode;
using ngm::check::require;

struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-inspection-XXXXXX").string();
        const auto result = mkdtemp(pattern.data());
        require(result != nullptr, "create isolated inspection check directory");
        path = result;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

void write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
    output.close();
    require(output.good(), "write synthetic retained evidence");
}

std::string read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "open sanitized fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}

template <typename Function>
void rejected(Error expected, Function&& function) {
    try {
        function();
    } catch(const ngm::InspectionError& error) {
        require(error.code() == expected, std::string("Expected ") + std::string(ngm::inspection_error_name(expected)) +
                                              ", received " + error.what());
        return;
    }
    throw std::runtime_error("Expected a typed inspection error");
}

struct Bundle {
    std::string metadata;
    std::string functions;
    std::string objects;
    bool failed = false;
    bool omit_objects = false;
    std::function<void(Json&)> report_edit{};
    std::function<void(Json&)> manifest_edit{};
    std::function<void(std::string&)> report_text_edit{};
};

Json process() {
    return {
        {"exit_code", 0}, {"signal", nullptr}, {"timed_out", false}, {"cancelled", false}, {"cleanup_confirmed", true}};
}

std::string publish(ngm::ArtifactStore& store, Bundle bundle) {
    const std::vector<std::string> required{"raw/capture.ngfx-capture", "raw/exports/metadata.raw", "raw/report.json"};
    auto writer = store.begin(Json::object(), required);
    Json report{{"schema_version", 1},
                {"project_version", "test-only"},
                {"evidence_origin", "nsight_capture"},
                {"backend", "documented_nsight_cli"},
                {"readable_capture", true},
                {"cleanup_confirmed", true},
                {"worker_outcome", "succeeded"},
                {"job", {{"job_id", "job-test"}, {"capture_id", writer.id()}, {"attempt", 1}}},
                {"nsight", {{"interface_ready", true}}},
                {"exports", Json::array()},
                {"environment", {{"private", "report-secret"}}},
                {"command_line", "report-command-secret"}};
    for(const auto* tool : {"capture", "replay"}) {
        report["nsight"][tool] = {{"path", std::string("/synthetic/ngfx-") + tool},
                                  {"version", "2026.3.1.0"},
                                  {"build", "38722833"},
                                  {"version_valid", true},
                                  {"help_valid", true}};
    }
    const std::string capture = "synthetic capture bytes";
    report["capture"] = {{"outcome", "success"},
                         {"launched", true},
                         {"process", process()},
                         {"executable", "/synthetic/ngfx-capture"},
                         {"output", "raw/capture.ngfx-capture"},
                         {"output_bytes", capture.size()}};
    write(writer.raw_directory() / "capture.ngfx-capture", capture);
    for(const auto& [kind, contents] : std::vector<std::pair<std::string, std::string>>{
            {"metadata", bundle.metadata}, {"functions", bundle.functions}, {"objects", bundle.objects}}) {
        const auto path = "raw/exports/" + kind + ".raw";
        if(kind != "objects" || !bundle.omit_objects) {
            write(writer.directory() / path, contents);
        }
        report["exports"].push_back({{"kind", kind},
                                     {"outcome", "success"},
                                     {"evidence_origin", "nsight_export"},
                                     {"launched", true},
                                     {"process", process()},
                                     {"executable", "/synthetic/ngfx-replay"},
                                     {"output", path},
                                     {"output_bytes", contents.size()}});
    }
    if(bundle.report_edit) {
        bundle.report_edit(report);
    }
    auto manifest = report;
    manifest["report_path"] = "raw/report.json";
    manifest.erase("environment");
    manifest.erase("command_line");
    if(bundle.manifest_edit) {
        bundle.manifest_edit(manifest);
    }
    auto serialized = report.dump();
    if(bundle.report_text_edit) {
        bundle.report_text_edit(serialized);
    }
    write(writer.raw_directory() / "report.json", serialized);
    store.update_provenance(writer, manifest);
    const auto info =
        bundle.failed ? store.publish_failure(writer, "synthetic failed capture") : store.publish_success(writer);
    return info.summary.id;
}

void bounded_result(const Json& result) {
    const auto serialized = result.dump();
    require(serialized.size() + Json(serialized).dump().size() + 65536 + 1024 <=
                ngm::InspectionService::maximum_result_bytes,
            "structured result plus escaped text fallback fits protocol result budget");
    require(serialized.find("report-secret") == std::string::npos &&
                serialized.find("report-command-secret") == std::string::npos &&
                serialized.find("process_environment") == std::string::npos &&
                serialized.find("process_command_line") == std::string::npos,
            "summaries omit process context and unselected report fields");
}

void checks(const fs::path& fixtures, const fs::path& root) {
    ngm::ArtifactOptions options;
    options.root = root;
    options.max_bytes = 0;
    options.max_age = std::chrono::seconds(0);
    ngm::ArtifactStore store(options);
    ngm::InspectionService inspection(store);
    const Bundle baseline{read(fixtures / "reference-metadata.json"), read(fixtures / "reference-functions.json"),
                          read(fixtures / "reference-objects.json")};
    const auto id = publish(store, baseline);
    const auto metadata = inspection.metadata(id);
    bounded_result(metadata);
    require(metadata["capture_id"] == id && metadata["metadata"]["metadata_version"] == 1 &&
                metadata["metadata"]["captured_frame"] == "2" && metadata["metadata"]["resolution"] == "192x128",
            "capture-scoped selected fields preserve the observed types");
    require(metadata["producer"]["capture_tool"]["version"] == "2026.3.1.0" &&
                metadata["producer"]["metadata_nsight_version"] == "2026.3.1" &&
                metadata["producer"]["replay_tool"]["build"] == "38722833",
            "producer CLI/export strings remain exact and distinct");
    require(metadata["metadata"]["_metadata_collection_"]["warnings"].size() == 1 &&
                metadata["unavailable_from_these_exports"].size() == 8,
            "collection warning and unavailable detailed evidence remain explicit");
    auto events = inspection.events(id, 0, 3);
    bounded_result(events);
    require(events["total"] == 22 && events["events"].size() == 3 && events["offset"] == 0 &&
                events["next_offset"] == 3,
            "event pages have correct export-order counts and continuation");
    events = inspection.events(id, 14, 1);
    require(events["events"][0]["event_index"] == 14 && events["events"][0]["function_name"] == "vkCmdDraw" &&
                events["events"][0]["sequence_id"].is_null(),
            "optional missing sequence IDs remain null; no pipeline association is invented");
    const auto objects = inspection.objects(id, 31, 100);
    bounded_result(objects);
    require(objects["total"] == 32 && objects["objects"].size() == 1 && objects["next_offset"].is_null(),
            "last object page terminates");
    const auto empty = inspection.events(id, 100000, 1);
    require(empty["events"].empty() && empty["total"] == 22 && empty["next_offset"].is_null(),
            "beyond-end offsets return an explicit empty terminal page");
    require(!store.inspect(id).summary.in_use, "inspection releases its usage lease after returning");

    auto changed = baseline;
    changed.metadata =
        R"({"metadata_version":1,"nsight_version":"2026.3.1","nsight_version_build_id":"38722833","primary_api":"Vulkan"})";
    const auto minimal = inspection.metadata(publish(store, changed));
    require(minimal["metadata"]["uuid"].is_null() && minimal["metadata"]["_metadata_collection_"].is_null(),
            "unexported optional metadata is explicit null");

    changed = baseline;
    changed.failed = true;
    rejected(Error::IncompleteCapture, [&] { inspection.metadata(publish(store, changed)); });
    changed = baseline;
    changed.manifest_edit = [](auto& manifest) { manifest["evidence_origin"] = "caller_provided_import"; };
    rejected(Error::NotCapture, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["job"]["capture_id"] = "bundle-" + std::string(32, '0'); };
    rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, changed)); });
    changed = baseline;
    changed.manifest_edit = [](auto& manifest) { manifest["nsight"]["replay"]["build"] = "different"; };
    rejected(Error::InvalidReport, [&] { inspection.objects(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["nsight"]["replay"]["version"] = "2027.1.0.0"; };
    rejected(Error::UnsupportedProducer, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    auto wrong_metadata = Json::parse(changed.metadata);
    wrong_metadata["metadata_version"] = 2;
    changed.metadata = wrong_metadata.dump();
    changed.functions = "not JSON";
    rejected(Error::InvalidExport, [&] { inspection.events(publish(store, changed)); });
    wrong_metadata["metadata_version"] = 1;
    wrong_metadata["nsight_version_build_id"] = "different";
    changed.metadata = wrong_metadata.dump();
    rejected(Error::UnsupportedProducer, [&] { inspection.objects(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["exports"][1]["outcome"] = "unavailable"; };
    rejected(Error::ExportUnavailable, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    changed.omit_objects = true;
    rejected(Error::ExportUnavailable, [&] { inspection.objects(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["exports"][2]["output_bytes"] = 1; };
    rejected(Error::InvalidReport, [&] { inspection.objects(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["exports"][1]["process"]["cleanup_confirmed"] = false; };
    rejected(Error::InvalidReport, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    changed.report_edit = [](auto& report) { report["exports"].push_back(report["exports"][1]); };
    rejected(Error::InvalidReport, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    changed.functions = R"([{"event_index":1,"function_name":"vkCmdDraw","thread_index":"0"}])";
    rejected(Error::InvalidExport, [&] { inspection.events(publish(store, changed)); });
    changed = baseline;
    changed.report_text_edit = [](auto& report) { report.insert(1, "\"schema_version\":1,"); };
    rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, changed)); });
    changed = baseline;
    changed.report_text_edit = [](auto& report) {
        report = "{\"extension\":" + std::string(32, '[') + "0" + std::string(32, ']') + "}";
    };
    rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, changed)); });
    changed = baseline;
    changed.objects = std::string(ngm::NsightEvidenceLimits::maximum_bytes + 1, ' ');
    rejected(Error::LimitExceeded, [&] { inspection.objects(publish(store, changed)); });

    // An actual import with copied service-looking reports cannot become a
    // capture: the origin label and imported partition remain authoritative.
    const auto source = root.parent_path() / "import-source";
    fs::create_directories(source);
    write(source / "report.json", store.read(id, "raw/report.json"));
    const auto imported = store.import_directory(source, {{"evidence_origin", "caller_provided_import"}});
    rejected(Error::NotCapture, [&] { inspection.metadata(imported.summary.id); });

    // More than 1 MiB of valid inventory exercises the explicit 16 MiB store
    // read extension. Escaped strings force byte pagination well before the
    // row limit. Every record must be retrievable once, without gaps/repeats.
    changed = baseline;
    Json large = Json::array();
    for(std::size_t index = 0; index < 120; ++index) {
        large.push_back({{"event_index", index * 2}, {"function_name", std::string(16000, '"')}, {"thread_index", 0}});
    }
    changed.functions = large.dump();
    require(changed.functions.size() > 1024U * 1024U, "inventory exceeds old core read cap");
    const auto large_id = publish(store, changed);
    std::size_t offset = 0;
    std::size_t count = 0;
    do {
        const auto page = inspection.events(large_id, offset, 100);
        bounded_result(page);
        require(page.dump().size() <= ngm::InspectionService::maximum_page_bytes && page["total"] == 120 &&
                    !page["events"].empty() && page["events"].size() < 100,
                "encoded-byte budget stops oversized pages with full total");
        for(const auto& record : page["events"]) {
            require(record["event_index"] == count * 2, "page preserves record order and capture-scoped ID");
            ++count;
        }
        if(page["next_offset"].is_null()) {
            break;
        }
        offset = page["next_offset"].get<std::size_t>();
        require(offset == count, "continuation uses rows consumed, not requested page size");
    } while(true);
    require(count == 120 && !store.inspect(large_id).summary.in_use,
            "all records retrieved and temporary inspection protections released");

    changed = baseline;
    changed.objects = Json::array({{{"uid", 1},
                                    {"api", std::string(16384, '\1')},
                                    {"object_name", std::string(16384, '\1')},
                                    {"type_name", std::string(16384, '\1')},
                                    {"access_flags", 0}}})
                          .dump();
    rejected(Error::LimitExceeded, [&] { inspection.objects(publish(store, changed)); });

    // Valid selected metadata may serialize much larger than its decoded text
    // budget, particularly because the capture UUID also scopes the summary.
    changed = baseline;
    changed.metadata = Json{{"metadata_version", 1},
                            {"nsight_version", "2026.3.1"},
                            {"nsight_version_build_id", "38722833"},
                            {"primary_api", "Vulkan"},
                            {"uuid", std::string(16384, '\1')},
                            {"primary_gpu", std::string(16384, '\1')},
                            {"driver_vendor", std::string(16384, '\1')},
                            {"resolution", std::string(16360, '\1')}}
                           .dump();
    (void)ngm::parse_nsight_metadata(changed.metadata);
    rejected(Error::LimitExceeded, [&] { inspection.metadata(publish(store, changed)); });

    for(const auto& bad : std::vector<std::pair<std::size_t, std::size_t>>{{0, 0}, {0, 101}, {100001, 1}}) {
        try {
            inspection.events(id, bad.first, bad.second);
            throw std::runtime_error("invalid pagination was accepted");
        } catch(const std::invalid_argument&) {
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "usage: ngm_inspection_check /path/to/sanitized/nsight-2026.3.1-fixtures");
        const Scratch scratch;
        checks(argv[1], scratch.path / "store");
    });
}
