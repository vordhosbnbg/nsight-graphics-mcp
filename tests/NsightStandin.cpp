#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
std::string environment(const char* name) {
    const auto* value = std::getenv(name);
    return value == nullptr ? "" : value;
}

std::string value(const std::vector<std::string>& arguments, std::string_view name) {
    for(std::size_t index = 0; index < arguments.size(); ++index) {
        if(arguments[index] == name && index + 1 < arguments.size()) {
            return arguments[index + 1];
        }
        const auto prefix = std::string(name) + '=';
        if(arguments[index].starts_with(prefix)) {
            return arguments[index].substr(prefix.size());
        }
    }
    return {};
}

bool has(const std::vector<std::string>& arguments, std::string_view name) {
    for(const auto& argument : arguments) {
        if(argument == name) {
            return true;
        }
    }
    return false;
}

[[noreturn]] void hang() {
    const auto child = fork();
    if(child < 0) {
        std::exit(7);
    }
    if(child == 0) {
        setsid();
        signal(SIGTERM, SIG_IGN);
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    signal(SIGTERM, SIG_IGN);
    {
        std::ofstream stream(environment("NGM_STANDIN_PIDS"));
        stream << getpid() << '\n' << child << '\n';
    }
    std::cout << "ready\n" << std::flush;
    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int launch_target(const std::vector<std::string>& arguments) {
    const auto executable = value(arguments, "--exe");
    auto working_directory = value(arguments, "--working-dir");
    if(working_directory.empty()) {
        working_directory = value(arguments, "--dir");
    }
    std::vector<std::string> target_arguments{executable};
    std::istringstream tokens(value(arguments, "--args"));
    for(std::string token; tokens >> token;) {
        target_arguments.push_back(std::move(token));
    }
    std::vector<char*> raw;
    for(auto& argument : target_arguments) {
        raw.push_back(argument.data());
    }
    raw.push_back(nullptr);
    const auto child = fork();
    if(child == 0) {
        if(chdir(working_directory.c_str()) != 0) {
            _exit(91);
        }
        execv(executable.c_str(), raw.data());
        _exit(92);
    }
    int status = 0;
    if(child < 0 || waitpid(child, &status, 0) < 0 || !WIFEXITED(status)) {
        return 93;
    }
    return WEXITSTATUS(status);
}
} // namespace

int main(int argc, char** argv) {
    const auto name = std::filesystem::path(argv[0]).filename().string();
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if(name == "target with spaces") {
        std::ofstream stream(environment("NGM_TARGET_RECORD"), std::ios::app);
        stream << "pid=" << getpid() << "\ncwd=" << std::filesystem::current_path().string() << '\n';
        stream << "xdg_data_dirs=" << environment("XDG_DATA_DIRS") << '\n';
        for(const auto& argument : arguments) {
            stream << argument.size() << ':' << argument << '\n';
        }
        std::cout << "target stdout\n";
        std::cerr << "target stderr\n";
        return 0;
    }
    const bool is_cli = name == "ngfx";
    const bool capture = name == "ngfx-capture";
    const auto expected_data_dirs = environment("NGM_EXPECT_XDG_DATA_DIRS");
    if(!expected_data_dirs.empty() && environment("XDG_DATA_DIRS") != expected_data_dirs) {
        std::cerr << "stand-in received unexpected XDG_DATA_DIRS\n";
        return 83;
    }
    if(has(arguments, "--version")) {
        if(environment("NGM_BAD_VERSION_TOOL") == name) {
            std::cout << "not a recognizable version\n";
        } else {
            if(is_cli) {
                std::cout << "NVIDIA (R) Nsight Graphics CLI\n";
            }
            const bool mismatch = environment("NGM_MISMATCH_TOOL") == name;
            const auto version = mismatch ? "2025.1.0.0" : "2026.3.1.0";
            const auto build = environment("NGM_MISMATCH_BUILD") == name ? "99999999" : "38722833";
            std::cout << (is_cli ? "Version " : "Version: ") << version << " (build " << build << ") (stand-in)\n";
        }
        return is_cli || environment("NGM_DISCOVERY_EXIT_ONE_TOOL") == name ? 1 : 0;
    }
    if(has(arguments, "--help") || has(arguments, "--help-all")) {
        if(environment("NGM_BAD_HELP_TOOL") == name) {
            std::cout << "unrelated program --help --version\n";
        } else if(is_cli) {
            std::cout << "NVIDIA Nsight Graphics [general_options] [activity_options]:\n--version --help --help-all\n";
            for(const auto* option : {"--activity", "--platform", "--exe", "--dir", "--output-dir", "--wait-frames",
                                      "--args", "--env", "--no-timeout"}) {
                if(environment("NGM_OMIT_CPP_OPTION") != option) {
                    std::cout << ' ' << option;
                }
            }
            std::cout << '\n';
        } else if(capture) {
            std::cout << "NVIDIA Nsight Graphics Capture CLI Tool\n--version --help --exe --working-dir --args "
                         "--output-file --output-dir --capture-frame --frame-count --delimiter-present "
                         "--delimiter-vk-frame-boundary-ext --delimiter-graphics-capture-api "
                         "--terminate-after-capture";
            if(environment("NGM_OMIT_CAPTURE_NO_BUNDLE_REPLAYER") != "1") {
                std::cout << " --no-bundle-replayer";
            }
            std::cout << '\n';
        } else {
            std::cout << "NVIDIA Nsight Graphics Replayer CLI Tool\n--version --help";
            for(const auto* option : {"--metadata", "--metadata-functions", "--metadata-objects", "--metadata-logs",
                                      "--metadata-screenshot"}) {
                if(environment("NGM_OMIT_REPLAY_OPTION") != option) {
                    std::cout << ' ' << option;
                }
            }
            std::cout << '\n';
        }
        return is_cli || environment("NGM_DISCOVERY_EXIT_ONE_TOOL") == name ? 1 : 0;
    }

    auto mode = environment("NGM_STANDIN_MODE");
    if(const auto* replay_mode = std::getenv("NGM_STANDIN_REPLAY_MODE");
       name == "ngfx-replay" && replay_mode != nullptr) {
        mode = replay_mode;
    }
    if(is_cli && !environment("NGM_STANDIN_CPP_MODE").empty()) {
        mode = environment("NGM_STANDIN_CPP_MODE");
    }
    std::cerr << "stand-in diagnostic only\n";
    if(mode == "hang") {
        hang();
    }
    if(mode == "fail") {
        std::cout << "Version: 2026.3.1.0 (build 38722833)\n";
        return 1;
    }
    if(is_cli) {
        if(value(arguments, "--activity") != "Generate C++ Capture" ||
           value(arguments, "--platform") != "Linux (x86_64)" || !has(arguments, "--no-timeout") ||
           value(arguments, "--env") != "NSIGHT_SUGGEST_GRAPHICS_CAPTURE=0" ||
           value(arguments, "--wait-frames").empty() ||
           !std::filesystem::is_directory(value(arguments, "--output-dir"))) {
            return 85;
        }
        const auto target_exit = launch_target(arguments);
        if(target_exit != 0 || mode == "missing") {
            return target_exit;
        }
        const auto project =
            std::filesystem::path(value(arguments, "--output-dir")) / "CppCaptures/cpu-synthetic-project";
        std::filesystem::create_directories(project);
        for(const auto* file : {"CommandLists.h",        "ReplayProcedures.h",
                                "FrameReset00.cpp",      "ReplayProcedures.cpp",
                                "PerfMarkersReset.cpp",  "PerfMarkersSetup.cpp",
                                "WinResourcesReset.cpp", "WinResourcesSetup.cpp",
                                "Resources.h",           "Resources00.cpp",
                                "CommandList00.cpp",     "Frame0Part00.cpp",
                                "FrameSetup00.cpp",      "ReadOnlyDatabase.cpp",
                                "ReadOnlyDatabase.h",    "DataScope.cpp",
                                "DataScope.h",           "data.bin",
                                "data.bin.rec",          "cpu-synthetic-project.ngfx-cppcap"}) {
            if(mode == "missing-resource" && std::string_view(file) == "data.bin") {
                continue;
            }
            std::ofstream stream(project / file);
            stream << "CPU stand-in generated project; not real Nsight source or serialized resource data\n";
        }
        {
            std::ofstream cmake(project / "CMakeLists.txt");
            cmake << "set(GeneratedReplayHeaders\n    CommandLists.h\n    ReplayProcedures.h\n    Resources.h\n)\n"
                     "add_library(GeneratedReplay ${ReplayExecutorLibraryType}\n"
                     "    CommandList00.cpp\n    Frame0Part00.cpp\n    FrameReset00.cpp\n    FrameSetup00.cpp\n"
                     "    PerfMarkersReset.cpp\n    PerfMarkersSetup.cpp\n    ReplayProcedures.cpp\n"
                     "    Resources00.cpp\n    WinResourcesReset.cpp\n    WinResourcesSetup.cpp\n";
            if(mode == "missing-partition") {
                cmake << "    CommandList01.cpp\n";
            }
            if(mode == "unsafe-cmake-path") {
                cmake << "    ../outside.cpp\n";
            }
            cmake << "\n${GeneratedReplayHeaders})\n";
        }
        if(mode == "missing-header") {
            std::filesystem::remove(project / "CommandLists.h");
        }
        {
            std::ofstream metadata(project / "metadata.json");
            metadata << R"({"metadata_version":1,"nsight_version":"2026.3.1","nsight_version_build_id":)"
                     << (mode == "wrong-producer" ? "42" : "38722833")
                     << R"(,"primary_api":"vulkan","primary_gpu":"CPU stand-in",)"
                        R"("project_filename":"cpu-synthetic-project","has_unsupported_operation":false)"
                     << (mode == "duplicate-key" ? R"(,"metadata_version":1})" : "}");
        }
        if(mode == "ambiguous") {
            std::filesystem::create_directories(project.parent_path() / "second-project");
            std::filesystem::copy_file(project / "metadata.json",
                                       project.parent_path() / "second-project/metadata.json");
        }
        if(mode == "symlink") {
            std::filesystem::create_symlink(environment("NGM_TARGET_RECORD"), project / "linked-data");
        } else if(mode == "fifo") {
            mkfifo((project / "fifo-data").c_str(), 0600);
        } else if(mode == "too-many") {
            for(int index = 0; index < 4100; ++index) {
                std::ofstream(project / ("extra-" + std::to_string(index))) << "synthetic overflow\n";
            }
        }
        std::ofstream image(project / "screenshot.bmp", std::ios::binary);
        image << (mode == "bad-bmp" ? "XX" : "BM") << std::string(52, '\0');
        return 0;
    }
    if(capture) {
        if(!has(arguments, "--terminate-after-capture") || !has(arguments, "--no-bundle-replayer") ||
           value(arguments, "--capture-frame").empty() || value(arguments, "--frame-count").empty() ||
           (!has(arguments, "--delimiter-present") && !has(arguments, "--delimiter-vk-frame-boundary-ext") &&
            !has(arguments, "--delimiter-graphics-capture-api"))) {
            return 81;
        }
        const auto target_exit = launch_target(arguments);
        if(target_exit != 0) {
            return target_exit;
        }
        if(mode == "missing") {
            return 0;
        }
        const auto output = std::filesystem::path(value(arguments, "--output-dir")) / value(arguments, "--output-file");
        if(mode == "symlink") {
            std::filesystem::create_symlink(environment("NGM_TARGET_RECORD"), output);
        } else if(mode == "fifo") {
            mkfifo(output.c_str(), 0600);
        } else {
            std::ofstream stream(output);
            if(mode != "empty") {
                stream << "CPU stand-in capture, not an Nsight capture\n";
            }
        }
        return 0;
    }
    if(mode == "missing" || mode == "empty") {
        return 0;
    }
    if(mode == "oversized") {
        std::cout << std::string(2048, 'x');
        return 0;
    }
    // Optional test-owned raw exports exercise the actual server/process
    // boundary. This stand-in never establishes real producer compatibility.
    for(const auto& [flag, kind] : std::vector<std::pair<std::string, std::string>>{
            {"--metadata", "metadata"}, {"--metadata-functions", "functions"}, {"--metadata-objects", "objects"}}) {
        const auto path = std::filesystem::path(argv[0]).parent_path() / "inspection-exports" / (kind + ".raw");
        if(has(arguments, flag) && std::filesystem::is_regular_file(path)) {
            std::ifstream input(path, std::ios::binary);
            std::cout << std::string(std::istreambuf_iterator<char>(input), {});
            return input.bad() ? 84 : 0;
        }
    }
    if(has(arguments, "--metadata-screenshot")) {
        std::ofstream stream(value(arguments, "--metadata-screenshot"));
        stream << "Opaque stand-in screenshot: no real format claim\n";
    } else if(has(arguments, "--metadata-objects")) {
        std::cout
            << (mode == "malformed"
                    ? "{unparseable JSON"
                    : R"([{"uid":7,"api":"Vulkan","object_name":"synthetic.buffer","type_name":"Buffer","access_flags":32}])");
    } else if(has(arguments, "--metadata-functions")) {
        std::cout << R"([{"event_index":10,"function_name":"CaptureBegin","thread_index":0,"sequence_id":0},)"
                     R"({"event_index":20,"function_name":"vkCmdDraw","thread_index":0},)"
                     R"({"event_index":42,"function_name":"vkQueuePresentKHR","thread_index":0,"sequence_id":4}])";
    } else if(has(arguments, "--metadata-logs")) {
        // Empty is a legitimate log export for a capture with no log entries.
    } else if(has(arguments, "--metadata")) {
        std::cout
            << R"({"metadata_version":1,"nsight_version":"2026.3.1","nsight_version_build_id":"38722833",)"
               R"("primary_api":"Vulkan","process_name":"synthetic-cpu-target",)"
               R"("process_command_line":"synthetic-private-argv","process_environment":["synthetic-private-env"],)"
               R"("_metadata_collection_":{"warnings":["Synthetic stand-in; no GPU or Nsight validation"]}})";
    } else {
        return 82;
    }
    return 0;
}
