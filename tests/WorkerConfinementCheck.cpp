#include "Check.hpp"
#include "ngm/File.hpp"
#include "ngm/Process.hpp"

#include <csignal>
#include <fstream>
#include <unistd.h>

int main(int argc, char** argv) {
    bool unavailable = false;
    const int result = ngm::check::run([&] {
        using ngm::check::require;
        require(argc == 2, "PROBE required");
        const auto executable = std::filesystem::absolute(argv[1]);
        std::string pattern = (std::filesystem::temp_directory_path() / "ngm-confinement-XXXXXX").string();
        const char* created = mkdtemp(pattern.data());
        require(created != nullptr, "isolated test root");
        const std::filesystem::path root(created);
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::filesystem::remove_all(path);
            }
        } cleanup{root};
        std::filesystem::create_directory(root / "input");
        std::ofstream(root / "input/data.bin") << "allowed";
        std::ofstream(root / "input/data.bin.rec") << "records";
        std::ofstream(root / "outside") << "sentinel";
        const auto run = [&](const std::string& mode) {
            ngm::ProcessOptions options;
            options.executable = executable;
            options.arguments = {(root / "input").string(), (root / "outside").string(), mode};
            options.stdout_path = root / (mode + ".out");
            options.stderr_path = root / (mode + ".err");
            options.timeout = std::chrono::seconds(10);
            return ngm::run_process(options);
        };
        const auto probe = run("probe");
        require(probe.cleanup_confirmed && !probe.timed_out, "probe cleaned up");
        if(probe.exit_code == 77) {
            require(ngm::read_regular_file(root / "probe.err", 4096).find("confinement unavailable") !=
                        std::string::npos,
                    "unsupported host fails closed with diagnosis");
            std::cout << "Confinement unavailable; rejection verified, enforcement unqualified on this host\n";
            unavailable = true;
            return;
        }
        require(probe.exit_code == 0, ngm::read_regular_file(root / "probe.err", 4096));
        require(ngm::read_regular_file(root / "probe.out", 4096) == "confined\n", "probe completed all checks");
        require(ngm::read_regular_file(root / "outside", 4096) == "sentinel", "outside input unchanged");
        require(ngm::read_regular_file(root / "input/data.bin", 4096) == "allowed", "allowed input immutable");
        const auto output = run("output");
        require(output.cleanup_confirmed && !output.timed_out && output.signal == SIGXFSZ,
                "output limit terminates worker");
        require(std::filesystem::file_size(root / "output.out") == 128U * 1024U, "output size cap enforced");
        const auto cpu = run("cpu");
        require(cpu.cleanup_confirmed && !cpu.timed_out && cpu.signal == SIGKILL, "CPU limit terminates worker");
        // Only child processes were restricted. Parent retains normal file access.
        std::ofstream(root / "parent-writable") << "ok";
        require(ngm::read_regular_file(root / "parent-writable", 4096) == "ok", "parent remains usable");
    });
    return result == 0 && unavailable ? 77 : result;
}
