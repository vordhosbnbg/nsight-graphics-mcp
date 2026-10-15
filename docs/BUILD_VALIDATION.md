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
