#include "ngm/Nsight.hpp"

#include "ngm/File.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <tuple>
#include <unistd.h>

namespace ngm {
namespace {
constexpr std::size_t observation_limit = 256U * 1024U;

std::filesystem::path absolute_path(const std::filesystem::path& path, std::string_view purpose) {
    if(path.empty() || path.native().find('\0') != std::string::npos) {
        throw std::runtime_error(std::string(purpose) + " must be a nonempty path without NUL bytes");
    }
    return std::filesystem::absolute(path).lexically_normal();
}

std::filesystem::path directory_path(const std::filesystem::path& path, std::string_view purpose) {
    auto result = std::filesystem::canonical(absolute_path(path, purpose));
    if(!std::filesystem::is_directory(result)) {
        throw std::runtime_error(std::string(purpose) + " must be an existing directory: " + result.string());
    }
    return result;
}

std::filesystem::path executable_path(const std::filesystem::path& path) {
    auto result = std::filesystem::canonical(absolute_path(path, "Executable"));
    if(!std::filesystem::is_regular_file(result) || access(result.c_str(), X_OK) != 0) {
        throw std::runtime_error("Expected an executable regular file: " + result.string());
    }
    return result;
}

std::filesystem::path fresh_file(const std::filesystem::path& path) {
    const auto absolute = absolute_path(path, "Output");
    const auto result = directory_path(absolute.parent_path(), "Output parent") / absolute.filename();
    std::error_code error;
    const auto status = std::filesystem::symlink_status(result, error);
    if(error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Cannot inspect output path: " + result.string() + ": " + error.message());
    }
    if(status.type() != std::filesystem::file_type::not_found) {
        throw std::runtime_error("Output already exists; use a fresh attempt directory: " + result.string());
    }
    return result;
}

void validate_context(const NsightRunContext& context) {
    directory_path(context.log_directory, "Log directory");
    if(context.timeout <= std::chrono::milliseconds::zero() ||
       context.terminate_grace < std::chrono::milliseconds::zero()) {
        throw std::runtime_error("Timeout must be positive and termination grace must be nonnegative");
    }
}

ProcessOptions process_options(const std::filesystem::path& executable, const NsightRunContext& context,
                               const std::string& log_name) {
    validate_context(context);
    ProcessOptions process;
    process.executable = executable_path(executable);
    process.working_directory = directory_path(context.log_directory, "Log directory");
    process.stdout_path = fresh_file(context.log_directory / (log_name + ".stdout.txt"));
    process.stderr_path = fresh_file(context.log_directory / (log_name + ".stderr.txt"));
    process.environment = context.environment;
    process.timeout = context.timeout;
    process.terminate_grace = context.terminate_grace;
    return process;
}

bool clean_exit(const ProcessResult& process, bool allow_one = false) {
    return process.cleanup_confirmed && process.error.empty() && !process.timed_out && !process.cancelled &&
           !process.signal && (process.exit_code == 0 || (allow_one && process.exit_code == 1));
}

std::string probe_output(const ProcessOptions& process) {
    return read_regular_file(process.stdout_path, observation_limit) + '\n' +
           read_regular_file(process.stderr_path, observation_limit);
}

void append_problem(std::string& destination, const std::string& message) {
    if(!destination.empty()) {
        destination += "; ";
    }
    destination += message;
}

NsightToolObservation inspect_tool(const ExecutableObservation& discovered, const NsightRunContext& context,
                                   std::stop_token stop) {
    NsightToolObservation observation;
    observation.name = discovered.name;
    observation.path = discovered.path;
    observation.version_process.cleanup_confirmed = true;
    observation.help_process.cleanup_confirmed = true;
    if(!discovered.path) {
        observation.problem = discovered.name + " was not found";
        return observation;
    }

    const bool is_cli = discovered.name == "ngfx";
    try {
        auto process = process_options(*discovered.path, context, discovered.name + "-version");
        process.arguments = {"--version"};
        observation.version_stdout = process.stdout_path;
        observation.version_stderr = process.stderr_path;
        observation.version_process = run_process(process, stop);
        if(clean_exit(observation.version_process, is_cli)) {
            const auto output = probe_output(process);
            const std::regex version_pattern(R"((?:^|\n)Version:? ([0-9]+(?:\.[0-9]+){2,3}) \(build ([0-9]+)\))");
            std::smatch match;
            const bool banner_valid = !is_cli || output.find("NVIDIA (R) Nsight Graphics CLI") != std::string::npos;
            if(banner_valid && std::regex_search(output, match, version_pattern)) {
                observation.version = match[1].str();
                observation.build = match[2].str();
                observation.version_valid = true;
                observation.accepted_discovery_exit_one = observation.version_process.exit_code == 1;
            }
        }
        if(!observation.version_valid) {
            append_problem(observation.problem, "Unrecognized or failed " + discovered.name + " --version");
        }
    } catch(const std::exception& error) {
        append_problem(observation.problem, error.what());
    }

    if(!observation.version_process.cleanup_confirmed) {
        append_problem(observation.problem, "Version probe process cleanup was not confirmed");
        return observation;
    }

    try {
        auto process = process_options(*discovered.path, context, discovered.name + "-help");
        process.arguments = {is_cli ? "--help-all" : "--help"};
        observation.help_stdout = process.stdout_path;
        observation.help_stderr = process.stderr_path;
        observation.help_process = run_process(process, stop);
        if(clean_exit(observation.help_process, is_cli)) {
            const auto output = probe_output(process);
            const auto banner = is_cli ? "NVIDIA Nsight Graphics [general_options] [activity_options]"
                                : discovered.name == "ngfx-capture" ? "NVIDIA Nsight Graphics Capture CLI Tool"
                                                                    : "NVIDIA Nsight Graphics Replayer CLI Tool";
            if(output.find(banner) != std::string::npos && output.find("--version") != std::string::npos &&
               output.find("--help") != std::string::npos) {
                observation.help_valid = true;
                observation.accepted_discovery_exit_one |= observation.help_process.exit_code == 1;
                const std::regex option_pattern("--[a-z][a-z0-9-]*");
                for(auto match = std::sregex_iterator(output.begin(), output.end(), option_pattern);
                    match != std::sregex_iterator(); ++match) {
                    observation.documented_options.insert(match->str());
                }
            }
        }
        if(!observation.help_valid) {
            append_problem(observation.problem, "Unrecognized or failed " + discovered.name + " help");
        }
    } catch(const std::exception& error) {
        append_problem(observation.problem, error.what());
    }
    return observation;
}

bool matching_tools(const NsightToolObservation& first, const NsightToolObservation& second) {
    return first.path && second.path && first.path->parent_path() == second.path->parent_path() &&
           first.version_valid && second.version_valid && first.version == second.version &&
           first.build == second.build;
}

void require_option(const NsightToolObservation& tool, const std::string& option) {
    if(!tool.documented_options.contains(option)) {
        throw std::runtime_error(tool.name + " help does not advertise required option " + option);
    }
}

std::string delimiter_option(NsightDelimiter delimiter) {
    switch(delimiter) {
        case NsightDelimiter::Present:
            return "--delimiter-present";
        case NsightDelimiter::VulkanFrameBoundary:
            return "--delimiter-vk-frame-boundary-ext";
        case NsightDelimiter::GraphicsCaptureApi:
            return "--delimiter-graphics-capture-api";
    }
    throw std::runtime_error("Unknown capture delimiter");
}

std::string export_option(NsightExportKind kind) {
    switch(kind) {
        case NsightExportKind::Metadata:
            return "--metadata";
        case NsightExportKind::Functions:
            return "--metadata-functions";
        case NsightExportKind::Objects:
            return "--metadata-objects";
        case NsightExportKind::Logs:
            return "--metadata-logs";
        case NsightExportKind::Screenshot:
            return "--metadata-screenshot";
    }
    throw std::runtime_error("Unknown metadata export kind");
}

std::string target_arguments(const std::vector<std::string>& arguments) {
    constexpr std::string_view characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-./:=,+@%";
    std::string joined;
    for(const auto& argument : arguments) {
        if(argument.empty() || argument.find_first_not_of(characters) != std::string::npos) {
            throw std::runtime_error("This Nsight adapter cannot preserve empty or quoted/escaped target arguments; "
                                     "use nonempty ASCII tokens containing letters, digits or _-./:=,+@%");
        }
        if(!joined.empty()) {
            joined += ' ';
        }
        joined += argument;
    }
    return joined;
}

std::uint64_t regular_file_size(const std::filesystem::path& path, bool allow_empty, std::uint64_t maximum_bytes) {
    struct File {
        int descriptor;
        ~File() {
            if(descriptor >= 0) {
                close(descriptor);
            }
        }
    } file{open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC)};
    struct stat attributes{};
    if(file.descriptor < 0 || fstat(file.descriptor, &attributes) != 0 || !S_ISREG(attributes.st_mode) ||
       attributes.st_size < 0 || (!allow_empty && attributes.st_size == 0) ||
       (maximum_bytes != 0 && static_cast<std::uint64_t>(attributes.st_size) > maximum_bytes)) {
        throw std::runtime_error("Expected a readable regular output within its size limit: " + path.string());
    }
    if(attributes.st_size != 0) {
        char byte;
        ssize_t count;
        do {
            count = pread(file.descriptor, &byte, 1, 0);
        } while(count < 0 && errno == EINTR);
        if(count != 1) {
            throw std::runtime_error("Output is not readable: " + path.string());
        }
    }
    return static_cast<std::uint64_t>(attributes.st_size);
}

bool run_operation(NsightOperationResult& result, const ProcessOptions& process, std::stop_token stop) {
    result.executable = process.executable;
    result.arguments = process.arguments;
    result.stdout_path = process.stdout_path;
    result.stderr_path = process.stderr_path;
    result.launched = true;
    result.process = run_process(process, stop);
    if(!result.process.cleanup_confirmed) {
        result.outcome = NsightOutcome::CleanupFailed;
        result.message = "Owned process cleanup was not confirmed; keep the GPU reservation";
    } else if(result.process.cancelled) {
        result.outcome = NsightOutcome::Cancelled;
        result.message = "Nsight operation was cancelled";
    } else if(result.process.timed_out) {
        result.outcome = NsightOutcome::TimedOut;
        result.message = "Nsight operation exceeded its deadline";
    } else if(!clean_exit(result.process)) {
        result.outcome = NsightOutcome::Failed;
        result.message = result.process.error.empty()
                             ? "Nsight process did not exit successfully; inspect retained logs"
                             : result.process.error;
    } else {
        return true;
    }
    return false;
}

bool usable_installation(NsightOperationResult& result, const NsightInstallation& installation) {
    if(!installation.interface_ready || !matching_tools(installation.capture, installation.replay)) {
        result.outcome = NsightOutcome::Unavailable;
        result.message = "Nsight capture/replay interface is unavailable: " + installation.problem;
        return false;
    }
    return true;
}

// Both qualified producers emit these literal blocks. Read the declarations
// without evaluating CMake; platform-conditional runtime blocks are separate.
std::set<std::string> generated_cmake_inputs(const std::string& cmake) {
    std::set<std::string> result;
    const std::regex filename("[A-Za-z][A-Za-z0-9_]*\\.(cpp|h)");
    for(const auto& [begin, end, extension] : std::vector<std::tuple<std::string, std::string, std::string>>{
            {"set(GeneratedReplayHeaders\n", "\n)", ".h"},
            {"add_library(GeneratedReplay ${ReplayExecutorLibraryType}\n", "\n${GeneratedReplayHeaders})", ".cpp"}}) {
        const auto start = cmake.find(begin);
        if(start == std::string::npos || (start != 0 && cmake[start - 1] != '\n') ||
           cmake.find(begin, start + begin.size()) != std::string::npos) {
            throw std::runtime_error("Missing, ambiguous, or unsupported generated CMake file-list block");
        }
        const auto finish = cmake.find(end, start + begin.size());
        if(finish == std::string::npos) {
            throw std::runtime_error("Truncated generated CMake file-list block");
        }
        std::istringstream tokens(cmake.substr(start + begin.size(), finish - start - begin.size()));
        std::size_t count = 0;
        for(std::string token; tokens >> token;) {
            if(++count > 1024 || token.size() > 255 || !std::regex_match(token, filename) ||
               !token.ends_with(extension) || !result.insert(token).second) {
                throw std::runtime_error("Invalid or repeated literal generated-source filename in CMake");
            }
        }
        if(count == 0) {
            throw std::runtime_error("Empty generated CMake file-list block");
        }
    }
    for(const auto* required :
        {"CommandLists.h", "ReplayProcedures.h", "Resources.h", "CommandList00.cpp", "Resources00.cpp",
         "FrameSetup00.cpp", "FrameReset00.cpp", "Frame0Part00.cpp", "ReplayProcedures.cpp", "PerfMarkersReset.cpp",
         "PerfMarkersSetup.cpp", "WinResourcesReset.cpp", "WinResourcesSetup.cpp"}) {
        if(!result.contains(required)) {
            throw std::runtime_error(std::string("Generated CMake omits required source/header ") + required);
        }
    }
    return result;
}
} // namespace

NsightInstallation inspect_nsight(const NsightInspectionOptions& options, std::stop_token stop) {
    NsightInstallation installation;
    installation.discovery = discover_prerequisites(options.installation_root);
    installation.cli = inspect_tool(installation.discovery.executables[0], options.context, stop);
    installation.capture = inspect_tool(installation.discovery.executables[1], options.context, stop);
    installation.replay = inspect_tool(installation.discovery.executables[2], options.context, stop);
    for(const auto* tool : {&installation.cli, &installation.capture, &installation.replay}) {
        installation.cleanup_confirmed &=
            tool->version_process.cleanup_confirmed && tool->help_process.cleanup_confirmed;
    }
    if(!installation.cleanup_confirmed) {
        installation.problem = "Nsight interface inspection did not confirm owned process cleanup";
    } else if(!matching_tools(installation.capture, installation.replay)) {
        installation.problem =
            "Capture and replay require recognizable matching versions/builds from one tool directory";
    } else if(!installation.capture.help_valid || !installation.replay.help_valid) {
        installation.problem = "Capture and replay require recognizable help output";
    } else if(installation.cli.path && !matching_tools(installation.capture, installation.cli)) {
        installation.problem = "Discovered ngfx does not match the selected capture/replay installation";
    } else {
        try {
            for(const auto* option :
                {"--exe", "--working-dir", "--args", "--output-file", "--output-dir", "--capture-frame",
                 "--frame-count", "--delimiter-present", "--terminate-after-capture", "--no-bundle-replayer"}) {
                require_option(installation.capture, option);
            }
            require_option(installation.replay, "--metadata");
            installation.interface_ready = true;
        } catch(const std::exception& error) {
            installation.problem = error.what();
        }
    }
    if(!installation.interface_ready) {
        for(const auto* tool : {&installation.cli, &installation.capture, &installation.replay}) {
            if(!tool->problem.empty()) {
                append_problem(installation.problem, tool->problem);
            }
        }
    }
    return installation;
}

std::string_view nsight_outcome_name(NsightOutcome outcome) {
    switch(outcome) {
        case NsightOutcome::Success:
            return "success";
        case NsightOutcome::InvalidInput:
            return "invalid_input";
        case NsightOutcome::Unavailable:
            return "unavailable";
        case NsightOutcome::Failed:
            return "failed";
        case NsightOutcome::InvalidOutput:
            return "invalid_output";
        case NsightOutcome::TimedOut:
            return "timed_out";
        case NsightOutcome::Cancelled:
            return "cancelled";
        case NsightOutcome::CleanupFailed:
            return "cleanup_failed";
    }
    return "unknown";
}

std::string_view nsight_export_name(NsightExportKind kind) {
    switch(kind) {
        case NsightExportKind::Metadata:
            return "metadata";
        case NsightExportKind::Functions:
            return "functions";
        case NsightExportKind::Objects:
            return "objects";
        case NsightExportKind::Logs:
            return "logs";
        case NsightExportKind::Screenshot:
            return "screenshot";
    }
    return "unknown";
}

NsightOperationResult run_nsight_capture(const NsightInstallation& installation, const NsightCaptureOptions& options,
                                         std::stop_token stop) {
    NsightOperationResult result;
    result.process.cleanup_confirmed = true;
    if(!usable_installation(result, installation)) {
        return result;
    }
    try {
        if(options.capture_frame < 2 || options.frame_count < 1 || options.frame_count > 600) {
            throw std::runtime_error("Capture frame must be >= 2 and frame count must be in [1, 600]");
        }
        const auto target = executable_path(options.executable);
        const auto working_directory = directory_path(options.working_directory, "Application working directory");
        const auto arguments = target_arguments(options.arguments);
        result.output_file = fresh_file(options.capture_file);
        if(result.output_file.extension() != ".ngfx-capture") {
            throw std::runtime_error("Capture output must use the .ngfx-capture extension");
        }
        const auto delimiter = delimiter_option(options.delimiter);
        require_option(installation.capture, delimiter);
        require_option(installation.capture, "--no-bundle-replayer");
        auto process = process_options(*installation.capture.path, options.context, "capture");
        if(result.output_file == process.stdout_path || result.output_file == process.stderr_path) {
            throw std::runtime_error("Capture output and process logs must be distinct");
        }
        process.arguments = {"--exe",
                             target.string(),
                             "--working-dir",
                             working_directory.string(),
                             "--output-dir",
                             result.output_file.parent_path().string(),
                             "--output-file",
                             result.output_file.filename().string(),
                             "--capture-frame",
                             std::to_string(options.capture_frame),
                             "--frame-count",
                             std::to_string(options.frame_count),
                             delimiter,
                             "--terminate-after-capture",
                             "--no-bundle-replayer"};
        if(!arguments.empty()) {
            process.arguments.push_back("--args=" + arguments);
        }
        if(!run_operation(result, process, stop)) {
            return result;
        }
    } catch(const std::exception& error) {
        result.outcome = NsightOutcome::InvalidInput;
        result.message = error.what();
        return result;
    }
    try {
        result.output_bytes = regular_file_size(result.output_file, false, 0);
        result.outcome = NsightOutcome::Success;
        result.message = "Capture command produced a nonempty regular file; matching metadata export is still required";
    } catch(const std::exception& error) {
        result.outcome = NsightOutcome::InvalidOutput;
        result.message = error.what();
    }
    return result;
}

NsightOperationResult export_nsight_capture(const NsightInstallation& installation, const NsightExportOptions& options,
                                            std::stop_token stop) {
    NsightOperationResult result;
    result.process.cleanup_confirmed = true;
    if(!usable_installation(result, installation)) {
        return result;
    }
    if(options.capture_tool_version != installation.replay.version ||
       options.capture_tool_build != installation.replay.build) {
        result.outcome = NsightOutcome::Unavailable;
        result.message = "Capture provenance and replay version/build must match exactly";
        return result;
    }
    try {
        if(options.maximum_bytes == 0) {
            throw std::runtime_error("Export byte limit must be positive");
        }
        const auto capture = absolute_path(options.capture_file, "Capture input");
        regular_file_size(capture, false, 0);
        result.output_file = fresh_file(options.output_file);
        const auto flag = export_option(options.kind);
        if(!installation.replay.documented_options.contains(flag)) {
            result.outcome = NsightOutcome::Unavailable;
            result.message = installation.replay.name + " help does not advertise requested export " + flag;
            return result;
        }
        auto process = process_options(*installation.replay.path, options.context,
                                       "export-" + std::string(nsight_export_name(options.kind)));
        if(result.output_file == process.stdout_path || result.output_file == process.stderr_path) {
            throw std::runtime_error("Export output and process logs must be distinct");
        }
        process.arguments = {flag};
        if(options.kind == NsightExportKind::Screenshot) {
            if(result.output_file.extension() != ".png") {
                throw std::runtime_error("Screenshot output must use the .png extension");
            }
            process.arguments.push_back(result.output_file.string());
        }
        process.arguments.push_back(capture.string());
        if(!run_operation(result, process, stop)) {
            return result;
        }
    } catch(const std::exception& error) {
        result.outcome = NsightOutcome::InvalidInput;
        result.message = error.what();
        return result;
    }
    try {
        if(options.kind == NsightExportKind::Screenshot) {
            result.output_bytes = regular_file_size(result.output_file, false, options.maximum_bytes);
        } else {
            const auto output = read_regular_file(result.stdout_path, options.maximum_bytes);
            if(output.empty() && options.kind != NsightExportKind::Logs) {
                throw std::runtime_error("Required metadata export is empty");
            }
            if(options.kind == NsightExportKind::Objects && !nlohmann::json::accept(output)) {
                throw std::runtime_error("Object metadata is not valid JSON; raw stdout is retained");
            }
            if(!std::filesystem::copy_file(result.stdout_path, result.output_file)) {
                throw std::runtime_error("Could not preserve metadata export output");
            }
            result.output_bytes =
                regular_file_size(result.output_file, options.kind == NsightExportKind::Logs, options.maximum_bytes);
        }
        result.outcome = NsightOutcome::Success;
        result.message = options.kind == NsightExportKind::Screenshot
                             ? "Embedded final-present screenshot exported; image format/content remain unvalidated"
                             : "Bounded raw metadata exported; no event or object schema is assumed";
    } catch(const std::exception& error) {
        result.outcome = NsightOutcome::InvalidOutput;
        result.message = error.what();
    }
    return result;
}

NsightCppCaptureResult run_nsight_cpp_capture(const NsightInstallation& installation,
                                              const NsightCppCaptureOptions& options, std::stop_token stop) {
    namespace fs = std::filesystem;
    NsightCppCaptureResult result;
    auto& operation = result.operation;
    operation.process.cleanup_confirmed = true;
    if(!usable_installation(operation, installation)) {
        return result;
    }
    const auto& cli = installation.cli;
    const bool profile_20263 = cli.version == "2026.3.1.0" && cli.build == "38722833";
    const bool profile_20262 = cli.version == "2026.2.0.0" && cli.build == "37991608";
    if(!cli.path || !cli.help_valid || !matching_tools(cli, installation.capture) ||
       (!profile_20263 && !profile_20262)) {
        operation.outcome = NsightOutcome::Unavailable;
        operation.message = "Generate C++ Capture requires a qualified matching 2026.3.1.0/38722833 or "
                            "2026.2.0.0/37991608 installation";
        return result;
    }
    for(const auto* flag : {"--activity", "--platform", "--exe", "--dir", "--output-dir", "--wait-frames", "--args",
                            "--env", "--no-timeout"}) {
        if(!cli.documented_options.contains(flag)) {
            operation.outcome = NsightOutcome::Unavailable;
            operation.message = std::string("ngfx help does not advertise required C++ capture option ") + flag;
            return result;
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + options.context.timeout;
    fs::path output;
    try {
        if(options.wait_frames < 2 || options.wait_frames > 1000000) {
            throw std::runtime_error("C++ capture wait_frames must be in [2, 1000000]");
        }
        const auto target = executable_path(options.executable);
        const auto directory = directory_path(options.working_directory, "Application working directory");
        const auto arguments = target_arguments(options.arguments);
        output = fresh_file(options.output_directory);
        // ngfx requires an existing output directory, unlike ngfx-capture's file output.
        // Claim it exclusively after rejecting existing paths; never reuse another attempt.
        if(!fs::create_directory(output)) {
            throw std::runtime_error("Could not create fresh C++ capture output directory");
        }
        auto process = process_options(*cli.path, options.context, "cpp-capture");
        // Nsight's migration notice explicitly documents this per-target value.
        // See retained runtime instruction and suppression qualification in I-015.
        // --no-timeout disables ngfx's internal deadline, not our owned-process deadline.
        process.arguments = {"--activity=Generate C++ Capture",
                             "--platform=Linux (x86_64)",
                             "--no-timeout",
                             "--env=NSIGHT_SUGGEST_GRAPHICS_CAPTURE=0",
                             "--exe=" + target.string(),
                             "--dir=" + directory.string(),
                             "--output-dir=" + output.string(),
                             "--wait-frames=" + std::to_string(options.wait_frames)};
        if(!arguments.empty()) {
            process.arguments.push_back("--args=" + arguments);
        }
        if(!run_operation(operation, process, stop)) {
            return result;
        }
    } catch(const std::exception& error) {
        operation.outcome = NsightOutcome::InvalidInput;
        operation.message = error.what();
        return result;
    }
    try {
        if(fs::symlink_status(output).type() != fs::file_type::directory) {
            throw std::runtime_error("C++ capture did not produce a real output directory");
        }
        std::size_t entries = 0;
        std::vector<fs::path> metadata_files;
        std::vector<fs::path> files;
        for(auto it = fs::recursive_directory_iterator(output); it != fs::recursive_directory_iterator(); ++it) {
            if(stop.stop_requested() || std::chrono::steady_clock::now() >= deadline) {
                operation.outcome = stop.stop_requested() ? NsightOutcome::Cancelled : NsightOutcome::TimedOut;
                operation.message = "C++ capture stopped during generated-project validation";
                return result;
            }
            if(++entries > 3500 || it.depth() > 8 || it->path().lexically_relative(output).native().size() > 384 ||
               it->path().native().size() > 4096) {
                throw std::runtime_error("Generated C++ project exceeds file-count, nesting, or path limits");
            }
            const auto kind = it->symlink_status().type();
            if(kind == fs::file_type::regular) {
                if(fs::hard_link_count(it->path()) != 1) {
                    throw std::runtime_error("Generated project contains a multiply linked file");
                }
                regular_file_size(it->path(), true, 0);
                files.push_back(it->path());
                if(it->path().filename() == "metadata.json") {
                    metadata_files.push_back(it->path());
                }
            } else if(kind != fs::file_type::directory) {
                throw std::runtime_error("Generated project contains a symlink or non-regular file");
            }
        }
        if(metadata_files.size() != 1) {
            throw std::runtime_error("Expected exactly one generated C++ project with metadata.json");
        }
        const auto project = metadata_files.front().parent_path();
        result.metadata = parse_nsight_cpp_metadata(read_regular_file(metadata_files.front(), 1024U * 1024U));
        if(result.metadata.nsight_version != (profile_20263 ? "2026.3.1" : "2026.2.0") ||
           result.metadata.build_id != (profile_20263 ? 38722833U : 37991608U) ||
           result.metadata.primary_api != "vulkan" || result.metadata.project_filename != project.filename().string()) {
            throw std::runtime_error("Generated C++ metadata does not match the selected Vulkan producer/project");
        }
        for(const auto* name : {"CMakeLists.txt", "Resources.h", "Resources00.cpp", "CommandList00.cpp",
                                "Frame0Part00.cpp", "FrameSetup00.cpp", "ReadOnlyDatabase.cpp", "ReadOnlyDatabase.h",
                                "DataScope.cpp", "DataScope.h", "data.bin", "data.bin.rec", "screenshot.bmp"}) {
            regular_file_size(project / name, false, 0);
        }
        const auto declared = generated_cmake_inputs(read_regular_file(project / "CMakeLists.txt", 1024U * 1024U));
        for(const auto& file : declared) {
            regular_file_size(project / file, false, 16U * 1024U * 1024U);
        }
        regular_file_size(project / (project.filename().string() + ".ngfx-cppcap"), false, 1024U * 1024U);
        const auto screenshot = read_regular_file(project / "screenshot.bmp", 64U * 1024U * 1024U);
        if(screenshot.size() < 54 || !screenshot.starts_with("BM")) {
            throw std::runtime_error("Generated screenshot lacks a BMP header");
        }
        for(const auto& file : files) {
            const auto relative = file.lexically_relative(project);
            if(relative.empty() || *relative.begin() == "..") {
                throw std::runtime_error("Generated output contains files outside its single project");
            }
            if(file.extension() == ".cpp") {
                regular_file_size(file, false, 16U * 1024U * 1024U);
                result.source_files.push_back(relative);
            }
        }
        std::sort(result.source_files.begin(), result.source_files.end());
        if(stop.stop_requested() || std::chrono::steady_clock::now() >= deadline) {
            operation.outcome = stop.stop_requested() ? NsightOutcome::Cancelled : NsightOutcome::TimedOut;
            operation.message = "C++ capture stopped after generated-project validation";
            return result;
        }
        result.project_directory = project;
        operation.output_file = metadata_files.front();
        operation.output_bytes = regular_file_size(operation.output_file, false, 1024U * 1024U);
        operation.outcome = NsightOutcome::Success;
        operation.message = "One generated Vulkan C++ project has matching metadata and required source/data files; "
                            "source compilation, database decoding, image decoding, and GPU replay are not performed";
    } catch(const std::exception& error) {
        operation.outcome = NsightOutcome::InvalidOutput;
        operation.message = error.what();
    }
    return result;
}

void validate_profile_settings(const ProfileSettings& settings) {
    if((settings.delimiter != "frames" && settings.delimiter != "submits") || settings.start_after > 1000000 ||
       settings.limit < 1 || settings.limit > 1000 || settings.duration_ms < 1 || settings.duration_ms > 10000)
        throw std::invalid_argument(
            "Profile requires frames/submits, start_after 0..1000000, limit 1..1000, duration_ms 1..10000");
    constexpr std::string_view characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_";
    for(const auto* value : {&settings.architecture, &settings.metric_set})
        if(value->empty() || value->size() > 64 || value->find_first_not_of(characters) != std::string::npos ||
           value->front() == ' ' || value->back() == ' ')
            throw std::invalid_argument("Profile architecture and metric_set require 1..64 ASCII name characters");
}

NsightProfileResult run_nsight_profile(const NsightInstallation& installation, const NsightProfileOptions& options,
                                       std::stop_token stop) {
    namespace fs = std::filesystem;
    NsightProfileResult result;
    auto& operation = result.operation;
    operation.process.cleanup_confirmed = true;
    if(!usable_installation(operation, installation))
        return result;
    const auto& cli = installation.cli;
    if(!cli.path || !cli.help_valid || !matching_tools(cli, installation.capture) ||
       !((cli.version == "2026.3.1.0" && cli.build == "38722833") ||
         (cli.version == "2026.2.0.0" && cli.build == "37991608"))) {
        operation.outcome = NsightOutcome::Unavailable;
        operation.message = "GPU Trace requires matching qualified 2026.3.1.0/38722833 or 2026.2.0.0/37991608 tools";
        return result;
    }
    for(const auto* flag :
        {"--activity", "--platform", "--exe", "--dir", "--output-dir", "--args", "--auto-export", "--architecture",
         "--metric-set-name", "--set-gpu-clocks", "--collect-screenshot", "--max-duration-ms", "--start-after-frames",
         "--start-after-submits", "--limit-to-frames", "--limit-to-submits"}) {
        if(!cli.documented_options.contains(flag)) {
            operation.outcome = NsightOutcome::Unavailable;
            operation.message = std::string("ngfx help does not advertise GPU Trace option ") + flag;
            return result;
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + options.context.timeout;
    fs::path output;
    try {
        validate_profile_settings(options.settings);
        const auto target = executable_path(options.executable);
        const auto directory = directory_path(options.working_directory, "Application working directory");
        const auto arguments = target_arguments(options.arguments);
        output = fresh_file(options.output_directory);
        auto process = process_options(*cli.path, options.context, "profile");
        if(!fs::create_directory(output))
            throw std::runtime_error("Could not claim fresh GPU Trace output directory");
        process.arguments = {"--activity=GPU Trace Profiler",
                             "--platform=Linux (x86_64)",
                             "--exe=" + target.string(),
                             "--dir=" + directory.string(),
                             "--output-dir=" + output.string(),
                             "--start-after-" + options.settings.delimiter + "=" +
                                 std::to_string(options.settings.start_after),
                             "--limit-to-" + options.settings.delimiter + "=" + std::to_string(options.settings.limit),
                             "--max-duration-ms=" + std::to_string(options.settings.duration_ms),
                             "--architecture=" + options.settings.architecture,
                             "--metric-set-name=" + options.settings.metric_set,
                             "--set-gpu-clocks=unaltered",
                             "--collect-screenshot=0",
                             "--auto-export"};
        if(!arguments.empty())
            process.arguments.push_back("--args=" + arguments);
        if(!run_operation(operation, process, stop))
            return result;
    } catch(const std::exception& error) {
        operation.outcome = NsightOutcome::InvalidInput;
        operation.message = error.what();
        return result;
    }
    const auto interrupted = [&] {
        if(!stop.stop_requested() && std::chrono::steady_clock::now() < deadline)
            return false;
        operation.outcome = stop.stop_requested() ? NsightOutcome::Cancelled : NsightOutcome::TimedOut;
        operation.message = "GPU Trace stopped during export validation";
        return true;
    };
    try {
        if(fs::symlink_status(output).type() != fs::file_type::directory)
            throw std::runtime_error("GPU Trace output is not a real directory");
        std::size_t entries = 0;
        std::uint64_t bytes = 0;
        std::vector<fs::path> traces, metadata;
        constexpr std::uint64_t maximum_bytes = 1024ULL * 1024 * 1024;
        for(auto it = fs::recursive_directory_iterator(output); it != fs::recursive_directory_iterator(); ++it) {
            if(interrupted())
                return result;
            if(++entries > 256 || it.depth() > 3 || it->path().lexically_relative(output).native().size() > 384)
                throw std::runtime_error("GPU Trace output exceeds entry/depth/path bounds");
            const auto type = it->symlink_status().type();
            if(type == fs::file_type::directory)
                continue;
            if(type != fs::file_type::regular || fs::hard_link_count(it->path()) != 1)
                throw std::runtime_error("GPU Trace output contains non-regular or multiply linked evidence");
            const auto size = regular_file_size(it->path(), true, maximum_bytes);
            if(size > maximum_bytes - bytes)
                throw std::runtime_error("GPU Trace output exceeds 1 GiB");
            bytes += size;
            if(it->path().extension() == ".ngfx-gputrace")
                traces.push_back(it->path());
            if(it->path().filename() == "REPRO_INFO.xls")
                metadata.push_back(it->path());
        }
        if(traces.size() != 1 || metadata.size() != 1)
            throw std::runtime_error("Expected exactly one GPU Trace and reproduction export");
        regular_file_size(traces.front(), false, maximum_bytes);
        const auto repro =
            parse_profile_reproduction(read_regular_file(metadata.front(), ProfileEvidenceLimits::bytes));
        if(repro.product_version != cli.version + " (build " + cli.build + ") (public-release)")
            throw std::runtime_error("GPU Trace producer differs from selected tools");
        const std::string suffix = options.settings.delimiter == "frames" ? " Frames" : " Submits";
        if(repro.settings.at("GPU Clocks") != "Unaltered" ||
           repro.settings.at("Metric Set") != options.settings.metric_set ||
           repro.settings.at("Multi-Pass Metrics") != "Disabled" ||
           repro.settings.at("Start After") != std::to_string(options.settings.start_after) + suffix ||
           repro.settings.at("Limited To") != std::to_string(options.settings.limit) + suffix ||
           !repro.settings.contains("Max Duration ") ||
           repro.settings.at("Max Duration ") != std::to_string(options.settings.duration_ms) + " ms")
            throw std::runtime_error("GPU Trace exported settings differ from requested collection");
        for(const auto& [name, file, kind] : std::vector<std::tuple<std::string, std::string, ProfileTableKind>>{
                {"frame_duration", "FRAME.xls", ProfileTableKind::FrameDuration},
                {"frame_metrics", "GPUTRACE_FRAME.xls", ProfileTableKind::FrameMetrics},
                {"event_durations", "D3DPERF_EVENTS.xls", ProfileTableKind::EventDurations},
                {"regime_metrics", "GPUTRACE_REGIMES.xls", ProfileTableKind::RegimeMetrics}}) {
            if(interrupted())
                return result;
            const auto path = metadata.front().parent_path() / file;
            (void)parse_profile_table(read_regular_file(path, ProfileEvidenceLimits::bytes), kind);
            result.tables.emplace(name, path);
        }
        if(interrupted())
            return result;
        result.reproduction = repro;
        result.reproduction_file = metadata.front();
        result.trace_file = traces.front();
        operation.output_file = output;
        operation.output_bytes = bytes;
        operation.outcome = NsightOutcome::Success;
        operation.message = "Live GPU Trace and four bounded text exports validated; numeric positions and physical "
                            "scaling are not inferred";
    } catch(const std::exception& error) {
        operation.outcome = NsightOutcome::InvalidOutput;
        operation.message = error.what();
    }
    return result;
}
} // namespace ngm
