# Build and development validation

## Arch Linux packaging (R-016, 0.4.1)

Date: **2026-09-25**. Product **0.4.1**, package **0.4.1-1**, x86-64 Arch Linux
with GCC **16.2.1**, CMake **4.4.3**, Ninja **1.13.2**, Python **3.14.7**,
makepkg/pacman **7.1.0**, glibc **2.44**, and Linux **7.2.6-zen2-1-zen**.
This is an individual packaging addition with a patch increment, rather than a
new workflow milestone. The working source is based on **7ca115d** with the
packaging changes; the archive records that dirty state explicitly.

The documented `package` configure preset and `ngm_arch_package` target generate
a checksummed source archive with all seven exact dependency revisions, PKGBUILD,
and `.SRCINFO`. `makepkg` builds from the extracted archive using the host's Arch
hardening and LTO flags, runs **31/31 CPU checks without skips**, and creates
`nsight-graphics-mcp-0.4.1-1-x86_64.pkg.tar.zst`. The first complete CPU run takes
**116.37 seconds**, including the new staged-install check and existing binary/
archive linkage audit. No source downloads occur during configure or compilation.
Five unused-result warnings originate in the pinned fastmcpp process helper;
they do not prevent the build.

Extraction and inspection of the actual package verify:

- Only `nsight-graphics-mcp` and `ngm-capture` are installed in `/usr/bin`; both
  report version **0.4.1** and run from the extracted staging directory.
- Documentation, preview images, the project's MIT license, and fastmcpp,
  nlohmann/json, cpp-httplib and LodePNG license notices are present.
- Both executables have direct ELF dependencies on `libstdc++.so.6`,
  `libgcc_s.so.1`, and `libc.so.6`, with no RPATH/RUNPATH. These match the declared
  `gcc-libs` and `glibc` dependencies. Development archives and proprietary
  toolchain helpers are absent from the package.
- The extracted server negotiates MCP **2025-11-25**, lists **26 tools**, returns
  `capabilities`, and exits cleanly on stdin EOF with an empty tool-search PATH
  and an explicitly selected empty Nsight directory.

The initial sandbox attempt encountered the host's read-only ccache directory;
the authorized local rerun passed. Verification uses the existing Arch host and
an independent extracted source tree, rather than a clean devtools chroot.
No package was installed into the host, and no GPU integration was rerun for this
packaging change. Existing capture qualification keeps its original build IDs.
Local logs and receipts are in `build/package/arch-package.log`,
`build/package/arch/`, and `build/package/validation/`.

## Visual release milestone (0.3.0)

The related R-010/R-002/R-006/R-007/R-015/R-014 group completes at **0.3.0** on
2026-09-18. The minor increment subsumes a patch increment. Capture/inspection
behavior is unchanged from 0.2.12; the first-party code delta is the authoritative
CMake version. The main GCC Debug `ngm_check` aggregate passes **25/25 without
skips in 218.92 seconds**. Both configured optional workers rebuild at 0.3.0;
CLI versions, negotiated MCP identity, 22-tool listing and retained pinned-artifact
retrieval pass. The fresh GPU walkthrough below remains explicitly a 0.2.12 run.

Fresh-context final release review validates the roadmap/group/version rules,
document links and retained qualification identities. It corrected one matrix
phrase: C++ repairs can retain identical shaders while application build identities
change. The original pinned snapshot preserves the earlier draft; the final
release record retains the correction, final documents, reviews and check logs.
Source-installation evidence is separate from the newly rebuilt release binaries.
The final record is complete/pinned as
`bundle-c4319d1418a5b4ad16e1d173d8261c0c` in
`artifacts/source-install-validation`: 28 files, 572,438 bundle bytes and 27
verified payload hashes, with a checked reference to the primary qualification.

## Linux source-installation walkthrough (R-014)

Date: **2026-09-18**. Source commit **15df044**, product **0.2.12**.
[INSTALL.md](INSTALL.md) is the exercised build-to-repair guide. A separate clone
under `build/source-install-validation/checkout` used no copied build outputs or
machine-specific cache from the development tree. Submodule acquisition used
command-scoped local source mirrors at the seven exact gitlinks; this is source
acquisition, not a prebuilt dependency substitution or a test of public repository
access. The initial pre-submodule configure correctly failed with the documented
initialization instruction; its log is retained.

Configure, the 204-step default build, and `ngm_check` ran with networking disabled
using `unshare --user --map-root-user --net`. All **25 CPU checks pass without
skips in 217.06 seconds**, including actual binary/archive linkage inspection.
The host has GCC **16.2.1 (20260810)**, CMake **4.4.3**, Ninja **1.13.2**, Python
**3.14.7**, pkg-config **3.0.7**, XCB **1.17.0**, and Linux
**7.2.6-zen2-1-zen x86-64**. The default clean build has no optional SDK or resource
worker. A matching 2026.3 worker subsequently builds from the fresh capture's
qualified helpers, also without network access. This validates a clean source/
build tree on the recorded host, not a fresh distribution image or every compiler
configuration. `unshare` is a validation wrapper, not a product prerequisite.

On the recorded RTX 3080 Ti / driver 615.71.09 / KDE Wayland-Xwayland-XCB desktop,
the guide's seed 42, 192x128, frame-2 tutorial passes with matching Nsight
**2026.3.1.0/build 38722833** tools. Three fresh MCP C++ captures cover the correct
reference, original `shader-error`, and rebuilt `shader-error`. All three
screenshots exactly match independently launched frame-2 application readbacks
from `ngm-experiment`. The original fault differs from the reference at **7,337
pixels**, with maximum channel difference **140**; the repaired output has
**zero** differing pixels/channel error at tolerance zero.

The generated draw's fragment resource in each capture byte-matches its retained
application SPIR-V. Retained GLSL identifies the faulty red/blue swizzle. The only
source edit removes `.bgr` from `shaders/fixture/shader-error.frag`; a normal
network-isolated CMake rebuild recompiles that shader and the fixture build
identity. Original/repaired executable and shader hashes differ and are retained.
The main checkout's deliberate defect is unchanged. All three capture bundles,
three resource-read attempts and three imported application baselines are complete
and pinned across restart. Exact request/response logs, source/build inputs and
comparison records are archived.

Codex CLI **0.154.0** independently launches this clean 0.2.12 stdio build using
temporary command-line configuration, discovers 22 tools, calls `capabilities`
and retrieves the complete/pinned repaired capture with `artifact_info`; it exits
zero. No persistent user configuration is changed. This is a bounded real-client
interoperability check; the repair was performed by the development agent with
ordinary tools, with protocol requests recorded by the walkthrough driver.
It does not claim a new autonomous Codex CLI diagnosis session.

Fresh-context guide review corrected overly broad compiler-qualification wording.
Its final independent audit rehashes **510 payloads in nine pinned bundles**,
decodes the screenshots/readbacks with Pillow, verifies the three shader/source
correlations and changed build identities, and checks the actual Codex events.
The fresh tutorial uses 2026.3; [VISUAL_RELEASE.md](VISUAL_RELEASE.md) consolidates
the separately versioned two-release qualification and R-007/R-015 acceptance.

The complete installation/acceptance snapshot is pinned as
**`bundle-49217a19ae75f41fbe607152bbbd3699`** in
`artifacts/source-install-validation`. It retains 121 payload files, the guide,
source archive, clean binaries/logs, workflow responses, independent reviews and
the earlier visual-acceptance audit. Its 902 unique referenced bundles remain
explicitly pinned in their original stores. Publication checks validate payload
sizes/hashes, referenced manifest identities/pins and the archived guide.
Local reproducible scripts and receipts are under
`build/source-install-validation`; the snapshot retains the corresponding inputs.

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

At that milestone, the implementation had these independently scoped real results:

- **Basic capture/export:** three fresh MCP captures per release pass on
  2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608, with all five exports,
  process cleanup, raw evidence access, and pins after restart. Exact baseline,
  capture, and report bundles are in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).
- **Retained typed queries:** `capture_metadata`, `capture_events`, and
  `capture_objects` pass on the real 2026.3.1.0 correct/faulty pair, with 22 events
  and 32 objects each, five-record pages, exact capture scope, bounded protocol
  output, and stable metadata after server restart. The C++ client supplied no
  desktop variables and set `PATH=/nonexistent`. The then-current profile rejects the
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

## Two producer profiles and advanced capture validation at 0.2.1

Date: **2026-09-18**, GCC **16.2.1 Debug**, product **0.2.1**. The capture harness
adds optional workload selection while preserving the basic default. Retained
inspection adds the exact 2026.2.0.0/build 37991608 Vulkan producer profile next
to 2026.3.1.0/build 38722833, and preserves the observed optional unsigned
`indirect_index` field without inferring draw arguments or associations.
Capture, replay, and same-bundle metadata must match one complete profile.
Identified sanitized basic/indirect exports from both releases are checked in.

The focused parser, inspection, and actual stdio MCP checks after the indirect
field change all pass:

```sh
cmake --build --preset linux-gcc-debug --target \
  ngm_nsight_evidence_check_run ngm_inspection_check_run ngm_mcp_check_run
```

The recorded test times are **0.70**, **24.09**, and **14.44 seconds** respectively;
the complete build/check transcript is
`build/capture-acceptance/check-indirect-inventory-1.log`. Earlier in this patch,
the CLI/version check also passed in **0.05 seconds**, with its transcript in
`build/capture-acceptance/check-0.2.1-mcp-cli.log`. These are focused runs; the
20-check aggregate described above was not rerun or relabeled as 0.2.1.

Fresh-context reviews independently checked workload mappings and validation
order, complete producer-tuple matching, optional-field types and response
schemas, and the retained fixture provenance. Reviewers checked raw source
hashes, exact sanitization, matching tool tuples, complete manifests, persistent
pins, and membership in the passing hardware matrix. No actionable findings
remain in those reviewed changes. The review itself did not execute GPU work.

The final acceptance review found stale single-producer wording in capability
discovery, a parser comment, and two status documents, plus a mistaken function
name for the observed indirect marker in the inspection document. Those references now
describe both qualified producers or explicitly identify the historical result.
After the capability-text correction, `ngm_mcp_check_run` passes again in
**14.10 seconds**; transcript:
`build/capture-acceptance/check-0.2.1-capability-correction.log`.
`ngm-experiment` was relinked for the 0.2.1 version. The correction changes no
inventory parsing/query behavior; the retained real-query report below preserves
its pre-correction server hash. Local Markdown target and roadmap section/status
checks pass, and `git diff --check` reports no whitespace errors.

The real **18-run capture matrix** passes all nine basic/advanced workload pairs
on both matching Nsight releases, totaling **54 fresh captures**. Every run also
retains an independent standalone baseline, all five exports, process cleanup,
repeat/different PNG checks, durable pins, and clean MCP EOF. The entire batch,
executed binary snapshots, exact harness/client sources, commands, and inventory
summary are explicitly pinned as `bundle-a4bce77c1a8d615412702da25e33fa75`.
Individual baseline/report/capture bundles are pinned too. See
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) for exact setup and workload coverage.

Typed queries are validated separately from capture. A first C++ MCP probe
passes on both basic correct/faulty pairs, retaining report/transcripts/sources
as `bundle-0a4c5bc3e9902c1e0526f5d2d1ff1843`. The final probe then passes on all
**54** batch captures using the then-current server: metadata, all paginated event
and object fields/order compared with raw exports, bounded protocol responses,
and identical metadata for every capture after restart. It supplies no desktop
variables and sets `PATH=/nonexistent`. Report, expectations, sources, and both
session transcripts are explicitly pinned as
`bundle-a1af3b8bb46feebabcc66a3bd821ab80`. This is C++ client coverage, not a new
Codex invocation. Capture and later query binaries retain their distinct hashes.

Together with the prior 57-launch synchronized application-readback matrix,
these results and the recorded export gaps complete **R-010**. The larger visual
workflow group remains unfinished, so this code commit increments patch rather
than minor. Inventory success does not qualify detailed state, GPU replay, SDK
control, diagnosis/source repair, compute, or profiling. R-006 and R-015 remain
in progress; no required skipped case is counted as a pass.

## Optional SDK control at 0.2.2

Date: **2026-09-18**. R-002 adds explicit optional SDK source selection, fixture
initialization before Vulkan and per-launch boundaries, and a shared
present/graphics-capture-API delimiter selection in the native command and MCP.
SDK requests retain application observations separately from the server's
requested delimiter. The default build needs no SDK installation.

Fresh-context reviews prompted retention of full application/build/workload
context before the first SDK call, clearing a previous boundary result before
entering the next call, request-scoped SDK status in generic capture reports,
and capability wording aligned with the selectable delimiter. An initial MCP
CPU run exposed a missing `maxLength` on the new schema string; the correction
passes the actual protocol check. The initial real SDK hardware-harness run
exposed a shader-manifest representation mismatch, independently identified by
review. Its pinned failure and passing correction are investigation **I-012**.

Executed checks and local logs under `build/capture-acceptance/`:

| Configuration / target | Result | Transcript |
| --- | --- | --- |
| GCC 16.2.1 Debug, capture service and MCP | Pass, 6.34 / 14.68 seconds | `check-0.2.2-sdk-harness-1.log` |
| GCC Debug, SDK 0.9.2 fixture contract | Pass, 28.22 seconds | `check-0.2.2-sdk-0.9.2.log` |
| GCC Debug, SDK 0.9.0 fixture contract | Pass, 28.86 seconds | `check-0.2.2-sdk-0.9.0.log` |
| GCC Debug, default SDK-free `ngm_check` | All 20 pass, 153.19 seconds | `check-0.2.2-default-aggregate.log` |
| Clang 22.1.8 Debug, default fixture contract after correction | Pass, 23.15 seconds | `check-0.2.2-clang-sdk-default-correction.log` |
| GCC Debug, default fixture contract after portability correction | Pass, 28.00 seconds | `check-0.2.2-gcc-sdk-portability-correction.log` |

The initial Clang default build rejected `completed_` as an unused private field
when SDK calls are compiled out; the failed log is
`check-0.2.2-clang-sdk-default.log`. A `[[maybe_unused]]` annotation fixes that
configuration without changing SDK behavior. The focused checks above verify
the correction on both compilers. The aggregate and GPU evidence precede this
annotation and retain their actual source/build hashes. Both configure presets
are left with an empty `NGM_NSIGHT_SDK_ROOT`; the two SDK-enabled builds were
explicitly selected for qualification, not made default.

Real C++ MCP qualification passes **18 fresh captures**: two independently
selected SDK boundaries and a no-SDK-call regression on each matching release,
with three targets per case. All exports, process cleanup, distinct PIDs,
retained identities, repeat/variant checks, and pins after restart pass.
Independent RGB decoding verifies all 12 reference screenshots against their
standalone application frames and all 6 controlled faulty-image differences.
[SDK_CONTROL.md](SDK_CONTROL.md) records exact inputs and limits. A fresh
acceptance reviewer independently checks the 31 matrix/snapshot/baseline/capture/
report bundles, raw tool and SDK identities, pins, file inventory sizes, PID
markers and decoded images; no hardware acceptance defect was found.

The combined source/input/report/comparison snapshot is pinned as
`bundle-7b6c2aa712deb108b66af94d86b50e75`. Its outer import manifest records the
retained **0.1.1** importer; the actual harness, server, fixture and capture
payloads identify **0.2.2**. No historical evidence is relabeled. This completes
R-002 for the recorded basic windowed workload. R-006/R-007/R-015, advanced SDK
qualification, no-presentation compute and profiling are not established by
these runs. The larger visual workflow group remains unfinished, so 0.2.2 is
a patch increment.

## Generated C++ capture implementation, 0.2.3

On 2026-09-18, GCC 16.2.1 Debug builds the separate C++ activity, shared service,
native CLI selection, typed generated metadata parser, MCP tool, and opt-in
`ngm_cpp_capture_integration` harness. Fresh-context backend and harness review
found missing declared source-unit validation, failed-tree publication gaps, and
harness provenance/failure-reporting weaknesses. Corrections validate every
observed CMake-declared unit, preserve safe failed output with removal records,
snapshot the server as well as target, and record case success after assertions.
Review follow-ups report no remaining blockers.

The full `cmake --build --preset linux-gcc-debug --target ngm_check` passes
**20/20 CPU checks**, total **182.67 seconds**, recorded in
`build/capture-acceptance/check-0.2.3-aggregate.log`. This aggregate predates the
subsequent narrow output-directory correction found by real hardware (I-016).
After that correction, `ngm_cpp_capture_integration` rebuilds and
`ngm_nsight_check_run`, `ngm_capture_service_check_run`, and `ngm_mcp_check_run`
all pass; log `build/capture-acceptance/check-0.2.3-output-directory.log`.
The stand-in now enforces the real launcher's existing-directory prerequisite.

Earlier CPU correction logs retain a GCC dangling-reference diagnostic resolved
by copying a small JSON scalar, a MCP test missing the requested pin, and an
entry-overflow service test whose observation deadline was shorter than durable
publication of thousands of files. Only that overflow case received a longer
request/observation budget. The corrected service case passed; these were
build/test failures rather than Nsight integration failures. The real failure
is retained with pinned evidence in I-016.

Hardware runs are separate from these CPU results. The C++ harness is excluded
from default builds, CTest, and `ngm_check`; exact retained results are in
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). Experimental two-release shader/image
comparisons at 0.2.2 remain labelled separately in [INSPECTION.md](INSPECTION.md).

### Advanced evidence and R-006 acceptance

The unchanged 0.2.3 product/harness then passes 24 advanced C++ captures on both
releases, bringing this format's qualified matrix to 30 captures across five pairs.
Four fixed-capture native readers compile without warnings and extract 52 selected
resources with unchanged generated helpers. Independent review checks their
exact source references, enforced input hashes, sizes, shader matches, and
comparison results. I-017 preserves the comparison assumption failure and fix.
Pinned qualification snapshot: `bundle-cb069965dca0cc90e2b672194195beb5`.

A separate fresh-context R-006 acceptance audit finds the investigation criteria
satisfied after status reconciliation and the consolidated capability/gap/next-action
matrix in INSPECTION.md. R-007 becomes the next active workflow; neither source
repair nor R-015 is complete. These follow-up changes are documentation/evidence
only: no additional product code or version increment, and no CPU-suite rerun.
Content/diff review supplements the already recorded 0.2.3 aggregate and focused
correction checks. All hardware commands and outcomes are retained separately.

## Image comparison and basic repair at 0.2.4

GCC Debug adds bounded P6/BMP artifact image comparison and the opt-in
`ngm_shader_repair_integration` harness. The full **20-check CPU aggregate**
passes in **179.44 seconds** (`build/capture-acceptance/check-0.2.4-image-aggregate.log`).
After hardware-target registration, the harness builds and `ngm_check_isolation_run`
passes separately. The aggregate predates that registration and the final
review corrections; it is not a claim that every subsequent edit reran the suite.

Fresh-context review prompts a stronger encoded-byte-limit regression: an
oversized valid image is compared against itself and must report the artifact
read-limit error, so a dimension mismatch cannot cause a false pass. The corrected
`ngm_inspection_check_run` passes in **23.75 seconds**, log
`build/capture-acceptance/check-0.2.4-inspection-review.log`. Decoder checks also
cover multi-column padded BMP rows, orientation, channel order, unused fourth
bytes, malformed headers, truncation, and signed-height extremes.

The repair harness review strengthens compiler/source/output identity checks
and rejects a copied prebuilt reference SPIR-V. An initial GCC comparison error
between `std::string` and JSON is corrected with explicit string extraction;
`check-0.2.4-repair-harness-corrected-2.log` records the successful rebuild.
This is a compiler/test correction, not a failed Nsight integration. The final
capability wording scopes producer validation to inventory queries, since image
comparison intentionally accepts imports and failed bundles. The rebuilt
`ngm_mcp_check_run` passes in **16.41 seconds**, log
`build/capture-acceptance/check-0.2.4-capability-review.log`. A further fresh-context
implementation review finds no blocking defect.

The hardware harness records six successful fresh captures across both selected
releases, three standalone synchronization-validated application baselines per
release, exact repaired/reference RGB equality, and successful cleanup/pin/restart
checks. The source edit and shader compilation are separate normal development
actions recorded alongside the harness evidence. Independent evidence review
reproduces comparisons and checks source/SPIR-V/provenance links;
[SHADER_REPAIR.md](SHADER_REPAIR.md) records exact commands, identities, limits,
and explicitly pinned reports/audit in `artifacts/nsight-repair-evidence`.
The hardware runs used the frozen server preceding the final description-only
correction. R-007 and R-015 remain in progress for the remaining visual workflows.

## PNG and bounded previews at 0.2.5

The seventh source submodule, LodePNG, is pinned at
`ed6fe5825c6a4fbb7f58ab35a4231c7543cd452a` and compiled into a static archive.
No upstream codec tools/examples/tests or disk helpers are built; no additional
zlib library is discovered. Source markers and the existing ELF/archive audit
cover the new dependency. Exact selected options are in DEPENDENCIES.md.

The GCC Debug aggregate passes **20/20 CPU checks in 182.70 seconds**, log
`build/capture-acceptance/check-0.2.5-aggregate.log`, including real ELF linkage,
missing-source errors, hardware-target isolation, image parsing, artifact
inspection, protocol behavior, and the 384×384 maximum preview response.
The initial focused build found a missing fastmcpp header include path in the
MCP test target after adding base64 payload verification; declaring its actual
fastmcpp dependency fixes that build. The corrected focused image, inspection,
and MCP checks pass (`check-0.2.5-focused-corrected.log`) before the aggregate.
This was a CPU build correction, not a failed Nsight integration.

Fresh-context implementation review finds no blocker. Independent evidence
review suggests a nonintegral varied-pixel downsampling regression, now added.
The updated `ngm_image_check_run` passes on GCC Debug and Clang Debug in 0.01
seconds each (`check-0.2.5-image-review.log` and
`check-0.2.5-clang-image-review-offline.log`). The full aggregate predates only
that added image-test case and subsequent documentation. Clang configuration
and the final focused build/test run succeed in a network namespace with no
network access, using the already initialized sources; this is not a fresh
full-project source-installation qualification. Logs are under
`build/capture-acceptance/`, including `configure-0.2.5-clang-offline.log`.

The opt-in `ngm_retained_image_integration` separately passes 18 retained sources
and 36 full/cropped previews, using actual MCP image content. Independent Pillow
decoding verifies every PNG payload, source/pixel hash, sampling coordinate, and
original-image comparison, plus both exact Nsight producer profiles and current
pins. The root also visually inspected a returned repaired preview. No GPU,
Nsight process, new capture, or new Codex client run was involved. The complete
report/source/transcript/preview/audit snapshot is explicitly pinned as
`bundle-f9e131446027c6dd2a10f64c5d4a1d2b` in `artifacts/nsight-repair-evidence`.
[IMAGE_PREVIEWS.md](IMAGE_PREVIEWS.md) records invocation, format, sampling,
and evidence limitations. R-007/R-015 remain in progress.

## Generated-source queries at 0.2.6

`capture_cpp_source` and `capture_cpp_draws` bring the stdio surface to **20 tools**.
[CPP_INSPECTION.md](CPP_INSPECTION.md) defines source lines, literal associations,
unsupported coverage, producer/index validation, and limits. Two new CPU checks
cover the pure grammar and retained C++ service. The executable Nsight stand-in
now supplies explicitly synthetic source records for the MCP boundary checks.

The final GCC Debug `ngm_check` run passes **22/22 checks in 186.54 seconds**:
`build/capture-acceptance/check-0.2.6-final.log`. This includes the new parser and
service checks, ordinary inspection/MCP behavior, source dependency checks and
actual ELF linkage. Focused checks preceded the aggregate. An additional build of
`CppEvidence.cpp` and `CppEvidenceCheck.cpp` with AddressSanitizer and
UndefinedBehaviorSanitizer passes with exit 0 and no diagnostics
(`check-0.2.6-cpp-sanitized-final.log`); supporting libraries in that executable use
their ordinary Debug build, so this is not a full-project sanitizer qualification.
A first retained-evidence harness build exposed string/JSON comparison type errors; explicit
string extraction corrected that CPU compile failure before any retained MCP run.

Fresh-context reviews identified and corrected false associations from executable
arguments, ambiguous initializer forms, duplicate outputs, scope/shadowing,
declaration order, unknown sibling constructors/helpers, and C++ line splicing.
Additional corrections bound all output categories, reject incomplete source
indexes, and cache invalid parent qualification. The observed generated progress,
nested platform setup, and exact native XCB handle forms received explicit
qualification and regression coverage. Failed retained-source probes are preserved
as I-018–I-020, rather than interpreted as universal Nsight limitations.

The final opt-in `ngm_retained_cpp_integration` runs use:

```sh
build/linux-gcc-debug/tests/ngm_retained_cpp_integration \
  build/linux-gcc-debug/nsight-graphics-mcp artifacts/nsight-repair-evidence \
  build/cpp-query-validation/nsight-repair-evidence-cases.json \
  build/cpp-query-validation/repair-final
build/linux-gcc-debug/tests/ngm_retained_cpp_integration \
  build/linux-gcc-debug/nsight-graphics-mcp artifacts/nsight-evidence \
  build/cpp-query-validation/nsight-evidence-cases.json \
  build/cpp-query-validation/captures-final
```

Both pass: **36 retained C++ captures, 48 resolved draws**, from the exact matching
2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 producers. These include
basic reference/fault/repaired captures and standalone/combined advanced captures.
The run directories are `repair-final/cpp-inspection-QTer55` (six cases) and
`captures-final/cpp-inspection-d9RvYz` (30 cases), under `build/cpp-query-validation`.
The harness verifies explicit pins, single-row draw pagination, source excerpts,
file hashes, coverage pages, and stable results after server restart without
desktop or Nsight environment. It runs no new capture, GPU work, shader/resource
extraction, source repair, or Codex client session.

A separate Python audit validates all **276 tool calls** against the advertised
input/output schemas and compares source spans, event annotations, pipeline
outputs, stage/module references and declared resource handles/lengths against
the retained text. Its source and results are retained with both MCP transcripts,
exact executable hashes, parser/service/test sources, generated-source subsets,
failed probes, and check logs in explicitly pinned **`bundle-ce6ae6385721977dfd7091469cb57888`**
in `artifacts/nsight-repair-evidence` (204 inventoried payload files; 5,436,164 total bundle bytes including management files). The copied input
subsets are qualification evidence; original service capture bundles remain
separately pinned. R-007/R-015 stay In Progress, and no pending item is marked Done.

## Fixed descriptor helper qualification at 0.2.7

The explicit `ngm_descriptor_hydration_integration` target builds and runs on
GCC Debug with GCC **16.2.1 20260810**. It qualifies four previously pinned
combined C++ captures: reference/postpass-fault on matching Nsight
**2026.3.1.0/build38722833** and **2026.2.0.0/build37991608**. No fresh capture,
Nsight invocation, desktop session, Vulkan call, or GPU replay was performed.
The original capture host remains the RTX3080Ti/615.71.09/KDE Wayland-Xwayland
configuration recorded in NSIGHT_VALIDATION.md.

Executed commands:

```sh
cmake --build --preset linux-gcc-debug --target ngm_descriptor_hydration_integration
build/linux-gcc-debug/tests/ngm_descriptor_hydration_integration \
  artifacts/nsight-evidence build/descriptor-hydration-validation
cmake --build --preset linux-gcc-debug --target ngm_check
```

Final run:
`build/descriptor-hydration-validation/descriptor-hydration-GfKcCr`.
All **four cases / eight update calls / twelve setup writes** pass exact-field
checks. The helper maps palette array elements0/1 to source buffers32/34 at
byte offset0/range16, and postpass set40 to view27/sampler29 in shader-read-only
layout. Each capture's source registrations and callback types establish its own
symbol identities. Both reference/fault pairs have different serialized resource
hashes but identical inspected typed fields. The first preliminary successful run
is `descriptor-hydration-R6mRgA`; final assertions additionally check exact field
values and cross-capture typed equality, with compiler/core/header identities.

Each final worker and its unchanged vendor helper translation units uses
ASan/UBSan; supporting `ngm_core` is the ordinary Debug archive. All four runs have
empty sanitizer stderr, exit0 and confirmed cleanup. Four separately copied,
altered-database cases exit2 in `input_identity`, before helper initialization,
with cleanup confirmed. This tests the exact-input gate, not malformed-input
safety of NVIDIA's helper. Compiler version, binary identities, 111 core/header
input records, copied helper/database/source bytes, source snapshots, argv and
exit/cleanup records are retained. The actual Vulkan ABI sizes are64/24/24 bytes
for write/buffer/image descriptor structs. Only count-one writes are exercised.

The failed first generated-worker compilation is preserved as I-021. The corrected
calling contract resolves I-017's uncertainty about these setup descriptor fields;
it neither explains serialized byte differences nor qualifies executed shader
selection, arbitrary event state, generic product extraction, or a source repair.
The MCP surface remains **20 tools**. See
[DESCRIPTOR_HYDRATION.md](DESCRIPTOR_HYDRATION.md) for the exact trust boundary.

The final GCC Debug CPU aggregate passes **22/22 checks in 189.38 seconds**,
including check isolation and binary linkage. Log:
`build/capture-acceptance/check-0.2.7-final.log`. First-party worker syntax/warnings
also pass with `-Wall -Wextra -Wpedantic -Werror`; scoped formatting and diff
checks pass. Fresh-context implementation and independent evidence reviews find
no blocker. The latter verifies 56 original generated files, all 111 build-input
identity records, 28 typed callbacks, setup labels, negative/cleanup records,
and original capture pins without executing helpers or opening the store.

Complete qualification is explicitly pinned as
**`bundle-93e2c3dd09c57553642b1afa873e8566`** in
`artifacts/nsight-repair-evidence`: **261 inventoried payload files**, with total
bundle usage **112,666,086 bytes** including the outer manifest and pin record.
It retains the final and preliminary passes, I-021's failed build, exact input
snapshots, compiler/worker/harness records, source and dependency identities,
CPU/warning logs, and review notes. `raw/imported/qualification.json` identifies
payload hashes and scope. Original four capture bundles remain separately pinned
in `artifacts/nsight-evidence`; copied inputs are qualification evidence, not
new service captures. R-007/R-015 remain active and no roadmap status changes.

## Postpass C++ source repair at 0.2.8

`ngm_source_repair_integration` now validates a supplied normal source-build
record for `combined-pass-error` and `pass-output-error`. The isolated fixture
source is archived commit `744cb68` (product0.2.7), with one channel-order
assignment changed and recompiled through CMake/Ninja. Original and repaired
embedded build identities differ only in `src/fixture/Fixture.cpp`; the server
and harness are product0.2.8. All three roles use one frozen shader bundle.
[Source patch, hashes, invocation, and limits](SOURCE_REPAIR.md) are retained.

The serial matrix under `build/source-repair-validation/matrix` passes all four
invocations, terminal exit0: combined and standalone multipass cases on matching
Nsight 2026.3.1.0/build38722833 and 2026.2.0.0/build37991608. This totals **12 fresh
C++ captures** and **12 independent frame-2 application-readback launches** with
synchronization validation, seed42, 192×128, no SDK, and C++ wait_frames2.
The observed host remains RTX3080Ti/driver615.71.09, KDE Wayland/Xwayland/XCB,
GCC16.2.1 20260810 Debug. No system settings were changed.

Every capture exactly matches its separate application PPM. Fault/reference
comparisons differ at 24,472 pixels for combined and 24,529 for standalone;
all four rebuilt/reference comparisons have **zero differing pixels and zero
maximum/mean RGB error** at tolerance0. Fresh source queries resolve two draws
per capture, preserve direct/indirect workload mode, and identify the postpass
fragment module through that capture's own source symbols. Numbered source
excerpts match their retained file hash and draw line. Cleanup, released GPU
reservation, source indexes, pin state, and comparison after server restart pass.
No standalone GPU replay or fresh generic resource extraction was attempted.

The four complete run reports are explicitly pinned in
`artifacts/nsight-repair-evidence`:

| Release / case | Run report bundle |
| --- | --- |
| 2026.3 combined | `bundle-6502aaa90e78a89e5bbbb56635b8f01d` |
| 2026.3 standalone | `bundle-c274a92c04336d5e65e3f80b59c89a56` |
| 2026.2 combined | `bundle-74dbef8e7502dc966effa15694e6b993` |
| 2026.2 standalone | `bundle-f7979c5e98e987501167cbdba378f8c9` |

Each report names its three separately pinned capture bundles and three pinned
application baseline imports: **28 protected bundles** across the matrix. Full
payloads include frozen server/target/shader inputs, source/build records,
application observations, MCP capture/restart transcripts, source queries and
pixel comparisons. Expected answer checks remain in the harness; the capture
server does not synthesize diagnoses from fixture scenario names.

Five deliberate negative preflight cases pass under
`build/source-repair-validation/preflight`: extra source edit, reused executable,
changed build identity, missing executable link record, and changed shader bytes.
Each exits2 before GPU work with a structured `preflight.json` failure record.
There was no unexpected integration failure to add to INVESTIGATIONS.md.
Fresh-context implementation review corrected the configurable original-input
paths and structured preflight recording before hardware runs; excerpt and
workload verification were also strengthened.

The separate read-only evidence review writes
`build/source-repair-validation/independent-audit/audit.py` and `report.json`.
It independently decodes all twelve BMP/PPM pairs with Pillow, checks all eighteen
original build-input hashes against the archived commit, the sole source delta,
shared shader files, exact tool profiles and executable roles, generated-source
associations, before/after metrics, cleanup/pins/restart, and all five refusals.
It does not execute the helper/fixture or open an ArtifactStore. These results
qualify the supplied postpass repairs; they do not finish R-007/R-015 or establish
an autonomous diagnosis harness for other defect families.

The final GCC Debug CPU aggregate passes **22/22 checks in 188.50 seconds**
(`build/capture-acceptance/check-0.2.8-final.log`). Scoped clang-format, diff, and
local-document-link checks pass. Fresh-context implementation and evidence
reviews found no remaining blocker for these two cases.

The compact qualification snapshot is explicitly pinned as
**`bundle-59b3155112a30535883c2d4fc68a3c1d`** in
`artifacts/nsight-repair-evidence`: **758 inventoried payload files** and
**9,063,594 total bundle bytes** including its outer manifest and pin record.
`raw/imported/qualification.json` records payload hashes, scope, CPU results,
source identities, audit, and the 28 separately pinned bundle references.
The snapshot retains matrix argv/results/reports/transcripts, source/build records,
common shaders, all five negative input mutations/rejection reports, and the
independent audit. It omits repeated executable and image copies, recording their
identities instead; the full successful inputs and images remain in the separately
pinned run/capture/baseline bundles above. It is an imported qualification record,
not a new service capture. R-007 and R-015 remain in progress.

## Binding and pipeline source repair at 0.2.9

The extended `ngm_source_repair_integration` accepts two basic scenario pairs with
independent isolated C++ repairs, preserving its original postpass profiles.
`docs/STATE_REPAIR.md` records the diagnoses, exact source edits and source-build
inputs. Normal CMake/Ninja builds of each isolated 0.2.8 source tree produce its
original and repaired fixture; the MCP server and harness are product0.2.9.
Only `Fixture.cpp` changes within each build-identity pair; all three roles share
one frozen shader bundle and use the original faulty scenario after repair.

The final matrix in `build/state-repair-validation/matrix-final` passes both new
pairs on matching Nsight2026.3.1.0/build38722833 and
2026.2.0.0/build37991608. It supplies 12 fresh captures plus12 independent
application baselines at seed42/192x128/frame2, with exact capture/application
and repaired/reference RGB equality. The observed configuration remains
RTX3080Ti/driver615.71.09, KDE Wayland/Xwayland/XCB, GCC16.2.1 Debug. Independent
source and shader semantics are distinguished from generated Nsight source and
fixed-input descriptor-helper evidence.

The complete GCC Debug CPU aggregate passes **22/22** in **192.61 seconds**
(`build/state-repair-validation/check-0.2.9.log`). That run preceded the final
hardware-only label-assertion correction described in I-023; final hardware
regressions exercise the corrected assertion. No CPU implementation changed.
Six final-harness negative preflight checks reject wrong repair kinds, extra
source edits, and reused executables before constructing a store or launching
an application (`build/state-repair-validation/preflight-final/report.json`).
No new default CTest or GPU test registration was added.

The final corrected-harness combined and standalone postpass regressions on
2026.3 also pass, bringing the final matrix to18 captures and18 application
readbacks. The read-only Pillow/source audit passes all six runs, including
source-archive inputs, selected descriptor names and color-write masks in each
basic capture, pins and restart. It is retained under
`build/state-repair-validation/audit`. The exact report bundles are listed in
[STATE_REPAIR.md](STATE_REPAIR.md).

The final reviewed compact snapshot is pinned as
`bundle-c55c149703d53c5a11ec75913cb5d747` in
`artifacts/nsight-state-repair-evidence`, with 1,779 payload files and 79 pinned
referenced bundles. Post-publication verification passed all manifest payload
hashes, current implementation hashes, and referenced pins.

## Advanced source repairs at 0.2.10

The existing source-repair harness now qualifies standalone/combined resource
selection and indirect-parameter repairs. Each of two isolated source archives
of commit `8d0be10` is built normally, edited at one prescribed C++ line and
rebuilt; the same faulty scenario is recaptured. Original/repaired fixture builds
report0.2.9, server/harness0.2.10. Independent GLSL/source and SPIR-V instruction
correspondence preserve distinct actual hashes when debug-path strings differ.
See [ADVANCED_REPAIR.md](ADVANCED_REPAIR.md) for exact diagnosis scope and receipts.

The eight new runs pass on matching Nsight2026.3.1.0/build38722833 and
2026.2.0.0/build37991608: 24 fresh captures and24 independent application baselines,
all exact capture/application matches and exact repaired/reference RGB equality.
Inputs are seed42/192x128/frame2 on RTX3080Ti/driver615.71.09,
KDE Wayland/Xwayland/XCB, GCC16.2.1 Debug. The matrix records workload features,
implicated scene/post draw, both shader stages, observed executable/build identity,
owned-process cleanup, persistent pins and comparison after restart.

The GCC Debug CPU aggregate passes **22/22** in **189.54 seconds**, retained in
`build/advanced-repair-validation/check-0.2.10.log`. Eight final-harness preflight
checks reject mismatched repair kinds/pairs, additional source edits and reused
executables before application execution. Seven exact-input CPU resource helper
experiments and independent reruns pass ASan/UBSan; seven altered-database checks
reject before helper initialization. These experiments are not generic product
resource access or GPU replay. I-024 retains their initial compilation failure.

No new default CTest or hardware registration was added. The same opt-in
`ngm_source_repair_integration` target accepts the additional pairs documented in
ADVANCED_REPAIR.md; the CMake `_run` default remains the combined postpass pair.

Four prior-profile regressions on 2026.3 also pass, bringing this final matrix to
12 runs, 36 fresh captures and 36 independent application baselines. Fresh-context
acceptance review and the separate root audit pass the full matrix and all eight
negative preflights. The complete 2,165-file qualification snapshot is pinned as
`bundle-1e6ebc236bc4232d22303d0be4ca3583` in
`artifacts/nsight-advanced-repair-evidence`, referencing 91 pinned bundles.
Post-publication payload/implementation hash and reference/pin verification passes.
R-007/R-015 remain in progress; this does not qualify generic resource access or
standalone GPU replay.

## Resource worker boundary at 0.2.11

Both optional resource workers build from the exact five-file helper closures of
Nsight 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608, using GCC 16.2.1
Debug. Build inputs are selected from retained generated captures, copied and
hashed in isolation; vendor helper code links statically. No new capture, GPU
replay, system setting change or runtime compilation is involved. See
[RESOURCE_WORKER.md](RESOURCE_WORKER.md) for the internal contract and prerequisites.

The retained-input matrix covers 42 capture cases: 35 generated projects with
94 shader resources matched against their separately retained application SPIR-V,
plus seven prior diagnosis cases with 33 independently reviewed shader/push/indirect
resources. Each resource passes whole-output, two-part reconstruction and end-offset
checks. Twenty negative runs cover wrong declarations, invalid offsets/handles,
malformed or absent data, symlink rejection and input-size bounds on both profiles.
The C++ process driver verifies cleanup and a ten-second wall deadline for each
invocation. I-025 records the two deliberately truncated inputs that terminate the
vendor reader with SIGSEGV inside confinement; these are rejected extraction
results, not successful data reads.

The first complete GCC Debug CPU aggregate passes 23/23 in 187.20 seconds. The
confinement check exercises actual enforcement on this host. Independent extended
CPU probes additionally verify denied file-descriptor duplication, executable
mprotect, io_uring, memfd, sendfile, directory contents and /proc contents. A
simulated pre-Landlock header build verifies that the default confinement stub
compiles and fails closed while explicitly requested optional workers fail
configuration with an actionable prerequisite error. The final aggregate passes
23/23 with no skips in 186.80 seconds after that compatibility correction. The
rebuilt matrix passes 636 invocations, including two additional 98,304-byte
resources checked by alternate bounded ranges and over-limit request refusals.
Those two cases establish range consistency, not an independent image oracle.
A fresh-context acceptance reviewer also independently passes 18 final-binary
argument/range checks across both profiles and audits the static runtime linkage.

The MCP surface remains 20 tools. This establishes the internal worker boundary;
source-reference enumeration, protected input snapshots, strict parent response
validation and general MCP resource access remain R-007 work.

Fresh-context acceptance audits all 636 final invocations: 614 successes and
22 expected failures, including two contained SIGSEGVs. The 602 oracle responses
cover 127 resources from 42 distinct captures; 12 responses separately establish
large-resource range consistency. No blocking finding remains. Qualification
bundle `bundle-78b10c057ee352241bb5eb276b22ff11` is pinned in
`artifacts/nsight-resource-worker-evidence`, with 2,837 payload files and 42 pinned
source-capture references. Every inventoried payload hash, implementation hash,
source manifest and pin verifies after publication. Runtime enforcement was
tested on Linux 7.2.6-zen2-1-zen x86-64; no host configuration was changed.


## Bounded resource MCP queries at 0.2.12

The implementation adds `capture_cpp_resources` and `capture_cpp_resource` (22
MCP tools), with literal source reference IDs, streaming leased snapshots,
compiled-in helper profiles, supervised workers and strict response validation.
[RESOURCE_QUERIES.md](RESOURCE_QUERIES.md) defines the byte and provenance limits.
Every read retains its exact database snapshots, including on publication failure.

The GCC 16.2.1 Debug CPU aggregate passes **25/25**, with no skips, in **217.67
seconds** (`build/resource-query-validation/cpu-aggregate.log`). New focused
checks cover macro grammar/conflicts/identity, streaming files larger than the
ordinary read cap, links/traversal/ownership, worker failures and malformed
responses, binary/range handling, child timeout cleanup, quota failure after
successful extraction, pins and restart. MCP checks exercise both valid explicit
listing sections and missing-worker errors. A separate default configuration with
both optional helper paths empty builds `ngm_inspection`; configure/build logs
are retained in the same validation directory.

The actual MCP qualification uses matching producer/worker profiles:
2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608. The environment remains
Linux 7.2.6-zen2-1-zen x86-64, RTX 3080 Ti, driver 615.71.09, KDE Wayland with
Xwayland/XCB, and GCC 16.2.1 Debug. It comprises:

| Run | Positive captures | Byte oracles | Oracle reads | Range-only reads | Expected provenance refusals |
| --- | ---: | ---: | ---: | ---: | ---: |
| Retained `resource-inspection-kg5d7v` | 30 | 84 | 336 | 0 | 5 |
| Advanced retained `resource-inspection-Kh56jH` | 7 | 33 | 132 | 0 | 0 |
| Fresh `resource-inspection-f785lB` | 6 | 42 | 168 | 30 | 0 |

The 43 positive captures therefore cover **159 byte oracles / 636 oracle reads**,
plus six 98,304-byte resources tested for range consistency with **30 reads**.
The six larger resources have no independent content oracle and do not establish
intermediate image contents. Each small resource is read whole, in two parts and
at its end offset; larger resources use two initial pages, a differently split
reconstruction and an end-offset read. Source references are paginated and hashes
checked. Every positive/negative listing and every result pin is checked again
after a server restart.

The six fresh captures come from `cpp-validation-VZ2w33` (2026.3) and
`cpp-validation-7t0CVj` (2026.2), each passing reference/repeat/indirect-fault runs,
cleanup, source retention, screenshot equality/difference and restart. The new
resource queries then compare shader bytes to unique separately retained SPIR-V
artifacts, push constants to the independently known seed-42/frame-2 fixture
inputs, and indirect data to the fixture's `(3,2,0,0)` or `(3,1,0,0)` tuple.
This qualification performs no source repair or standalone GPU replay; the
previous repair evidence remains separately recorded.

Fresh-context review found and corrected deletion of snapshot inputs before
publication, and a missing string bound that rejected explicit valid listing
sections. Review of the qualification harness added exact producer/helper/worker
identity and original-database checks. Independent offline audits validate
advertised schemas, exact source spans, original-versus-snapshot hashes/sizes,
worker hashes, raw worker stdout and retained reports, byte ranges and cleanup.
The first corrected run is additionally audited against the strengthened checks
because its harness preceded that strengthening. I-026 records the initial
qualification's mistaken inclusion of five historical probe bundles as positive
product cases; those remain explicit negative cases rather than weakening the
capture provenance contract.

Validation outputs are under `build/resource-query-validation/`. The independent
review script/results and exact implementation/tool identities accompany the
qualification snapshot.

Qualification is complete and pinned as **bundle-6365322f8eb3922893e1d2256983f732** in
`artifacts/nsight-resource-query-evidence`. It contains **238 payload files,
81,116,253 payload bytes**, or **81,142,535 bytes** including bundle metadata,
with **788 referenced pins** covering captures, read attempts and fresh-capture
matrix reports. Publication verification checks every inventoried length, all
237 independently recorded payload hashes, exact implementation snapshots, and
all referenced manifest hashes and persistent pins. The initial I-026 failure
is retained alongside the three successful query matrices. Independent acceptance
passes 1,332 input snapshots, 624 source references and 1,621 output-schema checks.
R-007 and R-015 remain in progress for complete workflow acceptance; this patch
completes the bounded resource-query slice, not the whole visual release.


## 0.3.1 compute correctness

R-008 passes implementation, fresh-context review/correction and independent
acceptance. [COMPUTE.md](COMPUTE.md) records the exact source-available scope.
The GCC 16.2.1 Debug aggregate passes 25/25 CPU checks, no skips, in 248.16 seconds.
The optional resource workers were rebuilt at 0.3.1. The compute standalone
matrix passes 12 fresh launches/30 frames with synchronization validation;
three original MCP captures diagnose two defects, and actual GLSL edits plus
source-built glslang recompilation produce three fresh captures with exact
reference equality. Original numerical/MCP runs retain 0.3.0 development
identities; repaired and explicit 2026.2 unsupported MCP runs use 0.3.1.

The independent evidence audit passes 1,143 assertions over raw arrays,
source/SPIR-V/producer/build identities and pins. Complete pinned snapshot
`bundle-1ed0b5e17d49e24b6333d6ae4b10ffc1` in `artifacts/compute-evidence`
contains 1,128 files. Publication verification recomputes all 1,127 payload hashes
and verifies its persistent pin after server restart, alongside ten independently
pinned native/MCP capture attempts. It includes the failed private investigations,
actual repair/build records, executable baselines and audit script/results.
Working publication records are `build/compute-qualification/publication.json`
and `verification.json`. No second-release compute success, generic Nsight
compute buffer/state inspection or standalone GPU replay success is claimed.

## Persistent loopback HTTP — 0.3.2

R-004 adds a private-token-authenticated Streamable HTTP listener over the same
22-tool registry and capture/inspection core. The GCC Debug `ngm_check` aggregate
passes **26/26 CPU checks**, no skips, in **254.13 seconds**; this includes the
existing stdio check (19.57 seconds), new HTTP integration (4.05 seconds), process/
job/artifact checks and static vendored-code linkage audit. No new dependency or
vendored-source change was required. The focused HTTP run also passes separately.

The HTTP check uses a real listener, independent per-client protocol sessions,
the pinned fastmcpp HTTP client/raw-result interface, and executable Nsight
stand-ins. It verifies actual capture lifecycle, shared cancellation, persistence
across session deletion, retained pins after restart, and SIGTERM/SIGINT cleanup.
Incomplete-header and acknowledged-incomplete-body requests exercise bounded
shutdown. Invalid bearer credentials, Host/Origin, versions, malformed messages,
body/header budgets and insecure token files are covered. These are CPU checks,
not new hardware qualification.

A separate **Codex CLI 0.154.0** session connects to the real 0.3.2 HTTP server,
negotiates MCP **2025-06-18**, discovers all 22 tools, calls `capabilities`, then
retrieves complete/pinned compute qualification bundle
`bundle-1ed0b5e17d49e24b6333d6ae4b10ffc1`. The server stays alive after Codex exits
and subsequently exits zero on SIGTERM with empty stdout. No persistent Codex
configuration was changed. The transcript preserves the artifact's original
0.3.1 provenance; HTTP retrieval is a distinct 0.3.2 observation.

Working evidence is in `build/http-qualification`; fresh-context transport and
acceptance reviews are in `build/http-review` and `build/http-acceptance-review`.
[HTTP.md](HTTP.md) records deployment policy, commands, protocol limits, client
library limitations, and the managed qualification reference.

## GPU Trace export parser slice — 0.3.3

GCC 16.2.1 Debug builds the pure GPU Trace parsers and rebuilt server at 0.3.3.
`ngm_profile_evidence_check` passes in 0.06 seconds, existing
`ngm_nsight_evidence_check` in 0.96 seconds and `ngm_check_isolation` in 0.16
seconds. A private C++ probe additionally parses all five complete real export
sets. The complete now-27-check aggregate was not rerun; the last complete
aggregate is the 0.3.2 result above.

The owner-approved temporary profiling capability access enabled five successful
GPU Trace runs on matching 2026.3.1.0/2026.2.0.0 tools. Every process reports exit 0
and confirmed cleanup, and all three bounded batches restore/verify original
ACLs and DeviceFileModify values. Ten private application launches separately
validate numerical output and measure dispatch durations. These experiments use
0.3.2 prototype binaries, not a completed 0.3.3 profiling service.

Independent fresh-context review verifies parser behavior statically, all 22
sanitized fixture projections and hashes, and documentation against raw results.
Its minor notes were corrected and re-reviewed. The complete experiment/parser
snapshot is pinned as `bundle-d345e0dadab7961610746f6f2d4845e2` in
`artifacts/performance-evidence`, with 284 payload hashes and pin-after-restart
verified. [PERFORMANCE.md](PERFORMANCE.md) records scope and limitations. R-009
remains In Progress; the 22 MCP tools do not yet include profiling or metric queries.

## Shared profiling service slice — 0.3.4

GCC 16.2.1 Debug passes all **28/28 CPU checks** (262.14 seconds). After the
review correction aligning inspection's whole-bundle inventory bound with the
artifact store, the focused `ngm_profile_workflow_check` passes again (9.35
seconds), including 520 application output files, same-size retained evidence
corruption, unsupported tool/help observations, malformed/linked exports,
timeout/cancellation, shared capture/profile GPU reservation and real stdio MCP
calls using CPU stand-ins. The aggregate preceded that final narrow correction;
it was not repeated afterward. Existing HTTP/stdin checks discover the shared
25-tool surface.

A separate C++ MCP integration runner invokes the actual server and installed
Nsight tools. Both 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 produce
successful, pinned profile bundles with confirmed cleanup. Actual
`profile_metadata` and `profile_metrics` calls retrieve all four export tables;
a fresh server without tool configuration returns identical metadata, and both
server lifetimes exit 0. The wrapper restores the exact original profiling ACLs
and DeviceFileModify settings. The server reports 0.3.4; the private workload
binary remains the previously retained 0.3.2 prototype, with exact copied hashes.
These are MCP protocol-client runs, not a new Codex-client acceptance claim.

Working records are in `build/profile-service-integration`, CPU logs in
`build/profile-aggregate.log` and `build/profile-workflow-final.log`, and
fresh-context reviews in `build/profile-service-review`. The source-integrated
workload, independent repeated comparisons and performance source-repair
acceptance remain unfinished under R-009. See [PERFORMANCE.md](PERFORMANCE.md).

Independent acceptance review checks actual MCP numeric results against the raw
exports, producer/cleanup identities and restoration records with no remaining
slice blocker. The complete service snapshot is pinned as
`bundle-9de7695e0225eb604775a6fe52c5a4dd` in `artifacts/performance-evidence`;
all 124 payload hashes and the pin after restart verify. Publication receipts are
in `build/profile-service-publication`.

## Integrated performance fixture slice — 0.3.5

The C++ fixture/experiment runner adds two performance scenarios with separate
shader compilation profiles, explicit warmup, all-frame numerical readback and
bounded Vulkan timestamp evidence. GCC 16.2.1 Debug passes **29/29 CPU checks**
(284.71 seconds). Following the final timestamp valid-bit correction, the focused
`ngm_performance_evidence_check` passes again (15.29 seconds); the aggregate was
not repeated for that narrow correction. Static fixture/measurement review finds
no outstanding blocker after provenance, timestamp and identity corrections.

The final opt-in `ngm_performance_integration_run` passes ten launches/566 frames,
with independent zero-tolerance numerical checks, two clean synchronization-
validation launches, and eight measurement launches after 30 warmup submits each.
The 320 measured submits compare local-size-1 and local-size-64 variants in
alternating order across four repetitions. Shader/executable/GPU identities
remain stable across the group. Observed median ranges are 638.256–638.816 µs
and 50.576–50.832 µs; substantial within-run tails/drift are retained and documented
in [PERFORMANCE.md](PERFORMANCE.md), rather than interpreted as constant clocks
or universal speedup. No profiling access or clock changes are involved.

Final evidence is under
`build/linux-gcc-debug/tests/performance-integration/run-97579-35035408232136`;
logs are `build/performance-fixture-{aggregate,final-build,final-validation}.log`.
The earlier batch under `build/performance-fixture-validation` is superseded.
This establishes the integrated workload and application evidence, not an actual
source repair or new-fixture Nsight acceptance. Those remain R-009 work.

Fresh-context acceptance independently verifies 9,181,970 outputs across all 566
frames, timing conversions, build/shader identities, validation and statistics,
with no fixture-slice blocker. The final snapshot is pinned as
`bundle-a5fbe932759bd51e5785cbc361b26b3a` in `artifacts/performance-evidence`;
all 990 payload hashes and persistent pin after restart verify. Receipts are in
`build/performance-fixture-publication`.

## 0.3.6 repeated profile comparison, 2026-09-25

`cmake --build --preset linux-gcc-debug --target ngm_check_all ngm-vulkan-fixture`
passes all **30 CPU checks** in 288.75 seconds. The final expanded
`ngm_profile_compare_check_run` passes in 6.37 seconds after adding headerless
frame-table and exception lease-release cases. Logs are
`build/profile-comparison-aggregate.log` and
`build/profile-comparison-final-focused.log`. The original focused comparison /
profiling workflow checks also pass (6.13 / 9.32 seconds).

The new check uses actual service jobs at the process boundary with synthetic
Nsight exports, and an actual offline stdio `profile_compare` call after restart.
It validates equal weighting of per-trace medians, required repetition/selection
bounds, producer/GPU/driver/settings/column/label mismatches, explicit units,
zero denominators and extreme finite arithmetic. Existing stdio/HTTP checks
verify the shared 26-tool surface. No CPU stand-in result establishes GPU support.

A fresh-context static review found no actionable correctness defects; its report
is `build/profile-comparison-review/review.md`. R-009 real repeated measurement
and source-repair acceptance remain separate work recorded in PERFORMANCE.md.

## 0.4.0 performance group acceptance, 2026-09-25

The R-009 completion build passes all **30 CPU checks** in **289.72 seconds**:
`cmake --build --preset linux-gcc-debug --target ngm_check_all ngm-vulkan-fixture`.
The final range guard checks finite/range bounds before converting comparison
arithmetic to double; subsequent focused comparison and profiling workflow checks
pass. A separate fresh-context review finds no defect in that guard. Logs are
`build/performance-release-{aggregate,final-focused}.log`; review is
`build/profile-comparison-review/range-guard.md`.

Real hardware acceptance uses the retained **0.3.6 development build**, not a
retroactive 0.4.0 identity. An actual selected-scenario source edit and pinned
shader rebuild preserve numerical output; sixteen repeated GPU Traces on the two
qualified Nsight releases and the real MCP comparison tool show a repeatable
measured dispatch-time change. An independent fresh-context audit accepts every
R-009 criterion, with separate evidence origins, units, warmup, repetition and
observed variability preserved. The main and diagnosis audits together check
**1,255 retained frames / 20,470,546 exact output values**. Permissions are restored
exactly after every batch, including the failed harness attempt. Detailed scoped
results and retention are in [PERFORMANCE.md](PERFORMANCE.md).

The final **0.4.0** binary also performs all six comparison queries against those
retained real cohorts, with results identical to the accepted 0.3.6 responses;
26-tool discovery and the 0.4.0 server identity are checked. This offline run uses
an empty tool installation and makes no new GPU collection or permission change.
Its transcript/report is `build/performance-repair/release-query`.

The complete qualification snapshot is pinned as `bundle-d4db174806f33a707b908900599404d5`
in `artifacts/performance-evidence`: 2914 payload hashes verify, and its pin is
confirmed after server restart. Publication receipts are in
`build/performance-repair-publication`; snapshot documents predate their own
publication reference and the final roadmap move. R-009 is Done, completing the
performance group at 0.4.0. No accepted roadmap work remains pending or active.

## 0.4.2 reader activation, 2026-10-15

`cmake --build --preset linux-gcc-debug --target ngm_check_all` passes **31/31
CPU checks**, no skips, in **325.33 seconds**, with GCC 16.2.1 and CMake 4.4.4 on
Linux 7.2.7-zen1-1-zen x86-64. This includes actual worker confinement and
wrong-profile/process-failure coverage. The optional 2026.3 worker uses the
unchanged exact five-file retained helper closure. No dependency is downloaded.

The current-version retained MCP runner accepts one optional worker, checks its
required tool names, and rejects an unknown resource reference. Actual whole and
split shader reads match the independent 2,700-byte SHA-256 oracle; exact input/
producer/helper/worker hashes and source/read pins survive restart. This retained
input qualification needs no desktop, Nsight execution or GPU access. Configuration
for new Codex server processes includes the separately installed verified worker;
existing connections keep their startup arguments. Detailed evidence, pins, exact
operator paths and the initial safe store-initialization refusal are recorded in
[RESOURCE_QUERIES.md](RESOURCE_QUERIES.md) and I-037.

## 0.4.3 per-job launch environments, 2026-10-15

The GCC Debug source build passes **32/32 CPU checks**, no skips, with
`ctest --preset linux-gcc-debug -j 1` in **312.14 seconds**. The initial aggregate
also passed 32/32 before the final cancellation/deadline guard. A four-way rerun
then passed 31/32: the profiling workflow's queued five-second request expired
under concurrent filesystem-heavy checks. The final documented serial run passes
that check in 9.72 seconds without changing its assertions or timeouts. The failed
parallel log is retained alongside the passing final run.

New checks cover the bounded allowlist, manifest/library identities, per-job
propagation through all discovery/graphics/C++/profile/export processes, queue
isolation, reserved HOME/XDG/temp paths, absent-key defaults, parent isolation,
selection mutation refusal, cancellation/deadlines and the actual capture CLI.
Actual stdio and HTTP requests exercise the shared schemas. The resource-reader
checks still enforce the worker's empty environment. ELF linkage and staged
installation checks pass with the new module and documentation.

Hardware probes use **0.4.3**, matching **Nsight 2026.3.1.0/build 38722833**,
RTX 3080 Ti, kernel driver **615.71.09**, KDE Wayland with the fixture's
**XCB/Xwayland** backend, GCC 16.2.1, CMake 4.4.4 and the source-built debug shader
bundle. The default NVML probe fails with library version 615.78; the explicitly
selected retained 615.71 libraries report the correct GPU/driver. No machine
settings or installed drivers/libraries are changed.

Two independent fresh 192×128/seed-42/frame-2 application baselines pass with the
selected environment and retained GLSL/SPIR-V/build identities. Three fresh MCP
capture attempts reach Nsight connection and select the intended driver library
but time out after 120 seconds, including a controlled run without the layer
filter. All confirm owned cleanup, release the GPU reservation and retain pinned
failed bundles. **No fresh Nsight capture compatibility is qualified by these
probes.** No SDL Wayland application or profiling permission change is exercised.
See I-038 and [LAUNCH_ENVIRONMENT.md](LAUNCH_ENVIRONMENT.md).

The implementation, final CPU logs, failed parallel run, baseline readback,
hardware transcripts/reports, exact selected paths/hashes and restart pin receipts
are pinned as `bundle-2a69ab0ef0bd54fd2df1fdeadc6fc423` in the configured external
artifact root. The three failed source captures remain independently pinned in
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/launch-nzs1_wiv/store`.
The snapshot records its staged source patch against 7123808; subsequent profile
compatibility work is excluded. Its importer is the existing installed 0.4.1
connection, while all nested implementation/runtime reports identify 0.4.3.

## 0.4.4 header-only profile compatibility, 2026-10-15

`cmake --build --preset linux-gcc-debug --target ngm_check_all` passes **32/32
CPU checks**, no skips, in **320.57 seconds** on the same GCC 16.2.1 / CMake 4.4.4 /
Linux 7.2.7-zen1-1-zen host. The parser preserves the exact retained two-field
empty event header, all 147 regime header fields, 146 frame metrics and the
unaltered frame value 2.33344. The fixture provenance records raw/fixture hashes.
A real process-boundary stand-in publishes those shapes with explicitly synthetic
producer/GPU metadata; typed queries retrieve every frame row and page the empty
headers after restart. No raw game file is promoted into a hardware profile.

Actual stdio checks exercise empty event coverage and maximum-length escaped
empty regime headers, preserving all 64 positions through byte-limited pages
within the MCP response budget. Comparisons refuse empty action/regime selections
with a specific missing-row error while frame comparisons still work. Existing
malformed/empty-file/ragged/numeric/producer/provenance failures remain rejected.
Metadata exposes table coverage; metric pages expose label/column headers and
explicit availability. The artifact schema and 26-tool count are unchanged.
The expanded happy-path workflow uses a 15-second job deadline for queued work
and 520-file publication after an earlier five-second happy-path check failed;
its deliberately short 500 ms deadline and cancellation checks are unchanged.

Implementation, parser fixtures, process/MCP checks and final CPU logs are pinned
as `bundle-2051be7525227bb35f9e959f9d96c9ca` in the configured external artifact
root. The snapshot records its source patch against e6d4b9e; the importing
connection remains 0.4.1, and nested source/runtime reports identify 0.4.4.
No new GPU profiling collection or per-draw timing/counter support is qualified.

### Separate current Generate C++ Capture feasibility probe

While CPU checks ran, the existing C++ `ngm-capture --format cpp` path launched a
fresh source-built **0.4.4** reference fixture with seed 42, 192×128, wait-frames 2,
application final frame 20 and the explicit matching-driver overlay without the
layer-disable filter. Matching Nsight **2026.3.1.0/build 38722833** completes in
**9,864 ms** with cleanup confirmed. The complete source capture
`bundle-ab86c4e307dc933f44923389135174de` is pinned in
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/cpp-feasibility-cb52_ry9/store`.
A new server session verifies its persistent pin, one literal draw/pipeline/shader
source relationship, no unsupported recording, and eleven resource references.

The current source server and existing qualified 0.4.2 worker then read fragment
shader handle 14 through its listed resource reference. All **1,300 bytes** match
independently retained application `scene.frag.spv`; SHA-256 is
`425fc681c678f5965bad5dc1cbd42a48958895f5eb5d3c65e0d5c151be881d36`.
The read bundle `bundle-237ed55fe5983258749219d8e4fc6598` is explicitly pinned
in that store. Exact inputs, original output, offline MCP queries and byte oracle
are retained alongside the source store and in the pinned compatibility snapshot.
This establishes a current generated-source/serialized-input path for R-021;
it does not establish executed draw state or resolve I-038's separate graphics
capture timeout. No application source, machine control or profiling permission
is changed by this probe.


## 0.4.5 recorded draw-state subset, 2026-10-15

R-021 adds its first recorded-state capability with **28 shared stdio/HTTP tools**.
The patch increment accompanies the new interpreter, adapter, input fingerprints,
fixture/harness and checks. R-021 remains In Progress for the remaining state/value
assessment; R-022 remains Pending. This does not complete general effective-state
or GPU-observed event-state support. [DRAW_STATE.md](DRAW_STATE.md) defines the
qualified grammar, field statuses and refusals.

The source build is based on **0126133**, with the 0.4.5 changes explicitly dirty.
GCC **16.2.1**, CMake **4.4.4**, Ninja **1.13.2**, Linux **7.2.7-zen1-1-zen**,
Debug diagnostic GLSL/SPIR-V and the pinned submodule revisions are retained in
qualification records. No dependency or proprietary helper closure is added.
The optional reader remains the qualified stable **0.4.2** worker.

Executed checks:

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check_all
```

All **33/33 CPU checks pass**, without skips, in **351.98 seconds**. The new
`ngm_draw_state_check` covers static/dynamic disagreement, missing and partial
setters, static-bind invalidation, valid setter/rebind ordering, reused submitted
recordings, one-time reuse refusal, duplicate annotations, multipart/secondary/
indirect/unknown-command refusals, reset/unsubmitted identity, unknown extensions,
nonfinite values and depth-not-applicable distinction. Actual MCP checks cover
bounded selected results, reader links, foreign/guessed references, same-size
source/database/record-file mutation and stable references/values/pins after
restart. Artifact hashing also exercises a 17 MiB streamed file, bounds, deadline,
path traversal and symlink-directory refusal; descriptor hashing preserves the
caller's file offset. Existing source-relationship and byte-reader contracts pass.
HTTP discovery reports the same 28-tool registry.

After that aggregate, the private fixture witness was strengthened to retain
actual successful pipeline-create/bind handles and shader-module API bytes/hash.
Only the fixture, its private harness and the fixture contract check changed.
The focused `ngm_fixture_contract_check` then passes in **41.59 seconds**, including
all four state scenario selections without a desktop and invalid witness-path
refusal. The final fixture/harness were rebuilt, then all four hardware cases
below were freshly repeated. The earlier four-case feasibility run remains pinned
under `capability-followup/draw-state-Z6C0y7`; it is not substituted for this final
qualification.

The final C++ runner used explicit absolute fixture/shader/tool/output/environment
paths. Commands and complete argv arrays are retained as `draw-state-final-inputs.json`
and `verify-mcp-inputs.json`. The executable modes exercised are:

```sh
cmake --build --preset linux-gcc-debug --target ngm_draw_state_integration
build/linux-gcc-debug/tests/ngm_draw_state_integration FIXTURE SHADERS NSIGHT_ROOT OUTPUT_PARENT ENV_JSON
build/linux-gcc-debug/tests/ngm_draw_state_integration --verify-mcp SERVER WORKER RUN
```

Each case first launches an independent normal fixture, then one fresh documented
Generate C++ Capture with seed **42**, **192×128**, wait-frames **2**, final application
frame **20** and the selected matching-driver overlay without the layer-disable
filter. Matching **2026.3.1.0/build 38722833** tools succeed with cleanup confirmed
and reservations released. C++ metadata reports **RTX 3080 Ti**, NVIDIA driver
`61571`, Linux/Arch; the independent Vulkan baseline reports **615.71.09**, Vulkan
**1.4.351**. The desktop is KDE Wayland, `DISPLAY=:1`, `WAYLAND_DISPLAY=wayland-0`;
the fixture uses XCB/Xwayland. No system permission, clock, driver or display
setting changed. I-038's graphics-format timeout remains a separate unresolved
result.

Final external run/store:
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/draw-state-mukBfY/`.

| Case | Pinned source capture |
| --- | --- |
| state-static | `bundle-69d5b5ae7b8d928bd71b35801bc22fa6` |
| state-dynamic | `bundle-deca2f1209070a68ae77884d5a086208` |
| state-indexed | `bundle-bf8195d8714d160e31fb80b69dbbaf1f` |
| state-count | `bundle-bd1d5937a4bfb03f49531aabe1d0b2bc` |

The selected `frame.2` submission matches the private successful-submission witness.
Seven raster/depth values match independent API inputs. Pipeline create/bind and
active render-pass handles agree inside the private fixture witness; exported
pipeline/stage relationships are audited separately rather than joining native
handles to Nsight symbols by number. Both stages' actual `vkCreateShaderModule`
input bytes/hash agree with the independently retained SPIR-V and exact confined
resource reads. Dynamic creation values deliberately disagree with setters.

The eight independently pinned read bundles, in case order (vertex/fragment), are:
`bundle-880a1b112fd3ff6cc0916713cb198d46`,
`bundle-c75ebab94d247af282ed9c655742caa3`,
`bundle-60a3d8fbcc3a3736d4f36061020dc8ee`,
`bundle-32606bf199a330f9ef8fdbc9f0c474f7`,
`bundle-2ca4fcb9587ff057b0c5f857cbcfe3ee`,
`bundle-7daf7d7d4ecad151f86e77ce339bc24d`,
`bundle-b04f2d2379e7ccf5deabc01bfd234598`,
`bundle-37e8ce7b30f634a0a77eb7057461bfdc`.

Actual source-built 0.4.5 MCP calls return identical fields, references, input
hashes and source/read pins after server restart. All **24** listing/state/read
results pass their advertised JSON schemas (`jsonschema` is a validation aid,
not a product dependency). The byte reader snapshots match the state selection's
capture-time database hashes. Original captures and read bundles stay independently
pinned in the external store. A consolidated **2,517,937-byte / 46-file** report,
transcript, test-log and first-party-source snapshot is pinned in the configured
central store as **bundle-556efc8c7f65c8ebdb96e360197fc015**. Its importer remains
the connected installed **0.4.1** server; payload reports and exact server/fixture/
harness identities describe the tested **0.4.5** build. Import does not promote raw
reports into typed captures or imply the live connection was upgraded.

New state queries refuse unanchored historical inputs and unqualified 2026.2
producers. Descriptors, push constants and uniform/material values explicitly
remain unavailable. Captured GPU execution correspondence and full effective-state
coverage are always false in this subset; none of these checks qualify a general
post-event memory read, indirect/secondary execution, game diagnosis, timing or
counter correlation.


## 0.4.6 opaque push-constant inputs, 2026-10-15

The next R-021 increment adds per-stage byte-write/lifetime tracking and explicit
bounded input reads to `capture_draw_state`. The tool count stays **28**, the
recorded-state result profile becomes **v2**, and the artifact/index/input schemas
remain version 1. Existing 0.4.5 capture-time fingerprints are accepted. This
increment does not complete R-021 or infer descriptor contents, shader-reflected
material types or captured GPU execution state.

Source is based on **5536290**, with the 0.4.6 changes recorded dirty. The existing
GCC **16.2.1** Debug preset, CMake **4.4.4**, Ninja **1.13.2**, Linux
**7.2.7-zen1-1-zen**, matching Nsight **2026.3.1.0/build 38722833** and qualified
stable **0.4.2** worker are used. No dependency, helper closure or machine setting
changes. The diagnostic fixture adds `state-constants.frag`; its complete shader
inventory becomes **13**, and source/provenance/experiment checks agree.

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check_all
```

All **34/34 CPU checks pass**, without skips, in **343.25 seconds**. The new
`ngm_push_state_check` verifies full/middle/overlapping writes against the Khronos
incremental-update byte oracle, undefined holes, complete-source-but-unread
status, compatible layouts with different symbols, incompatible layouts, bind/
restore lifetime, overlapping-stage mask rules, size mismatch, no-push layouts
and atomic refusal of truncated resolved data. Existing source and raster checks
pass after sharing the bounded value grammar. MCP checks exercise linked opaque
references, no hidden default snapshots, explicit worker prerequisite errors and
`pin`/read validation. The reader's existing stand-in tests also refuse an expired
composing deadline. All default checks remain independent of Nsight and GPU work.

The C++ hardware runner is built explicitly, then invoked with the same absolute
fixture, shader, matching-tool, managed-output and launch-environment inputs used
for the preceding qualification, adding `--constants`. Its exact argument arrays
are retained in `capture-inputs.json` and `verify-push-inputs.json`:

```sh
cmake --build --preset linux-gcc-debug --target ngm_draw_state_integration
build/linux-gcc-debug/tests/ngm_draw_state_integration FIXTURE SHADERS NSIGHT_ROOT OUTPUT_PARENT ENV_JSON --constants
build/linux-gcc-debug/tests/ngm_draw_state_integration --verify-push SERVER WORKER RUN
```

Both independent normal baselines and fresh C++ captures succeed with confirmed
cleanup and released reservations. Inputs are seed **42**, **192×128**, wait-frames
**2**, final application frame **20**. The driver selection is the existing
matching 615.71.09 overlay without the layer-disable filter. Exports report
**RTX 3080 Ti**, driver `61571`, Linux/Arch; independent Vulkan observations report
**615.71.09**, Vulkan **1.4.351**. The desktop remains KDE Wayland with XCB/Xwayland.
The first case has three labelled draws and one submission; the second has the
same three draws submitted twice with a fence wait and no rerecording. This is
Vulkan command-buffer reuse within one fresh target, not application-session reuse.

External source/read store:
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/draw-state-ipCOis/store`.

| Case | Independently pinned source |
| --- | --- |
| state-constants | `bundle-afc5e803e3e2dab6a81e132711f9f9d0` |
| state-constants-reuse | `bundle-5ed33cfb1b4494c052d882f6826c34c6` |

The private API witness records per-draw banks formed from the actual full input
and later four-byte update, plus native pipeline/layout identities. The independently audited C++
export contains four ordered binds (`41,47,53,47` within each capture), two push
calls, and respectively one/two queue submits. The compatible layouts have equal
push ranges; the intervening incompatible layout has a different range. No native
handle is joined to an Nsight symbol by numeric equality.

All **nine** listed occurrences have distinct capture/submission/draw references.
Default queries preserve a null aggregate push value, explicit unread segments and
unchanged completed artifact bytes. Explicit reads reconstruct the expected
16-byte banks exactly, including preserved prefix/suffix and restored values:
`01000000010000000000000078563412`, then
`01000000010000000100000078563412`. These are opaque recorded input bytes; only
the private fixture interprets its own uint32 layout. The push-consuming fragment
SPIR-V also matches its independently retained application bytes/hash exactly.

The final native query/restart validation retains **19** independently pinned
read artifacts: fifteen push reads for the nine occurrences, two fragment reads,
and two first-draw push rereads after restart. Their exact IDs and schemas are in
`push-schema-validation.json`/`push-query-report.json`. Default fields, references,
source hashes, capture pins and all earlier/new read pins survive restart. All
**41** actual paging/default/resolved/shader results pass advertised JSON schemas.
The source export audit is retained separately as `push-export-audit.json`.

The consolidated **3,734,137-byte / 44-file** report, transcript, CPU log, code diff
and first-party-source snapshot is pinned centrally as
**bundle-754e06a3afa984d58e2d073bac3b590f**. Its importer remains the connected
installed **0.4.1** server; the payload binds the source-built **0.4.6** server,
fixture and harness hashes. No live connection upgrade is implied. Initial probe
read artifacts remain pinned separately and are not substituted for the final
native qualification.

Read requests retain the existing 256 MiB database, 16 MiB record/blob and 64 KiB
range limits. This composition further limits push inputs to 256 bytes per stage,
eight distinct reads/2,048 input bytes, and a shared read deadline set twenty
seconds after query entry; existing source parsing bounds and owned-process
cleanup remain separate. Missing, incompatible or unread bytes never become
zero-filled values. Descriptor/offset/value assessment, new 2026.2 state support
and R-022 timing/counters remain outside this completed increment.

## 0.4.7 recorded descriptor bindings and offsets, 2026-10-15

This R-021 increment adds compatible descriptor-set/allocation/layout identities
and dynamic-offset mappings to `capture_draw_state`. The shared tool count stays
**28**, the recorded-state result profile becomes **v3**, and artifact/index/input
schemas remain version 1. It accepts existing 0.4.5 capture-time fingerprints.
Descriptor contents, effective buffer addresses, shader array access, material
types and GPU execution correspondence remain unavailable.

Source is based on **224f0e1**, with the 0.4.7 changes recorded dirty. The tested
host uses GCC **16.2.1** Debug, CMake **4.4.4**, Ninja **1.13.2**, Linux
**7.2.7-zen1-1-zen**, RTX **3080 Ti**, driver **615.71.09**, Vulkan **1.4.351**,
KDE Wayland and the fixture's XCB/Xwayland path (`DISPLAY=:1`). Matching Nsight
**2026.3.1.0/build 38722833** performs documented Generate C++ Capture. The existing
explicit per-job library/ICD overlay is retained. No dependency, shader inventory,
proprietary helper closure or machine setting changes; descriptor queries need
no resource worker.

Validation commands:

```sh
cmake --preset linux-gcc-debug
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
cmake --build --preset linux-gcc-debug --target ngm_draw_state_integration
build/linux-gcc-debug/tests/ngm_draw_state_integration \
  /absolute/ngm-vulkan-fixture /absolute/shaders/fixture /absolute/Nsight-2026.3 \
  /absolute/output-parent /absolute/launch-environment.json --descriptors
build/linux-gcc-debug/tests/ngm_draw_state_integration --verify-descriptors \
  /absolute/nsight-graphics-mcp /absolute/returned/draw-state-run
```

The fresh native capture runner uses seed **42**, **192×128**, application final
frame **20** and C++ `wait_frames=2`. Both ordinary launches and both fresh
captures succeed, with cleanup confirmed and GPU reservations released. Each
recording has five labelled draws; the reuse case submits it twice with a fence
wait and no rerecording. This is command-buffer reuse within a fresh target.

External source store:
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/draw-state-pC6JVF/store`.

| Case | Independently pinned source |
| --- | --- |
| state-descriptors | `bundle-160c221f248ae3873459eb60055300b0` |
| state-descriptors-reuse | `bundle-f20b57b94ec9606cec85662623ab880e` |

The private API witness retains actual set definitions, allocations, pipeline
layouts, bind argument arrays, successful submission occurrences and a partial
descriptor-array update. An independent literal export audit associates fixture
debug names with source symbols and pipeline layout inputs. Native handles are
not joined to source symbols by numeric equality. The product never consumes
this private witness or audit.

Dynamic offsets deliberately differ by set/binding/array element and layout
declarations are out of binding-number order. Compatible partial rebinding
preserves the other set. A changed set-zero definition disturbs the higher set;
restoring the pipeline alone cannot revive it. The fixture shader uses only set
zero, so draws with disturbed unused set one remain valid. Product coverage
reports every declared set and does not infer shader usage or descriptor contents.

The final serial aggregate passes **35/35 CPU checks without skips in 329.37
seconds**, including `ngm_descriptor_state_check`, existing draw/push/source/read
checks and actual stdio/HTTP process boundaries. The first full run caught an
unaccepted descriptor flag in the fake test target; the corrected focused MCP
check and final full run pass. Initial/focused/final logs are retained separately.
Formatting and whitespace checks pass. No failed Nsight collection or extraction
attempt occurs in this increment.

The final C++ MCP verifier compares every available raster field, shader-stage/
entry/module-size declaration, descriptor definition, allocation/bind identity
and dynamic offset against the private API inputs for **15** recorded draw
occurrences. The expected disturbed sets stay unavailable. It checks distinct
references, exact selection, paging, unchanged completed artifact bytes, source
pins and identical results after a fresh server process. All **46** state
responses (**16** pages and **30** selections across both passes) pass their
advertised JSON schemas. Including storage checks, all **110** tool results pass.
These queries extract no serialized resources and create no read artifacts;
capture-input fingerprint verification still reads the retained files.

The consolidated qualification is pinned centrally as
**bundle-b59b7343be911eec0e49b85ffe5767ef**, reported as **4,331,358 bytes / 46
files**. It contains the final capture/query reports, transcript, source audit,
schema results, initial/focused/final CPU logs, exact commands, code diff and a
hash-verified **28-file** first-party source snapshot. Native source captures
remain independently pinned in the external store above. The generic importer's
metadata records the connected installed **0.4.1** server; the payload explicitly
binds the tested **0.4.7** server, fixture and harness hashes. The import is not a
service capture, and no installed/live-server upgrade is implied.

R-021 remains In Progress for the descriptor contents/value and general
captured-execution feasibility outcome. R-022 timing/counters stays Pending.
New state support on 2026.2 remains unqualified. No broader state or value
availability follows from this recorded binding capability.

## 0.4.8 profile collection controls and undefined cells, 2026-10-15

This first R-022 slice retains the **28-tool** surface and artifact/index schema
version 1. Nullable `time_every_action`/`hes_enabled` settings preserve tool
defaults, retain exact requests/argv and verify exported per-action selection.
HES activation remains unverified. Only exact **2026.3.1.0/build 38722833** regime
`nan` cells become null/unavailable values. Finite cells remain available;
comparison refuses a selected missing value. The C++ optional number and nullable
MCP metric value require consumer handling; see [PROFILE_CONTROLS.md](PROFILE_CONTROLS.md).

The source build is based on **4ab02f2**, with the tested 0.4.8 code explicitly
dirty. The recorded host is GCC **16.2.1**, CMake **4.4.4**, Ninja **1.13.2**,
Linux **7.2.7-zen1-1-zen**, RTX **3080 Ti** / driver **615.71.09**, KDE Wayland
with the fixture's XCB/Xwayland path. Only the matching 2026.3 installation is
present. The explicit per-job overlay selects the existing matching library/ICD
directory, QT XCB and SDL Wayland; no layer-disable filter is supplied. Desktop,
overlay/library/ICD, compiler, diagnostic shader and executable identities are
retained. The default NVML/library mismatch remains outside this change.

The final serial aggregate passes **35/35 checks, zero skipped, in 352.11 s**:

```sh
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
```

Existing profile evidence/workflow/comparison checks cover nullable types,
defaults, explicit false versus unspecified settings, missing help, export
mismatches, finite/unavailable neighbors and comparison refusal. The pure parser
rejects unqualified producers/tokens and nonfinite timing/frame values. The
sanitized 587-byte actual regime subset retains two undefined cells and duplicate
column names. Discovery now recognizes option declaration lines, preventing
flag-shaped diagnostic paths from satisfying a missing-help gate. Actual stdio
and shared HTTP checks pass within the aggregate. Scoped formatting/whitespace
review passes; no product code changes follow the final aggregate.

The opt-in C++ `ngm_profile_controls_integration` target is excluded from ordinary
CPU checks. Its reproduction command uses six absolute input paths:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_controls_integration
/absolute/build/tests/ngm_profile_controls_integration \
  /absolute/build/nsight-graphics-mcp /absolute/build/ngm-vulkan-fixture \
  /absolute/build/shaders/fixture /absolute/Nsight-installation \
  /absolute/validation-parent /absolute/launch-environment.json
```

The runner explicitly forwards the desktop environment, then requests two fresh
`state-descriptors-reuse` profiles: seed **42**, **192×128**, final application
frame **300**, collect three frames after five presents, **1000 ms**, Throughput
Metrics / Ampere GA10x, HES false, per-action timing false then true, **120000 ms**
deadline, pinned. Both jobs succeed with owned-process cleanup and no retained GPU
reservation. `REPRO_INFO.xls` confirms **No/Yes** respectively. No repeated-run
performance or overhead comparison is inferred from this pair.

External native validation store:
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/profile-controls-NP8O6H/store`.

| Per-action request | Independently pinned profile | Regime rows × numeric columns | Finite / unavailable cells |
| --- | --- | --- | --- |
| false | `bundle-238953e1f0532f26e65e9df6974dacf2` | 41 × 438 | 17812 / 146 |
| true | `bundle-4c11944bc2bd355501defdefea0c8b04` | 42 × 438 | 18396 / 0 |

An independent raw-table hash/position audit agrees with whole-table coverage
and sampled finite/null metric cells. All four tables remain inspectable.
Metadata and pins match after restart. All **202** native-run tool results pass
their actual JSON Schema 2020-12 output contracts and text/structured parity.
A final read-only restart after isolating I-041's incomplete negative copies
confirms unchanged metadata and complete/pinned status; **four** additional
results pass their schemas. The current server, fixture and harness binary
hashes still match the exact successful-run report.

A separate **CPU-only** compatibility run uses the installed **0.4.1** server and
C++ Nsight stand-ins to create an intact ten-field native profile through the
normal service. The source-built 0.4.8 server reads that unmodified store in two
new processes, preserving the old request, pins and identical metadata. Omitted
controls stay null; the exported default is observed rather than asserted as an
explicit request. This is old-product-format compatibility, not real 2026.2
hardware/tool qualification.

I-040 preserves the earlier real export failure, including 292 undefined regime
cells with timing enabled. The fresh off profile above also contains undefined
cells; causation is unresolved. I-041 preserves the refused incomplete historical
native-bundle restoration and its store-wide integrity consequence. Neither
failed bundle is repaired/promoted. Before/after observations show unchanged
profiling restriction, capability control files and device ACLs; pre-existing
profiler-device/trace access suffices. No permission/clock/driver/display change
is performed.

The consolidated qualification is pinned as
**bundle-193d06c97b109c55fb8a30e35f12b8f9**, **2,641,144 bytes / 63 files**, with a
hash-verified **28-file** first-party source snapshot. All 63 imported files
match their source hashes; complete/pinned state and released usage are checked.
It retains reports, transcripts, actual output schemas, selected raw exports, final CPU log,
exact commands, failure/isolation audits, code diff and hash-verified first-party
sources. This is a selected-evidence snapshot, not a complete native-bundle
backup or a typed profile import. Its connected generic importer is still the
installed **0.4.1** server; payload identities bind the tested **0.4.8** binaries.
No installed/live-server upgrade is implied.

**R-022 remains In Progress.** Actual timing rows are present, but their
direct/indexed draw or marker-occurrence correlation, repeated/nested/reused
cases, independent timestamp/overhead sanity and counter scopes remain to be
qualified. No draw-local metric, fragment-only interpretation, duplicate-column
statistic/frame identity or new two-release support follows from this slice.

## 0.4.9 graphics timing qualification workloads, 2026-10-15

This R-022 slice adds three graphics timing scenarios and an opt-in C++ matrix
runner. It retains **28 tools**, artifact/index version 1, the thirteen-shader
bundle and existing dependencies. No product timing/counter query is added.
[PROFILE_CORRELATION.md](PROFILE_CORRELATION.md) defines the actual workload,
timestamp scope, predeclared tolerance and remaining query qualification.

Source is based on **7342514**, with tested 0.4.9 changes explicitly dirty.
The host remains GCC **16.2.1**, CMake **4.4.4**, Ninja **1.13.2**, Linux
**7.2.7-zen1-1-zen**, RTX **3080 Ti** / driver **615.71.09**, requested Vulkan
**1.3** on the recorded **1.4.351** device, KDE Wayland with XCB/Xwayland.
Diagnostic GLSL/SPIR-V and exact executable/compiler/driver/desktop/overlay
identities are retained. No profiling permission, clock, driver or display
setting is changed; before/after capability files and ACLs match.

The verified build/collection commands are:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_correlation_integration
/absolute/build/tests/ngm_profile_correlation_integration \
  /absolute/build/nsight-graphics-mcp /absolute/build/ngm-vulkan-fixture \
  /absolute/build/shaders/fixture /absolute/Nsight-installation \
  /absolute/validation-parent /absolute/launch-environment.json
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
```

The final serial aggregate passes **35/35 CPU checks, zero skipped, in 353.28 s**
(353.26 s summed CPU-test time). It runs after native collection, with no GPU
matrix concurrent. Fixture contract coverage includes all three timing scenarios,
fresh/existing witness paths and pre-display rejection without fabricated timing
evidence. Scoped formatting and whitespace checks pass. Current server, fixture
and harness hashes still match the successful native report; no first-party code
changes follow the aggregate.

All **six baselines** and **fifteen fresh matching 2026.3.1.0/build 38722833
profiles** pass. The three validation baselines have synchronization validation
active and clean. Selected images agree exactly across direct/indexed/reused
cases within each final-frame input. The independent audit checks **400**
timestamped submissions, actual begin-label parent/occurrence identities,
direct/indexed arguments and query conversions/wrap bounds. The selected-frame
readback label remains outside the timed scene. GPU timestamps have **64** valid
bits and a **1 ns** period.

For the three one-frame profiles, exported marker trees match actual API inputs.
Nsight versus private scene totals differ by **32 ns**, **64 ns** and **160 ns**
for direct, indexed and reused aggregate respectively, at most **0.024%**. All
pass the predeclared **max(0.025 ms, 25%)** whole-scene tolerance. Per-action
on/off observations remain descriptive/inconclusive under uncontrolled clocks,
desktop load and one fresh launch per matrix condition. They are not causal
overhead or speedup claims. All samples and their scope are retained.

The native source store is
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/correlation-final-7h9rhq73/profile-correlation-IqjCBy/store`.
All fifteen source profiles are independently pinned; the report lists every
request/ID. The single-frame profiles used for the numerical sanity check are:

| Case | Pinned source |
| --- | --- |
| Direct | `bundle-044dab66242bb5eeac71fd3f1c8c7be5` |
| Indexed | `bundle-9ace4501580edf645c9ff7a2b1146578` |
| Reused recording | `bundle-5c9adcfd691099021e4611056bdab72d` |

Identical metadata/event pages and source pins pass after a fresh server process.
All **1,615** native tool results pass their advertised JSON Schema 2020-12
contracts and text/structured parity: 15 profiles, 1510 job-status results and
30 each of artifact info, profile metadata and profile metrics. Raw-table hashes,
finite/unavailable positions, marker paths and actual API witnesses are audited
independently from product parsing. I-043 records missing action/occurrence/sample
keys and counter-scope limits; no missing identities are fabricated.

I-042 preserves the private verifier's initial false rejection of the valid
eleventh selected-frame readback marker. That application exits 0 and no Nsight
job starts. The corrected full matrix above is a fresh run. An earlier exploratory
fifteen-profile path-only witness matrix remains separately identified and pinned
at its source; it is superseded for marker-occurrence qualification.

The consolidated qualification snapshot is complete/pinned as
**bundle-40d1c8f983a49246a26d5294f20636bb**, reported as **15,725,753 bytes / 199
files**, including **75** raw text exports and a **24-file** first-party source
snapshot. All 199 imported files match their source hashes; current source and
tested binary identities match, and released usage/pin state are checked.
It retains final raw-export identities, reports, transcripts/schemas, exact
commands, independent audit/verifier, private witnesses, image references,
source diff/snapshots and the final CPU receipt. It is selected evidence rather
than a complete native-bundle backup or typed profile import. The generic
importer remains installed **0.4.1**; payload identities bind tested **0.4.9**
binaries. No installed/live-server update is implied.

**R-022 remains In Progress.** The next product slice is the bounded unique
single-frame marker query with explicit ambiguity/scope refusals. General draw
IDs, queue/submission correspondence, multi-column frame identity, isolated
draw counters and physical counter scaling remain unqualified. New support on
2026.2 remains unqualified; that matching installation is absent locally.

## 0.4.10 retained marker-range queries, 2026-10-15

This R-022 slice adds **profile_marker** to the shared **29-tool** stdio/HTTP
surface. It returns exported marker-range duration in ms and up to eight exact
raw regime metrics for a uniquely associated single-frame path. Original
execution IDs, draw membership, physical counter scaling and shader-stage
isolation remain unavailable. [PROFILE_MARKERS.md](PROFILE_MARKERS.md) specifies
the qualified producer, collection shape, bounds and refusal contract.
No dependency, artifact/index schema or proprietary helper closure changes.

The source build is based on **4fbf834**, with the tested 0.4.10 changes explicitly
dirty. It uses GCC **16.2.1**, CMake **4.4.4**, Ninja **1.13.2** and Linux
**7.2.7-zen1-1-zen**. Actual profiles retain the **0.4.9** collection identities,
matching Nsight **2026.3.1.0/build 38722833**, RTX **3080 Ti** / **615.71.09**
and desktop/configuration recorded in the preceding section. This validation
launches no Nsight/GPU process and changes no machine setting or installed server.

The verified commands are:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_marker_check_run
cmake --build --preset linux-gcc-debug --target ngm_profile_workflow_check_run
cmake --build --preset linux-gcc-debug --target ngm_profile_marker_integration
/absolute/build/tests/ngm_profile_marker_integration \
  /absolute/build/nsight-graphics-mcp /absolute/source-correlation-run \
  /absolute/validation-parent
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
```

The focused pure marker check passes and the expanded process-boundary workflow
check passes in **18.61 s**. All **36/36 serial CPU checks pass, zero skipped, in
355.68 s** (355.66 s summed CPU-test time). The regression subset preserves exact
single-frame event rows and two regime columns from the pinned 0.4.9 direct
profile. Pure coverage includes unique/different-branch/repeated paths, malformed
hierarchy, absent anchors, header-only regimes and invalid/zero timing values.
Workflow coverage includes missing/duplicate/undefined metrics, unsupported
collection/producer cases, strict arguments and actual stdio restarts. C++
stand-ins establish no additional hardware compatibility.

The opt-in C++ verifier queries every event row of all **fifteen** retained native
profiles, in two fresh source-built stdio processes. Its **360 marker responses**
contain **32 available**, **48 ambiguous/unavailable** and **280 unsupported**
selections. Independent raw TSV parsing and the original API/timestamp audit
check exact paths, values/text, row/column positions and source hashes. The
one-draw and two-draw fixture markers both preserve null product draw membership.
All results are identical after restart; all fifteen native source bundles stay
complete/pinned with unchanged inventories and released usage leases.

All **391** native tool results pass their actual JSON Schema 2020-12 contracts
and text/structured parity: capabilities ×1, profile_marker ×360 and artifact_info
×30. A separate authenticated HTTP process on an ephemeral IPv4 loopback port
discovers the same 29 tools and exactly matches available, ambiguous and
unsupported stdio selections. Its **eight** schema-valid tool results comprise
capabilities ×1, profile_marker ×3 and artifact_info ×4. Source inventories/pins
remain stable; session deletion returns 204, SIGTERM exits 0 and stdout is empty.
The temporary 0600 token file is removed; no credential is retained in evidence.

The validation root is
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/profile-marker-93WsgI`.
The original source store remains the 0.4.9 correlation store above; its fifteen
native pins are separate from the marker qualification. The consolidated
qualification is complete/pinned as **bundle-b8d3515b5cd6079e2ba2c8cb5637067e**,
reported as **10,377,072 bytes / 189 files**, with **34** first-party source files
and **75** original raw text exports. It retains
reports, actual schemas/transcripts, independent export/source references, exact
first-party source/diff, binary hashes, HTTP checker and final CPU receipt.
Every imported payload hash and the current source/server/verifier identities
verify; usage is released. This selected evidence is not a complete native
profile backup or typed import. The generic importer remains installed **0.4.1**;
payload identities bind the tested **0.4.10** reader and **0.4.9** profile sources.
Scoped formatting, document links and whitespace checks pass. No first-party
code changes follow the passing aggregate.

**R-022 remains In Progress** for the remaining scope/overhead assessment.
Repeated paths, multi-column sample identity, general action/queue/submission
mapping, isolated draw counters and physical scaling remain explicit limitations
under I-043. New 2026.2 marker support remains unqualified.

## 0.4.11 repeated collection-control assessment, 2026-10-15

This slice adds `--repeat-overhead` to the existing opt-in C++
`ngm_profile_correlation_integration` runner. Product behavior, **29 tools**,
artifact/index version 1, the shader/dependency inventory and proprietary helper
closure remain unchanged. [PROFILE_OVERHEAD.md](PROFILE_OVERHEAD.md) records the
predeclared scope, eight-launch order, warmup, tolerances and all measurements.

Source is based on **2b3dbac** with explicitly dirty 0.4.11 changes. GCC **16.2.1**,
CMake **4.4.4**, Ninja **1.13.2**, Linux **7.2.7-zen1-1-zen**, RTX **3080 Ti** /
**615.71.09**, matching Nsight **2026.3.1.0/build 38722833** and the existing
KDE Wayland/Xwayland-XCB path remain recorded. Exact executable, shader, overlay
and desktop identities are retained. No permission, clock, driver, display or
other-application change is performed; access observations match before/after.

Verified commands:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_correlation_integration
/absolute/build/tests/ngm_profile_correlation_integration \
  /absolute/build/nsight-graphics-mcp /absolute/build/ngm-vulkan-fixture \
  /absolute/build/shaders/fixture /absolute/Nsight-installation \
  /absolute/validation-parent /absolute/launch-environment.json --repeat-overhead
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
```

Both direct-workload baselines pass, with synchronization validation active/clean
on the validation baseline. All **eight** fresh one-frame profiles succeed in
four alternate-order pairs after **30** warmup presents. Independent auditing
verifies **326** private timestamped submissions, exact marker/API input trees
and all selected frame-30 scene durations, with **32–96 ns** export/timestamp
differences inside the declared max(0.025 ms, 25%) tolerance. Every source job
confirms owned cleanup, released GPU reservation and its own persistent pin.

All **913** actual MCP results pass JSON Schema 2020-12 and text/structured parity:
profile ×8, job_status ×817, artifact_info ×16, profile_metadata ×16,
profile_metrics ×40 and profile_marker ×16. Metadata/event/scene query results
and pins remain identical after server restart. The complete **1,168** raw frame
cells match actual MCP pages; **80** selected raw counter facts preserve exact
names/text, source hashes and unresolved units/physical aggregation.

Private scene off/on medians are **0.333424 / 0.333872 ms**; paired ratios are
**0.999616, 1.009751, 1.002107, 0.994629**. Mixed changes under uncontrolled
desktop/clocks/power remain inconclusive as causal overhead. All **92** read-only
telemetry probes succeed, but do not prove interval-local clock stability.
Raw frame draw-count cells vary **10/71** while every selected scene has six
private API draw calls; timing-on scene regime cells vary **7/7/7/3**. I-044 and
I-045 retain these limitations without excluding samples or inferring a cause.

All **36/36 serial CPU checks pass, zero skipped, in 353.15 s**
(353.13 s summed CPU-test time). The CPU aggregate runs after GPU
collection; no concurrent hardware matrix is present. Scoped formatting,
document-link and whitespace checks pass. No first-party code changes follow
the successful aggregate.

The native root is
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/overhead-final-u6kqwk8_/profile-overhead-Bft1vF`.
All eight complete source profiles are independently pinned in its `store`.
The consolidated qualification is complete/pinned as **bundle-8d667f47f660fc46a582f440ee786c5e**.
It reports **12,878,496 bytes / 458 files**, including **40** original raw text
exports and **259** first-party source/fixture files.
It retains the declared plan, every request/response/sample, raw text exports,
private API/timestamp witnesses, independent verifier/audit, telemetry/access
observations, exact source/diff and final CPU receipt. Imported payload/source/
binary hashes and released retention state verify. This selected evidence is
not a full native-profile backup or typed import. The generic importer remains
installed **0.4.1**; payload identities bind **0.4.11** collection/reader binaries.
The installed/live server is unchanged. New 2026.2 support remains unqualified.
R-022 stays In Progress for its final scope/conditional-outcome audit.

## 0.5.0 capability follow-up completion, 2026-10-15

The **R-018–R-022 group** completes at **0.5.0**, with **29 tools**. The minor
increment subsumes a patch increment and resets PATCH. First-party runtime logic
is unchanged from 0.4.11; the source delta is the authoritative CMake version and
the conditional scope/completion documents. Source is based on **99119b1**, with
the tested 0.5.0 change explicitly dirty. GCC **16.2.1**, CMake **4.4.4**, Ninja
**1.13.2** and Linux **7.2.7-zen1-1-zen** remain the recorded build host.
All seven exact dependency pins and the five-file helper policy are unchanged.

[CAPABILITY_FOLLOWUP_COMPLETION.md](CAPABILITY_FOLLOWUP_COMPLETION.md) audits
every A–E requirement against code, executed checks and retained sources.
[DRAW_STATE_FEASIBILITY.md](DRAW_STATE_FEASIBILITY.md) and
[PROFILE_SCOPE_FEASIBILITY.md](PROFILE_SCOPE_FEASIBILITY.md) retain the specified
conditional unavailable outcomes. Completing the plan does not claim observed
full GPU state, execution-time descriptor/material values, isolated draw/fragment
counters, physical counter normalization or a causal instrumentation correction.

The final serial aggregate passes **36/36 CPU checks, zero skipped, in 355.49 s**
(355.47 s summed CPU-test time). This includes actual worker confinement, parser/
state/refusal matrices, process/queue/cleanup cases, stdio/HTTP contracts, binary
linkage and staged installation. No GPU collection runs concurrently or at this
release increment. All earlier hardware results retain their actual versions.

The final C++ retained checks use complete byte-identical copies of the original
service stores in isolated directories. No manifests are reconstructed and no raw
imports are promoted to typed inputs. Existing verifier modes execute no Nsight
or GPU process. Verified commands include:

```sh
cmake --build --preset linux-gcc-debug --target nsight-graphics-mcp ngm-capture \
  ngm_retained_resource_integration ngm_profile_marker_integration
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
cmake --build --preset linux-gcc-debug --target ngm_draw_state_integration
/absolute/build/tests/ngm_draw_state_integration --verify-mcp \
  /absolute/build/nsight-graphics-mcp /absolute/verified-worker /absolute/raster-copy
/absolute/build/tests/ngm_draw_state_integration --verify-push \
  /absolute/build/nsight-graphics-mcp /absolute/verified-worker /absolute/push-copy
/absolute/build/tests/ngm_draw_state_integration --verify-descriptors \
  /absolute/build/nsight-graphics-mcp /absolute/descriptor-copy
/absolute/build/tests/ngm_retained_resource_integration \
  /absolute/build/nsight-graphics-mcp /absolute/reader-copy/store \
  /absolute/cases.json /absolute/reader-copy /absolute/verified-worker -
/absolute/build/tests/ngm_profile_marker_integration \
  /absolute/build/nsight-graphics-mcp /absolute/marker-source-copy /absolute/final-root
```

All five retained verifier groups pass. Four raster captures, nine push draws
and fifteen descriptor draws supply **28 distinct recorded occurrences**, checked
against their original private API witnesses in two fresh processes per group.
Eight exact shader reads, explicit partial-byte reconstruction, the independent
2,700-byte whole/split/end oracle, foreign references and restart pins pass.
The stable configured worker remains the qualified **0.4.2** binary; the test
server/inspection version is **0.5.0**. Native sources retain 0.2.12/0.4.5/0.4.6/
0.4.7 identities and their exact producer tuples.

The final marker reader repeats all **360** selections from the fifteen actual
0.4.9 profiles, preserving **32 available**, **48 ambiguous/unavailable** and
**280 unsupported** results across restart. An authenticated ephemeral loopback
HTTP process discovers the same 29 tools and exactly matches the available,
ambiguous and unsupported selections. Session deletion, clean exit/empty stdout,
source inventory/pin stability and private-token removal pass.

All **691 successful tool results** pass their actual JSON Schema 2020-12
contracts and text/structured parity: raster **48**, push **117**, descriptor
**110**, reader **17**, marker **391**, HTTP **8**. The ten expected tool-error
refusals (eight stale draw references/two unknown resource references) are
separately checked by the C++ verifiers. All **31** new read artifacts are
complete/pinned. Original input payloads in the copies remain unchanged.

The read-only retention audit verifies **14** earlier qualification/failure
snapshots, all **1,281** payloads against their frozen import sources, and
**87** independently pinned native bundles with **3,099** payloads. The native
inventory contains 84 complete bundles and the three explicitly failed I-038
captures. Existing configuration still names the separately installed qualified
0.4.2 worker; its hash matches the original qualification. No system setting,
operator launch configuration or installed server is changed.

Final evidence is under
`/home/vordhosbn/.local/share/nsight-graphics-mcp/capability-followup/followup-final-38iee9ng`.
Its native copies/new read results retain independent pins. The consolidated
completion snapshot is complete/pinned as **bundle-bc107b133c527aa81f11c17fa93c9743**,
reported as **43,443,811 bytes / 617 files**, including **259** first-party
source/fixture files and selected complete evidence from all **31** new read
bundles. It retains the requirement audit, exact current source/diff and binary identities,
all final transcripts/schemas/reports, CPU receipt, original pin/hash inventories,
verifier commands and selected new-read evidence. This is selected qualification
evidence, not a full native-store backup or typed import. Every imported payload
and current source/binary hash verifies; usage is released. Scoped document-link
and whitespace checks pass. No first-party code changes follow the aggregate.
The installed generic importer/live server remains **0.4.1**; payloads bind the
source-built **0.5.0** reader and their original GPU collection/build identities.

R-018–R-022 are Done within the accepted conditional scope. I-038/I-039 and
I-043–I-045 remain explicit limitations with revisit conditions, not new pending
priorities. No fresh game run, new two-release state/timing qualification,
hydration extension, installation, publication or machine-setting change is
implied by group completion.

## 0.5.1 fresh Graphics Capture reliability (R-023), 2026-10-15

R-023 completes on the measured **Nsight 2026.3.1.0/build 38722833** path at
**0.5.1**, retaining **29 tools**. The host is Linux
**7.2.7-zen1-1-zen**, RTX **3080 Ti**, kernel driver **615.71.09**, KDE
Wayland/Xwayland with the fixture's **XCB** backend, GCC **16.2.1**, CMake
**4.4.4** and Ninja **1.13.2**. The explicit existing matching-library/ICD overlay
is the one retained by the earlier launch investigation; SDK control is unused.
No driver, display, permission, clock, installed library or user-server change
is made.

The initial **0.5.0** reproduction passes its independent application baseline
but times out after **120211 ms**. Read-only owned-process observations show
the injected fixture waiting for its Zenity low-space warning: about **5.4 GB**
is available, below the tool's recommended **20 GB**. The warning and installed
help document `--no-low-disk-space-check`. The adapter now supplies it only when
advertised; retained 2026.2 help lacks it and the CPU legacy branch rejects an
unknown flag. Managed quota/publication failures and existing job cleanup still
apply. I-046 retains the diagnosis; no cause is assigned retroactively to
I-038's earlier attempts without child-state evidence.

The current C++ `ngm_launch_environment_integration` freezes server, fixture
and shader inputs and accepts `--scenario reference|shader-error`. Its explicit
server/fixture/shaders/Nsight/store/report/overlay invocation runs three separate
MCP cases, each with a fresh standalone frame-2 baseline:

| Case | Capture job time | Independently pinned source |
| --- | ---: | --- |
| Reference | 6655 ms | `bundle-644cc39112ddd32696ce24823764ca97` |
| Reference repeat | 6354 ms | `bundle-4163726d809bb9b09cc1758f7d0399d2` |
| Shader fault | 6537 ms | `bundle-7b77529e3352f19296bd669906110bfa` |

All captures and five exports per capture succeed; owned cleanup and released
GPU reservations verify. Target PIDs **3549142**, **3549323** and **3549516**
are distinct. Independent Pillow RGB8 decoding gives exact screenshot versus
application-readback equality for every pixel in all three cases. References
repeat exactly and the faulty image differs. These are actual source-server
MCP captures, not stand-ins or embedded images presented as GPU replay output.

Source store:
`/home/vordhosbn/.local/share/nsight-graphics-mcp/roadmap-resolution-aytro126/graphics-fixed-store`.
The failed original **bundle-1dae31356d8a009e11c1befd3e684be3** is separately
pinned in the adjacent `graphics-current-store`.

The verified build/check sequence includes:

```sh
cmake --build --preset linux-gcc-debug --target ngm_nsight_check_run ngm_capture_service_check_run ngm-capture nsight-graphics-mcp ngm_launch_environment_integration
CTEST_PARALLEL_LEVEL=1 cmake --build --preset linux-gcc-debug --target ngm_check_all
cmake --build --preset linux-gcc-debug --target ngm_mcp_check_run ngm_http_check_run
```

The serial aggregate passes **36/36 CPU checks**, no skips, in **354.85 s**
(354.83 s summed test time). Its new low-space stand-in blocks without the
documented control and the legacy help branch rejects sending it. An initial
focused run caught the new test launches changing an existing two-target count;
the additional cases were moved after that assertion before the passing aggregate.

The aggregate precedes a final narrow schema correction. The independent actual
result audit found the pre-existing missing `producer.api_identification` output
property (I-049). The common metadata/event/object schema now declares the bounded
two-value discriminator. Focused actual-process MCP and HTTP checks then pass in
**27.26 s / 4.30 s**, including closed-member/discriminant regression checks.
The final server passes full JSON Schema 2020-12 validation for **20 actual
tool results** in five server processes. Two graphics-store processes return
identical metadata and complete/pinned source state; two failed-store processes
preserve the failed pin. A fifth process validates the alternate discriminator
through metadata/events/objects on retained compute
`bundle-1a3778da489d8ddd8f09e8099946db65`; that GPU collection retains its **0.3.0**
identity. All **192** audited source files remain byte-identical.

The GPU collection server precedes that schema-only correction; qualification
retains both collection and final-server binary hashes/bytes rather than
relabeling the earlier collection. No new GPU collection occurs after the
correction. The ordinary CPU suite remains separate from hardware qualification;
the 2026.2 branch is CPU/help compatibility, not a new second-release GPU claim.

Complete/pinned qualification **bundle-ab49ccec89e6f4584051359b650e7f41** is in the
configured central store. All **407** source/imported payload hashes and lengths
verify; the bundle reports complete, pinned and no active use. It retains exact
source/diff/submodule identities, both server binaries, fixture/harness/CLI,
raw case and audit transcripts, independent baselines, process observations,
CPU/focused receipts and I-047/I-048's private replay-probe build corrections.
Its importer is the installed **0.4.1** process; payloads identify tested
**0.5.1** binaries and the original **0.5.0** failed run. Publication receipts and
working orchestration are under the external `roadmap-resolution-aytro126` root.

One separately pinned private matching-tool replay succeeds as
`bundle-1ed49923b28aa4c6c8dff50ebf85d95d` in `replay-store`, with initialization
and execution-engine completion plus successful process exit. This is preliminary
R-027 evidence; product replay integration and independent rendered-output/
repeatability qualification remain In Progress. R-024–R-026 are still Pending.

## 0.5.2 retained-capture GPU replay (R-027), 2026-10-15

R-027 completes at **0.5.2** with **30 shared stdio/HTTP tools**. The typed
`run_nsight_replay` backend, `CaptureService::replay` and MCP `replay` tool use
finite matching-tool GPU execution, a source lease across queue/execution,
cancellable immutable snapshots, fresh header validation and the shared
capture/profile GPU reservation. A common finalizer retains typed worker outcomes,
cleanup quarantine, failed evidence and quota/publication recovery. New Graphics
Captures retain payload SHA-256; legacy inputs explicitly keep an unverified
original-byte identity scope. [REPLAY.md](REPLAY.md) specifies limits and behavior.

The serial GCC Debug command `cmake --build --preset linux-gcc-debug --target
ngm_check_all -j 4` passes **37/37 CPU checks**, **381.19 seconds** elapsed in CTest.
The new replay workflow passes in 9.97 seconds, including native-format/import/
failed-result refusals, source mutation after queueing, wrong selected release,
bounds, queued source retention after unpin, queued/active cancellation, deadlines,
escaped child reaping, shared capture/profile serialization, service shutdown,
persistent pins and actual stdio calls. Actual HTTP also submits/inspects replay.
`ngm_resource_read_check` preserves narrower reader limits while checking the
extended copier's cancellation and 1 GiB upper bound. The aggregate includes
Nsight, ordinary capture, MCP/HTTP, dependency, linkage and install checks.
Focused CPU checks previously passed; I-052 retains initial build/test corrections.

Actual **0.5.2** collection uses the matching **2026.3.1.0/build 38722833** tools,
RTX 3080 Ti / kernel driver **615.71.09**, KDE Wayland and XCB/Xwayland.
The explicit process-local driver-library/ICD overlay is the previously validated
matching selection; no machine setting changes. The original application is not
launched by replay. The separately retained **0.5.1** source captures keep their
original executable/shader/capture identities.

| Source case | New native replay bundle | Actual result |
| --- | --- | --- |
| Reference `bundle-644cc39112ddd32696ce24823764ca97` | `bundle-96495afffde5b48b55f5c68d6500f68e` | Three loops, 3976 ms, success/cleanup |
| Repeat `bundle-4163726d809bb9b09cc1758f7d0399d2` | `bundle-7d53c8a03e7981510d6720adecd9455d` | Three loops, 3907 ms, success/cleanup |
| Fault `bundle-7b77529e3352f19296bd669906110bfa` | `bundle-03b3a8c85ca5e3f92654a96d168f8b2a` | Three loops, 3851 ms, success/cleanup |

All input snapshots match observed source hashes; legacy sources have no
capture-time payload hash and remain labelled accordingly. Two server restarts
preserve reports/pins. **73** actual successful results validate against advertised
schemas. An independent lifecycle run observes the execution-engine phase before
cancellation, deadline and EOF shutdown; all three retain failed/pinned bundles
with confirmed cleanup and released GPU ownership:
`bundle-3380de532946d3654741e9d0cf24b99f`,
`bundle-d3aa9d37d2b8ffe30d8c46e1d034b13d`, and
`bundle-a94f31f4a748bee9c14f32695bdf0561`. Its **442** successful results validate
against the same schemas, totaling **515**. These are intentional failure checks.

Earlier **0.5.1** private probes supply the independent image evidence: three
plain replays and four captures of replay pass; healthy/repeated/faulty output
matches independent frame-1 application RGB8 pixels exactly. The embedded source
image is a distinct frame-2 view; a no-blit control changes fresh output. I-050/
I-051 retain the failed shell-launch verification and initial oracle mismatch.
The pinned 340-payload probe snapshot remains
**bundle-9592f2cd01c4623855009d7589ed37af**, with all eight probe attempts separately
pinned. No generic application-frame mapping, executed event state or replay
performance measurement is inferred. The older 2026.2 replay path remains
unqualified and is refused by the new backend.

Final qualification is complete/pinned
**bundle-1c59130aafc0cb1a946cd9e0c4da869c**. All **741** source/imported payloads
(**94,894,691 bytes** of payload) match SHA-256 and length, and the portable
imported verifier passes all 515 schemas, six job bundles and independent image
checks. It contains the full nine-bundle native store snapshot, original probe
payloads, frozen tested server/source, request/response transcripts, CPU and ELF
logs, and diagnostics. All nine native source/normal/cleanup bundles remain
independently pinned. `artifact_info` confirms complete/pinned/not in use.
Importer identity remains the installed **0.4.1**, separately recorded from the
tested **0.5.2** binary.

The final build and both collected server copies are byte-identical SHA-256
`d1f7c3d7577047c00e5e9d2493d0d41a375b336142813cf20909beef19b9744e`.
MCP and capture CLI ELF dependencies are limited to the documented system
C/C++/math runtimes; vendored code remains static. No installed server/worker,
proprietary helper closure, profiling access, driver, clock or desktop setting
changes. **R-024–R-026 remain Pending** and require their actual documented
state/correlation/counter evidence; R-027 completion does not satisfy them.

## 0.5.3 consecutive capture intervals, R-025 foundation, 2026-10-15

This slice keeps **R-025 In Progress** and the shared surface at **30 tools**.
`capture.frame_count` and `ngm-capture --frame-count` expose the existing backend's
bounded 1..600 interval control, default one. Counts above one require matching
**2026.3.1.0/build 38722833**; older single-interval behavior remains available.
Non-graphics formats reject the option. The exact count is retained in report/
manifest provenance. The typed metadata view preserves the producer's separate
plural `captured_frames` raw string and singular `captured_frame`, with missing
fields null. Replay compares both fields against its fresh matching header.
[MULTI_INTERVAL_CAPTURE.md](MULTI_INTERVAL_CAPTURE.md) defines the contract.

The first CPU build exposed a check-local name collision (I-057), with its full
failure log retained. After correction, all **37 CPU checks** pass in **372.66
seconds**, before adding the plural metadata field. The **final** full serial
GCC Debug aggregate passes **37/37** in **368.46 seconds**:
`cmake --build --preset linux-gcc-debug --target ngm_check_all -j 4`.
It includes the actual tool-boundary producer gate, shared service count/format/
bounds checks, HTTP count-three forwarding, closed MCP input validation, raw
plural-field parser cases, replay, linkage and installation checks.

Serial actual hardware uses RTX 3080 Ti / kernel driver **615.71.09**, existing
KDE Wayland-Xwayland/XCB and the explicit matching-driver library/ICD overlay.
The private queue-label fixture retains its **0.5.2** build identity; process
supervision helper **0.5.1** is distinct from the **0.5.3** server/CLI. Baselines
at application frames **6, 7, 8, 9, 10** are declared and run independently before
the captures, with exact executable/shader/GPU/runtime inputs retained.

| Serial product case | Native artifact | Result |
| --- | --- | --- |
| MCP frame 7, omitted count (default 1) | `bundle-420c3ddae43aed8e42acc1f98a010f72` | Complete/pinned; embedded RGB8 equals only the preselected frame-7 oracle |
| MCP frame 7, count 3 | `bundle-59a280028a05a9eedfd7d29e876dd0ba` | Complete/pinned; embedded RGB8 equals only the frame-9 oracle; raw range is `"7 - 9"` |
| CLI frame 7, count 3 | Recorded in the serial CLI receipt | Complete/pinned; image exactly equals the MCP count-three output |
| Standalone replay of the count-three source | Serial and final-reader receipts | Both succeed with capture-time/observed input hashes equal and cleanup confirmed |

The serial collection validates **118** actual successful results against its
advertised schemas. Two restarts preserve metadata/pins. That collection precedes
the additive plural-range parser/schema/identity extension. The final reader
then validates **36** actual results against its own advertised schemas, exposes
singular `"7"` versus plural `"7 - 9"`, completes a new actual GPU replay of the
retained multi-interval input, and repeats two restart checks without changing
source payloads. Thus **154** qualified results retain their actual build
identities. Frozen collection/final server and CLI binaries and source are
separate; no byte-identical-build claim is made across that parser change.

R-025's private feasibility probes yield a concrete next path, not a public
correlation query. Unique queue/submission and per-draw leaf labels survive a
single-interval profile and literal generated C++ source. Profiling and generating
C++ from the **same saved capture** preserve original source labels. A three-
interval input has independently retained C++ views for original labelled frames
7/8/9. The documented SDK trace trigger yields `Replay Pass Start` and frame-7
labels with exit zero/cleanup; explicit after-one/after-two-frame profiles expose
frame-8/frame-9 labels. Ordinary generic profile validation still refuses changed
settings. Multi-column live timing exports remain ambiguous: the event tree has
only first-interval paths while regime rows contain all three intervals.

I-053–I-061 retain the exact failures, startup controls, settings disagreements,
initial overlapping GPU probes, correction and verifier parse error. The initial
product/SDK overlap is retained and excluded from clean timing qualification;
corrected product and SDK runs are explicitly serialized. No physical counter
interpretation, causal overhead correction, independent per-draw timestamp
validation, general frame-number formula or public draw-timing correlation is
claimed. Those requirements remain in R-025/R-026; R-024 remains Pending.

Complete/pinned qualification **bundle-487017236ba14e318c20a04bbc66dd5a** retains
**2,082** verified payloads (**114,076,046 bytes**). It includes all **21** separately
pinned native success/failure bundles, plain supervised CLI probes, raw exports,
requests/transcripts, failed and successful controls, source/build identities,
CPU logs, and the portable verifier. Frozen executables are losslessly gzip-
compressed with independently checked original lengths/SHA-256. Every source/
imported payload hash/length matches; the imported verifier passes all 154
schemas, images/ranges/input hashes, binary identities and native pin checks.
`artifact_info` confirms complete/pinned/not in use. The installed importer
remains **0.4.1** and its identity is kept separate from tested **0.5.3** builds.
No installed server/worker, helper closure, permission, driver, clock, desktop
or other machine setting changes.

## 0.5.4 same-input timing evidence collector, R-025 slice, 2026-10-15

`capture_analyze_timing` adds the **31st** shared stdio/HTTP tool. It collects
Generate C++ and single-interval GPU Trace evidence from one protected native
Graphics Capture, with one coordinated job/deadline and a fixed replay target.
**R-025 remains In Progress**: a public source-call/time query, independent
per-draw timestamp acceptance and the full correlation workflow remain unfinished.
[REPLAY_TIMING.md](REPLAY_TIMING.md) defines inputs, phase settings and scope.

The final GCC Debug build passes **38/38 CPU checks** in **382.75 seconds** via
`cmake --build --preset linux-gcc-debug --target nsight-graphics-mcp ngm_check_all -j4`.
The new `ngm_replay_timing_check` passes in **12.04 seconds**, covering all three
intervals, exact source/target/trigger/settings refusals, queued source leases,
shared scheduling, cancellation, escaped-child reaping, deadlines, shutdown and
restart pins. Count-two capture remains independent; timing selection refuses
that unqualified mapping before launch. MCP and HTTP checks exercise the new
collector through their real protocol adapters and executable stand-ins. No
stand-in result establishes GPU support. Earlier 20/20-loop and 60/60-loop
builds have their own aggregate logs and are separate development controls.
The initially mistyped aggregate target `ngm_checks` fails before building;
`ngm_check_all` is the existing verified aggregate target.

Actual serial final-build collection uses matching **2026.3.1.0/build 38722833**,
**RTX 3080 Ti**, kernel driver **615.71.09**, KDE Wayland/Xwayland and the
explicit process-local matching-driver library/ICD overlay. No machine policy
or installed server/worker changes. C++ uses 20 replay loops; profiling uses 60.
Source counts one/three and the measured raw metadata shape are qualified;
other counts refuse. The original application is not relaunched.

| Native source | Selected interval | Final analysis | Result |
| --- | --- | --- | --- |
| `bundle-59a280028a05a9eedfd7d29e876dd0ba` | 0 | `bundle-cb9f1be49f63e6e3ee1781720b550c53` | Both phases pass; SDK trigger exports `Replay Pass Start`; literal queue labels retain frame 7. |
| Same three-interval source | 1 | `bundle-9846183bc91dcb1f7c1f4a3fb5629d29` | Both phases pass; after-one-frame trigger and literal frame-8 labels. |
| Same three-interval source | 2 | `bundle-060f96fae811b1e3f979fccf80688ed6` | Both phases pass; after-two-frame trigger and literal frame-9 labels. |
| `bundle-420c3ddae43aed8e42acc1f98a010f72` | 0 | `bundle-e5b93c406a5a35591d3e09b8244756be` | Both phases pass; SDK trigger and literal frame-7 labels. |

The final three-interval run validates **261** successful results against its
advertised schemas; the single-interval run validates **92**, totaling **353**.
Both run two additional server restarts and preserve report/index text, source
hashes and pins. Each complete analysis retains the same input hash, replayer
hashes, child-index hashes, original labels and explicit settings. These are
collection/label facts, not a draw-time association or original frame-ID rule.
The final frozen server SHA-256 is
`28b3218659cd17e835702663706ca9b170fa4d66558df209cda39f5cde22c9d7`.

Earlier direct-core and MCP 20/20-loop three-interval collections pass but keep
their exact earlier binary identities. I-063's single-interval 20-loop profile
exports tables then reaches its deadline with confirmed cleanup. A private
60-loop control exits zero; both print final connection-error text, which alone
cannot classify success or establish a cause. I-064 retains a separate missing
output-directory harness error. The intermediate 60/60 product build passes
index zero, then fails C++ frame-debugger entry at index one (I-065). The final
20/60 combination passes all four cases above. No causal diagnosis is inferred
from these controls.

I-062 retains the existing recorded-state parser's actual refusal and narrow
private diagnostic controls. The product grammar is unchanged. No generated
replay code or wider proprietary helper closure is compiled. Draw correlation,
independent per-draw timestamps, physical counter attribution and unannotated
source-call correspondence remain unfinished.

Complete/pinned **bundle-d95a9298e94a1dfc4b1e598bc2445d3f** retains all **2,302**
payloads (**197,849,965** bytes), the **15** independently pinned native bundles,
successful/failed controls, raw exports, requests/transcripts, exact final
source and separately identified compressed binaries. Every source/imported
payload length and SHA-256 agrees. The imported portable verifier passes the
353 final schemas, all retained transcript schemas, input/child/source hashes,
native pins and compressed executable identities. I-066 preserves and corrects
an initial copy omission before final verification. `artifact_info` confirms
complete/pinned/not in use. The installed **0.4.1** importer remains distinct
from the tested **0.5.4** product.

## 0.5.5 retained replay draw association, R-025 slice, 2026-10-15

`capture_draw_timings` adds the **32nd** shared stdio/HTTP tool. It validates a
complete one/three-interval timing-analysis group and associates unique literal
source/event/regime paths containing one submitted direct/indexed call. It
preserves generated-C++ selections and raw replay times with hashes, source
spans and explicit coverage. Original numeric capture event IDs, original live
cost and physical counter interpretation remain unavailable. **R-025 stays In
Progress** for independent per-draw timestamps, new-input qualification and the
complete source-edit/rebuild/recapture workflow. See [REPLAY_TIMING.md](REPLAY_TIMING.md).

The GCC Debug aggregate passes **40/40 CPU checks** in **413.34 seconds** using
`cmake --build --preset linux-gcc-debug --target ngm_check_all`, with
`CTEST_PARALLEL_LEVEL=1`. The new `ngm_replay_timing_correlation_check` passes
in **0.16 seconds** and `ngm_replay_timing_inspection_check` in **12.26 seconds**.
The former covers mixed direct/indexed parameters, nested/repeated/multi-draw
paths, reused recordings, queue-stream labels, pool/reset lifetimes, qualified
initial non-draw callbacks, host queries and unsupported execution grammar.
The latter exercises actual process-boundary collection with CPU stand-ins,
then full-group identity, cross-interval source/trace ambiguity, regime-only
paths, unsupported peers, mutations, paging, leases, pins and restart stability.
MCP and HTTP checks pass in **29.12** and **6.77 seconds**, including positive
bounded reads and incomplete-group refusal. Stand-ins do not qualify GPU work.

The final frozen **0.5.5** server reads the four unchanged **0.5.4** analyses in
the preceding section. It returns **36** available source-call ranges across
the three-interval source and **12** for the single-interval source, with no
unsupported analyses. Independent fixture API witnesses verify the call,
parameters and unique original label paths; separate raw source/table checks
verify source spans and exact returned duration cells. The witnesses have
whole-scene timestamps only and **do not** establish independent per-draw
timestamp acceptance.

All **58** successful actual results pass their advertised JSON schemas across
three stdio starts and two authenticated loopback HTTP starts; **14** invalid
or legacy requests refuse. Caller reordering, paging and restart preserve
references and values. All **704** selected native payload lengths/SHA-256 stay
unchanged and the six native capture/analysis pins remain set. An empty
toolchain directory is selected for every read, proving that the retained
query does not require a live Nsight installation or GPU operation. No new
GPU collection occurs. The collection host remains the recorded matching
2026.3.1.0/build 38722833 / RTX 3080 Ti / 615.71.09 / KDE Wayland-Xwayland host.

The frozen final reader SHA-256 is
`4b29fb5e32ef721b01726c5d9a3d4366f0b98d45a33517792f945372374439f0`.
I-067 records the initial timing grammar refusals and their queue-stream/pool
corrections; I-068–I-073 retain the compile, schema and check-setup failures.
Each private control keeps its exact source/build identity. The source/input
qualification remains complete/pinned
`bundle-d95a9298e94a1dfc4b1e598bc2445d3f`; its retained native snapshots provide
the raw inputs for this additive reader qualification. The installed **0.4.1**
server/importer and **0.4.2** optional worker remain unchanged, as do machine
profiling, driver, desktop and clock policies.

Complete/pinned **bundle-975763dfe5b2d8a5bbc97f0219e6ccec** retains **483**
payloads (**74,752,747 bytes**) for this reader qualification, including all new
successful/failed controls, source/patch/submodule identities, CPU logs, API
witnesses, actual schemas/transcripts and eight losslessly compressed
executable identities. The imported portable verifier passes; all source/
imported payload lengths and SHA-256 match. It also checks all 704 input files
against the separately pinned 0.5.4 ancestor snapshot, avoiding duplicate raw
capture history. `artifact_info` confirms complete/pinned/not in use. The code
snapshot precedes only this final documentation/pin bookkeeping.

## 0.5.6 independent draw timing and repair, R-025 completion, 2026-10-15

The optional fixture `--timing-scope draw` adds six per-call timestamp pairs and
unique queue/submission/leaf labels; default scene witnesses retain their earlier
schema. Product queries remain at **32 tools**, without requiring application
witnesses. [DRAW_TIMING_VALIDATION.md](DRAW_TIMING_VALIDATION.md) records the
settings, numeric policy, producer scope and actual repair. New qualification is
matching **2026.3.1.0/build 38722833**, **RTX 3080 Ti**, kernel driver **615.71.09**,
KDE Wayland/Xwayland and GCC **16.2.1**. No new 2026.2 GPU qualification is claimed.

The final GCC Debug `ngm_check_all` aggregate passes **40/40 CPU checks** in
**402.41 seconds** with `CTEST_PARALLEL_LEVEL=1`. The expanded fixture contract
passes in **54.06 seconds**, replay timing inspection in **10.58**, correlation
in **0.16**, MCP in **28.98** and HTTP in **6.81**. No code changes follow this run.
The frozen fixture SHA-256 is
`fc850d54025fd504bd2c77d33eef5637241744579130c7180228b63f558b515e`;
the server is
`06ed9b55e47325539bd7be459301b57b0c66af516ab28154415f0c2fd195d287`.
Both match the final built binaries. The source-built glslang executable is
`0148c5f3673bcb25dbecd96d701731824d71e45b95ca7d4c61bab75609530371`.

All actual GPU operations run serially with isolated configurations and owned
cleanup. Five fresh application cases pass **294** independent wide-integer/
Decimal per-call timestamp checks. Scene/draw instrumentation produces identical
reference pixels. Four fresh live profiles compare **36** leaf ranges to Vulkan
queries from the **same live execution**, with **0–32 ns** error under the
previously declared `256 ns + 5% + two timestamp ticks` tolerance. These are unit/
envelope checks, separate from saved replay costs and shader-only cost claims.
All four profile bundles are complete/pinned and two restarts preserve values.

Four fresh Graphics Captures and six analyses pass **48** original literal-path/
source-parameter associations: six direct, six indexed and 36 mixed/reused calls
across three intervals. The scene-only ambiguity control returns two available
and four unavailable calls. Two restarts preserve results and all ten native
pins. Original numeric capture event IDs remain null. The live timestamp checks
and saved replay durations are distinct executions.

An actual source repair uses `timing-reuse`, seed **987**, **256×160**, frame **9**.
MCP source pages tie the recorded draw's pipeline to fragment-module/resource
creation within each ordinary C++ capture. The unchanged exact five-file worker
extracts **1,356 bytes** matching each rebuilt fragment module. Captured debug
source reveals `palette.tint.bgr`; editing it to `.rgb`, rebuilding with pinned
source-built glslang (`-V --target-env vulkan1.3 -g -Od`) and recapturing repairs
**12,286** differing pixels/max-channel **75** to **zero/zero** against an
independent correct frame-9 reference. All twelve draw associations pass before/
after with equal literal paths/call parameters and distinct capture references.
Two additional restarts preserve timing, image, resource and pin results.
Numeric pipeline/module IDs are scoped to each capture; cross-capture numeric
equality is not used. Typed descriptor or execution-time values are not inferred.
Before SPIR-V SHA-256 is
`e951bdc426e1425c1a0bff42bc213f257552ba637d24171d2e40beeadb1254aa`;
after is `b7dd0e0292ee9bf6ec8fd02730c677e2e74ff68b8915e5b680b1650878cb77d5`.

Actual successful results total **1,327**: 362 live-profile, 586 saved-input,
160 before repair, 159 after, ten source-review results per variant and 40 repair
restart results. Every result passes its advertised input/output schemas. The
repair store has thirteen complete/pinned bundles, including four restart shader
reads. Together the three stores retain **27 native bundles / 2,152 files**.

I-074 retains the deleted previous process-local runtime and default
615.78.08-userspace/615.71.09-kernel NVML failure. A fresh private extraction of
the official **615.71.09** runtime restores matching per-job selection; the
published archive SHA-256 and all 32 selected library identities are retained.
The installer is never run and no system driver is replaced. I-075 preserves the
desktop's unsupported 160×96 sizing control and confirmed cleanup; the explicit
224×160 replacement passes. Existing readable profiler/trace capability nodes
match before/after exactly. Permissions, desktop settings, clocks and installed
server/worker configuration remain unchanged.

Complete/pinned **bundle-168c040e6fa84e7efd645b16a5413735** retains **329** payloads
(**83,527,614 bytes**), including exact source/patch/submodule identities and a
lossless **2,640-member** experiment/prerequisite archive. Every native manifest
file and pin is preserved, including configuration files and GPU cache payloads.
The archive's explicit per-member size/SHA-256 inventory supports temporary
GPU-free extraction; the imported portable verifier passes all member hashes,
27 native manifests/pins, 1,327 schemas, timestamp/call/raw-table comparisons,
captured shader bytes and exact repair pixels. Every imported payload agrees
with its source hash/length; `artifact_info` confirms complete/pinned/not in use.
The code snapshot precedes only final documentation/pin bookkeeping. The installed
**0.4.1** importer remains distinct from the tested **0.5.6** source/binaries and
unchanged **0.4.2** worker. **R-025 is Done**; R-024 and R-026 remain open.

## 0.5.7 confined descriptor worker, R-024 foundation, 2026-10-15

The owner explicitly approves the exact two-file `VulkanStructHydrator.cpp/.h`
extension and recorded bounds instrumentation. [DESCRIPTOR_WORKER_PLAN.md](DESCRIPTOR_WORKER_PLAN.md)
records the decision; [DESCRIPTOR_WORKER.md](DESCRIPTOR_WORKER.md) documents the
implemented optional build and internal protocol. Only seven fingerprinted
generated helper files are used, with original/derived hashes and a unified
instrumentation patch. No replay/application code, wider helper closure or
dependency download is involved. There is no new MCP tool; the surface stays
**32** and **R-024 stays In Progress** for selected-draw history and typed values.

The final GCC Debug aggregate passes **41/41 CPU checks** in **400.32 seconds**
with `CTEST_PARALLEL_LEVEL=1`. The new `ngm_descriptor_bounds_check` and existing
`ngm_worker_confinement_check` pass; MCP/HTTP pass in **28.60/6.75 seconds**.
The explicitly invoked, optional `ngm_descriptor_hydrator_integration_run` passes
single/multiple buffer writes and image arrays, distinct tokens and malformed
pointer/count/input/output/extension controls. A changed helper fingerprint
refuses during configure, before compiling a worker.

Ten complete/pinned 0.5.6 C++ source inputs pass supervised confined reads with
empty environments, regular output files and confirmed cleanup. All input
lengths/hashes match the independently supervised existing byte worker. Every
source-local serialized object type/ID matches its exact source registrations;
the ten measured uniform-buffer writes have binding/element zero and range
0/16 bytes. This does not infer typed material values or a selected GPU draw's
descriptor lifetime. Four invalid argument/missing-handle controls refuse with
no stdout and confirmed cleanup.

I-076 retains the initial combined bounds/alignment failure. Seven database
blocks have pointer alignment remainder four. Relocating opaque bytes into
aligned storage preserves the helper's relative-offset contract and all bounds
checks; the corrected matrix passes without weakening alignment validation.
The initial frozen worker and failed attempt remain distinct from final worker
SHA-256 `375272fed5ce3936005e25e1a37277b4c9c92cd549f4da1d6d9c0c176fc9337e`.
ELF inspection finds only `libstdc++`, `libm`, `libgcc_s` and `libc`; the static
helper archive contains exactly the database, scope and hydrator object files.

Complete/pinned **bundle-8e3eb665326adc38968783d5809c7973** retains **336** payloads
(**8,138,832 bytes**), including all **278** lossless snapshot members, original/
instrumented helpers and patch, first-party source, frozen worker/check binaries,
successful/failed attempts, actual requests/receipts, opaque-byte oracles and
CPU/build/ELF logs. The imported portable verifier passes every member and
payload hash, the ten source-local descriptor/byte comparisons, seven alignment
corrections, four worker refusals and configure rejection. It also verifies
**40** exact source/input payloads directly against the already pinned 0.5.6
ancestor **bundle-168c040e6fa84e7efd645b16a5413735**, retaining original captures
without duplicate history. `artifact_info` confirms complete/pinned/not in use.
The source snapshot precedes final documentation/pin bookkeeping only. No new
GPU operation, installed server/worker activation or machine setting change
occurs. Installed importer **0.4.1** and byte worker **0.4.2** remain separate
from the tested **0.5.7** source/optional worker. R-026 remains Pending.
