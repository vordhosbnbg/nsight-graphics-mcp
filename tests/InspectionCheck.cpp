#include "Check.hpp"
#include "ngm/ImageEvidence.hpp"
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
    std::string tool_version = "2026.3.1.0";
    std::string tool_build = "38722833";
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
                                  {"version", bundle.tool_version},
                                  {"build", bundle.tool_build},
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
    Bundle compute{read(fixtures / "compute-metadata.json"), read(fixtures / "compute-functions.json"),
                   read(fixtures / "compute-objects.json")};
    compute.report_edit = [](Json& report) {
        report["capture_settings"] = {{"delimiter", "vk_frame_boundary"}, {"capture_frame", 2}};
        report["capture"]["arguments"] = {"--delimiter-vk-frame-boundary-ext"};
    };
    const auto compute_id = publish(store, compute);
    const auto compute_meta = inspection.metadata(compute_id);
    require(compute_meta["metadata"]["primary_api"] == "" &&
                compute_meta["producer"]["api_identification"] == "graphics_apis_for_vk_frame_boundary",
            "observed no-presentation API identification preserves the empty primary field");
    const auto compute_events = inspection.events(compute_id);
    require(compute_events["total"] == 16 && inspection.objects(compute_id)["total"] == 17,
            "observed compute inventories are available through the qualified profile");
    for(unsigned mode = 0; mode < 8; ++mode) {
        auto bad = compute;
        auto meta = Json::parse(bad.metadata);
        if(mode == 0)
            meta.erase("primary_api");
        if(mode == 1)
            meta["primary_api"] = "OpenGL";
        if(mode == 2)
            meta["graphics_apis"]["OpenGL"] = {"general"};
        if(mode == 3)
            meta.erase("graphics_features");
        if(mode == 4)
            meta["has_unsupported_operation"] = true;
        if(mode == 5)
            bad.report_edit = [](Json& report) { report["capture_settings"] = {{"delimiter", "present"}}; };
        if(mode == 6)
            bad.report_edit = [](Json& report) {
                report["capture_settings"] = {{"delimiter", "vk_frame_boundary"}};
                report["capture"]["arguments"] = {"--delimiter-present"};
            };
        if(mode == 7)
            meta["primary_api"] = nullptr;
        bad.metadata = meta.dump();
        rejected(mode == 7 ? Error::InvalidExport : Error::UnsupportedProducer,
                 [&] { inspection.metadata(publish(store, bad)); });
    }
    for(const auto& invalid : {Json(nullptr), Json::object(), Json(""), Json(std::string("bad\0arg", 7))}) {
        auto malformed = compute;
        malformed.report_edit = [invalid](Json& report) {
            report["capture_settings"] = {{"delimiter", "vk_frame_boundary"}};
            report["capture"]["arguments"] = Json::array({"--delimiter-vk-frame-boundary-ext", invalid});
        };
        rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, malformed)); });
    }
    auto conflict = compute;
    conflict.manifest_edit = [](Json& manifest) { manifest["capture_settings"]["delimiter"] = "present"; };
    rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, conflict)); });
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
void profile_checks(const fs::path& first, const fs::path& second, const fs::path& root) {
    ngm::ArtifactOptions options;
    options.root = root;
    options.max_bytes = 0;
    options.max_age = std::chrono::seconds(0);
    ngm::ArtifactStore store(options);
    ngm::InspectionService inspection(store);
    for(const bool second_release : {false, true}) {
        const auto& fixtures = second_release ? second : first;
        const auto& other_fixtures = second_release ? first : second;
        const std::string version = second_release ? "2026.2.0.0" : "2026.3.1.0";
        const std::string metadata_version = second_release ? "2026.2.0" : "2026.3.1";
        const std::string build = second_release ? "37991608" : "38722833";
        const std::string profile = "nsight-" + metadata_version + "-build-" + build + "-vulkan";
        for(const auto* prefix : {"reference", "shader-error", "indirect-reference", "indirect-parameter-error"}) {
            const bool indirect = std::string_view(prefix).starts_with("indirect-");
            Bundle baseline{read(fixtures / (std::string(prefix) + "-metadata.json")),
                            read(fixtures / (std::string(prefix) + "-functions.json")),
                            read(fixtures / (std::string(prefix) + "-objects.json"))};
            baseline.tool_version = version;
            baseline.tool_build = build;
            const auto id = publish(store, baseline);
            const auto metadata = inspection.metadata(id);
            bounded_result(metadata);
            require(metadata.at("schema_profile") == profile && metadata.at("capture_id") == id &&
                        metadata["producer"]["capture_tool"]["version"] == version &&
                        metadata["producer"]["replay_tool"]["version"] == version &&
                        metadata["producer"]["metadata_nsight_version"] == metadata_version &&
                        metadata["producer"]["metadata_nsight_version_build_id"] == build,
                    "one exact producer profile governs the complete tool/metadata tuple");
            const auto& warnings = metadata["metadata"]["_metadata_collection_"]["warnings"];
            require(second_release ? warnings.is_null() : warnings.size() == 1,
                    "observed absent warnings and actual warning arrays remain distinct");
            const auto events = inspection.events(id, 3, 5);
            bounded_result(events);
            require(events.at("schema_profile") == profile && events.at("offset") == 3 &&
                        events.at("total") == (indirect ? 23 : 22) && events.at("events").size() == 5 &&
                        events.at("next_offset") == 8,
                    "event pagination retains the selected profile");
            const auto draw_page = inspection.events(id, 14, 3);
            const auto marker = draw_page.at("events").at(1).at("indirect_index");
            require(indirect && !second_release ? marker == 0 : marker.is_null(),
                    "typed event pages preserve observed indirect_index zero or explicit absence");
            const auto objects = inspection.objects(id);
            bounded_result(objects);
            require(objects.at("schema_profile") == profile && objects.at("total") == (indirect ? 34 : 32) &&
                        objects.at("next_offset").is_null(),
                    "object page retains the selected profile");
            require(objects.at("objects") == Json::parse(baseline.objects),
                    "all observed object fields, identities, labels, and opaque access integers are preserved");

            auto changed = baseline;
            changed.report_edit = [second_release](Json& report) {
                report["nsight"]["replay"]["version"] = second_release ? "2026.3.1.0" : "2026.2.0.0";
                report["nsight"]["replay"]["build"] = second_release ? "38722833" : "37991608";
            };
            rejected(Error::UnsupportedProducer, [&] { inspection.metadata(publish(store, changed)); });
            changed = baseline;
            changed.metadata = read(other_fixtures / (std::string(prefix) + "-metadata.json"));
            changed.functions = "not JSON";
            rejected(Error::UnsupportedProducer, [&] { inspection.events(publish(store, changed)); });
            changed = baseline;
            changed.tool_build = "unqualified-build";
            rejected(Error::UnsupportedProducer, [&] { inspection.objects(publish(store, changed)); });
            changed = baseline;
            changed.tool_version = metadata_version;
            rejected(Error::UnsupportedProducer, [&] { inspection.metadata(publish(store, changed)); });
            changed = baseline;
            auto unsupported_api = Json::parse(changed.metadata);
            unsupported_api["primary_api"] = "OpenGL";
            changed.metadata = unsupported_api.dump();
            rejected(Error::UnsupportedProducer, [&] { inspection.events(publish(store, changed)); });
            changed = baseline;
            changed.report_edit = [](Json& report) { report["nsight"]["capture"].erase("version"); };
            rejected(Error::InvalidReport, [&] { inspection.metadata(publish(store, changed)); });
        }
    }
}
void image_checks(const fs::path& root) {
    ngm::ArtifactOptions options;
    options.root = root;
    ngm::ArtifactStore store(options);
    auto first = store.begin({{"evidence_origin", "synthetic_image"}}, {"raw/image.ppm"}, true);
    write(first.raw_directory() / "image.ppm", std::string("P6\n1 1\n255\n") + "\1\2\3");
    const ngm::ImageReference reference{first.id(), "raw/image.ppm"};
    bool denied = false;
    try {
        (void)ngm::compare_artifact_images(store, reference, reference, 0);
    } catch(const ngm::ArtifactError&) {
        denied = true;
    }
    require(denied, "staging images cannot be compared as published evidence");
    store.publish_success(first);
    auto second = store.begin({{"evidence_origin", "synthetic_failed_attempt"}}, {}, true);
    write(second.raw_directory() / "image.ppm", std::string("P6\n1 1\n255\n") + "\1\3\6");
    write(second.raw_directory() / "invalid.bin", "not an image");
    write(second.raw_directory() / "wide.ppm", std::string("P6\n2 1\n255\n") + "abcdef");
    // Valid P6 dimensions, but encoded file exceeds the artifact comparison cap.
    write(second.raw_directory() / "oversized.ppm", "P6\n2400 2400\n255\n" + std::string(2400U * 2400U * 3U, 'x'));
    store.publish_failure(second, "synthetic capture failure");
    const ngm::ImageReference candidate{second.id(), "raw/image.ppm"};
    const auto report = ngm::compare_artifact_images(store, reference, candidate, 2);
    require(report.at("differing_pixels") == 1 && report.at("max_channel_difference") == 3 &&
                report.at("reference").at("artifact_status") == "complete" &&
                report.at("candidate").at("artifact_status") == "failed" &&
                report.at("evidence_origin") == "caller_selected_artifact_images",
            "comparison reports pixels and failed-source status without implying a capture success");
    require(report.at("reference").at("rgb_sha256") != report.at("candidate").at("rgb_sha256") &&
                ngm::compare_artifact_images(store, reference, candidate, 3).at("matches_within_tolerance") == true,
            "pixel identities and inclusive tolerance remain distinct");
    const ngm::ImageReference oversized{second.id(), "raw/oversized.ppm"};
    denied = false;
    try {
        (void)ngm::compare_artifact_images(store, oversized, oversized, 0);
    } catch(const ngm::ArtifactError& error) {
        denied = error.code() == ngm::ArtifactErrorCode::InvalidArgument &&
                 std::string(error.what()).find("exceeds the read limit") != std::string::npos;
    }
    require(denied, "equal-sized valid images exceeding the encoded-byte cap fail at the artifact read limit");
    const auto preview = ngm::preview_artifact_image(store, candidate);
    require(preview.metadata.at("source").at("artifact_status") == "failed" &&
                preview.metadata.at("source").at("rgb_sha256") == report.at("candidate").at("rgb_sha256") &&
                preview.metadata.at("resampled") == false &&
                ngm::decode_image(preview.png).rgb == std::vector<std::uint8_t>{1, 3, 6},
            "preview preserves failed-bundle status and original pixel identity");
    for(const auto* path : {"raw/invalid.bin", "raw/wide.ppm", "raw/missing.ppm", "../image.ppm"}) {
        denied = false;
        try {
            (void)ngm::compare_artifact_images(store, reference, {second.id(), path}, 0);
        } catch(const std::exception&) {
            denied = true;
        }
        require(denied, "invalid, mismatched, uninventoried or escaping images are rejected");
    }
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 3, "usage: ngm_inspection_check <2026.3.1 fixture directory> <2026.2.0 fixture directory>");
        const Scratch scratch;
        checks(argv[1], scratch.path / "store");
        profile_checks(argv[1], argv[2], scratch.path / "profiles");
        image_checks(scratch.path / "images");
    });
}
