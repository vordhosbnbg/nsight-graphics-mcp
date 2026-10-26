#include "ShaderTraceFixture.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/Inspection.hpp"

#include <bit>
#include <cmath>
#include <functional>

namespace {
using namespace ngm::check::shader_trace;
using ngm::check::require;
using namespace std::chrono_literals;
template <typename F>
void invalid(F operation) {
    try {
        operation();
    } catch(const std::invalid_argument&) {
        return;
    }
    require(false, "malformed trace is explicitly rejected");
}
template <typename F>
void rejected(ngm::InspectionErrorCode code, F operation) {
    try {
        operation();
    } catch(const ngm::InspectionError& error) {
        require(error.code() == code, "expected typed trace inspection refusal: " + std::string(error.what()));
        return;
    }
    require(false, "invalid retained trace must refuse");
}
void parser_checks() {
    const auto good = payload();
    const auto trace = ngm::parse_shader_trace(good.dump());
    const auto first = ngm::shader_trace_page(trace, {}, 0, 1);
    const auto& row = first.at("records")[0];
    require(first.at("records_total") == 4 && first.at("matched_records") == 4 && first.at("next_offset") == 1 &&
                first.at("producer_claims_verified") == false && first.at("original_execution") == false &&
                first.at("capture_event_correspondence") == false,
            "bounded pagination retains explicit application-claim scope");
    require(row.at("bits") == good.at("records")[0].at("bits") && row.at("bits_hex")[3] == "0x7fc12345" &&
                row.at("values")[2].is_null() && row.at("values")[3].is_null() && row.at("primitive").is_null() &&
                std::signbit(row.at("values")[1].get<float>()),
            "raw nonfinite payloads, signed zero and unavailable primitive survive decoding");
    ngm::ShaderTraceSelection selected;
    selected.frame = 10;
    auto page = ngm::shader_trace_page(trace, selected, 1, 1);
    require(page.at("matched_records") == 2 && page.at("next_offset").is_null() &&
                page.at("records")[0].at("values")[0] == UINT32_MAX,
            "offset applies after exact application-frame selection");
    selected.frame = 11;
    selected.probe = 2;
    page = ngm::shader_trace_page(trace, selected);
    require(page.at("matched_records") == 1 && page.at("records")[0].at("values")[0] == -1,
            "int32 probe values retain their sign");
    selected.pixel_x = 4;
    require(ngm::shader_trace_page(trace, selected).at("records").empty(), "unmatched selection is empty data");
    for(const auto& edit : std::vector<std::function<void(Json&)>>{
            [](Json& j) { j["schema_version"] = 2; },
            [](Json& j) { j["original_execution"] = true; },
            [](Json& j) { j["capture_event_correspondence"] = true; },
            [](Json& j) { j["evidence_origin"] = "observed_gpu_registers"; },
            [](Json& j) { j["extra"] = 0; },
            [](Json& j) { j["capacity"] = 4097; },
            [](Json& j) { j["capacity"] = 0; },
            [](Json& j) { j["attempted_records"] = std::uint64_t{UINT32_MAX} + 1; },
            [](Json& j) { j["dropped_records"] = std::uint64_t{UINT32_MAX} + 1; },
            [](Json& j) { j["attempted_records"] = 5; },
            [](Json& j) { j["records"][0]["bits"][0] = -1; },
            [](Json& j) { j["records"][0]["bits"][0] = std::uint64_t{UINT32_MAX} + 1; },
            [](Json& j) { j["records"][0]["bits"][0] = 1.5; },
            [](Json& j) { j["records"][0]["probe"] = 99; },
            [](Json& j) { j["records"][0]["bits"].push_back(0); },
            [](Json& j) { j["records"][0]["pixel_x"] = 4; },
            [](Json& j) { j["records"][0]["primitive"] = "unknown"; },
            [](Json& j) { j["probes"][1]["id"] = 0; },
            [](Json& j) { j["probes"][0]["components"] = 5; },
            [](Json& j) { j["probes"][0]["type"] = "float64"; },
            [](Json& j) { j["shader"]["stage"] = "compute"; },
            [](Json& j) { j["shader"]["sha256"] = std::string(64, 'z'); },
            [](Json& j) { j["extent"]["width"] = 0; },
            [](Json& j) { j["selection"]["pixel_y"] = 6; }}) {
        auto changed = good;
        edit(changed);
        invalid([&] { (void)ngm::parse_shader_trace(changed.dump()); });
    }
    for(const auto* path : {"../escape.spv", "shaders/../escape.spv", "/absolute.spv", "a//b", "a\\b", "./x"}) {
        auto changed = good;
        changed["shader"]["path"] = path;
        invalid([&] { (void)ngm::parse_shader_trace(changed.dump()); });
    }
    for(const auto* key : {"schema_version", "entry_point", "pixel_x", "components"}) {
        auto raw = good.dump();
        const auto at = raw.find(std::string("\"") + key + "\":");
        require(at != std::string::npos, "duplicate-key witness field exists");
        raw.insert(at, std::string("\"") + key + "\":0,");
        invalid([&] { (void)ngm::parse_shader_trace(raw); });
    }
    invalid([&] { (void)ngm::parse_shader_trace(std::string(ngm::shader_trace_maximum_bytes + 1, ' ')); });
    invalid([&] { (void)ngm::parse_shader_trace(std::string(20, '[') + std::string(20, ']')); });
    invalid([&] { (void)ngm::shader_trace_page(trace, {}, 0, 0); });
    invalid([&] { (void)ngm::shader_trace_page(trace, {}, 0, 101); });
    auto truncated = good;
    truncated["attempted_records"] = 6;
    truncated["dropped_records"] = 2;
    require(ngm::parse_shader_trace(truncated.dump()).at("dropped_records") == 2,
            "bounded buffer overflow remains explicit dropped data");
    truncated["attempted_records"] = UINT32_MAX;
    truncated["dropped_records"] = UINT32_MAX - 4;
    require(ngm::parse_shader_trace(truncated.dump()).at("records").size() == 4,
            "largest representable attempt count stays bounded without arithmetic overflow");
    auto empty = good;
    empty["records"] = Json::array();
    empty["attempted_records"] = 0;
    require(ngm::shader_trace_page(ngm::parse_shader_trace(empty.dump())).at("records").empty(),
            "zero observations do not become zero-valued observations");
}
void retained_checks(const fs::path& standin) {
    Scratch scratch;
    ngm::CaptureServiceOptions options;
    options.nsight_root = installation(standin, scratch.path);
    options.artifacts.root = scratch.path / "store";
    options.environment = {{"DISPLAY", ":99"}, {"PATH", "/usr/bin:/bin"}};
    ngm::CaptureService service(options);
    ngm::InspectionService inspect(service.artifacts());
    ngm::CaptureRequest request;
    request.executable = fs::read_symlink("/proc/self/exe");
    request.working_directory = scratch.path;
    request.application_output_option = "--output";
    request.timeout = 15s;
    auto collect = [&](const char* mode) {
        request.arguments = {"--produce", mode};
        const auto job = service.capture(request);
        const auto state = service.wait(job.identity.job_id, 15s);
        require(state && ngm::job_terminal(state->state) && state->cleanup_confirmed && !state->worker_running &&
                    !state->gpu_reserved && !state->finalization_pending,
                "synthetic capture completes with owned cleanup");
        require(state->state ==
                    (std::string_view(mode) == "failure" ? ngm::JobState::failed : ngm::JobState::succeeded),
                "expected synthetic capture outcome: " + state->error);
        return job.artifact_id;
    };
    const auto id = collect("valid");
    const auto result = inspect.shader_trace(id);
    require(result.at("capture_id") == id && result.at("records_total") == 4 &&
                result.at("shader").at("retained_content_matches_reported_hash") == true &&
                result.at("launch").at("identity_scope") ==
                    "path contents before and after launch; running image unverified",
            "retained trace is bound to service output and content identities without execution attestation");
    const auto clone = [&](const std::function<void(Json&, Json&)>& edit) {
        const auto source = service.artifacts().inspect(id);
        auto writer = service.artifacts().begin(Json::object(), source.required_outputs);
        auto manifest = source.provenance;
        auto report = Json::parse(service.artifacts().read(id, "raw/report.json", 2U * 1024U * 1024U));
        manifest["job"]["capture_id"] = writer.id();
        report["job"]["capture_id"] = writer.id();
        edit(manifest, report);
        std::size_t offset = 0;
        do {
            const auto files = service.artifacts().files(id, offset, 100);
            for(const auto& file : files.files)
                if(file.path != "raw/report.json")
                    service.artifacts().snapshot_file(id, file.path, writer, file.path, 16U * 1024U * 1024U,
                                                      std::chrono::steady_clock::now() + 10s);
            if(!files.next_offset)
                break;
            offset = *files.next_offset;
        } while(true);
        write(writer.raw_directory() / "report.json", report.dump());
        service.artifacts().update_provenance(writer, manifest);
        return service.artifacts().publish_success(writer).summary.id;
    };
    const auto mismatched = clone([](Json& manifest, Json&) { manifest["application"]["output_path"] = "raw/other"; });
    rejected(ngm::InspectionErrorCode::InvalidReport, [&] { (void)inspect.shader_trace(mismatched); });
    const auto uncontrolled = clone([](Json& manifest, Json& report) {
        for(auto* owner : {&manifest, &report})
            (*owner)["application"]["output_evidence_origin"] = "caller_provided";
    });
    rejected(ngm::InspectionErrorCode::InvalidReport, [&] { (void)inspect.shader_trace(uncontrolled); });
    const auto changed_executable = clone([](Json& manifest, Json& report) {
        for(auto* owner : {&manifest, &report})
            (*owner)["application"]["sha256_after_launch"] = std::string(64, 'b');
    });
    rejected(ngm::InspectionErrorCode::InvalidReport, [&] { (void)inspect.shader_trace(changed_executable); });
    rejected(ngm::InspectionErrorCode::ExportUnavailable, [&] { (void)inspect.shader_trace(collect("missing")); });
    for(const auto* mode : {"bad-hash", "not-spirv", "bad-count"})
        rejected(ngm::InspectionErrorCode::InvalidExport, [&] { (void)inspect.shader_trace(collect(mode)); });
    rejected(ngm::InspectionErrorCode::IncompleteCapture, [&] { (void)inspect.shader_trace(collect("failure")); });
    const auto import_source = scratch.path / "import";
    write(import_source / "shader-trace.json", payload().dump());
    const auto imported = service.artifacts().import_directory(import_source, Json::object(), {}, false);
    rejected(ngm::InspectionErrorCode::NotCapture, [&] { (void)inspect.shader_trace(imported.summary.id); });
}
} // namespace
int main(int argc, char** argv) {
    if(argc > 1 && std::string_view(argv[1]) == "--produce")
        return ngm::check::run([&] { require(produce(argc, argv) == 0, "synthetic producer failure requested"); });
    return ngm::check::run([&] {
        require(argc == 2, "Nsight process stand-in path required");
        parser_checks();
        retained_checks(fs::absolute(argv[1]));
    });
}
