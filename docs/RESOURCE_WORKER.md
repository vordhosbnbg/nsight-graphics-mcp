# Confined generated-resource worker

Version 0.2.11 adds the worker boundary for R-007's general resource queries.
Two optional executables use the unchanged, fingerprint-qualified generated
`ReadOnlyDatabase` and `DataScope` helpers. They accept new database contents and
resource handles; no capture ID or database hash is compiled into them. At the 0.2.11 milestone the MCP
surface remained 20 tools. Version 0.2.12 adds source-reference listing, leased
snapshots, parent response validation and MCP byte queries; see
[RESOURCE_QUERIES.md](RESOURCE_QUERIES.md).

## Build and dependency boundary

Generate a C++ capture with a matching qualified Nsight release, then select its
project directory explicitly. The five required files are `ReadOnlyDatabase.cpp`,
`ReadOnlyDatabase.h`, `DataScope.cpp`, `DataScope.h`, and `DllCommon.h`. Configure
accepts only the exact source fingerprints in `cmake/ResourceWorker.cmake` for
2026.3.1.0/build 38722833 or 2026.2.0.0/build 37991608. It copies those files to an
isolated build directory, hashes the copies, and compiles only that closure.
Changed helper inputs trigger reconfiguration. No generated build script,
application source, Vulkan replay code, runtime compiler, or dependency download
is used. The helper libraries link statically into the worker executables.

These optional sources are outputs of the separately installed Nsight toolchain,
like the separately supplied NGFX SDK headers. They are a documented exception
to the project dependency submodule policy, not a replacement for pinned vendored
dependencies. Generated proprietary sources stay outside version control. Default
builds require neither helper profile. Optional worker builds require Linux x86-64
and Linux UAPI headers providing Landlock ABI 3 (Linux 6.2 or newer).

```sh
cmake --preset linux-gcc-debug \
  -DNGM_RESOURCE_HELPERS_2026_3=/absolute/2026.3/CppCapture/project \
  -DNGM_RESOURCE_HELPERS_2026_2=/absolute/2026.2/CppCapture/project
cmake --build --preset linux-gcc-debug \
  --target ngm-resource-worker-2026_3 ngm-resource-worker-2026_2
build/linux-gcc-debug/ngm-resource-worker-2026_3 --profile
cmake --build --preset linux-gcc-debug --target ngm_worker_confinement_check_run
```

The configure paths above are placeholders for local generated outputs; the
recorded validation uses retained captures. `--profile` reports the exact compiled
helper hashes, producer profile and product version. It does not parse a database
or establish that runtime confinement is available.

## Execution contract

This is an internal worker interface, not a user-facing capture command. Launch
it through `run_process` with an empty environment, `/dev/null` stdin, regular
write-only stdout/stderr logs, a private immutable input snapshot, and a bounded
wall-clock deadline. Arguments are `SNAPSHOT HANDLE OFFSET LENGTH DECLARED_BYTES`;
the snapshot must contain `data.bin` and `data.bin.rec`. Zero declared bytes means
unknown; otherwise the helper's reported size must match. The service derives that declaration
from source evidence rather than trusting an MCP caller.

The worker applies all restrictions before constructing the database reader or
parsing payloads. It closes inherited descriptors above stderr, disables core
dumps and dumpability, sets `no_new_privs`, grants Landlock read access to only
the two regular input files, and installs an architecture-checked seccomp allowlist.
There is no fallback to unconfined execution. Setup failure exits with diagnostics.
The runtime requires Landlock ABI 3 or newer and seccomp filtering.

| Bound | Limit |
| --- | ---: |
| Database / record-file input | 256 MiB / 16 MiB |
| Helper-reported resource | 16 MiB |
| Returned byte range | 64 KiB |
| Process address space | 256 MiB |
| CPU time | 3 seconds |
| Each regular output file | 128 KiB |
| Open file descriptors | 32 |

Network, process creation, exec, ptrace, ioctl, unrelated-process signals,
filesystem mutation, and creation of executable memory are denied. Writes are
limited to stdout/stderr. Allowed `stat` and `readlink` operations can still expose
path metadata; the boundary restricts file contents, not all metadata visibility.
The output-file limit does not bound a pipe or terminal. The supervisor supplies
wall time and cleanup; CPU and file limits alone are insufficient.

Successful stdout is one JSON header line followed by exactly `returned_bytes`
opaque bytes. The header contains schema version, helper profile, handle, offset,
total bytes, and returned bytes. An offset at the resource end returns zero bytes.
Missing, oversized and mismatched resources fail. Nonzero exit, signal, timeout,
partial output or failed cleanup must never be accepted as resource evidence.
The parent independently validates the entire bounded response and
hash the exact input/output bytes. This worker does not authenticate data, detect
every corrupt database, resolve descriptors, execute Vulkan, or reconstruct
resource contents after a GPU event.

## Validation

The CPU confinement check tests permitted reads and denied outside reads, writes,
network, fork/exec, ptrace, signals, ioctl, x32 syscalls and executable mappings.
It verifies inherited descriptor closure, memory and output limits, CPU-limit
termination, cleanup, unchanged inputs and an unaffected parent. Missing Landlock
support is a CTest skip after verifying refusal, not passing enforcement. Setup
failure on a capable host fails the check. An independent extended probe also
checks descriptor duplication, `io_uring`, `memfd`, `sendfile`, `/proc` contents,
directory reads and executable `mprotect` denial.

Real helper qualification and final evidence references are recorded in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md). The retained-input tests require no GPU
execution. Their byte comparisons establish these reader profiles' behavior,
separately from the subsequent MCP resource-access qualification.

The final GCC Debug aggregate passes 23/23 with no skips. Fresh-context acceptance
audits all 636 reader invocations: 614 successful responses and 22 expected
failures. Of those successes, 602 responses match retained byte oracles across
127 resources and 42 distinct captures; the other 12 check range consistency on
two larger resources. Independent argument/range checks add 18 passing cases.
Both profiles were exercised on Linux 7.2.6-zen2-1-zen, x86-64, GCC 16.2.1.

Qualification is pinned as `bundle-78b10c057ee352241bb5eb276b22ff11` in
`artifacts/nsight-resource-worker-evidence`: 2,837 payload files, 72,393,983 payload
bytes, 72,776,686 bytes including bundle metadata. It retains implementation and
helper snapshots, final worker executables, process logs/results, independent
reviews, and 42 referenced pinned source captures in their original stores.
The two intentional symlink inputs are recorded as link metadata rather than
followed into the snapshot. Publication verification checks all inventoried
payload hashes, implementation identities, source manifest hashes and pins.

The interface uses NVIDIA's documented generated resource helper, not a private
database decoder. Primary references: [Generate C++ Capture](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html),
[Landlock](https://docs.kernel.org/userspace-api/landlock.html), and
[seccomp filtering](https://docs.kernel.org/userspace-api/seccomp_filter.html).
