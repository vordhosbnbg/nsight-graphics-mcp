#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <string>
#include <string_view>
#include <sys/ptrace.h>
#include <thread>
#include <unistd.h>

namespace {
void sleep_for(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void record_pid(int descriptor) {
    const auto line = std::to_string(getpid()) + '\n';
    if(write(descriptor, line.data(), line.size()) != static_cast<ssize_t>(line.size())) {
        _exit(91);
    }
}

int descendants(const char* path, bool leader_exits, bool escape) {
    struct sigaction action{};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, nullptr);
    const int descriptor = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
    int ready[2];
    if(descriptor < 0 || pipe(ready) != 0) {
        return 92;
    }
    record_pid(descriptor);
    const auto child = fork();
    if(child < 0) {
        return 93;
    }
    if(child == 0) {
        close(ready[0]);
        if(escape && setsid() < 0) {
            _exit(94);
        }
        record_pid(descriptor);
        const auto grandchild = fork();
        if(grandchild < 0) {
            _exit(95);
        }
        if(grandchild == 0) {
            record_pid(descriptor);
        }
        const char byte = 'R';
        if(write(ready[1], &byte, 1) != 1) {
            _exit(96);
        }
        close(ready[1]);
        close(descriptor);
        sleep_for(10000); // Finite even if a broken runner forgets cleanup.
        _exit(0);
    }
    close(ready[1]);
    close(descriptor);
    int count = 0;
    char byte;
    while(count < 2) {
        const auto read_count = read(ready[0], &byte, 1);
        if(read_count == 1) {
            ++count;
        } else if(read_count < 0 && errno == EINTR) {
            continue;
        } else {
            return 97;
        }
    }
    close(ready[0]);
    std::cout << "ready\n" << std::flush;
    if(!leader_exits) {
        sleep_for(10000);
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if(argc < 2) {
        return 2;
    }
    const std::string_view mode(argv[1]);
    if(mode == "echo") {
        std::cout << "cwd=" << std::filesystem::current_path().string() << '\n';
        const char* value = std::getenv("NGM_PROCESS_VALUE");
        const char* secret = std::getenv("NGM_PROCESS_PARENT_SECRET");
        std::cout << "env=" << (value == nullptr ? "<unset>" : value) << '\n';
        std::cout << "secret=" << (secret == nullptr ? "<unset>" : secret) << '\n';
        char byte;
        std::cout << "stdin_eof=" << (read(STDIN_FILENO, &byte, 1) == 0) << '\n';
        for(int index = 2; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            std::cout << argument.size() << ':' << argument << '\n';
        }
        std::cerr << "standin stderr\n";
        return 0;
    }
    if(mode == "check-fd" && argc == 3) {
        errno = 0;
        return fcntl(std::stoi(argv[2]), F_GETFD) == -1 && errno == EBADF ? 0 : 3;
    }
    if(mode == "exit" && argc == 3) {
        std::cout << "early stdout\n";
        std::cerr << "early stderr\n";
        return std::stoi(argv[2]);
    }
    if(mode == "signal") {
        raise(SIGUSR1);
        return 4;
    }
    if(mode == "trace-stop") {
        if(ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0) {
            std::cerr << "PTRACE_TRACEME failed: " << errno << '\n';
            return 92;
        }
        std::cout << "pid=" << getpid() << "\nready\n" << std::flush;
        raise(SIGSTOP);
        return 6; // The runner must time out and kill this stopped target.
    }
    if(mode == "output" && argc == 4) {
        const std::string_view kind(argv[3]);
        if(kind != "missing") {
            std::ofstream output(argv[2]);
            output << (kind == "malformed" ? "{ malformed export\n" : "{\"ok\":true}\n");
            if(!output) {
                return 5;
            }
        }
        return 0;
    }
    if(mode == "sleep" && argc == 3) {
        std::cout << "ready\n" << std::flush;
        sleep_for(std::stoi(argv[2]));
        return 0;
    }
    if(mode == "descendants" && argc == 5) {
        return descendants(argv[2], std::string_view(argv[3]) == "exit", std::string_view(argv[4]) == "escape");
    }
    return 2;
}
