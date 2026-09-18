# Build and development validation

## R-013 source-build foundation

Date: 2026-09-17. Product version: `0.0.1`.
Scope: the source-build foundation, with CPU-only library/compiler probes and
version-reporting entry points. No capture, rendering, or Codex interoperability
result is claimed here. The [source pins](DEPENDENCIES.md) identify all dependency
inputs; first-party files are the accompanying R-013 change.

## Toolchain policy and tested configurations

First-party C++20 is selected for standard lifetime/concurrency/data utilities,
including `std::span` and the check harness's `std::source_location`.
Vendored fastmcpp/glslang use their upstream C++17 settings. CMake 3.25 is the
minimum for preset schema 6 and `add_subdirectory(... SYSTEM)`; 3.25.3 was tested
explicitly. The tested compiler baseline is GCC 15.3 / Clang 22.1 with libstdc++;
older versions are not qualified, rather than assigned an untested compatibility
claim. CMake also enforces the C++20 feature requirement for first-party targets.

Host: Arch Linux x86-64; Ninja 1.13.2; GNU binutils; Git 2.55.0. No system
dependency packages, installed shader compiler, display, GPU, or Nsight were used
by the build/check paths.

| Configuration | Compiler | CMake | Python selected by glslang | Result |
| --- | --- | --- | --- | --- |
| `linux-gcc-debug` | GCC 16.2.1 (20260810) | 4.4.3 | 3.14.7 | Build and all 7 checks passed. |
| Clean `linux-clang-debug` snapshot | Clang 22.1.8, libstdc++ | 3.25.3 | 3.12.14 | Network-isolated configure/build and all 7 checks passed. |
| `linux-gcc-release`, explicit `gcc-15`/`g++-15` | GCC 15.3.0 | 4.4.3 | 3.14.7 | Network-isolated build and all 7 checks passed. |

GCC 15.3 Release emitted upstream glslang `-Wmaybe-uninitialized` warnings in
`TType::shallowCopy` during qualifier parsing. Vendored sources were not changed
or warnings suppressed; first-party warnings remain errors. The build and shader
checks passed. This is not a claim that all upstream shader paths were tested.

The older CMake host tool was obtained from Kitware's official 3.25.3 release and
verified against its published SHA-256 list. The x86-64 archive hash was
`d4d2ba83301b215857d3b6590cd4434a414fa151c5807693abe587bd6c03581e`.
This is a host build tool, not a substitute for source-built project dependencies.

## Commands and evidence

The working-tree GCC debug build used:

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
cmake --build --preset linux-gcc-debug --target ngm_dependency_check_run
ctest --preset linux-gcc-debug
```

Because the repository started with no commits, clean-checkout validation used a
temporary local snapshot repository containing the first-party build inputs and
the six exact gitlinks. A fresh clone recursively initialized those submodules
from local source mirrors; its `git status --porcelain` was empty. This avoided
committing the user's working tree and did not reuse any build outputs.
Configuration, compilation, shader generation, and aggregate checks then ran with
networking unavailable, using the CMake 3.25.3 executable:

```sh
unshare --user --map-root-user --net cmake --preset linux-clang-debug
unshare --user --map-root-user --net cmake --build --preset linux-clang-debug
unshare --user --map-root-user --net cmake --build --preset linux-clang-debug --target ngm_check
```

`cmake` in this block denotes the verified 3.25.3 host tool. Network isolation is
a validation technique, not an ordinary build prerequisite. Normal source builds
need only the instructions in [README.md](../README.md).

The Release configuration used the second installed GCC toolchain, with the
network namespace applied to configure, build, and aggregate checks:

```sh
unshare --user --map-root-user --net cmake --preset linux-gcc-release -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15
unshare --user --map-root-user --net cmake --build --preset linux-gcc-release
unshare --user --map-root-user --net cmake --build --preset linux-gcc-release --target ngm_check
```

The bootstrap versions both reported `0.0.1`. glslang reported 16.4.0, GLSL 4.60,
and SPIR-V 1.6. `ngm_shader_check` inspected actual generated instructions for
retained source/line data. A malformed shader was compiled over an earlier good
output; the build helper failed and removed that output, then recovered with good
source. None of this establishes Nsight shader-source inspection.

`ngm_linkage_check` inspected both entry points, the dependency-call consumer,
glslang, and the fastmcpp/volk/glslang/resource-limits archives. The observed shared
libraries were `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`, and the
ELF loader, with no vendored shared dependencies. `ldd` and file-open tracing also
confirmed the current runtime boundary:

```sh
ldd build/linux-gcc-debug/nsight-graphics-mcp
strace -f -e trace=openat -o build/linux-gcc-debug/version-runtime.log build/linux-gcc-debug/nsight-graphics-mcp --version
strace -f -e trace=openat -o build/linux-gcc-debug/dependency-runtime.log build/linux-gcc-debug/tests/ngm_dependency_check
```

Raw CTest/linkage/runtime logs are disposable ignored build outputs. These are
build checks, not Nsight investigation or visual-verification evidence bundles.
The reproducible commands and source pins are the retained record. Runtime-loaded
Vulkan/driver/desktop/Nsight libraries are discussed separately in
[DEPENDENCIES.md](DEPENDENCIES.md); their live validation was pending at R-013.

## R-013/R-005/R-003 group completion

Date: 2026-09-18. Product version: **0.1.0**. This minor increment completes the
related build, deterministic basic fixture, and local stdio integration group.
It does not declare the first visual-debugging release complete.

The GCC 16.2.1 Debug preset passes all **13 CPU checks** after review corrections
(39.02 seconds for the final aggregate run). The source-built glslang remains
16.4.0 with Vulkan 1.3/SPIR-V 1.6 and `-g -Od`. Focused checks were used during
correction; the final group check was:

```sh
cmake --build --preset linux-gcc-debug --target ngm_check
```

The opt-in `ngm_fixture_validation_run` passes 17 fresh windowed launches on
RTX 3080 Ti / NVIDIA 615.71.09 / KDE Wayland through Xwayland and XCB 1.17.0.
Each scenario/configuration pair repeats exactly and matches an independent
analytic image oracle; synchronization validation is active and clean. A shader
override preserves executable identity while changing shader-bundle metadata.
See [FIXTURE.md](FIXTURE.md) for the matrix, tolerances, and retained run reports.

Codex CLI 0.154.0 launches the corrected version 0.1.0 server, negotiates MCP
2025-06-18, discovers `capabilities`, completes one real call, and exits cleanly.
The subprocess protocol checks cover malformed input, invalid parameters, size
and nesting limits, lifecycle, output isolation, and EOF. [MCP.md](MCP.md) records
the exact client setup, library adapter boundaries, and local logs.

Separate fresh-context reviewers examined fixture/provenance, process ownership,
and protocol code. Their findings led to corrections for atfork deadlocks and
ptrace stops, unbounded JSON nesting/raw NUL, protocol negotiation/tool errors,
executable hashing races, presentation lifetime, and incomplete retained metadata.
Correction reviews included independent subprocess probes; the final provenance
review verified all 17 retained GPU records and manifest/inline identity agreement.
The last strict retained-schema issue has a process-boundary CPU regression.

Binary linkage checks include the real server, renderer, and experiment runner.
A successful fixture reference run under file-open tracing records the runtime
Vulkan/NVIDIA/desktop dependencies in [DEPENDENCIES.md](DEPENDENCIES.md).
The new implementation is qualified here on GCC Debug only; R-013's separate
Clang/Release/clean-source results above do not qualify all later code on those
configurations. Full delivery qualification remains R-014/R-015.

No Nsight capture, replay, SDK-control, profiling, advanced fixture, or source
repair workflow is claimed by this milestone. Fixture records are application
readback and remain local experiment directories until R-012 imports and pins
the required baselines. Temporary review probes and CTest logs are ordinary
development evidence, not managed capture bundles.

## Capture/evidence implementation progress

Date: 2026-09-18. The historical progress below used product **0.1.1**. The
final capture/evidence acceptance record follows this history and completes
R-012/R-011/R-001 at **0.2.0**.

Subsequent focused checks pass for the new bounded export parsers
(`ngm_nsight_evidence_check`, 0.69 seconds), hardware result classification
(`ngm_capture_validation_check`), and optional export availability
(`ngm_nsight_check`, 1.57 seconds). The parser and harness corrections each
received an independent review; isolated ASan/UBSan parser probes also passed.
These additions are separate from the historical 17-check aggregate below;
they do not claim an aggregate run over ongoing inspection/advanced-fixture work.

The checked-in opt-in `ngm_capture_integration_run` now passes its complete
basic hardware matrix on product 0.1.1 and the same recorded 2026.3.1.0 setup:
three fresh captures, all five export outcomes, bounded MCP evidence access,
repeat/different PNG checks, persistent pins after restart, and clean EOF.
Its report and transcript are pinned as
`bundle-9e029958d57c655a53e191cef3bb580c`. See
[CAPTURE_VALIDATION.md](CAPTURE_VALIDATION.md) for the verified command and exact
retained baseline/captures, and investigation I-008 for the initial harness
failure and reviewed correction. Actual GPU replay, decoded cross-origin image
comparison, and source repair are explicitly outside that harness's checks.

After the production environment correction and independent review, the shared
GCC 16.2.1 Debug `ngm_check` run passed all **17 CPU checks** in **82.67 seconds**.
The complete transcript is retained in ignored
`build/capture-acceptance/check-0.1.1-xdg.log`. Focused capture-service, backend,
and MCP checks also passed. Four checks extend the 0.1.0 suite:
`ngm_artifacts_check`, `ngm_jobs_check`, `ngm_nsight_check`, and
`ngm_capture_service_check`. The expanded `ngm_mcp_check` exercises the 12-tool
stdio surface through executable Nsight stand-ins and a CPU target. The CLI and
linkage checks also cover the native `ngm-capture` executable. These checks
exercise real process boundaries, coordinator state/cleanup, artifact
publication/retention, and synthetic export failures; they establish no real
Nsight file or GPU compatibility. Component scope and focused validation records
are in [ARTIFACTS.md](ARTIFACTS.md), [JOBS.md](JOBS.md),
[NSIGHT_BACKEND.md](NSIGHT_BACKEND.md), and [MCP.md](MCP.md).

An earlier aggregate run passed all 17 checks in 83.30 seconds. CTest's local
cost data retained that successful run history, but a subsequent `ctest -N`
replaced `LastTest.log`; its full transcript was not retained. The later
82.67-second run has the separate transcript identified above. This implementation
is qualified on the shared GCC Debug configuration only. The earlier foundation's
Clang/Release/clean-build results and the 0.1.0 real Codex capability query remain
separate historical evidence; no new real Codex workflow validation is claimed.

The first real `ngm-capture` attempt used matching Nsight Graphics **2026.3.1.0,
build 38722833** on RTX 3080 Ti / driver 615.71.09 / KDE Wayland through Xwayland.
The uninstrumented fixture's standalone application-readback preflight passed.
Under capture injection, the target reported missing `VK_KHR_surface` and exited
3; `ngfx-capture` exited 255. No capture file or replay export was produced.
Owned-process cleanup was confirmed and the failed attempt was published and
pinned. Its report was subsequently retrieved through the MCP artifact tools.

The pinned preflight and failed capture are respectively
`bundle-1d1bd01e1378fc5f759d0e108b5e1aec` and
`bundle-2681d459696671f55ed0ab2a15a7d71f` under `artifacts/nsight-evidence`.
[I-001](INVESTIGATIONS.md#i-001--window-system-extension-unavailable-under-initial-capture-injection)
records exact inputs, identities, reproduction, and retention. Related I-002/I-003
controls retain the passthrough failure and successful target rendering with
process injection disabled, despite the latter wrapper's nonzero exit. These
application results do not establish capture support or identify the failure's
cause. Investigation records track subsequent controls and their pinned evidence.

A subsequent diagnostic `CaptureService` run with the same retained fixture
executable and an explicit per-process
`XDG_DATA_DIRS=/usr/local/share:/usr/share` succeeded. Pinned complete bundle
`bundle-0863b91c8d1a178482d1cd3abebff32c` retains a 100,968-byte capture and
successful metadata, functions, objects, logs, and screenshot exports, with
`readable_capture: true` and confirmed cleanup. The ignored diagnostic runner
supplied the environment override. The production service now preserves explicit
`XDG_DATA_DIRS` and materializes `/usr/local/share:/usr/share` when unset or empty;
the MCP configuration forwards caller-provided values. Metadata warned that
capture version `2026.3.1` was newer than replayer `2026.3` despite matching tool
version/build observations. Export success
does not establish actual GPU replay or detailed inspection of the exported data.

A subsequent retained C++ MCP probe, without an `XDG_DATA_DIRS` override,
completed three captures through the production stdio server on the same
Nsight/GPU/driver/desktop setup. Every capture produced all five exports, confirmed
owned-process cleanup, and retained a pin across server restart:

| Workload | Fresh target PID | Pinned capture bundle |
| --- | ---: | --- |
| Reference | 90251 | `bundle-019e2411c941dafdbfba953018451416` |
| Reference repeat | 90471 | `bundle-55689147d2b220f4b7b72aff355e0800` |
| Shader-error | 90601 | `bundle-3f5db7a4c55c1a3e1b6a53636b091066` |

The probe sources and MCP transcript are pinned as
`bundle-5ff26740ed6e0aa24ca466c348da8799` under `artifacts/nsight-evidence`.
This uses a C++ MCP client, not a new Codex invocation. Decoded reference
screenshots repeat exactly and match a standalone fixture readback at application
frame 2; the shader-error screenshot differs on 7,337 pixels. The comparison and
script are pinned as `bundle-01f092932bf93bbeb91abd8eb9ee45fe`.
[I-005](INVESTIGATIONS.md#i-005--capture-frame-numbering-needs-an-observed-application-correspondence)
records the failed frame-1 comparison, frame-2 correspondence, and exact workload
identities. The screenshots are embedded final-present capture images. These
measurements establish neither replay-rendered output nor a source diagnosis or
repair.

The separate documented Generate C++ Capture activity failed while connecting
to the fixture and produced no generated source. Its logs and report are pinned
as `bundle-cb453ee75e7a466f98fcaf75621bf3b8`; see
[I-006](INVESTIGATIONS.md#i-006--generate-c-capture-times-out-while-connecting-to-the-fixture).
That failed source-export probe does not invalidate the working Graphics Capture
and metadata-export path or establish a universal capability limit.

Those early probes established no actual GPU replay, detailed event state, SDK
control, profiling, source repair/recapture, or two-release compatibility.

## R-012/R-011/R-001 capture/evidence group completion

The group completes on **2026-09-18** at product **0.2.0**, following independent
component reviews, correction cycles, and a fresh acceptance audit. R-006,
R-010, and R-015 remain in progress; this milestone is not the first complete
visual-debugging release.

Before the version-only minor increment, GCC 16.2.1 Debug **0.1.1** passed all
**20 CPU checks** in **136.40 seconds**. Transcript:
`build/capture-acceptance/check-0.1.1-inspection-advanced-2.log`. This includes
the three new parser, retained-inspection, and capture-harness classification
checks, as well as the expanded 15-tool MCP and advanced fixture contracts.
The preceding aggregate build found two invalid string/JSON comparisons in
the new inspection implementation. Both now use validated string extraction;
the final aggregate and the independent reviewer's separate warnings-as-errors
inspection check pass. The failed build is retained as
`check-0.1.1-inspection-advanced-1.log` in the same directory.

After setting the authoritative version to **0.2.0**, the focused
`ngm_cli_check_run` passes in **0.05 seconds** and `ngm_mcp_check_run` in
**14.39 seconds**, verifying rebuilt standalone versions and MCP identity and
workflow behavior. `ngm-experiment` was relinked too. Transcripts:
`build/capture-acceptance/check-0.2.0-version.log` and
`build/capture-acceptance/check-0.2.0-mcp.log`. The earlier complete aggregate
and hardware evidence retain their actual 0.1.1 identity; a version increment
does not relabel existing evidence.

The current implementation has these independently scoped real results:

- **Basic capture/export:** three fresh MCP captures per release pass on
  2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608, with all five exports,
  process cleanup, raw evidence access, and pins after restart. Exact baseline,
  capture, and report bundles are in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).
- **Retained typed queries:** `capture_metadata`, `capture_events`, and
  `capture_objects` pass on the real 2026.3.1.0 correct/faulty pair, with 22 events
  and 32 objects each, five-record pages, exact capture scope, bounded protocol
  output, and stable metadata after server restart. The C++ client supplied no
  desktop variables and set `PATH=/nonexistent`. The current profile rejects the
  retained 2026.2 capture explicitly as `unsupported_producer`. Report, full MCP
  transcripts, and exact probe/client sources are pinned as
  `bundle-e8bedf549310ff13da710678546d0a9f`. This is not a new Codex invocation.
- **Advanced application rendering:** all **57** fresh launches pass with active,
  clean synchronization validation, deterministic repeats, and the independent
  analytic oracle. Complete retained matrix and runs are pinned as
  `bundle-a5e2bf5a70cfdff49bb8705afff9aba5`; see [FIXTURE.md](FIXTURE.md).

The acceptance reviewer checked lifecycle/storage source and regression cases,
the completed CPU transcript, and the real capture bundles and their inventories,
hashes, and persistent pins. No blocking defect remained for R-012/R-011/R-001.
Those checks do not qualify advanced captures, detailed pipeline/resource/shader
state, SDK control, source repair, profiling, or the full two-release workflow.
GPU replay attempts on both releases timed out during initialization, with
confirmed cleanup and pinned evidence in I-007/I-009/I-010/I-011.
