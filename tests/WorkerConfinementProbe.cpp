#include "ngm/WorkerConfinement.hpp"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#if __has_include(<linux/landlock.h>)
    #include <linux/landlock.h>
#endif
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
void check(bool ok, const char* message) {
    if(!ok)
        throw std::runtime_error(message);
}
} // namespace
int main(int argc, char** argv) {
    try {
        check(argc == 4, "ROOT OUTSIDE MODE required");
        const std::filesystem::path root(argv[1]);
        const std::string outside(argv[2]), mode(argv[3]);
        const int inherited = open(outside.c_str(), O_RDONLY | O_CLOEXEC);
        check(inherited >= 0, "open synthetic outside sentinel before confinement");
#if defined(LANDLOCK_ACCESS_FS_TRUNCATE) && defined(SYS_landlock_create_ruleset)
        const auto landlock_abi = syscall(SYS_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION);
#else
        const long landlock_abi = -1;
#endif
        try {
            ngm::confine_resource_worker(root / "data.bin", root / "data.bin.rec");
        } catch(const std::exception& error) {
            std::cerr << error.what() << '\n';
            // A setup regression on a capable host is a failure, not a skip.
            return landlock_abi < 3 ? 77 : 1;
        }
        if(mode == "cpu") {
            for(;;)
                asm volatile("" ::: "memory");
        }
        if(mode == "output") {
            const std::string block(4096, 'x');
            for(;;) {
                if(write(STDOUT_FILENO, block.data(), block.size()) < 0)
                    return 3;
            }
        }
        check(mode == "probe", "known mode");
        char byte = 0;
        check(read(inherited, &byte, 1) == -1 && errno == EBADF, "inherited descriptor closed");
        const int input = open((root / "data.bin").c_str(), O_RDONLY | O_CLOEXEC);
        check(input >= 0 && read(input, &byte, 1) == 1 && byte == 'a', "allowed input read");
        check(write(input, "z", 1) == -1 && errno == EPERM, "only stdout/stderr writes allowed");
        close(input);
        check(open(outside.c_str(), O_RDONLY) == -1 && errno == EACCES, "outside contents denied");
        check(open((root / "data.bin").c_str(), O_WRONLY | O_TRUNC) == -1 && errno == EACCES, "input writes denied");
        check(open((root / "new-file").c_str(), O_WRONLY | O_CREAT, 0600) == -1 && errno == EACCES,
              "file creation denied");
        check(socket(AF_INET, SOCK_STREAM, 0) == -1 && errno == EPERM, "network denied");
        check(syscall(SYS_fork) == -1 && errno == EPERM, "fork denied");
        check(syscall(SYS_execve, "/synthetic", nullptr, nullptr) == -1 && errno == EPERM, "exec denied");
        check(syscall(SYS_ptrace, 0, 0, 0, 0) == -1 && errno == EPERM, "ptrace denied");
        check(syscall(SYS_kill, 0, 0) == -1 && errno == EPERM, "signals denied");
        check(syscall(SYS_ioctl, 1, 0, 0) == -1 && errno == EPERM, "ioctl denied");
        check(syscall(SYS_getpid | 0x40000000) == -1 && errno == EPERM, "x32 denied");
        check(mmap(nullptr, 4096, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == MAP_FAILED &&
                  errno == EPERM,
              "executable allocation denied");
        check(mmap(nullptr, 512U * 1024U * 1024U, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) ==
                      MAP_FAILED &&
                  errno == ENOMEM,
              "memory bounded");
        const rlimit unlimited{RLIM_INFINITY, RLIM_INFINITY};
        check(setrlimit(RLIMIT_AS, &unlimited) == -1 && errno == EPERM, "limits cannot be raised");
        std::cout << "confined\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
