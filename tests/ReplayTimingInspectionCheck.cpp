#include "Check.hpp"
#include "ReplayTimingFixture.hpp"
#include "ngm/CaptureService.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Inspection.hpp"
#include "ngm/ReplayTiming.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <unistd.h>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using namespace std::chrono_literals;
struct Scratch {
    fs::path path;
    Scratch() {
        auto pattern = (fs::temp_directory_path() / "ngm-timing-inspection-XXXXXX").string();
        require(mkdtemp(pattern.data()) != nullptr, "create timing inspection scratch");
        path = pattern;
    }
    ~Scratch() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
void write(const fs::path& path, std::string_view bytes) {
    if(fs::exists(path))
        fs::permissions(path, fs::perms::owner_write, fs::perm_options::add);
    std::ofstream output(path, std::ios::binary);
    output << bytes;
    output.close();
    require(output.good(), "write timing fixture: " + path.string());
}
std::string read(const fs::path& path) {
    return ngm::read_regular_file(path, 16U * 1024U * 1024U);
}
std::string hash(std::string_view bytes) {
    return ngm::sha256(std::as_bytes(std::span(bytes.data(), bytes.size())));
}
void replace_all(std::string& value, const std::string& from, const std::string& to) {
    for(std::size_t at = 0; (at = value.find(from, at)) != std::string::npos; at += to.size())
        value.replace(at, from.size(), to);
}
template <typename Function>
void rejected(Function function) {
    try {
        function();
    } catch(const ngm::InspectionError&) {
        return;
    } catch(const ngm::ArtifactError&) {
        return;
    } catch(const std::invalid_argument&) {
        return;
    }
    require(false, "unproved timing input must refuse");
}
void complete(ngm::CaptureService& service, const ngm::CaptureSubmission& submission) {
    const auto result = service.wait(submission.identity.job_id, 15s);
    require(result && result->state == ngm::JobState::succeeded && result->cleanup_confirmed &&
                !result->worker_running && !result->gpu_reserved && !result->finalization_pending,
            "synthetic timing collection completes with owned cleanup: " + (result ? result->error : "no result"));
}
struct Copy {
    fs::path directory;
    Json report, manifest, index, cpp, profile;
    void inputs() {
        for(auto& file : cpp.at("recorded_state_inputs").at("files")) {
            const auto bytes = read(directory / file.at("path").get<std::string>());
            file["bytes"] = bytes.size();
            file["sha256"] = hash(bytes);
        }
    }
    void tables() {
        for(auto& ref : profile.at("tables"))
            ref["sha256"] = hash(read(directory / ref.at("path").get<std::string>()));
    }
    void source() {
        const auto h = hash(read(directory / "raw/input/report.json"));
        for(auto* object : {&report, &manifest, &index})
            object->at("source")["report_sha256"] = h;
    }
};
using Edit = std::function<void(Copy&)>;
std::string clone(ngm::ArtifactStore& store, const std::string& source, Edit edit = {}, bool failed = false) {
    const auto lease = store.lease(source);
    const auto info = store.inspect(source);
    auto writer = store.begin(Json::object(), info.required_outputs, true);
    for(std::size_t offset = 0;;) {
        const auto page = store.files(source, offset, 100);
        for(const auto& file : page.files) {
            const auto destination = writer.directory() / file.path;
            fs::create_directories(destination.parent_path());
            fs::copy_file(lease.directory() / file.path, destination);
        }
        if(!page.next_offset)
            break;
        offset = *page.next_offset;
    }
    const auto load = [&](const std::string& path) {
        auto value = read(writer.directory() / path);
        replace_all(value, source, writer.id());
        return Json::parse(value);
    };
    auto provenance = info.provenance.dump();
    replace_all(provenance, source, writer.id());
    Copy copy{writer.directory(),
              load("raw/report.json"),
              Json::parse(provenance),
              load("derived/timing-analysis.json"),
              load("derived/cpp-project.json"),
              load("derived/replay-profile.json")};
    const auto repro_path = copy.profile.at("reproduction").at("path").get<std::string>();
    auto repro = read(writer.directory() / repro_path);
    replace_all(repro, source, writer.id());
    write(writer.directory() / repro_path, repro);
    if(edit)
        edit(copy);
    copy.profile["reproduction"]["sha256"] = hash(read(writer.directory() / repro_path));
    const auto cpp = copy.cpp.dump(), profile = copy.profile.dump();
    copy.index["cpp_index"]["sha256"] = hash(cpp);
    copy.index["profile_index"]["sha256"] = hash(profile);
    write(writer.directory() / "raw/report.json", copy.report.dump());
    write(writer.directory() / "derived/timing-analysis.json", copy.index.dump());
    write(writer.directory() / "derived/cpp-project.json", cpp);
    write(writer.directory() / "derived/replay-profile.json", profile);
    store.update_provenance(writer, std::move(copy.manifest));
    return (failed ? store.publish_failure(writer, "test-owned failure") : store.publish_success(writer)).summary.id;
}
void coverage(const Json& result, std::size_t available, std::size_t unavailable, std::size_t unsupported = 0) {
    require(result.at("coverage") ==
                Json{{"available", available}, {"unavailable", unavailable}, {"unsupported_analyses", unsupported}},
            "timing coverage differs: " + result.at("coverage").dump());
    require(result.dump().size() <= ngm::InspectionService::maximum_result_bytes, "bounded result");
    for(const auto& draw : result.at("draws"))
        if(draw.at("status") == "unavailable")
            require(draw.at("duration_ms").is_null() && draw.at("trace").is_null(),
                    "refused association must not leak a timing");
}
} // namespace
int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "Nsight process stand-in required");
        Scratch scratch;
        ngm::CaptureServiceOptions options;
        options.nsight_root = scratch.path / "installation";
        options.artifacts.root = scratch.path / "store";
        options.environment = {{"NGM_TARGET_RECORD", (scratch.path / "targets.txt").string()},
                               {"NGM_STANDIN_CPP_MODE", "timing-analysis"},
                               {"NGM_STANDIN_PROFILE_MODE", "timing-analysis"}};
        const auto install = *options.nsight_root / "host/linux-desktop-nomad-x64";
        fs::create_directories(install);
        for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay", "ngfx-replay.bin"})
            fs::copy_file(fs::absolute(argv[1]), install / name);
        const auto target = scratch.path / "target with spaces";
        fs::copy_file(fs::absolute(argv[1]), target);
        std::vector<std::string> ids;
        std::string single, source;
        Json stable;
        {
            ngm::CaptureService service(options);
            ngm::CaptureRequest capture;
            capture.executable = target;
            capture.working_directory = scratch.path;
            capture.capture_frame = 7;
            capture.frame_count = 3;
            capture.timeout = 10s;
            capture.pin = true;
            const auto original = service.capture(capture);
            complete(service, original);
            source = original.artifact_id;
            ngm::ReplayTimingRequest request;
            request.capture_id = source;
            request.architecture = "Ampere GA10x";
            request.timeout = 10s;
            request.pin = true;
            for(std::size_t i = 0; i < 3; ++i) {
                request.interval_index = i;
                const auto job = service.analyze_timing(request);
                complete(service, job);
                ids.push_back(job.artifact_id);
            }
            capture.frame_count = 1;
            const auto one = service.capture(capture);
            complete(service, one);
            request.capture_id = one.artifact_id;
            request.interval_index = 0;
            const auto analysis = service.analyze_timing(request);
            complete(service, analysis);
            single = analysis.artifact_id;
            auto& store = service.artifacts();
            ngm::ReplayTimingInspection reader(store);
            stable = reader.draws(ids, 0, 100);
            coverage(stable, 12, 0);
            coverage(reader.draws({single}), 4, 0);
            const std::array expected{1.25, 2.5, 3.75, 4.5};
            for(std::size_t i = 0; i < 12; ++i)
                require(stable.at("draws")[i].at("duration_ms") == expected[i % 4] &&
                            stable.at("draws")[i].at("interval_index") == i / 4 &&
                            stable.at("draws")[i].at("selection").at("submission_index") == i % 4 / 2 &&
                            stable.at("draws")[i].at("original_capture_event_id").is_null(),
                        "source paths preserve explicit draw/submission/interval selections and exact raw times");
            require(stable == reader.draws({ids[2], ids[0], ids[1]}, 0, 100), "caller order cannot change identity");
            Json paged = Json::array();
            for(std::size_t offset = 0;;) {
                const auto page = reader.draws(ids, offset, 3);
                for(const auto& value : page.at("draws"))
                    paged.push_back(value);
                if(page.at("next_offset").is_null())
                    break;
                offset = page.at("next_offset");
            }
            require(paged == stable.at("draws") && reader.draws(ids, 30000).at("draws").empty(),
                    "paging preserves all stable timing references and bounded past-end behavior");
            for(const auto& invalid : std::vector<std::vector<std::string>>{
                    {}, {ids[0]}, {ids[0], ids[1]}, {ids[0], ids[0], ids[2]}, {ids[0], single, ids[2]}, {source}})
                rejected([&] { reader.draws(invalid); });
            rejected([&] { reader.draws(ids, 30001); });
            rejected([&] { reader.draws(ids, 0, 0); });
            rejected([&] { reader.draws(ids, 0, 101); });
            const auto copy = clone(store, ids[1]);
            const auto with_copy = reader.draws({ids[0], copy, ids[2]});
            coverage(with_copy, 12, 0);
            require(with_copy.at("group_sha256") != stable.at("group_sha256") &&
                        with_copy.at("draws")[0].at("timing_ref") != stable.at("draws")[0].at("timing_ref"),
                    "replacing group evidence changes every group's stable reference");
            const auto repeated = clone(store, ids[1], [](Copy& c) {
                const auto project = c.cpp.at("project_directory").get<std::string>();
                for(const auto& path :
                    {project + "/Frame0Part00.cpp", std::string("raw/profile/BASE_UNLOCKED/D3DPERF_EVENTS.xls"),
                     std::string("raw/profile/BASE_UNLOCKED/GPUTRACE_REGIMES.xls")}) {
                    auto bytes = read(c.directory / path);
                    replace_all(bytes, "interval.1", "interval.0");
                    write(c.directory / path, bytes);
                }
                c.inputs();
                c.tables();
            });
            coverage(reader.draws({ids[0], repeated, ids[2]}), 4, 8);
            const auto regime_only = clone(store, ids[1], [](Copy& c) {
                const auto path = c.directory / "raw/profile/BASE_UNLOCKED/GPUTRACE_REGIMES.xls";
                write(path, read(path) + "interval.0.a/leaf.direct\t0\n");
                c.tables();
            });
            coverage(reader.draws({ids[0], regime_only, ids[2]}), 11, 1);
            const auto unknown = clone(store, ids[1], [](Copy& c) {
                const auto path = c.directory / c.cpp.at("project_directory").get<std::string>() / "CommandList00.cpp";
                auto bytes = read(path);
                replace_all(bytes, "VulkanReplay_CmdDraw(", "VulkanReplay_CmdUnknown(");
                write(path, bytes);
                c.inputs();
            });
            coverage(reader.draws({ids[0], unknown, ids[2]}), 0, 8, 1);
            const auto hierarchy = clone(store, ids[1], [](Copy& c) {
                const auto path = c.directory / "raw/profile/BASE_UNLOCKED/D3DPERF_EVENTS.xls";
                auto bytes = read(path);
                replace_all(bytes, "        leaf.direct", "    leaf.direct");
                write(path, bytes);
                c.tables();
            });
            coverage(reader.draws({ids[0], hierarchy, ids[2]}), 0, 12, 1);
            for(const auto& edit : std::vector<Edit>{
                    [](Copy& c) { c.manifest["timing_settings"]["duration_ms"] = 1; },
                    [](Copy& c) { c.index["source"]["original_capture_sha256"] = std::string(64, '0'); },
                    [](Copy& c) { c.report["cpp_capture"]["arguments"][7] = "--wait-frames=999"; },
                    [](Copy& c) {
                        for(auto* v : {&c.report, &c.index})
                            v->at("replay_target")["sha256_after"] = std::string(64, '0');
                    },
                    [](Copy& c) {
                        const auto path = c.directory / "raw/input/capture.ngfx-capture";
                        write(path, "modified capture-time input");
                    },
                    [](Copy& c) {
                        const auto path = c.directory / "raw/input/report.json";
                        auto original_report = Json::parse(read(path));
                        original_report.at("capture").erase("sha256");
                        write(path, original_report.dump());
                        c.source();
                    },
                    [](Copy& c) {
                        const auto path = c.directory / c.profile.at("reproduction").at("path").get<std::string>();
                        auto bytes = read(path);
                        replace_all(bytes, "Time Every Action\tYes", "Time Every Action\tNo");
                        write(path, bytes);
                        c.profile["settings"]["Time Every Action"] = "No";
                    },
                    [](Copy& c) {
                        const auto path = c.directory / c.cpp.at("project_directory").get<std::string>() / "data.bin";
                        write(path, "tampered serialized input");
                    }}) {
                const auto changed = clone(store, single, edit);
                rejected([&] { reader.draws({changed}); });
                require(!store.inspect(changed).summary.in_use, "refusal releases analysis leases");
            }
            const auto tampered = clone(store, single);
            const auto path = store.lease(tampered).directory() / "derived/cpp-project.json";
            auto bytes = read(path);
            replace_all(bytes, "vulkan", "vulkar");
            write(path, bytes);
            rejected([&] { reader.draws({tampered}); });
            const auto failed = clone(store, single, {}, true);
            rejected([&] { reader.draws({failed}); });
            for(const auto& id : ids)
                require(store.inspect(id).summary.pinned && !store.inspect(id).summary.in_use,
                        "completed reads retain persistent pins and release leases");
            require(service.shutdown(2s).cleanup_confirmed, "test service shuts down");
        }
        ngm::ArtifactStore reopened(options.artifacts);
        require(ngm::ReplayTimingInspection(reopened).draws(ids, 0, 100) == stable,
                "restart preserves source hashes, interval identity and timing references");
        for(const auto& id : ids)
            require(reopened.inspect(id).summary.pinned, "source pin survives restart");
    });
}
