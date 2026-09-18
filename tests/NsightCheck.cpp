#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/Nsight.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <signal.h>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace {
using ngm::check::require;
using namespace std::chrono_literals;

class Scratch {
public:
    Scratch() {
        auto pattern = (std::filesystem::temp_directory_path() / "ngm nsight check XXXXXX").string();
        const auto directory = mkdtemp(pattern.data());
        require(directory != nullptr, "create scratch directory");
        path = directory;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

void copy_executable(const std::filesystem::path& source, const std::filesystem::path& destination) {
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(source, destination);
    std::filesystem::permissions(destination, std::filesystem::perms::owner_all);
}

ngm::NsightRunContext context(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    ngm::NsightRunContext result;
    result.log_directory = directory;
    result.timeout = 3s;
    result.terminate_grace = 25ms;
    return result;
}

ngm::NsightInstallation inspect(const std::filesystem::path& installation, const std::filesystem::path& directory,
                                const std::map<std::string, std::string>& environment = {}) {
    ngm::NsightInspectionOptions options;
    options.installation_root = installation;
    options.context = context(directory);
    options.context.environment = environment;
    return ngm::inspect_nsight(options);
}

ngm::NsightCaptureOptions capture_options(const std::filesystem::path& scratch, const std::filesystem::path& target,
                                          const std::string& name) {
    ngm::NsightCaptureOptions options;
    options.context = context(scratch / name / "logs");
    options.executable = target;
    options.working_directory = scratch / "target working directory";
    std::filesystem::create_directories(options.working_directory);
    options.capture_file = scratch / name / "capture.ngfx-capture";
    options.arguments = {"--scenario", "triangle", "--frame", "5", "--option=value", "dir/file", "vk:0"};
    options.capture_frame = 5;
    options.context.environment = {{"NGM_TARGET_RECORD", (scratch / "target records.txt").string()}};
    return options;
}

ngm::NsightExportOptions export_options(const std::filesystem::path& scratch,
                                        const ngm::NsightInstallation& installation,
                                        const std::filesystem::path& capture, const std::string& name,
                                        ngm::NsightExportKind kind) {
    ngm::NsightExportOptions options;
    options.context = context(scratch / name / "logs");
    options.capture_file = capture;
    options.capture_tool_version = installation.capture.version;
    options.capture_tool_build = installation.capture.build;
    options.kind = kind;
    options.output_file = scratch / name / (kind == ngm::NsightExportKind::Screenshot ? "output.png" : "output.txt");
    return options;
}

void require_success(const ngm::NsightOperationResult& result) {
    require(result.outcome == ngm::NsightOutcome::Success, "operation succeeded: " + result.message);
    require(result.process.exit_code == 0 && result.process.error.empty(), "zero exit with no process error");
    require(result.launched && result.process.cleanup_confirmed, "owned processes finished and cleanup is confirmed");
    require(std::filesystem::is_regular_file(result.output_file), "required output is a regular file");
    require(result.output_bytes == std::filesystem::file_size(result.output_file), "result records exact output size");
}

std::vector<pid_t> target_pids(const std::filesystem::path& path) {
    const auto text = ngm::read_regular_file(path, 65536);
    std::istringstream lines(text);
    std::vector<pid_t> result;
    for(std::string line; std::getline(lines, line);) {
        if(line.starts_with("pid=")) {
            result.push_back(static_cast<pid_t>(std::stol(line.substr(4))));
        }
    }
    return result;
}

void require_owned_pids_gone(const std::filesystem::path& path) {
    std::ifstream stream(path);
    pid_t pid = -1;
    int count = 0;
    while(stream >> pid) {
        require(pid > 1, "stand-in recorded a valid process identity");
        errno = 0;
        require(kill(pid, 0) == -1 && errno == ESRCH, "Nsight launcher and escaped child are reaped");
        ++count;
    }
    require(count == 2, "stand-in exercised both launcher and descendant");
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "usage: NsightCheck <stand-in executable>");
        Scratch scratch;
        const auto standin = std::filesystem::absolute(argv[1]);
        const auto root = scratch.path / "Nsight installation with spaces";
        const auto host = root / "host/linux-desktop-nomad-x64";
        for(const auto* name : {"ngfx", "ngfx-capture", "ngfx-replay"}) {
            copy_executable(standin, host / name);
        }
        const auto target = scratch.path / "target with spaces";
        copy_executable(standin, target);
        const auto installation = inspect(root, scratch.path / "inspection");
        require(installation.interface_ready,
                "matching stand-in documented interfaces are recognized: " + installation.problem);
        require(installation.cleanup_confirmed, "all inspection subprocesses were cleaned");
        require(installation.capture.version == "2026.3.1.0" && installation.capture.build == "38722833",
                "exact version and build are retained");
        require(installation.cli.accepted_discovery_exit_one && installation.cli.version_process.exit_code == 1 &&
                    installation.cli.help_process.exit_code == 1,
                "ngfx discovery quirk is explicitly accepted only with valid output");
        require(!installation.capture.accepted_discovery_exit_one && !installation.replay.accepted_discovery_exit_one,
                "capture and replay do not borrow the ngfx discovery quirk");
        require(ngm::read_regular_file(installation.capture.help_stdout, 65536).find("--capture-frame") !=
                    std::string::npos,
                "exact help output is retained separately from protocol stdout");
        const auto reused = inspect(root, scratch.path / "inspection");
        require(!reused.interface_ready && reused.cleanup_confirmed, "existing inspection logs are not overwritten");
        const auto missing = inspect(scratch.path / "missing installation", scratch.path / "missing inspection");
        require(!missing.interface_ready && missing.cleanup_confirmed,
                "missing tools own no process and are unavailable");

        for(const auto& [variable, tool] :
            std::vector<std::pair<std::string, std::string>>{{"NGM_MISMATCH_TOOL", "ngfx-replay"},
                                                             {"NGM_MISMATCH_BUILD", "ngfx-replay"},
                                                             {"NGM_MISMATCH_TOOL", "ngfx"},
                                                             {"NGM_DISCOVERY_EXIT_ONE_TOOL", "ngfx-capture"},
                                                             {"NGM_DISCOVERY_EXIT_ONE_TOOL", "ngfx-replay"},
                                                             {"NGM_BAD_VERSION_TOOL", "ngfx"},
                                                             {"NGM_BAD_HELP_TOOL", "ngfx-replay"}}) {
            const auto result = inspect(root, scratch.path / (variable + '-' + tool), {{variable, tool}});
            require(!result.interface_ready && result.cleanup_confirmed,
                    "invalid/mismatched observation is unavailable");
        }

        const auto missing_capture_option = inspect(root, scratch.path / "missing required capture option",
                                                    {{"NGM_OMIT_CAPTURE_NO_BUNDLE_REPLAYER", "1"}});
        require(missing_capture_option.capture.help_valid && missing_capture_option.capture.version_valid &&
                    missing_capture_option.replay.help_valid && missing_capture_option.replay.version_valid,
                "missing-option regression retains otherwise valid tool observations");
        require(!missing_capture_option.interface_ready && missing_capture_option.cleanup_confirmed &&
                    missing_capture_option.problem.find("--no-bundle-replayer") != std::string::npos,
                "readiness requires every unconditional capture option and identifies the missing flag");
        const auto unavailable_capture = ngm::run_nsight_capture(missing_capture_option, {});
        require(unavailable_capture.outcome == ngm::NsightOutcome::Unavailable && !unavailable_capture.launched,
                "a missing unconditional capture option prevents launch");

        // Discovery's PATH fallback must not accidentally combine releases even
        // when binaries in distinct installations report identical version text.
        const auto other = scratch.path / "other release";
        copy_executable(standin, other / "ngfx-replay");
        std::filesystem::rename(host / "ngfx-replay", host / "saved-replayer");
        const char* old_path = std::getenv("PATH");
        const auto saved_path =
            old_path == nullptr ? std::optional<std::string>() : std::optional<std::string>(old_path);
        const auto discovery_path = host.string() + ':' + other.string();
        setenv("PATH", discovery_path.c_str(), 1);
        ngm::NsightInspectionOptions mixed_options;
        mixed_options.context = context(scratch.path / "mixed installation inspection");
        const auto mixed = ngm::inspect_nsight(mixed_options);
        if(saved_path) {
            setenv("PATH", saved_path->c_str(), 1);
        } else {
            unsetenv("PATH");
        }
        std::filesystem::rename(host / "saved-replayer", host / "ngfx-replay");
        require(!mixed.interface_ready && mixed.cleanup_confirmed, "cross-directory tools cannot form an installation");

        auto options = capture_options(scratch.path, target, "capture one");
        const auto first = ngm::run_nsight_capture(installation, options);
        require_success(first);
        require(ngm::read_regular_file(first.stdout_path, 65536) == "target stdout\n",
                "target stdout stays in capture log");
        require(ngm::read_regular_file(first.stderr_path, 65536).find("target stderr") != std::string::npos,
                "target stderr is retained separately");
        const auto target_record = ngm::read_regular_file(scratch.path / "target records.txt", 65536);
        require(target_record.find("cwd=" + options.working_directory.string()) != std::string::npos,
                "application working directory with spaces survives tool launch");
        for(const auto& argument : options.arguments) {
            require(target_record.find(std::to_string(argument.size()) + ':' + argument + '\n') != std::string::npos,
                    "safe target tokens survive the stand-in launch boundary");
        }
        const auto second = ngm::run_nsight_capture(installation, capture_options(scratch.path, target, "capture two"));
        require_success(second);
        const auto pids = target_pids(scratch.path / "target records.txt");
        require(pids.size() == 2 && pids[0] != pids[1], "consecutive requests launch fresh application instances");
        require(first.output_file != second.output_file, "consecutive captures have separate destinations");
        const auto repeated = ngm::run_nsight_capture(installation, options);
        require(repeated.outcome == ngm::NsightOutcome::InvalidInput && !repeated.launched &&
                    repeated.process.cleanup_confirmed,
                "existing captures/logs are not replaced");

        const auto replay_failure_installation =
            inspect(root, scratch.path / "replay runtime failure inspection", {{"NGM_STANDIN_REPLAY_MODE", "fail"}});
        require(replay_failure_installation.interface_ready && replay_failure_installation.cleanup_confirmed,
                "replay runtime override does not affect version/help inspection");
        auto replay_failure_capture = capture_options(scratch.path, target, "capture before replay failure");
        replay_failure_capture.context.environment["NGM_STANDIN_REPLAY_MODE"] = "fail";
        const auto before_replay_failure = ngm::run_nsight_capture(replay_failure_installation, replay_failure_capture);
        require_success(before_replay_failure);
        auto replay_failure_export =
            export_options(scratch.path, replay_failure_installation, before_replay_failure.output_file,
                           "replay only failure", ngm::NsightExportKind::Metadata);
        replay_failure_export.context.environment = replay_failure_capture.context.environment;
        const auto replay_failure = ngm::export_nsight_capture(replay_failure_installation, replay_failure_export);
        require(replay_failure.outcome == ngm::NsightOutcome::Failed && replay_failure.process.exit_code == 1 &&
                    replay_failure.process.cleanup_confirmed,
                "replay-only runtime failure follows successful capture with the same environment");

        auto replay_override_export =
            export_options(scratch.path, installation, first.output_file, "replay mode overrides generic failure",
                           ngm::NsightExportKind::Metadata);
        replay_override_export.context.environment = {{"NGM_STANDIN_MODE", "fail"}, {"NGM_STANDIN_REPLAY_MODE", ""}};
        require_success(ngm::export_nsight_capture(installation, replay_override_export));

        int argument_index = 0;
        for(const auto& argument :
            std::vector<std::string>{"", "two words", "quoted\"", "slash\\", "$HOME", "`id`", "semi;colon",
                                     "line\nbreak", "*", "\xc3\xa9", std::string("a\0b", 3)}) {
            auto invalid = capture_options(scratch.path, target, "argument " + std::to_string(argument_index++));
            invalid.arguments = {argument};
            const auto result = ngm::run_nsight_capture(installation, invalid);
            require(result.outcome == ngm::NsightOutcome::InvalidInput && !result.launched &&
                        result.process.cleanup_confirmed,
                    "unsupported argument encoding is rejected before launching a tool");
        }
        auto invalid_frame = capture_options(scratch.path, target, "invalid frame");
        invalid_frame.capture_frame = 1;
        require(ngm::run_nsight_capture(installation, invalid_frame).outcome == ngm::NsightOutcome::InvalidInput,
                "documented frame boundary range is enforced");
        invalid_frame.capture_frame = 2;
        invalid_frame.frame_count = 601;
        require(ngm::run_nsight_capture(installation, invalid_frame).outcome == ngm::NsightOutcome::InvalidInput,
                "documented capture count range is enforced");

        for(const auto* mode : {"fail", "missing", "empty", "symlink", "fifo"}) {
            auto failed = capture_options(scratch.path, target, std::string("capture ") + mode);
            failed.context.environment["NGM_STANDIN_MODE"] = mode;
            const auto result = ngm::run_nsight_capture(installation, failed);
            require(result.outcome == (std::string_view(mode) == "fail" ? ngm::NsightOutcome::Failed
                                                                        : ngm::NsightOutcome::InvalidOutput),
                    "capture process exit and output validity are checked independently");
            require(result.process.cleanup_confirmed, "failed capture cleanup is confirmed");
        }

        for(const auto kind :
            {ngm::NsightExportKind::Metadata, ngm::NsightExportKind::Functions, ngm::NsightExportKind::Objects,
             ngm::NsightExportKind::Logs, ngm::NsightExportKind::Screenshot}) {
            auto output = export_options(scratch.path, installation, first.output_file,
                                         "export " + std::string(ngm::nsight_export_name(kind)), kind);
            const auto result = ngm::export_nsight_capture(installation, output);
            require_success(result);
            require(kind == ngm::NsightExportKind::Logs ? result.output_bytes == 0 : result.output_bytes > 0,
                    "only log exports can legitimately be empty");
            require(result.arguments.size() == (kind == ngm::NsightExportKind::Screenshot ? 3 : 2),
                    "each metadata mode has a separate invocation");
            output.context = context(scratch.path / ("duplicate export " + std::string(ngm::nsight_export_name(kind))));
            const auto duplicate = ngm::export_nsight_capture(installation, output);
            require(duplicate.outcome == ngm::NsightOutcome::InvalidInput && !duplicate.launched,
                    "existing raw exports are preserved");
        }

        for(const auto& [kind, flag] : std::vector<std::pair<ngm::NsightExportKind, std::string>>{
                {ngm::NsightExportKind::Functions, "--metadata-functions"},
                {ngm::NsightExportKind::Objects, "--metadata-objects"},
                {ngm::NsightExportKind::Logs, "--metadata-logs"},
                {ngm::NsightExportKind::Screenshot, "--metadata-screenshot"}}) {
            const auto partial =
                inspect(root, scratch.path / ("missing optional " + flag), {{"NGM_OMIT_REPLAY_OPTION", flag}});
            require(partial.interface_ready && partial.cleanup_confirmed,
                    "an absent optional export does not disable basic capture and metadata");
            const auto output = export_options(scratch.path, partial, first.output_file, "unavailable " + flag, kind);
            const auto result = ngm::export_nsight_capture(partial, output);
            require(result.outcome == ngm::NsightOutcome::Unavailable && !result.launched &&
                        result.process.cleanup_confirmed && result.message.find(flag) != std::string::npos &&
                        !std::filesystem::exists(output.output_file),
                    "absent optional export reports an actionable capability limit without executing a tool");
            require_success(ngm::export_nsight_capture(partial, export_options(scratch.path, partial, first.output_file,
                                                                               "metadata without " + flag,
                                                                               ngm::NsightExportKind::Metadata)));
        }

        for(const auto* mode : {"fail", "missing", "malformed", "oversized"}) {
            auto output = export_options(scratch.path, installation, first.output_file,
                                         std::string("bad export ") + mode, ngm::NsightExportKind::Objects);
            output.context.environment["NGM_STANDIN_MODE"] = mode;
            output.maximum_bytes = 1024;
            const auto result = ngm::export_nsight_capture(installation, output);
            require(result.outcome == (std::string_view(mode) == "fail" ? ngm::NsightOutcome::Failed
                                                                        : ngm::NsightOutcome::InvalidOutput),
                    "failed, empty, malformed or oversized object exports fail explicitly");
            require(result.process.cleanup_confirmed && std::filesystem::exists(result.stdout_path),
                    "failed export retains raw process output and confirmed cleanup");
        }
        auto mismatch = export_options(scratch.path, installation, first.output_file, "wrong provenance",
                                       ngm::NsightExportKind::Metadata);
        mismatch.capture_tool_build = "1";
        const auto mismatch_result = ngm::export_nsight_capture(installation, mismatch);
        require(mismatch_result.outcome == ngm::NsightOutcome::Unavailable && !mismatch_result.launched,
                "cross-build capture replay is rejected before launch");

        auto timed = capture_options(scratch.path, target, "timed capture");
        timed.context.timeout = 150ms;
        timed.context.environment["NGM_STANDIN_MODE"] = "hang";
        timed.context.environment["NGM_STANDIN_PIDS"] = (scratch.path / "timed pids.txt").string();
        const auto timed_result = ngm::run_nsight_capture(installation, timed);
        require(timed_result.outcome == ngm::NsightOutcome::TimedOut && timed_result.process.cleanup_confirmed,
                "capture timeout forwards to process ownership cleanup");
        require_owned_pids_gone(scratch.path / "timed pids.txt");

        auto cancelled = export_options(scratch.path, installation, first.output_file, "cancelled export",
                                        ngm::NsightExportKind::Metadata);
        cancelled.context.environment["NGM_STANDIN_MODE"] = "hang";
        cancelled.context.environment["NGM_STANDIN_PIDS"] = (scratch.path / "cancelled pids.txt").string();
        std::stop_source stop;
        auto task = std::async(std::launch::async,
                               [&] { return ngm::export_nsight_capture(installation, cancelled, stop.get_token()); });
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        bool ready = false;
        while(std::chrono::steady_clock::now() < deadline) {
            if(std::filesystem::exists(scratch.path / "cancelled pids.txt")) {
                ready = true;
                break;
            }
            if(task.wait_for(0ms) == std::future_status::ready) {
                break;
            }
            std::this_thread::sleep_for(5ms);
        }
        stop.request_stop();
        const auto cancelled_result = task.get();
        require(ready, "stand-in reached the process boundary before cancellation");
        require(cancelled_result.outcome == ngm::NsightOutcome::Cancelled && cancelled_result.process.cleanup_confirmed,
                "export cancellation forwards to process ownership cleanup");
        require_owned_pids_gone(scratch.path / "cancelled pids.txt");

        const auto cpp_options = [&](const std::string& name) {
            ngm::NsightCppCaptureOptions options;
            options.context = context(scratch.path / name / "logs");
            options.executable = target;
            options.working_directory = scratch.path / "target working directory";
            options.output_directory = scratch.path / name / "generated";
            options.arguments = {"--scenario", "reference"};
            options.wait_frames = 5;
            options.context.environment["NGM_TARGET_RECORD"] = (scratch.path / "cpp targets.txt").string();
            return options;
        };
        const auto cpp = ngm::run_nsight_cpp_capture(installation, cpp_options("cpp success"));
        require_success(cpp.operation);
        require(cpp.metadata.primary_api == "vulkan" && cpp.metadata.build_id == 38722833 &&
                    cpp.source_files.size() == 12 &&
                    std::filesystem::is_regular_file(cpp.project_directory / "data.bin"),
                "C++ capture retains its distinct metadata schema and generated source/data paths");
        require(std::find(cpp.operation.arguments.begin(), cpp.operation.arguments.end(), "--wait-frames=5") !=
                    cpp.operation.arguments.end(),
                "C++ wait control reaches the real process boundary");
        for(const auto* mode :
            {"fail", "missing", "missing-resource", "missing-partition", "missing-header", "unsafe-cmake-path",
             "wrong-producer", "duplicate-key", "ambiguous", "symlink", "fifo", "bad-bmp"}) {
            auto options = cpp_options(std::string("cpp ") + mode);
            options.context.environment["NGM_STANDIN_CPP_MODE"] = mode;
            const auto result = ngm::run_nsight_cpp_capture(installation, options);
            require(result.operation.outcome == (std::string_view(mode) == "fail"
                                                     ? ngm::NsightOutcome::Failed
                                                     : ngm::NsightOutcome::InvalidOutput) &&
                        result.operation.process.cleanup_confirmed,
                    "C++ project rejects failed/incomplete/ambiguous or invalid evidence: " + std::string(mode));
        }
        auto absent_option = installation;
        absent_option.cli.documented_options.erase("--wait-frames");
        require(ngm::run_nsight_cpp_capture(absent_option, cpp_options("cpp unsupported option")).operation.outcome ==
                    ngm::NsightOutcome::Unavailable,
                "C++ interface requires its documented options");
        auto unqualified = installation;
        unqualified.cli.build = unqualified.capture.build = unqualified.replay.build = "99999999";
        const auto unqualified_result = ngm::run_nsight_cpp_capture(unqualified, cpp_options("cpp unqualified"));
        require(unqualified_result.operation.outcome == ngm::NsightOutcome::Unavailable &&
                    !unqualified_result.operation.launched,
                "an unqualified C++ producer cannot borrow another producer's schema profile");
        auto cpp_timed = cpp_options("cpp timed");
        cpp_timed.context.timeout = 150ms;
        cpp_timed.context.environment["NGM_STANDIN_CPP_MODE"] = "hang";
        cpp_timed.context.environment["NGM_STANDIN_PIDS"] = (scratch.path / "cpp timed pids.txt").string();
        const auto cpp_timeout = ngm::run_nsight_cpp_capture(installation, cpp_timed);
        require(cpp_timeout.operation.outcome == ngm::NsightOutcome::TimedOut &&
                    cpp_timeout.operation.process.cleanup_confirmed,
                "--no-timeout cannot disable the outer C++ capture deadline or descendant cleanup");
        require_owned_pids_gone(scratch.path / "cpp timed pids.txt");
    });
}
