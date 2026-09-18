#include "CaptureValidation.hpp"
#include "Check.hpp"
#include "McpClient.hpp"
#include "ngm/Experiment.hpp"
#include "ngm/File.hpp"
#include "ngm/Hash.hpp"
#include "ngm/Version.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
using ngm::check::require;
using ngm::check::mcp::Client;
using ngm::check::mcp::ClientTimeouts;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

constexpr std::uint32_t seed = 42;
constexpr std::uint32_t width = 192;
constexpr std::uint32_t height = 128;
constexpr std::uint32_t baseline_frame = 2;
constexpr std::uint32_t capture_frame = 2;
constexpr std::uint32_t application_final_frame = 20;
constexpr auto capture_timeout = 120s;
constexpr auto job_timeout = 180s;
constexpr std::size_t maximum_file_bytes = 16U * 1024U * 1024U;

void save_json(const fs::path& path, const Json& value) {
    const auto temporary = fs::path(path.string() + ".tmp");
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << value.dump(2) << '\n';
    output.close();
    fs::rename(temporary, path);
}

void save_text(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool contains_path(const fs::path& parent, const fs::path& child) {
    return std::mismatch(parent.begin(), parent.end(), child.begin(), child.end()).first == parent.end();
}

fs::path allocate(const fs::path& root) {
    fs::create_directories(root);
    auto pattern = (root / "capture-validation-XXXXXX").string();
    const auto* directory = mkdtemp(pattern.data());
    require(directory != nullptr, "allocate isolated capture validation directory");
    return directory;
}

Json observation(const std::string& status, const std::string& reason) {
    return {{"status", status}, {"reason", reason}};
}

struct Report {
    fs::path directory;
    Json value;

    void save() const {
        save_json(directory / "report.json", value);
    }
    void transcript(const Json& event) const {
        std::ofstream output(directory / "mcp-transcript.ndjson", std::ios::binary | std::ios::app);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << event.dump() << '\n';
    }
    void failure(const std::string& step, const std::string& reason) {
        value["failures"].push_back({{"step", step}, {"reason", reason}});
        value["status"] = "fail";
        save();
    }
};

// Keep complete requests/responses on disk, including tool errors. The compact
// report records outcomes and references rather than duplicating every poll.
class Session {
public:
    Session(Report& report, std::string name, const fs::path& server, const std::vector<std::string>& options,
            const std::vector<std::string>& environment) :
        report_(report), name_(std::move(name)),
        client_(server.string(), "/usr/bin:/bin", options, false, environment, ClientTimeouts{30s, 30s}) {
        report_.value["sessions"][name_] = observation("running", "Server started; EOF cleanup not yet observed");
        report_.save();
    }
    ~Session() {
        if(!closed_) {
            try {
                close();
            } catch(...) {
                // Client still requests EOF and bounds its fallback cleanup.
            }
        }
    }
    Json request(const char* method, const Json& params = Json::object()) {
        const auto id = next_id_++;
        report_.transcript({{"session", name_},
                            {"direction", "request"},
                            {"message", {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}}}});
        const auto response = client_.request(id, method, params);
        report_.transcript({{"session", name_}, {"direction", "response"}, {"message", response}});
        return response;
    }
    void initialize() {
        const auto response = request(
            "initialize", {{"protocolVersion", "2025-11-25"},
                           {"capabilities", Json::object()},
                           {"clientInfo", {{"name", "ngm-capture-integration"}, {"version", ngm::project_version()}}}});
        require(response.contains("result"), "MCP initialization succeeds");
        const auto& result = response.at("result");
        require(result.at("serverInfo").at("name") == "nsight-graphics-mcp" &&
                    result.at("serverInfo").at("version") == ngm::project_version(),
                "server identity matches the harness project version");
        require(result.at("protocolVersion") == "2025-11-25", "selected MCP protocol revision");
        const Json notification{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}};
        report_.transcript({{"session", name_}, {"direction", "request"}, {"message", notification}});
        client_.send(notification);
        report_.value["sessions"][name_]["initialize"] = result;
        report_.save();
    }
    Json tool(const char* name, const Json& arguments = Json::object()) {
        const auto response = request("tools/call", {{"name", name}, {"arguments", arguments}});
        require(response.contains("result") && !response.at("result").value("isError", false),
                std::string(name) + " succeeds: " + response.dump());
        const auto& result = response.at("result");
        require(result.contains("structuredContent") && result.at("structuredContent").is_object(),
                "tool supplies structured content");
        require(result.at("content").size() == 1 && result.at("content").at(0).at("type") == "text" &&
                    Json::parse(result.at("content").at(0).at("text").get<std::string>()) ==
                        result.at("structuredContent"),
                "structured and text MCP results agree");
        return result.at("structuredContent");
    }
    void close() {
        if(closed_) {
            return;
        }
        closed_ = true;
        auto& result = report_.value["sessions"][name_];
        try {
            result["exit_code"] = client_.finish();
            require(result.at("exit_code") == 0, "server exits normally after EOF cleanup");
            result["status"] = "pass";
            result["reason"] = "EOF, drained protocol output, and normal server exit observed";
        } catch(const std::exception& error) {
            result["status"] = "fail";
            result["reason"] = error.what();
            save_text(report_.directory / (name_ + ".stderr.log"), client_.diagnostics());
            report_.save();
            throw;
        }
        save_text(report_.directory / (name_ + ".stderr.log"), client_.diagnostics());
        report_.save();
    }

private:
    Report& report_;
    std::string name_;
    Client client_;
    int next_id_ = 1;
    bool closed_ = false;
};

std::vector<std::string> desktop_environment(Json& recorded) {
    std::vector<std::string> result{"LANG=C", "LC_ALL=C"};
    recorded = {{"PATH", "/usr/bin:/bin"}, {"LANG", "C"}, {"LC_ALL", "C"}};
    for(const auto* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE",
                            "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP", "DBUS_SESSION_BUS_ADDRESS"}) {
        if(const auto* value = std::getenv(name); value && *value) {
            result.emplace_back(std::string(name) + '=' + value);
            recorded[name] = value;
        }
    }
    if(!recorded.contains("XAUTHORITY")) {
        if(const auto* home_directory = std::getenv("HOME")) {
            const auto authority = fs::path(home_directory) / ".Xauthority";
            if(fs::is_regular_file(authority)) {
                const auto path = fs::absolute(authority).string();
                result.emplace_back("XAUTHORITY=" + path);
                recorded["XAUTHORITY"] = path;
            }
        }
    }
    // Omission exercises the capture service's system XDG data-search default.
    // No inherited overlay, Vulkan-loader or Nsight injection variables enter.
    return result;
}

Json files(Session& session, const std::string& id) {
    Json result = Json::array();
    std::size_t offset = 0;
    do {
        const auto page = session.tool("artifact_files", {{"artifact_id", id}, {"offset", offset}, {"limit", 100}});
        for(const auto& file : page.at("files")) {
            result.push_back(file);
        }
        require(result.size() <= 10000, "bounded published file inventory");
        if(page.at("next_offset").is_null()) {
            return result;
        }
        const auto next = page.at("next_offset").get<std::size_t>();
        require(next > offset, "file inventory pagination advances");
        offset = next;
    } while(true);
}

struct RetainedFile {
    std::string bytes;
    Json reference;
};

RetainedFile read_file(Session& session, const fs::path& store, const std::string& id, const std::string& path,
                       bool reference_only = false) {
    require(id.size() == 39 && id.starts_with("bundle-") &&
                id.find_first_not_of("0123456789abcdef", 7) == std::string::npos,
            "server returned a scoped artifact ID");
    const fs::path relative(path);
    require(!relative.empty() && !relative.is_absolute() && relative.lexically_normal() == relative,
            "normalized relative artifact path");
    for(const auto& component : relative) {
        require(component != "." && component != ".." && !component.empty(), "contained artifact path");
    }
    const auto info = session.tool("artifact_info", {{"artifact_id", id}});
    require(info.at("pinned") == true && info.at("quarantined") == false &&
                (info.at("status") == "complete" || info.at("status") == "failed"),
            "only read published, pinned validation evidence");
    const auto response =
        session.tool("artifact_read", {{"artifact_id", id}, {"path", path}, {"max_bytes", reference_only ? 1 : 65536}});
    const auto size = response.at("bytes").get<std::size_t>();
    require(size <= maximum_file_bytes, "evidence file fits the harness read bound");
    require(response.at("artifact_id") == id && response.at("path") == path, "artifact read identity matches");
    const auto expected = store / "bundles" / id / relative;
    const auto local = fs::path(response.at("local_path").get<std::string>());
    require(local == expected && fs::canonical(local) == expected, "published file reference stays within the bundle");
    auto bytes = response.at("status") == "text" ? response.at("text").get<std::string>()
                                                 : ngm::read_regular_file(local, maximum_file_bytes);
    require(bytes.size() == size, "read bytes match the published file inventory");
    Json reference{{"artifact_id", id},
                   {"path", path},
                   {"bytes", size},
                   {"sha256", ngm::sha256(std::as_bytes(std::span(bytes)))},
                   {"transport", response.at("status")},
                   {"read_origin", response.at("status") == "text" ? "mcp_text" : "bounded_local_reference"}};
    return {std::move(bytes), std::move(reference)};
}

Json await_job(Session& session, Report& report, std::size_t index) {
    auto& capture = report.value["captures"][index];
    const auto submission = capture.at("submission");
    const auto id = submission.at("identity").at("job_id").get<std::string>();
    const auto deadline = Clock::now() + job_timeout;
    while(Clock::now() < deadline) {
        const auto job = session.tool("job_status", {{"job_id", id}});
        capture["job"] = job;
        report.save();
        require(job.at("identity") == submission.at("identity"),
                "job polling preserves exact capture/attempt identity");
        const auto state = job.at("state").get<std::string>();
        if(state != "queued" && state != "running" && job.at("worker_running") == false &&
           job.at("finalization_pending") == false) {
            return job;
        }
        std::this_thread::sleep_for(200ms);
    }
    // Cancellation is recorded too; EOF remains the final cleanup request if
    // this call fails or the server cannot complete the job before our deadline.
    session.tool("job_cancel", {{"job_id", id}});
    throw std::runtime_error("Capture did not finish cleanup/publication within the 180-second polling bound");
}

Json target_pid(const RetainedFile& file) {
    const auto connection = file.bytes.find("Connection Established:");
    if(connection == std::string::npos) {
        return observation("unsupported", "Nsight capture stdout has no Connection Established PID marker");
    }
    const auto line_end = file.bytes.find('\n', connection);
    const auto begin = file.bytes.find("(pid: ", connection);
    const auto end = begin == std::string::npos ? std::string::npos : file.bytes.find(')', begin);
    if(begin == std::string::npos || end == std::string::npos || (line_end != std::string::npos && end > line_end)) {
        return observation("unsupported", "Nsight connection line has no complete PID marker");
    }
    const auto pid = file.bytes.substr(begin + 6, end - begin - 6);
    require(!pid.empty() && pid.size() <= 10 && pid.find_first_not_of("0123456789") == std::string::npos &&
                std::stoull(pid) > 0 && std::stoull(pid) <= 2147483647,
            "Nsight target PID marker is a positive process ID");
    return {{"status", "pass"},
            {"pid", pid},
            {"source", file.reference},
            {"marker", file.bytes.substr(connection, end - connection + 1)},
            {"scope", "Target PID observed in this capture's retained Nsight stdout; not a process-liveness claim"}};
}

void validate_png(const std::string& bytes) {
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    require(bytes.size() >= 24 &&
                std::equal(signature.begin(), signature.end(), bytes.begin(),
                           [](unsigned char a, char b) { return a == static_cast<unsigned char>(b); }) &&
                bytes.substr(12, 4) == "IHDR",
            "screenshot is a PNG with an IHDR header");
    const auto dimension = [&](std::size_t offset) {
        std::uint32_t value = 0;
        for(std::size_t index = offset; index < offset + 4; ++index) {
            value = value * 256 + static_cast<unsigned char>(bytes[index]);
        }
        return value;
    };
    require(dimension(16) == width && dimension(20) == height, "screenshot dimensions match the selected workload");
}

void inspect_capture(Session& session, Report& report, const fs::path& store, std::size_t index,
                     const std::string& fixture_hash, const Json& baseline_shader) {
    auto& capture = report.value["captures"][index];
    const auto id = capture.at("submission").at("artifact_id").get<std::string>();
    capture["artifact"] = session.tool("artifact_info", {{"artifact_id", id}});
    capture["files"] = files(session, id);
    const auto raw_report = read_file(session, store, id, "raw/report.json");
    const auto source = Json::parse(raw_report.bytes);
    capture["report_reference"] = raw_report.reference;
    capture["capture_report"] = source;
    report.save();
    require(capture.at("job").at("cleanup_confirmed") == true && capture.at("job").at("gpu_reserved") == false,
            "capture cleanup releases the GPU reservation");
    const auto state = capture.at("job").at("state").get<std::string>();
    const auto ready = source.contains("nsight")
                           ? std::optional<bool>(source.at("nsight").at("interface_ready").get<bool>())
                           : std::nullopt;
    const auto disposition = ngm::check::capture::classify_job(state, ready);
    if(disposition == ngm::check::capture::JobDisposition::unsupported) {
        capture["status"] = "unsupported";
        capture["reason"] = source.at("nsight").at("problem");
        report.save();
        return;
    }
    require(disposition == ngm::check::capture::JobDisposition::inspect,
            "capture job failed validation (" + state + "): " + capture.at("job").value("error", std::string()));
    require(capture.at("artifact").at("status") == "complete" && source.at("readable_capture") == true,
            "matching Nsight tools read the completed capture");
    require(source.at("application").at("sha256_before_launch") == fixture_hash &&
                source.at("application").at("sha256_after_launch") == fixture_hash,
            "capture observes the isolated fixture executable identity");
    require(source.at("capture_settings").at("capture_frame") == capture_frame &&
                source.at("capture_settings").at("frame_count") == 1,
            "capture uses the requested single frame");
    const auto stdout_file = read_file(session, store, id, source.at("capture").at("stdout").get<std::string>(), true);
    capture["target_process"] = target_pid(stdout_file);
    const auto shader = read_file(session, store, id, "raw/application/shaders/provenance.json");
    require(Json::parse(shader.bytes) == baseline_shader, "capture and baseline retain the same shader build manifest");
    capture["shader_provenance"] = shader.reference;
    std::set<std::string> expected{"metadata", "functions", "objects", "logs", "screenshot"};
    capture["exports"] = Json::array();
    for(const auto& exported : source.at("exports")) {
        const auto kind = exported.at("kind").get<std::string>();
        require(expected.erase(kind) == 1, "each expected export appears exactly once");
        Json result{{"kind", kind}, {"operation", exported}};
        if(exported.at("outcome") == "unavailable") {
            result["status"] = "unsupported";
        } else {
            result["status"] = exported.at("outcome") == "success" ? "pass" : "fail";
        }
        capture["exports"].push_back(result);
        report.save();
        if(result.at("status") != "pass") {
            continue;
        }
        // Logs may contain NUL separators; images are binary. Retrieve their
        // references through MCP and inspect bounded pinned files locally.
        const auto evidence = read_file(session, store, id, exported.at("output").get<std::string>(),
                                        kind == "screenshot" || kind == "logs");
        require(ngm::check::capture::valid_export_size(kind, evidence.bytes.size()),
                "successful non-log export contains retained evidence");
        capture["exports"].back()["reference"] = evidence.reference;
        if(kind == "screenshot") {
            validate_png(evidence.bytes);
            capture["screenshot"] = evidence.reference;
        }
    }
    require(expected.empty(), "all five export outcomes are recorded");
    capture["status"] = "pass";
    for(const auto& exported : capture.at("exports")) {
        if(exported.at("status") == "fail") {
            capture["status"] = "fail";
            break;
        }
        if(exported.at("status") == "unsupported") {
            capture["status"] = "unsupported";
        }
    }
    capture["reason"] =
        "Job, artifact publication, matching exports, MCP evidence reads and retained identities checked";
    report.save();
}

void compare_captures(Report& report) {
    const auto& captures = report.value.at("captures");
    auto& checks = report.value["checks"];
    std::set<std::string> pids;
    bool all_pids = true;
    bool unique_pids = true;
    for(const auto& capture : captures) {
        if(!capture.contains("target_process") || capture.at("target_process").at("status") != "pass") {
            all_pids = false;
        } else if(!pids.insert(capture.at("target_process").at("pid").get<std::string>()).second) {
            unique_pids = false;
        }
    }
    checks["fresh_target_pids"] = observation(!all_pids     ? "unsupported"
                                              : unique_pids ? "pass"
                                                            : "fail",
                                              "Compare target PID markers from each retained Nsight capture stdout");
    if(captures.at(0).contains("screenshot") && captures.at(1).contains("screenshot")) {
        checks["repeat_reference_png"] = observation(
            captures.at(0).at("screenshot").at("sha256") == captures.at(1).at("screenshot").at("sha256") ? "pass"
                                                                                                         : "fail",
            "SHA-256 comparison of two reference PNG exports from fresh launches with identical inputs");
    }
    if(captures.at(0).contains("screenshot") && captures.at(2).contains("screenshot")) {
        checks["different_scenario_png"] = observation(
            captures.at(0).at("screenshot").at("sha256") != captures.at(2).at("screenshot").at("sha256") ? "pass"
                                                                                                         : "fail",
            "Reference and shader-error PNG files differ; scenario comparison does not verify a source repair");
    }
    report.save();
}

std::string overall_status(const Report& report) {
    if(!report.value.at("failures").empty()) {
        return "fail";
    }
    bool unsupported = false;
    for(const auto& capture : report.value.at("captures")) {
        const auto status = capture.at("status").get<std::string>();
        if(status == "fail" || status == "running") {
            return "fail";
        }
        unsupported |= status == "unsupported" || status == "skipped";
    }
    for(const auto* name :
        {"baseline", "fresh_target_pids", "repeat_reference_png", "different_scenario_png", "restart_pins"}) {
        const auto status = report.value.at("checks").at(name).at("status").get<std::string>();
        if(status == "fail") {
            return "fail";
        }
        unsupported |= status != "pass";
    }
    return unsupported ? "unsupported" : "pass";
}

int run(const fs::path& server, const fs::path& fixture, const fs::path& installation, const fs::path& store,
        const fs::path& output) {
    const auto directory = allocate(output);
    fs::create_directory(directory / "report");
    Report report{
        directory / "report",
        {{"schema_version", 1},
         {"project_version", ngm::project_version()},
         {"status", "running"},
         {"evidence_origin", "hardware_capture_mcp_validation"},
         {"directory", directory.string()},
         {"artifact_root", store.string()},
         {"nsight_root", installation.string()},
         {"inputs",
          {{"seed", seed},
           {"width", width},
           {"height", height},
           {"baseline_application_frame", baseline_frame},
           {"capture_frame", capture_frame},
           {"capture_application_final_frame", application_final_frame},
           {"capture_timeout_ms", std::chrono::duration_cast<std::chrono::milliseconds>(capture_timeout).count()},
           {"job_poll_timeout_ms", std::chrono::duration_cast<std::chrono::milliseconds>(job_timeout).count()}}},
         {"captures", Json::array()},
         {"failures", Json::array()},
         {"sessions", Json::object()},
         {"checks", Json::object()}}};
    for(const auto* scenario : {"reference", "reference", "shader-error"}) {
        report.value["captures"].push_back(
            {{"scenario", scenario}, {"status", "skipped"}, {"reason", "Not attempted"}});
    }
    for(const auto* name :
        {"baseline", "fresh_target_pids", "repeat_reference_png", "different_scenario_png", "restart_pins"}) {
        report.value["checks"][name] = observation("skipped", "Required evidence not yet available");
    }
    report.value["checks"]["cross_origin_pixels"] =
        observation("skipped", "PNG pixels are not decoded by this harness; app frame 2 and capture frame 2 remain "
                               "explicitly distinct selectors");
    report.value["checks"]["source_edit_rebuild_recapture"] =
        observation("skipped", "This capture slice does not perform source repair");
    report.value["checks"]["gpu_replay"] =
        observation("skipped", "Metadata export and a saved screenshot do not execute GPU replay");
    report.save();

    std::unique_ptr<Session> session;
    std::vector<std::string> environment;
    const std::vector<std::string> options{"--artifact-root", store.string(), "--nsight-root", installation.string()};
    std::vector<std::string> retained;
    fs::path executed_server = server;
    try {
        fs::create_directory(directory / "inputs");
        executed_server = directory / "inputs/nsight-graphics-mcp";
        const auto executed_fixture = directory / "inputs/ngm-vulkan-fixture";
        fs::copy_file(server, executed_server);
        fs::copy_file(fixture, executed_fixture);
        fs::permissions(executed_server, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
        fs::permissions(executed_fixture, fs::perms::owner_read | fs::perms::owner_exec, fs::perm_options::replace);
        const auto fixture_hash = ngm::sha256_file(executed_fixture);
        report.value["build_identity"] = {{"harness_sha256", ngm::sha256_file("/proc/self/exe")},
                                          {"harness_compiler", __VERSION__},
                                          {"server",
                                           {{"source_path", server.string()},
                                            {"executed_path", executed_server.string()},
                                            {"sha256", ngm::sha256_file(executed_server)}}},
                                          {"fixture",
                                           {{"source_path", fixture.string()},
                                            {"executed_path", executed_fixture.string()},
                                            {"sha256", fixture_hash}}}};
        environment = desktop_environment(report.value["server_environment"]);
        report.value["capture_environment_policy"] =
            "Server starts without XDG_DATA_DIRS to exercise production defaults; raw capture reports retain effective "
            "worker environments";
        report.save();
        session = std::make_unique<Session>(report, "capture-session", executed_server, options, environment);
        session->initialize();
        report.value["capabilities"] = session->tool("capabilities");
        report.save();

        ngm::ExperimentOptions baseline_options;
        baseline_options.fixture = executed_fixture;
        baseline_options.output_root = directory / "baselines";
        baseline_options.scenario = "reference";
        baseline_options.seed = seed;
        baseline_options.width = width;
        baseline_options.height = height;
        baseline_options.frame = baseline_frame;
        const auto baseline = ngm::run_experiment(baseline_options);
        report.value["baseline"] = {{"directory", baseline.directory.string()}, {"report", baseline.report}};
        const auto baseline_status = baseline.report.at("status").get<std::string>();
        report.value["checks"]["baseline"] = observation(
            baseline_status == "pass" || baseline_status == "unsupported" ? baseline_status : "fail",
            "Fresh standalone application readback, with its independent GPU/driver/desktop/build/shader provenance");
        report.save();
        const auto imported = session->tool(
            "artifact_import",
            {{"source", baseline.directory.string()}, {"pin", true}, {"required_outputs", {"report.json"}}});
        report.value["baseline"]["artifact"] = imported;
        retained.push_back(imported.at("id").get<std::string>());
        require(imported.at("pinned") == true, "standalone baseline, including failures, is pinned");
        report.save();
        if(baseline_status == "pass") {
            const auto shaders = baseline.directory / "output/shaders";
            const auto baseline_shader =
                Json::parse(ngm::read_regular_file(shaders / "provenance.json", maximum_file_bytes));
            for(std::size_t index = 0; index < report.value.at("captures").size(); ++index) {
                auto& capture = report.value["captures"][index];
                capture["status"] = "running";
                capture["reason"] = "Capture submission pending";
                report.save();
                try {
                    const auto working = directory / ("capture-" + std::to_string(index));
                    fs::create_directory(working);
                    const auto arguments =
                        Json{{"executable", executed_fixture.string()},
                             {"working_directory", working.string()},
                             {"arguments",
                              {"--scenario", capture.at("scenario"), "--seed", std::to_string(seed), "--width",
                               std::to_string(width), "--height", std::to_string(height), "--frame",
                               std::to_string(application_final_frame), "--shader-dir", shaders.string()}},
                             {"capture_frame", capture_frame},
                             {"timeout_ms", 120000},
                             {"application_output_option", "--output"},
                             {"pin", true}};
                    capture["arguments"] = arguments;
                    report.save();
                    capture["submission"] = session->tool("capture", arguments);
                    retained.push_back(capture.at("submission").at("artifact_id").get<std::string>());
                    report.save();
                    await_job(*session, report, index);
                    inspect_capture(*session, report, store, index, fixture_hash, baseline_shader);
                } catch(const std::exception& error) {
                    capture["status"] = "fail";
                    capture["reason"] = error.what();
                    report.failure("capture-" + std::to_string(index), error.what());
                    // Stop submissions after a failure. EOF below requests
                    // cleanup, then restart inspection retains any failed bundle.
                    break;
                }
            }
        } else {
            for(auto& capture : report.value["captures"]) {
                capture["reason"] = "Standalone baseline did not pass; no capture attempted";
            }
        }
        compare_captures(report);
    } catch(const std::exception& error) {
        report.failure("capture-workflow", error.what());
    }
    if(session) {
        try {
            session->close();
        } catch(const std::exception& error) {
            report.failure("capture-session-eof", error.what());
        }
        session.reset();
    }

    // A new process must rediscover durable pins after the capture session has
    // exited. Inspect failed captures as well; never prune validation evidence.
    try {
        Session restarted(report, "restart-session", executed_server, options, environment);
        restarted.initialize();
        report.value["retained_after_restart"] = Json::array();
        bool pins_pass = !retained.empty();
        for(const auto& id : retained) {
            try {
                const auto info = restarted.tool("artifact_info", {{"artifact_id", id}});
                report.value["retained_after_restart"].push_back(info);
                pins_pass &= info.at("pinned") == true && info.at("quarantined") == false &&
                             (info.at("status") == "complete" || info.at("status") == "failed");
                report.save();
            } catch(const std::exception& error) {
                pins_pass = false;
                report.value["retained_after_restart"].push_back({{"id", id}, {"error", error.what()}});
                report.save();
            }
        }
        report.value["checks"]["restart_pins"] = observation(
            pins_pass ? "pass" : "fail", "Pinned baseline and every submitted capture inspected after server restart");
        restarted.close();
    } catch(const std::exception& error) {
        report.failure("restart-inspection", error.what());
    }
    report.value["status"] = overall_status(report);
    report.value["report_publication"] = observation(
        "skipped",
        "This immutable snapshot precedes its own import; publication-receipt.json records the import outcome and ID");
    report.save();

    // Import a frozen copy so the server cannot copy a transcript/report that
    // changes during its own artifact_import request. The outer receipt records
    // this self-import's ID and EOF result; the bundle contains the complete
    // capture/restart transcript and their final statuses.
    try {
        const auto frozen = directory / "publication";
        fs::copy(report.directory, frozen, fs::copy_options::recursive);
        Session publisher(report, "publication-session", executed_server, options, environment);
        publisher.initialize();
        const auto imported =
            publisher.tool("artifact_import", {{"source", frozen.string()},
                                               {"pin", true},
                                               {"required_outputs", {"report.json", "mcp-transcript.ndjson"}}});
        report.value["report_publication"] = {{"status", "pass"}, {"artifact", imported}};
        report.save();
        require(imported.at("pinned") == true, "validation report import is durably pinned");
        publisher.close();
    } catch(const std::exception& error) {
        report.value["report_publication"]["status"] = "fail";
        report.value["report_publication"]["reason"] = error.what();
        report.failure("report-publication", error.what());
    }
    save_json(directory / "publication-receipt.json",
              {{"publication", report.value.at("report_publication")},
               {"report_status", report.value.at("status")},
               {"session", report.value.at("sessions").value("publication-session", Json(nullptr))}});
    report.save();
    std::cout << Json{{"status", report.value.at("status")},
                      {"report", (report.directory / "report.json").string()},
                      {"publication", report.value.at("report_publication")},
                      {"retained_artifact_ids", retained}}
                     .dump(2)
              << '\n';
    return report.value.at("status") == "pass" ? 0 : report.value.at("status") == "unsupported" ? 3 : 1;
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    try {
        require(argc == 6, "usage: ngm_capture_integration SERVER FIXTURE NSIGHT_ROOT ARTIFACT_ROOT OUTPUT_ROOT");
        const auto server = fs::canonical(argv[1]);
        const auto fixture = fs::canonical(argv[2]);
        const auto installation = fs::canonical(argv[3]);
        const auto store = fs::weakly_canonical(fs::absolute(argv[4]));
        const auto output = fs::weakly_canonical(fs::absolute(argv[5]));
        require(fs::is_regular_file(server) && access(server.c_str(), X_OK) == 0 && fs::is_regular_file(fixture) &&
                    access(fixture.c_str(), X_OK) == 0,
                "server and fixture must be executable regular files");
        require(fs::is_directory(installation), "Nsight root must be an existing installation directory");
        require(!contains_path(store, output) && !contains_path(output, store),
                "artifact and output roots must not overlap; reports are imported from outside the managed store");
        return run(server, fixture, installation, store, output);
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
