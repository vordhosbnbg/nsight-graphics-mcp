#include "ngm/WorkerConfinement.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#if __has_include(<linux/landlock.h>)
    #include <linux/landlock.h>
#endif
#include <linux/seccomp.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace ngm {
#if defined(LANDLOCK_ACCESS_FS_TRUNCATE) && defined(SYS_landlock_create_ruleset) && defined(SYS_landlock_add_rule) &&  \
    defined(SYS_landlock_restrict_self)
namespace {
void require(bool ok, const char* operation) {
    if(!ok)
        throw std::runtime_error(std::string("Resource worker confinement unavailable: ") + operation + " (errno " +
                                 std::to_string(errno) + ')');
}
struct Descriptor {
    int value;
    ~Descriptor() {
        if(value >= 0)
            close(value);
    }
};
void limit(int kind, rlim_t value) {
    const rlimit limits{value, value};
    require(setrlimit(kind, &limits) == 0, "setrlimit");
}
void allow_file(int rules, const std::filesystem::path& path, std::uint64_t maximum_bytes) {
    Descriptor file{open(path.c_str(), O_PATH | O_CLOEXEC | O_NOFOLLOW)};
    require(file.value >= 0, "open input");
    struct stat status{};
    require(fstat(file.value, &status) == 0 && S_ISREG(status.st_mode) && status.st_size >= 0 &&
                static_cast<std::uint64_t>(status.st_size) <= maximum_bytes,
            "bounded regular input required");
    landlock_path_beneath_attr rule{};
    rule.allowed_access = LANDLOCK_ACCESS_FS_READ_FILE;
    rule.parent_fd = file.value;
    require(syscall(SYS_landlock_add_rule, rules, LANDLOCK_RULE_PATH_BENEATH, &rule, 0) == 0, "landlock_add_rule");
}
void syscall_filter() {
    #if defined(__x86_64__) && !defined(__ILP32__)
    std::vector<sock_filter> code;
    const auto statement = [&](unsigned short op, std::uint32_t value) { code.push_back(BPF_STMT(op, value)); };
    const auto jump = [&](std::uint32_t value, unsigned char yes, unsigned char no) {
        code.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, value, yes, no));
    };
    statement(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, arch));
    jump(AUDIT_ARCH_X86_64, 1, 0);
    statement(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS);
    statement(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr));
    // Unknown calls (including the x32 ABI) fail closed. No networking, process
    // creation/exec, ioctl, ptrace, cross-process signals or filesystem mutations.
    for(const auto call :
        {SYS_read, SYS_close, SYS_fstat, SYS_newfstatat, SYS_lseek, SYS_openat, SYS_readlink, SYS_readlinkat, SYS_brk,
         SYS_munmap, SYS_madvise, SYS_futex, SYS_rt_sigaction, SYS_rt_sigprocmask, SYS_rt_sigreturn, SYS_clock_gettime,
         SYS_getrandom, SYS_exit, SYS_exit_group}) {
        jump(call, 0, 1);
        statement(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
    }
    // Only pre-opened output streams can be written. No dup/fcntl/open-by-handle
    // calls exist in the filter, and Landlock refuses writable opens.
    jump(SYS_write, 0, 4);
    statement(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[0]));
    jump(STDOUT_FILENO, 1, 0);
    jump(STDERR_FILENO, 0, 1);
    statement(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
    // Reload nr after the write argument checks. Writable executable mappings
    // are unnecessary; deny all creation of executable memory.
    statement(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr));
    jump(SYS_mmap, 1, 0);
    jump(SYS_mprotect, 0, 4);
    statement(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[2]));
    statement(BPF_ALU | BPF_AND | BPF_K, PROT_EXEC);
    jump(0, 0, 1);
    statement(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
    statement(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM);
    sock_fprog program{static_cast<unsigned short>(code.size()), code.data()};
    require(prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0, "seccomp filter");
    #else
    throw std::runtime_error("Resource worker confinement requires Linux x86-64");
    #endif
}
} // namespace

void confine_resource_worker(const std::filesystem::path& database, const std::filesystem::path& records) {
    require(syscall(SYS_close_range, 3U, ~0U, 0U) == 0, "close inherited descriptors");
    require(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0, "no_new_privs");
    require(prctl(PR_SET_DUMPABLE, 0) == 0, "disable dumpability");
    limit(RLIMIT_CORE, 0);
    limit(RLIMIT_AS, 256U * 1024U * 1024U);
    limit(RLIMIT_CPU, 3);
    limit(RLIMIT_FSIZE, 128U * 1024U);
    limit(RLIMIT_NOFILE, 32);
    require(syscall(SYS_landlock_create_ruleset, nullptr, 0, LANDLOCK_CREATE_RULESET_VERSION) >= 3,
            "Landlock ABI >= 3 required");
    // ABI 3 filesystem rights. Additional filesystem operations are denied by
    // the syscall allowlist, independently of future Landlock ABI extensions.
    landlock_ruleset_attr rules{};
    rules.handled_access_fs =
        LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_READ_FILE |
        LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
        LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |
        LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
        LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_REFER | LANDLOCK_ACCESS_FS_TRUNCATE;
    Descriptor descriptor{static_cast<int>(syscall(SYS_landlock_create_ruleset, &rules, sizeof(std::uint64_t), 0))};
    require(descriptor.value >= 0, "landlock_create_ruleset");
    allow_file(descriptor.value, database, 256U * 1024U * 1024U);
    allow_file(descriptor.value, records, 16U * 1024U * 1024U);
    require(syscall(SYS_landlock_restrict_self, descriptor.value, 0) == 0, "landlock_restrict_self");
    syscall_filter();
}
#else
void confine_resource_worker(const std::filesystem::path&, const std::filesystem::path&) {
    throw std::runtime_error("Resource worker confinement unavailable: build needs Linux headers with Landlock ABI 3");
}
#endif
} // namespace ngm
