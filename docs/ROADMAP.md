# Project Roadmap

This roadmap is the project planning source for nsight-graphics-mcp. Keep it short
enough to scan at the start of a coding session and specific enough that the next
useful task is obvious. Items describe accepted work toward an MCP server for
Vulkan graphics and compute on Linux.

Planning state: **Build/basic-fixture group complete at 0.1.0; R-012 artifacts, R-011 jobs, and R-001 capture in progress.**
Last updated: **2026-09-18**.

The technology evaluation lives separately in [TECH_STACK.md](TECH_STACK.md).
The initial interview established the first-release requirements; all implementation
items follow the sequence below. Support applications with source access and unmodified
applications, targeting visual diagnosis and fix verification through Codex over
local stdio. Exact component versions, capability availability, and measured
defaults are implementation investigations assigned below. Compute correctness,
performance analysis, and a persistent Streamable HTTP service follow later.

## Planning Decisions

- Workflow order: **visual rendering correctness, compute correctness, then GPU
  performance analysis**, selected in interview round 4.
- Development target: a **dedicated C++ Vulkan test application in this repository**,
  created for MCP development and validation. No existing application is required
  as the primary fixture.
- Exercise both ordinary application launch without NGFX integration and optional
  application-side capture control. Test-application helpers must not mask missing
  capabilities for unmodified applications.
- First visual-debugging release: **diagnose, edit shader/source, rebuild,
  recapture, and verify the fix**, selected in interview round 5. Codex performs
  edits/builds with its normal development tools; the MCP server supplies capture
  and inspection evidence. Live shader breakpoints/stepping/variable inspection
  are outside first-release scope.
- Backend integration uses **documented Nsight interfaces only**. Accept explicit
  limitations; private interfaces, GUI automation, reverse-engineered formats, and
  complementary backends are excluded from the selected scope.
- Record **each failed integration or extraction attempt** for future
  investigation in [INVESTIGATIONS.md](INVESTIGATIONS.md), with reproduction steps,
  evidence, version context, and revisit conditions. These decisions came from
  interview round 5; they do not establish that detailed inspection is available.
- First-release boundary, selected in round 6: **complete the visual diagnosis and
  fix-verification workflow**. Compute correctness (R-008), performance analysis
  (R-009), and Streamable HTTP (R-004) are outside that release. Graphics capture
  with optional application-side controls remains within the visual scope;
  compute-specific boundary validation belongs to R-008.
- First-release renderer coverage, selected in round 6: **basic rasterization,
  multiple passes, offscreen render targets, post-processing, bindless resources,
  and indirect draws**. R-005 establishes the deterministic fixture and R-010 adds
  the advanced cases; both are needed before release. Exact API feature choices,
  window-system backend, and scenario inventory remain to be refined.
- First-release application lifecycle, selected in round 7: **launch a fresh
  application instance for every capture**. R-011 manages each job's processes;
  reusable application sessions and attachment to existing processes are outside
  this release. The local stdio server can still handle multiple sequential jobs.
- The user delegated the test shader language in round 7. The agent selected
  **GLSL compiled to SPIR-V with glslang**; rationale and compiler validation are
  recorded in [TECH_STACK.md](TECH_STACK.md).
- First-release display requirement, selected in round 8: **an existing Linux
  desktop session with a windowed application**. Fully headless operation is
  outside this release; offscreen render passes within that application remain
  part of the selected workload.
- Artifact retention, selected in round 8: **automatic pruning by configurable
  age/storage limits, with explicitly pinned artifacts protected**. R-012 records
  retention state, protects active work, and provides pin/unpin operations. Pin
  evidence relied on by investigation records or retained verification baselines;
  references alone do not exempt data from cleanup.
- First-release delivery, selected in round 9: **documented source builds for
  other Linux users**. Prebuilt release binaries are not a first-release
  requirement; record the actual tested compiler/platform/Nsight matrix.
- Build/dependencies, selected in round 9: **CMake, exact-commit git submodules,
  and dependencies built from source**, using `../lava-chan-viewer/` as the
  reference. Use its `external/` layout and Ninja preset conventions; configure
  and build must not download dependencies after submodule initialization. Build
  the fixture's shader compiler from pinned source too.
- The user clarified round 9's static-linking requirement to **match the
  reference: static vendored libraries, with documented system/runtime/GPU
  exceptions**. Audit actual binary linkage and runtime-loaded dependencies;
  this does not require an ELF executable with no dynamic dependencies.
- Validation, selected in round 10: **local build/tests and explicitly invoked
  local GPU integration checks only**. Hosted CI and dedicated GPU CI runners
  are outside the first-release plan.
- Compatibility, selected in round 10: **validate at least two distinct Nsight
  Graphics releases before the first release**. R-015 owns the test matrix and
  evidence; include the installed baseline and select another release compatible
  with the test environment. Record exact tool/SDK, GPU/driver, desktop, and
  compiler versions. A second GPU generation is not required by this decision.
- After reviewing `../lava-chan-viewer/`, the user adopted five practices:
  **focused local verification, executable tooling stand-ins, coordinated job
  state with exact completion identities, atomic artifact publication with
  coordinated retention, and an isolated reproducible experiment runner**.
  Durable rules are in `AGENTS.md`; R-013, R-011, R-012, R-005, R-007, and R-015
  own implementation and validation. Adoption does not mark those items complete.
- Versioning uses **Kiln's policy**, as clarified by the user: the top-level CMake
  declaration is authoritative; patch increments accompany commits changing
  first-party code, minor increments complete identified groups of related
  roadmap items, and major increments require explicit owner instruction.
  `AGENTS.md` defines the reset, exception, and same-commit rules. R-013 implemented
  version plumbing; R-003 verifies the MCP server
  identity. The root `.clang-format` is copied from `lava-chan-viewer`.

## Implementation Sequence

The R-013/R-005/R-003 build/basic-fixture group is complete at 0.1.0.
The capture/evidence group is active; its exit check requires real Nsight runs
with the shared job and storage components.

| Step | Items | Exit check |
| --- | --- | --- |
| Build and basic fixture | R-013, R-005, R-003 | Pinned source build, deterministic scene, and a real Codex capability query. |
| Capture and evidence | R-012, R-011, R-001, early R-006 | Owned job lifecycle, retained artifacts, a readable capture, and demonstrated export capabilities or recorded gaps. |
| Complete visual workflow | R-010, R-002, complete R-006, R-007 | Advanced fixture coverage, optional SDK control, and source repair verified by a fresh capture. |
| Validate and document | R-015, R-014 | Local checks, real workflows on two Nsight releases, and reproducible Linux source-installation instructions. |

Start R-006's basic inspection probes before investing in the full advanced fixture;
its final capability matrix must also cover R-010. Capture integrations and their
shared job/storage components have joint integration checks and can be completed
together. Record missing evidence explicitly; a documented gap does not implement
a dependent diagnostic capability or satisfy its acceptance check.

R-013's source pins and C++20/CMake/toolchain decisions are recorded in
[DEPENDENCIES.md](DEPENDENCIES.md) and [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
R-005 selected XCB and the basic defect cases. Remaining implementation choices
have owners: R-010 selects the advanced scenarios; R-011 validates process control and R-002 the
per-launch SDK configuration mechanism; R-012 sets retention defaults and index
format from measured artifacts; R-015 selects the second Nsight release. Later
HTTP service policies are resolved in R-004. These do not require reopening the
initial requirements interview.

## Item Schema

Adapted from `../lava-chan-viewer/docs/ROADMAP.md` (path relative to the repository
root). The fields and status semantics are preserved; area names are adapted to
this project.

Use this shape for every roadmap item:

```text
ID: R-000
Status: In Progress | Pending | Blocked | Done
Area: core | platform | mcp | capture | replay | profile | analysis | shaders | artifacts | build | test | docs
Title: Short imperative or noun phrase
Goal: One or two sentences describing the intended outcome.
Scope: Concrete files, modules, or behavior expected to change.
Acceptance: Observable checks that prove the item is complete.
Notes: Constraints, follow-up risks, or decisions still open.
```

Status rules:

- `In Progress`: active or next-up work for the current development phase.
- `Pending`: accepted future work, not active yet.
- `Blocked`: accepted work waiting on a decision, dependency, or prerequisite.
- `Done`: completed work retained only when it helps preserve context.
- Every item must live under the section matching its `Status`; do not leave
  `Done` items under `In Progress`.
- Items within a status section are ordered by current project priority, not by
  numeric ID.

Item conventions:

- Start at `R-001`, assign stable IDs, and do not reuse them.
- Follow the reference's entry layout: a `### R-001` heading supplies the ID,
  followed by a `text` block containing `Status` through `Notes`.
- Use `/` to separate multiple areas when needed.
- Put dependencies in `Notes` using item IDs. Separate required prerequisites
  from useful follow-up work.
- Keep unaccepted ideas and unanswered questions out of implementation items.
  A planning question alone does not justify a `Blocked` item.
- Record completion evidence in `Notes`; documentation or a tool schema alone
  does not demonstrate that a server capability works.

## In Progress

### R-012

```text
Status: In Progress
Area: artifacts/core/mcp/test
Title: Store capture evidence with configurable retention and pinning
Goal: Keep capture evidence inspectable across server sessions while automatically managing eligible artifact storage and preserving explicitly retained evidence.
Scope: Server-managed artifact bundles with stable IDs, raw captures/exports/logs/reports, provenance manifests and derived indexes; staging and atomic publication on the destination filesystem, explicit failed-attempt bundles, and interrupted-publication recovery; configurable maximum age and storage budget, oldest-eligible-first pruning, persistent pin/unpin state, temporary usage leases for active readers/writers, storage-usage reporting, and small records explaining expired references. Coordinate protection checks with deletion claims and prune whole bundles. Expose bounded artifact queries and retention controls through MCP.
Acceptance: Artifacts and pin state remain discoverable after restart; readers never observe partial bundles as complete, interrupted publication is reconciled, and failed attempts retain available evidence with an explicit failure status; fake-clock/filesystem tests demonstrate age and budget pruning of completed unpinned bundles, protection of pinned/in-use data including a pin/lease acquired while pruning is choosing candidates, and cleanup confined to managed artifacts; related files are retained or expired coherently; expired references report their status; quota exhaustion or disk-full failures are actionable without deleting protected data; a before/after capture comparison and an investigation entry can explicitly pin and retrieve their evidence bundles.
Notes: Retention policy selected in round 8. This is a first-release prerequisite shared by R-011, R-001, R-002, R-006, and R-007; core storage tests do not need a GPU and R-003 supplies the MCP adapter. Pinning exempts data from automatic pruning; references alone do not. Pinned bytes still count toward reported usage, so a storage budget is not a guarantee when protected data exceeds it. Choose and document default limits and age semantics after measuring fixture artifacts. Index format and exact cleanup scheduling remain implementation choices; this item does not require SQLite.
```

### R-011

```text
Status: In Progress
Area: core/platform/mcp/test
Title: Manage one application launch per capture job
Goal: Give each capture a fresh, owned application instance and a predictable lifecycle without leaving job processes running after completion or cancellation.
Scope: Job IDs and state transitions, executable/argument/working-directory inputs, process ownership, captured subprocess logs, configurable deadlines, cancellation, and bounded cleanup on success, failure, or server shutdown; a single coordinator applies worker completions after checking job/capture/attempt identity and expected state. Add test-only C++ command-line stand-ins at the real process boundary, controlling output streams, exits, files, delays, and child processes. Expose job status and cancellation through the MCP adapter.
Acceptance: Consecutive capture jobs use separate application instances and separately identified outputs; executable stand-ins exercise launch failure, early exit, malformed/missing exports, output-stream handling, hangs, timeout, cancellation, and shutdown without requiring a GPU or Nsight; stale completions cannot revive terminal jobs or cross capture identities; success/cancellation/timeout races resolve once, and GPU reservations remain held until owned-process cleanup is confirmed; owned child processes are cleaned up while unrelated processes remain untouched; a real Nsight capture exercises the same lifecycle; subprocess output stays separate from MCP protocol stdout.
Notes: First-release lifecycle selected in round 7. Core process handling is independent of MCP; R-003 exposes it and R-001/R-002 use it. Use R-012 for output bundles and protect artifacts while a job is writing or reading them. Real capture acceptance is checked with those integrations and R-005. Validate Nsight launcher/target process behavior before choosing the cleanup mechanism. This work does not introduce application-session reuse or attachment to existing processes.
```

### R-001

```text
Status: In Progress
Area: capture/platform
Title: Capture unmodified Vulkan applications
Goal: Let agents capture supported Vulkan workloads without requiring changes to the application's source or an application-side integration.
Scope: A fresh application launch for each capture through Nsight's documented interface, with executable, arguments, working directory, supported capture triggers, and a managed output destination; expose capture results and useful failure information through R-011's job lifecycle.
Acceptance: A reproducible Vulkan application produces a saved capture without source changes; the matching replayer reads its metadata; repeating the request launches a new application instance and produces a separate capture; launch/capture failures are reported without claiming success; applications lacking a usable capture delimiter receive an explicit limitation.
Notes: Accepted application mode in round 2 and fresh-launch lifecycle in round 7. Depends on R-003 for MCP access, R-011's process supervisor, and R-012's artifact store; use R-005 with NGFX integration disabled as the primary fixture and extend capture validation to R-010 before release. Round 8 requires an existing desktop session; verify its display prerequisites and actual window-system path. Exact API features remain to validate. Existing-process attachment, application-session reuse, and fully headless operation are outside first-release scope; compatibility with every Vulkan feature is not promised.
```

## Pending

### R-006

```text
Status: Pending
Area: analysis/replay/shaders/artifacts
Title: Establish the detailed capture-inspection interface
Goal: Determine and demonstrate which pipeline, shader, and resource evidence can be retrieved programmatically for the visual-debugging workflow.
Scope: Representative R-005 and R-010 captures, documented Nsight interfaces and real export schemas, event/object identity, draw-to-pipeline and shader association, bindings, and selected resource contents; probes and a capability matrix for R-015's selected Nsight releases, distinguishing native capture data, application-provided evidence, and unavailable information; a record of every failed integration/extraction attempt in docs/INVESTIGATIONS.md. Cover pass relationships, bindless resource selection, and indirect draw evidence where documented interfaces expose them.
Acceptance: Each required evidence category has a reproducible extraction example or a demonstrated gap tied to the tested backend/version; a minimal extraction path is exercised on a correct/faulty visual pair; each failed attempt records its documented entry point, reproduction steps, expected/observed results, evidence, and revisit condition; unresolved gaps and the chosen next action are recorded before dependent diagnostic features are claimed complete.
Notes: Depends on R-001 and R-005; initial probes can start with basic fixtures, then the matrix must cover R-010 before release. Store evidence through R-012 and explicitly pin bundles referenced by investigation records. Round 5 limits integration to documented Nsight interfaces and accepts explicit limitations; do not add private RPC, GUI automation, reverse-engineered formats, or a complementary backend to bypass a gap. Detailed state must not be inferred from function names or object counts alone. A documented gap completes investigation of that category, not the missing inspection capability needed by R-007; insufficient evidence leaves the dependent capability incomplete.
```

### R-010

```text
Status: Pending
Area: test/platform/shaders
Title: Add multipass, bindless, and indirect rendering scenarios
Goal: Make the development application representative of the advanced visual workloads selected for the first release while retaining reproducible defect isolation.
Scope: Extend R-005 with multiple passes, offscreen render targets, post-processing, bindless resource access, and indirect draws; selectable feature/scenario configurations, a combined workload, correct references, and controlled visual defects involving pass output, resource selection, or indirect draw parameters.
Acceptance: Each feature can be exercised in a reproducible scenario and the combined workload runs on the validated development configuration; correct and faulty outputs have documented references/tolerances; requested feature support is checked explicitly; the configuration and required API features accompany each run; capture compatibility and inspection gaps are recorded through R-001/R-006 without silently substituting simpler scenarios.
Notes: Required first-release renderer coverage selected in interview round 6; depends on R-005. Basic fixtures may enable earlier capture experiments, but they do not satisfy this item's advanced coverage. Exact Vulkan feature/extension choices remain to validate on the development GPU and Nsight version. Fixtures should use valid, deterministic API usage for visual defects; arbitrary out-of-bounds access or undefined behavior cannot serve as a stable reference. Standalone compute diagnosis and profiling remain later work.
```

### R-007

```text
Status: Pending
Area: analysis/replay/shaders/mcp/test
Title: Diagnose visual defects and verify source fixes
Goal: Let Codex investigate incorrect rendered output, identify the responsible shader, resource binding, or pipeline state, and verify a source fix using inspectable before/after evidence.
Scope: Bounded queries over available events, pipeline/shader associations and resource data; output previews/comparison; evidence references; and integration checks covering diagnosis, source/shader edits, rebuild, recapture, and fix verification against R-005's basic and R-010's advanced visual-defect scenarios. Reuse R-005's isolated experiment runner to select equivalent inputs/frames and link before/after reports with explicit evidence origins. Codex's normal development tools perform edits and builds.
Acceptance: For each supported scenario, Codex identifies the implicated draw/pass and explains the defect using retrievable evidence, edits the responsible source/shader, rebuilds, and obtains a new capture; the repaired output matches the correct reference within declared tolerances; before/after captures, build identities, and the source change are linked; unavailable state is reported explicitly; expected-answer metadata or selecting a prebuilt correct variant does not substitute for diagnosis and repair.
Notes: First workflow priority from round 4, expanded to the edit/rebuild/recapture/verify loop in round 5 and selected as the first-release boundary in round 6. Depends on R-003, R-005, R-010, R-012's evidence store, and a sufficient documented inspection path from R-006. Protect inputs during comparison and explicitly pin retained verification baselines. Repair validation uses a source-available fixture; unmodified applications still support capture and available diagnosis without promising source repair. Live breakpoints/stepping/variable inspection, compute correctness, and performance profiling are outside first-release scope.
```

### R-002

```text
Status: Pending
Area: capture/platform
Title: Add optional application-side capture controls
Goal: Give applications whose source is available precise native capture control at meaningful workload boundaries while preserving the unmodified-application path.
Scope: A C++ integration with the NGFX SDK, initialization before Vulkan instance creation, explicit workload boundaries, and per-launch capture configuration/result reporting so the server can request and observe a capture within a fresh application invocation.
Acceptance: A newly launched instrumented graphics fixture captures a chosen visual workload boundary and returns a usable artifact through the job lifecycle; a second request uses a fresh instance; the unmodified capture path continues to work without this integration.
Notes: Accepted application mode and native integration priority in rounds 1–2. Depends on R-003, R-005, R-011, R-012, and the capture contract also used by R-001. Add an instrumented fixture mode while retaining a run without NGFX integration. Round 7 makes a reusable application control session unnecessary for this release; choose a per-launch configuration/result mechanism and validate SDK synchronization. Round 6 moves compute-without-presentation validation to R-008. May move earlier if the visual workflow needs precise application-side capture boundaries.
```

### R-015

```text
Status: Pending
Area: test/platform/capture/replay/docs
Title: Validate locally against two Nsight Graphics releases
Goal: Establish first-release compatibility through reproducible local checks and real GPU workflows on at least two distinct Nsight releases.
Scope: Local CMake/CTest commands for first-party checks, opt-in hardware integration runs using R-005's isolated experiment runner, explicit Nsight installation selection, matching capture/replay/SDK configuration per release, small sanitized export fixtures and capability differences, and a versioned results matrix for the basic and advanced visual workloads. Start with the installed baseline and select a compatible second release from documented prerequisites.
Acceptance: Local non-GPU checks cover the relevant parser, job, artifact, and MCP behavior; on each selected Nsight release, real runs demonstrate unmodified capture, optional SDK capture, available evidence queries, and a source-edit/rebuild/recapture verification case; the basic and advanced scenario matrix records pass/fail/unsupported/skipped separately; each result identifies the exact tools, SDK, app/shader build, GPU/driver, desktop, and retained evidence; missing prerequisites and unsupported operations return explicit errors; two-release support is not claimed from mocks, parser fixtures, or skipped runs.
Notes: Validation location and at least two releases selected in round 10. Uses R-013's build/test entry points and the implemented first-release fixture, MCP, capture, inspection, job, and artifact paths. R-006 supplies per-version capability evidence and R-014 publishes results. Only the 2026.3 installation was observed in /opt/nsight-graphics during planning; a second release must be obtained and validated, not assumed present. Test each release with its own matching tools; cross-version capture-file replay is not required or presumed. Hosted CI, GPU CI runners, and additional GPU generations are outside this item. Failed or unavailable required cases remain incomplete rather than being counted as successful support.
```

### R-014

```text
Status: Pending
Area: docs/build/platform/test
Title: Validate and document Linux source installation
Goal: Let another Linux user build the project from source and reproduce the supported first-release visual-debugging workflow without workstation-specific assumptions.
Scope: Clone/submodule initialization and local configure/build/CTest instructions, compiler/build-tool and runtime prerequisites, Nsight discovery/overrides, Codex stdio configuration, fixture launch/capture examples, retention/pinning behavior, troubleshooting, and R-015's tested matrix for at least two Nsight releases with explicit capability limits.
Acceptance: The documented source-build procedure succeeds in a clean build environment using only declared prerequisites and initialized source dependencies; configure/build require no dependency downloads; documented runtime dependencies match binary inspection and the static-vendored-library policy; on the recorded compatible desktop/GPU setup, the instructions lead from a built server and fixture through capture, diagnosis, source repair, rebuild, recapture, and comparison; readers can locate known gaps and investigation evidence.
Notes: First-release source distribution for other Linux users was selected in round 9. Depends on R-013, R-015's local two-release validation, and the completed first-release MCP, artifact, capture, and visual-diagnosis work (R-003, R-012, R-011, R-001, R-002, R-005, R-010, R-006, R-007). Clean build validation and real GPU workflow validation are distinct checks; do not claim untested distro/driver coverage. Prebuilt release publishing, fully headless operation, and CI setup are outside this item.
```

### R-008

```text
Status: Pending
Area: analysis/capture/shaders/test
Title: Diagnose incorrect Vulkan compute results
Goal: Extend the evidence-based investigation workflow to compute dispatches after visual correctness is established.
Scope: Deterministic compute scenarios in R-005 with known inputs and reference outputs, controlled numerical/indexing defects, dispatch and shader association, input/output resource inspection where supported, and capture boundaries for workloads without presentation.
Acceptance: A supported compute scenario without presentation runs reproducibly, is captured using an explicit documented boundary strategy, and exposes enough evidence for Codex to localize its defect; numerical outputs are checked against an independent reference with declared tolerances; correct variants pass; application-readback evidence is distinguished from capture-derived evidence.
Notes: Second workflow priority, following R-007 and outside the first release by round 6's decision. Reuses R-006's inspection boundary and extends it for compute; extends R-002's controls where application-side capture control is required, including no-presentation boundary validation. Access to arbitrary buffer contents and dispatch state must be proven rather than inferred from metadata availability.
```

### R-009

```text
Status: Pending
Area: profile/analysis/shaders/test
Title: Analyze GPU and shader performance bottlenecks
Goal: Add performance investigation after visual and compute correctness workflows are established.
Scope: Controlled performance workloads in the test app, Nsight GPU Trace collection and supported metric exports, bounded metric queries, and repeatable comparisons with capture/build/hardware provenance.
Acceptance: A known inefficient workload yields usable profiling evidence; Codex can identify a bottleneck supported by the available metrics; a corrected variant preserves output and shows a repeatable measured change; reports include warmup/repetition policy, units, variability, and relevant clock/replay settings.
Notes: Third workflow priority, following R-008 and outside the first release by round 6's decision. Verify metric availability and data semantics against the installed Nsight version and GPU. Keep performance configurations separate from shader-debug configurations and account for replay reset work. Detailed per-source-line claims require a verified export path.
```

### R-004

```text
Status: Pending
Area: mcp/platform/docs
Title: Add a persistent Streamable HTTP service
Goal: Expose the server's tools through a persistent process using MCP Streamable HTTP after the local stdio release.
Scope: An HTTP transport adapter over the shared core, service startup/shutdown and configuration, client connection behavior, and deployment documentation.
Acceptance: A supported HTTP client discovers and invokes the same implemented tools with equivalent results; service lifecycle and client-disconnection behavior are documented and checked; listener exposure and access controls are explicit; local stdio integration continues to pass its checks.
Notes: Accepted later work in interview round 3, outside the first release. Depends on R-003's shared MCP/core boundary. Deployment environment, authentication, multiple-client policy, and job ownership across disconnects remain to be scoped before implementation; the stdio milestone should not absorb these requirements.
```

## Blocked

## Done

### R-005

```text
Status: Done
Area: test/platform/shaders/build
Title: Build a deterministic Vulkan development application
Goal: Provide a small, reproducible target with known correct and faulty rendering behavior for developing and validating the MCP server.
Scope: A standalone windowed C++ Vulkan application in this repository using an existing Linux desktop session, fixed inputs and frame selection, a correct reference scene, selectable visual-defect scenarios, named passes/resources, GLSL shaders compiled to SPIR-V with glslang, retained sources/build settings/debug information, and image readback for comparison. Add a C++ experiment runner with per-run configuration/log/output isolation and explicit scenario, seed, resolution, and frame selection. Initial cases should cover shader calculation, resource binding, and pipeline-state mistakes; the application will grow with later compute and profiling work.
Acceptance: The app builds and runs independently of the MCP server in the validated desktop session; a selected scenario reproduces its output across fresh application launches within declared tolerances; the runner isolates each run from other runs and normal user configuration and records its inputs, environment, outcome, and evidence locations; each initial faulty scenario has a correct reference and a documented expected symptom/cause; the app can run without NGFX calls or the optional capture bridge; fixture configuration, compiler version/options, shader hashes, and build identity accompany results; compilation failures surface clearly and cannot silently reuse stale shader output; missing display prerequisites produce an actionable error.
Notes: User requested a new test application in interview round 4. Uses R-013's source-built dependencies and shader compiler. Start with small visual scenarios; R-010 extends the app with the advanced rendering coverage required for the first release by round 6. R-008 adds compute correctness cases and R-009 adds performance workloads after that release. Keep expected diagnoses in test-harness data rather than returning them as MCP inspection evidence. Tests using application readback must identify that evidence source. Round 7 selected GLSL/glslang under delegated shader-language choice; round 8 selected a desktop/windowed target. The selected compiler flags, XCB backend, and initial scenarios are validated below; headless operation is not a first-release acceptance requirement. Completed 2026-09-18 after implementation, independent review, and correction cycles: XCB windowed presentation on KDE Wayland/Xwayland, four deterministic basic scenarios, isolated executable snapshots, retained GLSL/SPIR-V/compiler and independent executable identities. All 13 CPU checks pass in GCC Debug. The synchronization-validation matrix passes 17 fresh launches on RTX 3080 Ti / driver 615.71.09 with repeated images identical and an independent oracle within one RGB8 channel step outside the declared edge band. Shader-override provenance stays separate from executable identity. See docs/FIXTURE.md and docs/BUILD_VALIDATION.md for exact inputs, review corrections, retained evidence, and limits. Application readback establishes fixture behavior, not Nsight capture support.
```

### R-003

```text
Status: Done
Area: mcp/core/build/docs
Title: Integrate the local stdio server with Codex
Goal: Give Codex access to the C++ server's implemented tools through a local stdio process on the GPU machine.
Scope: A reproducible fastmcpp-based server build, a small MCP adapter, capability discovery, server identity using R-013's project version, Codex launch/configuration instructions, and focused client interoperability checks.
Acceptance: Codex launches the server, discovers its tools, and completes a real capability query; the MCP server identity reports the same project version as the standalone command-line query; invalid arguments produce a structured error; diagnostics, version banners, and child-process output do not corrupt protocol stdout; normal client shutdown is handled cleanly; the documented setup is verified on the Linux development machine.
Notes: First-release client and transport selected in interview round 3. C++ throughout and fastmcpp preference come from round 2. Uses R-013's pinned submodule/static-build integration; record the actual Codex client/version and negotiated protocol. This establishes the MCP access layer for R-001 and R-002; it does not establish capture functionality by itself. HTTP and other-client validation are outside this item's scope. Completed 2026-09-18 after independent protocol review and correction: fastmcpp 3.4.7.1, bounded stdio adapter, one implemented read-only capabilities tool. Codex CLI 0.154.0 launched product 0.1.0, negotiated MCP 2025-06-18, completed one real capabilities call, and exited cleanly. All 13 CPU checks pass, including protocol errors, lifecycle, stdout isolation, versions, and EOF. See docs/MCP.md and docs/BUILD_VALIDATION.md. Capture and detailed inspection remain explicitly unavailable.
```

### R-013

```text
Status: Done
Area: build/platform/test/docs
Title: Establish the source-built submodule dependency graph
Goal: Build the server, test application, and required dependency code reproducibly from pinned sources with vendored libraries linked statically and platform/runtime dependencies documented.
Scope: Top-level CMake targets, shared Ninja configure/build/test presets, ignored build directories and local overrides, exact-commit dependency submodules under external/, required transitive sources, a source-built glslang compiler, dependency-purpose/revision records, and binary-linkage checks. Configure fastmcpp to consume local dependency targets and keep unnecessary features disabled. Add a small project-owned C++ check harness and a registration helper for focused run targets, CTest, and an explicit aggregate target, with hardware integration targets separate. Establish the single CMake project version and shared C++ version accessor under the adopted Kiln policy, with standalone command-line version reporting.
Acceptance: A clean recursively initialized checkout builds using documented host prerequisites without dependency downloads or prebuilt project dependency packages; required dependency libraries and the fixture shader compiler are built from pinned source; a missing submodule produces an actionable configure error; binary inspection confirms static vendored-library linkage and the documentation accounts for system/runtime/GPU dependencies, including runtime-loaded libraries; focused and aggregate first-party checks run independently of Nsight/GPU execution, and ordinary checks do not launch hardware integration; reported server/fixture command-line versions match the authoritative CMake declaration.
Notes: Build/source policy selected in round 9, with lava-chan-viewer as the reference. The user clarified that documented system/runtime/GPU exceptions are allowed; the requirement is static vendored code, not zero dynamic ELF dependencies. This supplies the build foundation for R-003 and R-005; R-014 owns full user-facing source-installation validation. Completed 2026-09-17: C++20, CMake 3.25+, six exact-commit source submodules, source-built glslang 16.4.0, version 0.0.1 and both CLI version queries; the seven ngm_* CPU checks pass in GCC Debug, Clang Debug, and GCC Release. A clean recursively initialized snapshot passed with CMake 3.25.3 in a network namespace; ELF/archive inspection and runtime library tracing passed. See README.md, docs/DEPENDENCIES.md, and docs/BUILD_VALIDATION.md for commands, exact compilers, and runtime exceptions. At R-013 completion, server/fixture entry points were build bootstraps. R-003/R-005 subsequently completed MCP serving and basic rendering; the R-013/R-005/R-003 group completes at 0.1.0 on 2026-09-18. The initial foundation commit alone did not trigger a minor increment.
```
