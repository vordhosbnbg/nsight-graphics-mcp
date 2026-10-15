# Confined descriptor write worker

The **0.5.7 R-024 foundation** adds a separate optional worker for source-scoped
descriptor writes on matching **2026.3.1.0/build 38722833**, Linux x86-64.
The owner explicitly approved its two-file extension and bounds instrumentation
on **2026-10-15**; [DESCRIPTOR_WORKER_PLAN.md](DESCRIPTOR_WORKER_PLAN.md) records
the exact scope. The existing five-file byte worker stays independent.

This page records the **0.5.7 worker foundation**. The subsequent **0.5.8**
[parent/query path](DESCRIPTOR_WRITES.md) exposes source-scoped writes through
MCP with exact profile checks and retained failures. R-024 still needs
update/copy/resource lifetimes, actual shader type/layout evidence and its fresh
repair acceptance before execution-time values can be claimed.

## Source build

Select an exact matching C++ export containing the existing five database/scope
helpers plus `VulkanStructHydrator.cpp/.h`. The header may be in the export's
`include` directory. The build needs the ordinary source-build prerequisites,
pinned Vulkan headers and `diff` from diffutils. No shader compiler, Vulkan
loader, GPU or active Nsight process is needed to run the worker.

```sh
cmake --preset linux-gcc-debug \
  -DNGM_DESCRIPTOR_HELPERS_2026_3=/absolute/matching/CppCapture/project
cmake --build --preset linux-gcc-debug --target ngm-descriptor-worker-2026_3
build/linux-gcc-debug/ngm-descriptor-worker-2026_3 --profile
cmake --build --preset linux-gcc-debug \
  --target ngm_descriptor_hydrator_integration_run ngm_descriptor_bounds_check_run
```

These commands and targets pass with the retained matching source used for
qualification; the configure path is a placeholder. The optional helper target
and its integration check are excluded from default builds and CTest. The pure
first-party bounds check belongs to the ordinary CPU suite. An absent option
leaves the proprietary helper unbuilt.

Configure copies and verifies all seven original fingerprints into an isolated
build directory. Only the exact database/scope helpers and instrumented hydrator
are compiled, using static helper libraries and pinned headers. It retains the
original sources, derived sources, unified instrumentation patch and all hashes.
The profile also identifies the bounds header and patch recipe. A changed helper
fingerprint refuses at configure, before compilation. No generated application,
replay source or build script is compiled, and no dependency is downloaded.

## Execution contract

Launch through the owned process supervisor with an empty environment, immutable
private `data.bin`/`data.bin.rec`, `/dev/null` stdin and regular stdout/stderr
files. The arguments are `SNAPSHOT HANDLE WRITE_COUNT OUTPUT_BYTES`; all values
come from verified retained source. `OUTPUT_BYTES` is the source-declared output
allocation, distinct from the database's actual input length. The supervisor
must provide a bounded wall deadline and confirm cleanup.

Confinement runs before database construction: existing fail-closed Landlock/
seccomp, descriptor closure, `no_new_privs`, disabled dumpability/core dumps and
the [resource worker's process limits](RESOURCE_WORKER.md#execution-contract).
Payload parsing never falls back to unconfined execution.

Input-relative arithmetic, extent, alignment, copy multiplication and output
capacity are checked on every generated pointer/copy operation. Assertions are
enforced; nonempty `pNext` chains refuse before traversal. Database blocks are
copied as opaque bytes into aligned storage before hydration, preserving the
helper's relative-offset contract. This handles the measured four-byte-aligned
database blocks without permitting unaligned typed access. The helper still owns
the serialization grammar.

| Worker bound | Limit |
| --- | ---: |
| Actual serialized input | 16 MiB |
| Source-declared output allocation | 64 KiB |
| Top-level writes | 64 |
| Descriptors per write / total returned | 128 / 512 |
| Translated objects / pointer-copy operations | 1,024 / 1,024 |
| Successful JSON response | Less than 64 KiB |

Supported output types are uniform/storage buffers, their dynamic variants and
combined image samplers. Object callbacks use distinct artificial tokens and
return capture-source serialized object type/ID pairs; tokens never reach
Vulkan. A parent must validate every returned pair against exact source
registrations before mapping it to a symbol. Missing/unknown IDs or uncertain
lifetimes cannot become execution-time evidence. Null image/sampler entries
remain null; the worker does not infer immutable samplers or shader access.

Success writes exactly one JSON object plus newline, with input length/hash,
source alignment remainder, source handle/count/output capacity, used output
bytes and descriptor fields. It exposes no live addresses. No output is emitted
until hydration and full response-size/type checks finish. Nonzero exit, signal,
timeout, unexpected diagnostics, partial response or failed cleanup must refuse.
The 0.5.8 parent validates this response before its bounded source query returns
fields; [DESCRIPTOR_WRITES.md](DESCRIPTOR_WRITES.md) defines that later contract.

## Qualification

The optional real-helper CPU check passes single buffer, two-write and two-image
array cases, with artificial-token checks and independent expected fields.
Truncated data, insufficient output, overflowing/unaligned pointers, excessive
counts, unsupported `pNext` and unexpected callback IDs refuse. The ordinary
bounds check additionally exercises overlapping/oversized windows, one-past-end
pointers, output alignment and the operation limit. Existing confinement checks
exercise allowed/denied syscalls, file access and process limits independently.

Ten actual retained 0.5.6 C++ inputs pass confined reads and exact comparison to
the independent existing 0.4.2 byte worker. Each result matches its source call's
handle/count/capacity and source-registered object identities. Seven original
database pointers have alignment remainder four; aligned relocation preserves
all input bytes. All ten source/input hashes remain unchanged. Four invalid
argument/missing-handle controls refuse with no stdout and confirmed cleanup.
A changed-helper configure control also refuses before compiling a worker.
I-076 retains the initial alignment failure and its correction. These checks
establish this worker boundary and measured descriptor writes, without proving
selected-draw lifetimes, resource values or GPU execution correspondence.

The **41/41** GCC Debug CPU aggregate passes in **400.32 seconds**, including
the new bounds check, unchanged confinement checks and shared MCP/HTTP workflows.
The final worker SHA-256 is
`375272fed5ce3936005e25e1a37277b4c9c92cd549f4da1d6d9c0c176fc9337e`.
ELF inspection shows only the documented compiler/OS runtime dependencies;
the static helper archive contains exactly database, scope and hydrator objects.
No new GPU collection, installed worker/server activation or machine change is
performed. [BUILD_VALIDATION.md](BUILD_VALIDATION.md) records retained evidence.

Complete/pinned **bundle-8e3eb665326adc38968783d5809c7973** retains **336** verified
payloads, including the lossless **278-member** worker snapshot and all failed
controls. Its imported portable verifier also checks 40 exact source/input files
against the pinned 0.5.6 capture ancestor. All source/imported lengths and hashes
agree. R-024 remains In Progress; this worker is its first implementation slice.
