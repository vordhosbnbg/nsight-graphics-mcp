# AGENTS.md

## Purpose and scope

Build an MCP server that lets AI agents capture, analyze, and debug shaders and
GPU pipelines using NVIDIA Nsight Graphics.

- The initial target is **Vulkan graphics and compute on Linux**.
- The first release uses an existing Linux desktop session with a windowed target
  application. Fully headless operation is outside that release. Offscreen passes
  within the test renderer remain in scope; validate the actual window-system
  path used on the development machine.
- Support applications whose source the user controls and unmodified Vulkan
  applications. Application-side instrumentation must remain optional.
- Target Codex through a local stdio process for the first release. A persistent
  Streamable HTTP service is accepted later work.
- Launch a fresh application instance for every capture in the first release.
  Reusable application sessions and attachment to existing processes are outside
  that release's scope. This is separate from the MCP server's process lifetime.
- Prioritize visual rendering correctness, then compute correctness, then GPU
  performance analysis. Use a dedicated C++ Vulkan test application developed in
  this repository as the primary development and validation target.
- The first visual-debugging release must support diagnosis followed by a source
  or shader edit, rebuild, recapture, and fix verification. Codex uses its normal
  development tools for edits/builds and this server for capture and evidence.
  Compute correctness and performance analysis are later releases.
- The first-release test renderer includes basic rasterization, multiple passes,
  offscreen render targets, post-processing, bindless resources, and indirect
  draws. Keep small, selectable scenarios for isolating defects; validate advanced
  feature support explicitly rather than silently falling back to simpler cases.
- Use GLSL compiled to SPIR-V with glslang for the test application, chosen by the
  agent after the user delegated the shader-language decision. Keep sources,
  compilation settings, debug information, and shader hashes for correlation;
  validate the actual compiler/Nsight path before advertising source inspection.
- Deliver documented source builds for other Linux users. Use CMake with
  dependencies pinned as git submodules and built from source, following the
  requested reference in `../lava-chan-viewer/`. Link vendored libraries statically;
  the user clarified that documented system/runtime/GPU exceptions are allowed,
  matching the reference's Linux policy.
- Run build/tests and GPU integration checks locally; hosted CI and a dedicated
  GPU CI runner are outside the first-release plan. Validate the first release
  against at least two distinct Nsight Graphics releases and publish the exact
  tested versions, GPU/driver, desktop, and compiler configuration.
- This is a software development repository. Home-level system-maintenance
  notebook workflows are outside this project's scope.
- Prefer small, working slices demonstrated against reproducible workloads.
- Distinguish implemented capabilities, verified tool interfaces, and proposals.
  Never advertise a tool as functional merely because its schema exists.

## Planning workflow

- Read [docs/ROADMAP.md](docs/ROADMAP.md) after this file for accepted work and
  current priorities. Preserve its item schema and keep items under the section
  matching their status.
- Evaluate and record the stack in [docs/TECH_STACK.md](docs/TECH_STACK.md), outside
  the roadmap. The user prioritizes extensive native Vulkan/tooling integration
  and prefers C++ throughout. Use that as the implementation direction, with
  fastmcpp as the MCP library based on the user's prior success. R-003 validates
  its pinned revision with Codex over stdio; supporting choices for subsequent
  components remain under evaluation.
- The initial requirements interview is complete through round 10: first-release
  scope/exclusions, priorities, dependencies, acceptance checks, validation, and
  delivery are recorded. Keep user decisions distinct from implementation choices.
  Resolve the remaining component/version details in their assigned roadmap items;
  ask focused questions if implementation exposes a material new scope decision.
- Keep stack evaluation separate from the roadmap. Do not turn unanswered
  questions or speculative ideas into committed implementation work, or reopen
  settled requirements without new evidence or a changed user preference.
- Keep roadmap updates factual and proportionate. An empty active/pending list
  does not authorize inventing priorities from completed entries.

## Starting point

The owner accepted **R-023–R-027** on **2026-10-15** in this priority order:
restore fresh Graphics Capture reliability; expose proven descriptor contents/
execution-time values; associate timings with draw occurrences; qualify counter
meaning/application attribution; qualify standalone GPU replay. Implementation
of all five is authorized. **R-023 completes at 0.5.1** and **R-027 at 0.5.2**;
**R-025 completes at 0.5.6; R-024 is In Progress and R-026 remains Pending**. Follow docs/ROADMAP.md for evidence dependencies
and acceptance. R-024–R-026 require documented evidence before any
new execution-time or physical attribution capability can be claimed.

R-023's matching 2026.3 reference/repeat/fault captures pass with exact frame-2
application-readback pixels, distinct targets, cleanup and restart pins. The
adapter conditionally uses the documented low-space warning control (I-046).
The 36-check serial CPU aggregate passes before a final producer-schema repair
(I-049); focused MCP/HTTP checks and 20 actual schema-valid results then pass.
All 407 qualification payloads verify in pinned
`bundle-ab49ccec89e6f4584051359b650e7f41`. See docs/BUILD_VALIDATION.md. Original
I-038 causes and new 2026.2 GPU support remain unproven. R-027 now has successful
private plain replays and independent replay-output captures, exact frame-1
application oracles and a changed-output no-blit control (I-050/I-051). The source
embedded image remains a distinct frame-2 view for this fixture. Probe evidence
is pinned as `bundle-9592f2cd01c4623855009d7589ed37af`; all 340 payloads verify.
The product replay job/MCP integration completes at **0.5.2**, with **30 tools**
over shared stdio/HTTP, queued/running source protection and snapshots, matching
metadata/payload checks and the shared capture/profile GPU reservation. All **37
CPU checks** pass. Three real MCP replays plus execution-phase cancellation,
deadline and EOF shutdown pass, with **515** schema-valid results and two
restart checks. Complete/pinned **bundle-1c59130aafc0cb1a946cd9e0c4da869c** retains
all **741** verified payloads and nine independently pinned native bundles.
See docs/REPLAY.md and docs/BUILD_VALIDATION.md. New 2026.2 replay support,
general pixel/frame/event correspondence and timing accuracy are not claimed.
No machine setting or installed server/worker is changed.

The **0.5.6 R-025 completion** qualifies original literal-path/source-call
association with independent per-draw timestamps and an actual shader repair.
The surface stays **32 tools**. All **40 CPU checks** pass in **402.41 seconds**;
**1,327** actual schemas, **294** application per-draw checks, **36** same-live
comparisons (0–32 ns error), **48** new saved-call associations and ambiguity
refusals pass. Actual source edit/build/recapture changes **12,286** differing
pixels to **zero**; all twelve associations and exact captured shader bytes
survive two restarts. Complete/pinned
**bundle-168c040e6fa84e7efd645b16a5413735** retains **329** verified payloads,
including a lossless **2,640-member** snapshot with all **27 native pins**;
its imported portable verifier passes. See docs/DRAW_TIMING_VALIDATION.md.
Original numeric capture IDs, original live cost and physical counter meanings
remain unavailable. R-024 is next; R-026 remains Pending. I-074/I-075 retain
runtime/desktop failures; a fresh process-local matching runtime replaces a
deleted overlay without changing the installed driver, permissions or clocks.
The installed 0.4.1 server and exact five-file 0.4.2 worker stay unchanged.
The following slice records retain their historical scope and build identities.

The **0.5.8 R-024 source-query slice** adds `capture_cpp_descriptor_writes`,
bringing the shared surface to **33 tools**. Its bounded parser and supervised
parent validate capture/source/database/helper/worker identities, map typed
serialized IDs to source symbols and retain successful/failed attempts. Fresh
uniform, combined storage/image and dynamic-array cases decode **20 writes**;
the partial replacement agrees with the private API oracle, seven complete
opaque inputs match the existing byte reader, and two restarts preserve fields
and pins. All **43 CPU checks** pass in **433.91 seconds**. Complete/pinned
**bundle-52a3d6a428d29f5fad1e043e61fb9983** retains **343** verified payloads,
including all **1,764 snapshot paths / 468 content blobs** and **41 native
bundle instances**. Its imported portable verifier passes **313** schema-valid
results, including **231** from final qualified runs. I-077–I-083 preserve the
failed build/schema/harness/collector attempts. See docs/DESCRIPTOR_WRITES.md.
**R-024 stays In Progress** for selected-draw visibility, update/copy/resource
lifetimes, actual SPIR-V type/layout/value evidence and repair acceptance;
**R-026 stays Pending**. Source writes alone establish none of those execution
claims. Installed server/worker and machine policies remain unchanged. The
following 0.5.7 record describes the earlier worker-only foundation.

The **0.5.7 R-024 foundation** adds the owner-approved optional seven-file
descriptor worker with recorded pointer/copy bounds instrumentation, aligned
opaque-input relocation and existing fail-closed confinement. All **41 CPU
checks** pass in **400.32 seconds**; ten retained 0.5.6 source-local reads match
the existing byte worker and source registrations. Four worker refusals and a
changed-helper configure rejection pass. Complete/pinned
**bundle-8e3eb665326adc38968783d5809c7973** retains **336** verified payloads,
including its lossless **278-member** snapshot and failed I-076 attempt; its
imported verifier also checks 40 ancestor source/input files. There is no new
MCP tool or selected-draw lifetime/typed-value claim. **R-024 stays In Progress**,
R-026 Pending. See docs/DESCRIPTOR_WORKER.md and the approved scope in
docs/DESCRIPTOR_WORKER_PLAN.md. Installed five-file worker/server and machine
policies are unchanged. The following records describe earlier slices.

The **0.5.3 R-025 foundation** adds bounded consecutive Graphics Capture intervals
through core/MCP/CLI and preserves raw plural range metadata. Defaults remain one;
counts above one are qualified only on matching 2026.3. All **37 final CPU checks**,
serial MCP/CLI exact image checks, replay and **154** actual schemas/restarts pass.
All **2,082** payloads and **21** native pins verify in complete/pinned
**bundle-487017236ba14e318c20a04bbc66dd5a**. See docs/MULTI_INTERVAL_CAPTURE.md.
At this 0.5.3 milestone, public draw association and independent per-draw
timestamps were still unfinished; the surface then had 30 tools; installed configuration, helper closure and machine policies are
unchanged. I-053–I-061 preserve failures and corrected serial qualification.

The **0.5.5 R-025 association slice** brings the shared surface to **32 tools**.
`capture_draw_timings` validates complete one/three-interval analysis groups and
associates unique one-call literal ranges with generated-C++ submission selections.
All **40 CPU checks** pass in **413.34 seconds**. Offline retained-data qualification
passes **58** actual schemas and **14** refusals across three stdio/two HTTP starts,
with **48** API-witness/raw-table matches and **704** unchanged native payloads.
Original numeric capture IDs, live cost and physical counter meaning remain
unavailable. At this 0.5.5 milestone, independent per-draw timestamps, new-input
qualification and the full repair workflow were still unfinished. See docs/REPLAY_TIMING.md and docs/BUILD_VALIDATION.md. I-067–I-073
retain the reader and check-setup failures. Complete/pinned
**bundle-975763dfe5b2d8a5bbc97f0219e6ccec** retains **483** verified payloads;
its imported verifier also checks the already pinned 0.5.4 input snapshot.
No new GPU collection, helper closure,
installed server/worker or machine policy change occurs in this slice.

The **0.5.4 R-025 collector slice** brings the shared surface to **31 tools**.
`capture_analyze_timing` leases/snapshots one native input and collects generated
C++ plus a selected-interval GPU Trace, with separate trigger/settings/hash
checks, a shared deadline and owned cleanup. Qualified source counts are one
and three on the matching 2026.3 producer. Four final-build actual MCP jobs pass
with **353** schema-valid results and two restarts per source. Collection does
not complete draw association or independent per-draw timestamp acceptance;
R-025 stays In Progress and R-024/R-026 stay Pending. All **38 CPU checks** pass
in **382.75 seconds**. Complete/pinned
**bundle-d95a9298e94a1dfc4b1e598bc2445d3f** retains **2,302** verified payloads
and **15** native pins; the imported verifier passes. I-062–I-066 preserve the
syntax, termination, private-harness and C++ entry failures. The final phase
counts are 20 for C++ and 60 for profiling; their internal failure causes remain
unproven. See docs/REPLAY_TIMING.md and docs/BUILD_VALIDATION.md for qualification
and retained evidence. No installed configuration, helper closure or machine
policy changes.

The **R-018–R-022 capability follow-up group completes at 0.5.0** with **29
tools**. Reader activation, launch controls, empty-table compatibility, bounded
recorded state/push bytes/descriptor bindings and unique exported marker queries
are implemented. The accepted conditional reports retain unavailable descriptor
contents, execution-time values, general GPU correspondence, isolated draw/
fragment counters, physical normalization and causal overhead correction.
See docs/CAPABILITY_FOLLOWUP_COMPLETION.md, docs/DRAW_STATE_FEASIBILITY.md and
docs/PROFILE_SCOPE_FEASIBILITY.md. All **36 CPU checks** and **691** actual
schema-validated final-server tool results pass, including 28 recorded draws,
exact byte oracles, stdio/HTTP and restart checks. **31** new read artifacts are
complete/pinned; 14 earlier qualification/failure snapshots and 87 native bundles
retain their verified payloads/pins. Final qualification is pinned as
`bundle-bc107b133c527aa81f11c17fa93c9743`. No new GPU collection occurs at 0.5.0; each earlier
run keeps its exact tool/build identity. Installed server **0.4.1** and configured
optional worker **0.4.2** remain unchanged. R-018–R-022 remain Done within their
accepted scope; R-023–R-027 are the accepted follow-ups above. Do not invent
additional priorities. The notes below are versioned qualification history.

R-022's repeated control assessment completes at **0.4.11**, retaining **29
tools**. The opt-in C++ runner mode collects four alternate-order off/on pairs
after 30 warmup presents. Two baselines/eight fresh matching 2026.3 profiles pass,
with 326 independently checked timestamped submissions, 32–96 ns scene timing
agreement, all 1,168 raw frame cells and 913 actual MCP schemas/restart pins.
All **36 CPU checks** pass. Causal overhead remains inconclusive and varying
hardware counter cells do not establish application draw/fragment attribution
(I-044/I-045). See docs/PROFILE_OVERHEAD.md and docs/BUILD_VALIDATION.md.
Qualification is pinned as `bundle-8d667f47f660fc46a582f440ee786c5e`; all eight native sources
are separately pinned. No machine setting, helper closure or installed server
is changed. R-022 remains In Progress for the final scope/conditional-outcome
audit; the earlier versioned qualifications below retain their identities.

The bounded R-022 marker-query slice completes at **0.4.10**, bringing the
shared stdio/HTTP surface to **29 tools**. `profile_marker` returns duration in
ms and up to eight raw regime metrics for a uniquely associated qualified
2026.3 single-frame exported path. Original execution IDs, draw membership and
physical counter scaling stay unavailable. All **36 CPU checks**, **391**
retained-profile stdio results and **eight** HTTP results pass actual schemas;
360 marker selections preserve values/refusals after restart. No new GPU run is
performed; the fifteen source profiles retain their 0.4.9 identities and pins.
See docs/PROFILE_MARKERS.md and docs/BUILD_VALIDATION.md. Qualification is pinned
as `bundle-b8d3515b5cd6079e2ba2c8cb5637067e`. **R-022 stays In Progress** for the remaining
scope/overhead assessment. The installed/live server remains 0.4.1.

The R-022 graphics timing fixture/assessment slice completes at **0.4.9** with
the existing **28-tool** surface. Three new direct/indexed/reuse scenarios have
an explicit private API-marker/timestamp witness; no new product query is added.
All **35 CPU checks**, six baselines and fifteen fresh matching 2026.3 profiles
pass. Independent audit verifies 400 timestamped submissions, exact marker inputs,
32–160 ns scene timing agreement within the predeclared tolerance, 1,615 actual
MCP schemas and restart pins. See docs/PROFILE_CORRELATION.md and
docs/BUILD_VALIDATION.md. R-022 remains In Progress for the bounded unique
single-frame marker query and remaining scope/overhead qualification. Repeated
paths, missing action/queue/submission/sample IDs and counter scaling stay
explicit limits (I-043); I-042 preserves the corrected private harness failure.
No permission, clock, driver, display or proprietary helper policy changes occur.
Qualification is pinned as `bundle-40d1c8f983a49246a26d5294f20636bb`; all fifteen
native profile bundles remain independently pinned in the external source store.

The first R-022 slice completes at **0.4.8**, retaining **28 tools**. Profiles
accept nullable per-action/HES controls with exact retained requests and exported
timing verification; active HES state remains unverified. Qualified literal `nan`
regime cells return null/unavailable with finite neighbors intact. All **35 CPU
checks** and two fresh matching **2026.3.1.0/build 38722833** profiles pass; **202**
MCP results validate against advertised schemas, including restart checks.
CPU-only old-product profile-format compatibility also passes. No new 2026.2
control qualification, draw correlation, per-draw counter scope or timing-overhead
claim is made. **R-022 remains In Progress**. See docs/PROFILE_CONTROLS.md and
docs/BUILD_VALIDATION.md; I-040/I-041 retain failed attempts. Existing profiling
access is observed unchanged; no machine-setting change is performed.
Qualification is pinned as `bundle-193d06c97b109c55fb8a30e35f12b8f9`; the two
native profiles remain independently pinned in the external validation store.

R-021's descriptor binding/offset slice completes at **0.4.7**, retaining **28
tools**. `capture_draw_state` joins literal allocations/layouts, applies prefix
and push-range compatibility/disturbance, and maps dynamic offsets by set,
binding and array element. Descriptor contents, shader access and execution-time
values remain unavailable. All **35 CPU checks** pass. Two fresh matching 2026.3
captures cover partial rebinding, array offsets, disturbance/restoration and
repeated submissions; the private C++ oracle checks 15 recorded occurrences and
MCP restart stability. All **46** state responses pass advertised schemas.
Qualification is pinned as `bundle-b59b7343be911eec0e49b85ffe5767ef`.
See DRAW_STATE.md and BUILD_VALIDATION.md for the precise
qualification and pinned evidence. R-021's conditional feasibility assessment
is now complete: descriptor contents, execution-time values and general GPU
correspondence remain unavailable. See docs/DRAW_STATE_FEASIBILITY.md and pinned
audit `bundle-7269c28d30326b8f1fa3ef64ebd4ed36`. R-022 timing/counters is next and
In Progress. The proprietary helper closure and machine settings are unchanged.

R-021's push-constant slice completes at **0.4.6**, retaining the **28-tool**
surface. `capture_draw_state` tracks per-stage partial writes and push-layout
compatibility. Its explicit `read_push_constants` option resolves bounded opaque
input bytes with the existing five-file confined reader; optional `pin` protects
the resulting read artifacts. No typed material/descriptor interpretation is
inferred. All **34 CPU checks** pass. Two fresh matching 2026.3 captures cover
three labelled draws, partial updates, compatible layouts, incompatible bind/
restore and repeated primary-command-buffer submissions. Nine selected occurrences,
41 schema-validated MCP results and restart/source/read pins pass. Qualification
is pinned as `bundle-754e06a3afa984d58e2d073bac3b590f`. The fixture shader inventory
is now 13 files. R-021 remains In Progress for descriptor/offset/value and general
execution-correspondence assessment; R-022 remains Pending. See DRAW_STATE.md and
BUILD_VALIDATION.md for the exact scope and external evidence.

R-021's first bounded recorded-state capability is implemented at **0.4.5** with
**28 tools**. `capture_draw_executions` selects capture-local recorded submission
occurrences; `capture_draw_state` interprets pipeline/shader, depth/culling and
viewport/scissor fields with input fingerprints, static/dynamic lifetime rules
and explicit coverage/refusals. Four fresh matching 2026.3 C++ captures agree with
private fixture API witnesses; eight exact shader reads and MCP restart/schema
checks pass. This remains recorded API state, with GPU execution correspondence
and full effective-state coverage explicitly false. R-021 stays In Progress for
descriptor/constant/value and execution-correspondence assessment; R-022 remains
Pending. New 2026.2 state support is unqualified. See docs/DRAW_STATE.md and
docs/BUILD_VALIDATION.md; no wider proprietary helper closure or machine-setting
change is authorized by this slice.

All **33 CPU checks** pass in the serial preset, followed by the focused fixture
contract check after the final private-witness refinement. Qualification is pinned
as `bundle-556efc8c7f65c8ebdb96e360197fc015`; source captures and shader reads are
separately pinned in the external validation store documented in BUILD_VALIDATION.md.

R-020 completes header-only profile compatibility at **0.4.4**. All **32 CPU
checks** pass in the serial preset. Valid empty event/regime tables preserve
headers and explicit unavailable coverage; frame metrics remain queryable and
empty comparisons refuse. See docs/PERFORMANCE.md and docs/BUILD_VALIDATION.md.
A separate fresh Generate C++ Capture on matching 2026.3 tools succeeds at 0.4.4,
with source pin/restart queries and an exact 1,300-byte shader read. This is the
evidence path for R-021; no state query existed at that milestone. The later
0.4.5 recorded-state subset is described above.
R-022 remains pending. I-038's graphics-capture timeouts remain unresolved.

R-019 implements bounded per-job launch environments at **0.4.3**, shared by
capture, C++ capture, profile and the capture CLI. All **32 CPU checks** pass in
the serial preset. Standalone matching-driver XCB/Xwayland fixture baselines
pass, but three fresh current Nsight 2026.3 captures time out after connection;
cleanup/pins pass and fresh capture compatibility remains unqualified. See
[docs/LAUNCH_ENVIRONMENT.md](docs/LAUNCH_ENVIRONMENT.md) and I-038. No machine
settings are changed. R-020–R-022 remain active/pending follow-up work.

R-018 completes retained resource-reader activation at **0.4.2**. All 31 CPU
checks pass, including actual Linux confinement. Two offline current-server MCP
runs verify the independent 2,700-byte shader oracle, split ranges, provenance,
refusals and restart pins. The optional 2026.3 worker is configured for new Codex
server processes; existing connections retain their startup configuration.
See docs/RESOURCE_QUERIES.md and the pinned external qualification snapshot.
R-019–R-022 remain capability follow-up work; no new GPU support is implied.

R-016 adds native x86-64 Arch Linux packaging at **0.4.1**. The `package` preset
provides `ngm_arch_sources` and `ngm_arch_package`: a source archive containing
the exact dependency sources, generated checksummed PKGBUILD/.SRCINFO, and a
locally built package. Runtime installation contains the server, capture CLI,
documentation and license notices; fixtures and optional proprietary helpers
remain separate. All **31 CPU checks**, extracted-package CLI/MCP checks and
ELF linkage inspection pass on the recorded Arch host. See
[docs/ARCH_PACKAGE.md](docs/ARCH_PACKAGE.md) and
[docs/BUILD_VALIDATION.md](docs/BUILD_VALIDATION.md). No new GPU qualification
or AUR publication is implied.

R-004 completes at **0.3.2** with optional persistent Streamable HTTP on IPv4
loopback, required private-file bearer authentication and a shared trusted-user
workspace. All 26 CPU checks and real Codex HTTP capability/artifact calls pass
at that milestone. Client disconnect/session deletion preserves jobs; service
shutdown cleans them up. See [docs/HTTP.md](docs/HTTP.md).

The **R-009 performance group completes at 0.4.0**. The 26-tool surface includes
shared asynchronous GPU Trace jobs, bounded metric queries and repeated profile
comparison. Actual source-edit/rebuild acceptance at the retained 0.3.6 development
build passes on both matching Nsight releases, with four fresh traces per variant
per release and independent exact numerical validation. All 30 CPU checks pass
on 0.4.0, followed by focused numeric-guard checks and identical final-binary
comparisons against retained real traces. Independent reviews pass. The complete
2914-payload snapshot is pinned as bundle-d4db174806f33a707b908900599404d5 in
`artifacts/performance-evidence`; hashes and restart pin are verified. See
[docs/PERFORMANCE.md](docs/PERFORMANCE.md) for units, warmup, variability and scope.
All approved temporary profiling access has been restored. Future permission
changes still require an explicit owner decision; the product never changes them.
The original release items are Done. On 2026-10-15 the owner authorized the
capability follow-up plan; R-018–R-022 track its implementation and qualification.
Read docs/CAPABILITY_FOLLOWUP_PLAN.md and the current roadmap for that work.

R-008 completes at **0.3.1** with a source-available no-presentation compute
workflow on Nsight 2026.3.1.0, independent numerical validation and two actual
shader repairs. The extension boundary is unsupported on the measured 2026.2
injection path. Application source/readback stays distinct from Nsight dispatch
and object inventories. See [docs/COMPUTE.md](docs/COMPUTE.md) for the pinned
acceptance evidence. Performance and HTTP complete as described above.

The repository now implements the **0.3.0 visual first-release group**
(R-010/R-002/R-006/R-007/R-015/R-014), following the build/basic-fixture group at
0.1.0 and capture/evidence group at 0.2.0. It supplies C++20/CMake/Ninja source
builds, a deterministic windowed Vulkan fixture, a C++ experiment runner,
fastmcpp stdio integration, asynchronous capture jobs and managed artifact storage.
The server exposes **29 tools**, including bounded metadata/event/object queries,
image previews/comparisons, generated-source relationships and qualified optional
serialized-resource reads. `ngm-capture` shares the capture service.

All nine visual fault scenarios have actual source-edit/build/recapture validation
on matching Nsight **2026.3.1.0/build 38722833** and
**2026.2.0.0/build 37991608**. Optional basic SDK control is qualified with matching
SDK **0.9.2/0.9.0**. Independent final acceptance and the clean 0.2.12 source-build
walkthrough pass; the latter includes all **25 CPU checks**, three fresh captures,
three application baselines, shader-byte correlation, exact repaired/reference
pixels and a real Codex two-tool interoperability check. Each historical result
retains its exact product/build/tool identities; do not imply every run used 0.3.0.

Start with [docs/INSTALL.md](docs/INSTALL.md) and the consolidated capability/
repair matrix in [docs/VISUAL_RELEASE.md](docs/VISUAL_RELEASE.md). Detailed
validation and failed probes remain in docs/BUILD_VALIDATION.md,
docs/NSIGHT_VALIDATION.md and docs/INVESTIGATIONS.md. Qualification snapshots and
referenced evidence are explicitly pinned. General executed event state and
executed descriptor selection remain unavailable. Fixed-input descriptor hydration
experiments are not a generic product API. Standalone GPU replay timed out on both
releases at that visual milestone; the later 0.5.2 matching 2026.3 qualification
is recorded above and in docs/REPLAY.md. Compute correctness is qualified within the
R-008 scope above; performance analysis is qualified within the measured R-009
scope and Streamable HTTP within the local policy above. Update these instructions
as the project develops.

Local baseline, inspected on 2026-09-17:

- Nsight Graphics **2026.3.1.0**, installed under
  `/opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3`.
- Nsight Graphics SDK **0.9.2**, under `SDKs/NsightGraphicsSDK/0.9.2` in that install.
- NVIDIA GeForce **RTX 3080 Ti**, driver **615.71.09**; one GPU detected by `lspci`.
- `ngfx` is on `PATH`. `ngfx-capture` and `ngfx-replay` are available under the
  installation's `host/linux-desktop-nomad-x64` directory but were not on `PATH`.
- Version/help output and SDK headers were inspected. No end-to-end capture or
  profiling run has been validated yet.

Treat this as a dated development baseline. Discover installations and support an
explicit path override; do not hard-code this machine's paths or GPU into runtime
logic. Recheck relevant facts when diagnosing compatibility problems.

## Nsight integration boundaries

- Use only documented Nsight interfaces and exports for backend integration.
  GUI automation, private RPC, reverse-engineered formats, and complementary
  backends such as RenderDoc are outside the selected scope. Accept and report
  documented-interface limitations instead of silently substituting another path.
- Record every failed integration or evidence-extraction attempt in
  [docs/INVESTIGATIONS.md](docs/INVESTIGATIONS.md), including reproducible steps,
  versions, expected/observed results, evidence, and a concrete reason to revisit.
  Preserve inconclusive results as inconclusive; a failed probe does not establish
  that a capability is universally unavailable.
- `ngfx-capture` supports unattended capture; `ngfx-replay` supports replay and
  metadata export. Its `--metadata-functions`, `--metadata-objects`,
  `--metadata-logs`, and `--metadata-screenshot` options expose different views of
  a capture. The object export is JSON. Validate actual schemas with sample
  captures before designing dependent analysis tools.
- A function stream and object inventory do not establish complete pipeline
  state, descriptor bindings, shader bytecode, or resource contents at an event.
  Report missing information explicitly.
- `ngfx --activity="GPU Trace Profiler" --auto-export` provides a profiling path.
  Verify which metrics and tables the installed version exports; do not assume
  every GUI view has a corresponding machine-readable export.
- The NGFX SDK is a beta interface for application-side activity control. SDK
  initialization belongs in the target process before Vulkan instance creation.
  An optional application bridge can receive MCP requests and execute them at
  appropriate workload boundaries.
- Compute workloads without presentation need suitable delimiters, such as
  `VK_EXT_frame_boundary` where available or NGFX SDK controls.
- No documented public interface for live shader breakpoint/step control was
  found in the CLI/SDK reviewed. Nsight's live shader debugger also requires a
  separate host GPU or a remote host configuration. The single-GPU baseline
  does not satisfy the documented local setup.
- On the baseline installation, `ngfx --version` and `ngfx --help-all` emitted
  valid output but exited with status 1. Handle this discovery quirk explicitly;
  do not generalize it to capture or profiling failures.

NVIDIA described an in-development Nsight Graphics MCP preview at SIGGRAPH 2026.
Recheck its availability when evaluating new documented integration options.

## Implementation direction

- Follow the C++/fastmcpp direction in `docs/TECH_STACK.md` and resolve the remaining
  supporting choices there. Codex over local stdio is the selected first-release
  integration. CMake/Ninja and first-party C++20 are implemented by R-013. The
  artifact store now uses JSON manifests and directories, with provisional
  2 GiB/30-day limits; no SQLite dependency is selected. Revisit retention
  defaults after measuring representative real captures.
  Keep MCP handlers separate from job management, artifact storage, parsers, and
  backend adapters so the later HTTP service can share the same core.
- Expose a compact, typed tool surface organized around agent tasks: capability
  discovery, capture, profiling, job status/cancellation, event/object queries,
  metric queries, and run comparison. Add tools when their data is available.
- Use asynchronous jobs for long operations, with job IDs, explicit states,
  deadlines, and cancellation. Serialize profiling work on the same GPU.
- Separate worker completion from job-state mutation. Workers return typed
  completion records; one coordinator owns job-state transitions and validates
  job/capture/attempt identity and expected state before applying a result.
  Resolve success, timeout, and cancellation races once; stale completions cannot
  revive terminal jobs or attach evidence to another capture. Keep the GPU
  reservation until cleanup of owned processes is confirmed.
- Launch processes with argument arrays, validate paths and options, and avoid
  shell interpolation. Track process ownership; cancellation must only terminate
  processes belonging to the job. Capture jobs own a fresh application launch;
  handle success, failure, timeout, cancellation, and server shutdown with bounded
  cleanup of owned processes. Optional SDK control selects a boundary within that
  invocation and does not require a reusable application session.
- Reserve MCP stdout for protocol traffic. Send server diagnostics to stderr and
  capture subprocess logs separately.
- Keep capabilities sensitive to tool version, platform, GPU, and prerequisites.
  Return actionable errors for unavailable operations, failed exports, and
  incompatible captures.

## Build and dependency policy

- Optional generated-resource workers may compile the exact fingerprint-qualified
  five-file ReadOnlyDatabase/DataScope helper closure supplied by a separately
  installed Nsight C++ export. Keep these proprietary generated sources outside
  version control, copy/hash them into an isolated build directory, and compile
  no generated replay scripts or application code. This scoped toolchain-source
  exception follows the optional NGFX header policy; it does not permit network
  acquisition or replacement of pinned submodule dependencies. See
  [docs/RESOURCE_WORKER.md](docs/RESOURCE_WORKER.md).

- On 2026-10-15 the owner explicitly approved
  [docs/DESCRIPTOR_WORKER_PLAN.md](docs/DESCRIPTOR_WORKER_PLAN.md): the optional
  matching 2026.3 descriptor worker may additionally compile only the exact
  fingerprinted `VulkanStructHydrator.cpp/.h`, with confined execution and
  recorded bounds instrumentation in isolated copies. Preserve original/derived
  hashes and the instrumentation patch. This seven-file exception does not
  authorize generated replay/application code, a wider helper closure, machine
  changes or unverified execution-time values. The existing five-file resource
  worker remains independent. R-024 still requires its full acceptance checks.

- Track project dependency sources under `external/` as exact-commit git
  submodules, including required transitive sources. Initialize them explicitly
  with `git submodule update --init --recursive`; configure/build must then work
  without dependency downloads. Do not replace this with package-manager binaries
  or network FetchContent fallbacks.
- Build required dependency libraries and the glslang shader compiler from those
  sources. The installed shader compiler is a discovery observation, not the
  selected reproducible build input. Pin and record the tested source revisions.
- The explicitly enabled NGFX fixture uses fingerprinted headers from the matching
  separately installed Nsight toolchain, as documented in `docs/SDK_CONTROL.md`.
  This optional GPU-toolchain source exception does not change the vendored
  submodule policy or the SDK-free default build; do not download SDK sources at build time.
- Statically link vendored dependency code and audit the produced binaries, not
  just CMake flags. Document required OS/compiler-runtime, desktop, Vulkan-loader,
  and GPU-driver dependencies, including libraries loaded at runtime. This follows
  the user's clarified reference policy; do not describe the resulting binaries
  as having no shared-library or external runtime dependencies.
- Use shared configure/build/test presets and ignored `build/<preset-name>/`
  directories, with machine-specific overrides in ignored `CMakeUserPresets.json`.
  Keep first-party warnings and configuration choices scoped to their targets.
  Preserve shader debug information in configurations used for diagnosis.
- Use a small project-owned C++ check harness and a CMake registration helper
  providing focused executable/run targets, CTest registration, and an explicit
  aggregate target. Keep ordinary checks independent of Nsight and GPU execution;
  expose hardware integration through separately invoked targets. Target names
  become documented commands only after they exist and have been verified.
- Document the host compiler/build tools and separately installed Nsight/GPU
  stack as prerequisites. Disable unused dependency examples, tools, tests, and
  optional network/TLS features in the first stdio build. Validate that transitive
  dependency configuration cannot silently fetch or select prebuilt libraries.

## Project versioning

Use the policy adopted from [Kiln](../kiln/AGENTS.md#project-versioning). R-013
established the authoritative version in the top-level CMake
`project(... VERSION ...)` declaration, initially `0.0.1`, and the shared
`ngm::project_version()` accessor.
Derive command-line versions, MCP server identity, logs, artifact manifests, and
any future package versions from that declaration.

- Increment `MAJOR` only when the owner explicitly instructs it. Do not infer a
  major increment from a breaking change.
- Increment `MINOR` once when a group of related roadmap items is completed.
  Identify the group in the completing change and move every completed item to
  `Done`; do not treat each item or work session as an automatic minor increment.
- Increment `PATCH` for every commit containing a first-party code change,
  including source, headers, shaders, tests, build logic, packaging logic, or
  installation scripts.
- A major or minor increment subsumes the patch increment for the same commit.
  Reset minor and patch to zero on a major increment, and patch to zero on a
  minor increment.
- Documentation-only, roadmap-only, design-decision-only, and benchmark-result-only
  commits do not increment patch unless the owner explicitly requests it.
- Include the required version increment in the same commit as the triggering
  change; do not defer it to a later bookkeeping commit.
- Keep the product version separate from the negotiated MCP protocol revision,
  artifact schema version, and Nsight/tool/SDK versions. Record exact build
  provenance separately and document compatibility changes explicitly; these
  bump rules alone do not establish API or artifact compatibility.

## Evidence and artifacts

- Store large captures, traces, and exports on disk. Return bounded structured
  results, pagination, image previews, and artifact references to agents.
- Apply configurable age and storage limits to completed, unpinned artifact
  bundles. Persist pin/unpin state across server restarts, protect data in active
  use, and limit cleanup to server-managed artifacts. Report quota exhaustion
  when protected data prevents pruning; do not remove pinned evidence to meet a
  limit. Keep small expiration records so old references explain what happened.
- Explicitly pin the evidence bundles used by investigation records and important
  verification baselines. A reference alone does not pin data. Keep retention
  status with those references and update records if evidence is later removed.
- Assemble bundles in unique staging directories on the destination filesystem,
  validate their manifest and required outputs, then publish atomically. A
  partial capture must never appear as a complete bundle. Retain failed attempts
  as explicitly failed bundles with their available logs and evidence; reconcile
  interrupted staging/publication on restart.
- Acquire temporary usage leases for active writers, inspection, and comparison.
  Check leases and persistent pins under the same coordination that claims a
  bundle for deletion; an earlier unprotected snapshot is insufficient. Prune
  whole bundles and perform slow deletion after releasing the protection lock.
- Keep raw evidence and derived indexes separate within retained bundles. Record
  backend/tool versions, application build identity, shader hashes, capture/profile
  settings, GPU/driver details, and units alongside results.
- Scope event and object IDs to their capture. Do not assume numeric IDs match
  across runs. Keep measured facts separate from inferred diagnoses.
- Where application source is available, use Vulkan debug labels, reproducible
  inputs, and retained shader source/SPIR-V/debug information for correlation.
- Compare equivalent workloads after warmup with repeated measurements. Keep
  debug shader configurations distinct from performance measurements, and
  account for replay reset overhead when interpreting timings.
- Keep generated captures, proprietary binaries, application data, credentials,
  and large logs out of version control. Use small, sanitized fixtures for tests.

## Development and validation

1. Inspect `git status` and relevant existing code/docs before editing. Preserve
   unrelated changes and keep implementation work inside this repository.
   Use the checked-in [.clang-format](.clang-format), copied from the viewer,
   for first-party C++ formatting. Keep formatting scoped to the work and avoid
   unrelated or vendored-code churn. Formatter settings do not select the
   compiler's C++ language level.
2. Verify installed CLI help and primary documentation when relying on unfamiliar
   flags or SDK behavior. Keep version-dependent assumptions in backend adapters.
3. First prove a vertical slice with the repository's deterministic Vulkan test
   application: capture a visual defect, inspect real exports, and retrieve
   evidence through MCP. Establish detailed inspection capabilities explicitly;
   fixture-only instrumentation must not be presented as general Nsight support.
   Extend validation to compute correctness and then performance analysis.
4. Test meaningful parser behavior, process failures, timeouts, cancellation, and
   capability detection without requiring a GPU or Nsight installation.
   Use test-only C++ command-line stand-ins for Nsight tools at the real process
   boundary. Control arguments, stdout/stderr, exit codes, output files, delays,
   and child processes to exercise malformed/missing exports, hangs, cleanup,
   and protocol-output isolation. Use small, sanitized exports from identified
   Nsight versions for parser regression checks. Stand-ins do not establish
   real tool compatibility or evidence availability.
5. Use local CMake/CTest checks and explicitly invoked local GPU integration runs;
   keep hardware work separate from default checks. First-release validation must
   exercise at least two Nsight releases using each release's matching tools and
   documented prerequisites. Record pass/fail/unsupported/skipped distinctly;
   parser fixtures or a skipped hardware run do not establish release support.
6. Run checks actually configured in the repository. Document working setup and
   test commands once the tooling exists; do not invent commands or claim checks
   passed without running them. Documentation-only edits need a content/diff review.
   Use one relevant compiler/configuration preset and the smallest meaningful
   checks for routine edits. Expand to aggregate checks for shared behavior and
   additional presets for build, portability, or release work that warrants them.
7. Do not change GPU drivers, display-server configuration, system profiling
   permissions, or other machine settings as an implicit development step.
8. Use an explicit C++ experiment runner for reproducible fixture validation.
   Give each run isolated configuration, artifact, and log directories; select
   the scenario, seed, resolution, and frame explicitly and launch a fresh target.
   Record exact tool/SDK, GPU/driver, desktop, build, and shader identities plus
   pass/fail/unsupported/skipped outcomes and retained evidence references.
   Compare equivalent inputs and frames within declared tolerances. Reuse this
   runner for the source-edit/rebuild/recapture workflow, keeping edits/builds in
   normal development tools. Keep expected diagnoses private to the harness and
   label Nsight exports and application readback as distinct evidence sources.

The first end-to-end success criterion is an agent identifying a deliberately
introduced visual defect, editing the responsible shader/source, rebuilding,
recapturing, and verifying the fix against the correct reference. Preserve the
before/after evidence and build identities. Compute correctness and repeatable
performance measurements follow. Live shader breakpoints, stepping, and variable
inspection are outside first-release scope.

## Primary references

- [Capture and replay CLI](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
- [GPU Trace](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html)
- [NGFX SDK](https://docs.nvidia.com/nsight-graphics/UserGuide/sdk.html)
- [Shader debugger setup](https://docs.nvidia.com/nsight-graphics/UserGuide/shader-debugger-setup.html)
- [NVIDIA SIGGRAPH MCP preview description](https://www.nvidia.com/en-us/events/siggraph/)
- [Official MCP Python SDK](https://github.com/modelcontextprotocol/python-sdk)
