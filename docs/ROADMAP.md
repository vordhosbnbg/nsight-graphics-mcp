# Project Roadmap

This roadmap is the project planning source for nsight-graphics-mcp. Keep it short
enough to scan at the start of a coding session and specific enough that the next
useful task is obvious. Items describe accepted work toward an MCP server for
Vulkan graphics and compute on Linux.

Planning state: **R-023 fresh Graphics Capture reliability completes at 0.5.1 on the measured 2026.3 path. R-027 standalone GPU replay completes at 0.5.2 on its measured matching 2026.3 path; R-025 draw timing correlation is In Progress; R-024 and R-026 remain Pending and require documented evidence. The completed R-018–R-022 group retains its 0.5.0 conditional scope. The shared surface now has 31 tools, including same-input timing collection at 0.5.4; installed server/worker configuration is unchanged.**
Last updated: **2026-10-15**.

The technology evaluation lives separately in [TECH_STACK.md](TECH_STACK.md).
The initial interview established the first-release requirements; all implementation
items follow the sequence below. Support applications with source access and unmodified
applications, targeting visual diagnosis and fix verification through Codex over
local stdio. Exact component versions, capability availability, and measured
defaults are implementation investigations assigned below. Compute correctness,
performance analysis, and a persistent Streamable HTTP service follow the visual milestone.

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

After the 0.5.0 capability follow-up, the owner accepted R-023–R-027 in the
priority order recorded on each item. Required evidence dependencies can affect
execution order: R-024 needs proven captured-event identity, and R-026 needs
R-025 for any draw-level attribution. The completed R-018–R-022 items retain
their original conditional scope; the new items require actual capability
improvements before completion. The first-release sequence below is history.

The R-013/R-005/R-003 build/basic-fixture group is complete at 0.1.0.
The capture/evidence group completes at 0.2.0 after reviewed CPU checks and
real basic capture/export matrices on two matching Nsight releases. Early R-006
queries now pass on 54 basic/advanced captures from two exact producer profiles
at 0.2.1; R-010 is complete. Optional SDK control completes R-002 at 0.2.2 for
its recorded two-release basic workload. R-006 investigation completes at 0.2.3
with generated source/resource examples and explicit gaps across both releases;
R-007 now owns deeper product queries and the actual source-repair workflow.

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

### R-025

```text
Status: In Progress
Area: profile/inspection/capture/mcp/test
Title: Associate timings with captured draw occurrences
Goal: Identify the draw, submission and occurrence to which a reported GPU timing belongs, including repeated markers and reused recordings.
Scope: Qualify documented action/queue/submission/sample identities and any capture-to-profile correspondence, then add bounded correlation queries for demonstrated producer-specific scopes. Preserve the existing unique single-frame marker-range contract.
Acceptance: Direct/indexed draws, nested/repeated and multi-draw markers, reused command buffers and multiple collected intervals have independent occurrence/timestamp validation. Results retain unambiguous original identities, units and source provenance through restart. Ambiguous or unsupported joins refuse; row order, equal counts and matching numeric IDs alone never establish correspondence. Actual new correlation is required for completion.
Notes: Priority 3, accepted by the owner on 2026-10-15. Follow-up to I-043, I-039 and the completed conditional R-022 assessment; see docs/PROFILE_SCOPE_FEASIBILITY.md. Stable documented execution identifiers or an equivalently demonstrated association are required. Existing profile_marker support does not resolve this item. Supports R-024 and draw-level work in R-026; neither is a prerequisite for this investigation. Private queue/draw label probes now preserve unique submission and leaf paths, and profiling/generated C++ from the same saved input preserves original labels. Live multi-interval timing columns still repeat first-frame paths; the raw regime table separately contains all three interval paths. Replay-pass triggers and startup failures require distinct qualification. The 0.5.3 multi-interval capture foundation passes all 37 final CPU checks (368.46 seconds), serial MCP/CLI exact image checks, replay/range identity checks, 154 actual schema-valid results and restarts. Both raw captured_frame/captured_frames strings are exposed without inferred IDs. Complete/pinned bundle-487017236ba14e318c20a04bbc66dd5a retains all 2082 verified payloads and 21 native pins, with exact collection/final build identities. See docs/MULTI_INTERVAL_CAPTURE.md and docs/BUILD_VALIDATION.md. At 0.5.4, source-leased same-input C++/GPU Trace collection is implemented for qualified source counts one/three; four final-build actual MCP jobs pass with 353 schema-valid results and two restarts per source. Per-phase trigger/settings/hash checks, partial failure evidence and shared owned cleanup remain explicit. I-062–I-065 retain parser, termination, harness and C++ entry failures; see docs/REPLAY_TIMING.md and docs/BUILD_VALIDATION.md. All 38 final CPU checks pass in 382.75 seconds; complete/pinned bundle-d95a9298e94a1dfc4b1e598bc2445d3f retains 2302 verified payloads and 15 native pins, with imported verification passing. Literal call association, independent per-draw timestamps and the full correlation acceptance remain unfinished; no public draw-timing correlation query is claimed yet.
```

## Pending

### R-024

```text
Status: Pending
Area: inspection/capture/mcp/test
Title: Expose proven descriptor contents and execution-time values
Goal: Establish which descriptor resources and typed uniform/material values a selected captured draw actually used.
Scope: Qualify a documented selected-event evidence path, resource/range/write lifetimes and type/layout schemas; implement bounded queries with per-field provenance and explicit unsupported coverage. Preserve the distinction between recorded bindings or uploaded bytes and execution-time values.
Acceptance: Independent fixture oracles verify descriptor contents, resource ranges and typed values at selected draw occurrences, including updates between draws. Fresh capture/rebuild/recapture, bounded MCP results, exact source/tool/reader identities and restart/pin checks pass. Unknown lifetime, shader selection or type evidence refuses rather than inferring a value; a limitation report alone does not complete the item.
Notes: Priority 2, accepted by the owner on 2026-10-15. Follow-up to I-039 and the completed conditional R-021 assessment; see docs/DRAW_STATE_FEASIBILITY.md. Requires proven captured-event identity from R-025 or an independently qualified state interface before execution-time claims. The current five-file worker exception is unchanged; a wider proprietary helper closure still requires a separate owner decision. Missing documented evidence remains an explicit prerequisite, not a promised backend capability.
```

### R-026

```text
Status: Pending
Area: profile/analysis/mcp/test
Title: Qualify hardware counter meaning and application attribution
Goal: Make supported counters trustworthy evidence of application work with verified scope, units, scaling and aggregation.
Scope: Investigate I-045's differing totals for equivalent fixture API inputs; establish version/architecture-qualified documented definitions and independently validate application attribution on repeated controlled workloads. Expose physical or draw/fragment interpretations only where demonstrated, preserving raw cells and unavailable values.
Acceptance: Independent new-input workloads verify each advertised counter's units, normalization, aggregation and application boundary, and explain or bound the observed equivalent-workload differences. Draw-level claims require R-025 correlation; fragment/depth claims require stage/scope evidence. Raw zero and unavailable cells remain distinct, all samples are retained, and results/pins survive restart. An unresolved attribution report does not satisfy completion.
Notes: Priority 4, accepted by the owner on 2026-10-15. Follow-up to I-045 and I-043; see docs/PROFILE_SCOPE_FEASIBILITY.md and docs/PROFILE_OVERHEAD.md. No assumed API-count equivalence, frame-to-draw division or attribution to background GPU activity without evidence. R-025 is required for draw-level attribution, not for investigating frame-level meaning. No implicit clock, permission or background-process changes; causal overhead correction (I-044) is outside this item's scope.
```


## Blocked

## Done

### R-027

```text
Status: Done
Area: replay/capture/platform/test
Title: Qualify standalone GPU replay
Goal: Render retained captures reproducibly without launching the original application again.
Scope: Isolate the initialization stalls in I-007/I-009/I-010/I-011 through matching documented replay tools and controlled environments; implement the demonstrated correction in the supervised replay path and retain explicit version support.
Acceptance: Actual GPU replay completes for retained correct/faulty captures and demonstrates repeatable rendered output at equivalent inputs. Each advertised release has its own matching-tool pass; metadata-export success cannot substitute for GPU replay. Failure, deadline, cancellation and shutdown checks confirm bounded owned-process cleanup. Exact capture/tool/GPU/driver/desktop identities, logs, comparisons and persistent evidence pins are retained; an initialization-timeout report alone does not complete the item.
Notes: Priority 5, accepted by the owner on 2026-10-15. Completed at 0.5.2 on the measured matching 2026.3.1.0/build 38722833 desktop path with explicit matching-driver library/ICD selection; the historical stall causes are not retroactively established and 2026.2 GPU replay remains unqualified. The new replay tool shares stdio/HTTP, the GPU coordinator, source leases/snapshots, typed completions and failure publication. All 37 serial CPU checks pass (381.19 seconds). Three actual MCP replay jobs pass; execution-phase cancellation, deadline and EOF shutdown each confirm cleanup. All 515 actual results validate against advertised schemas; source hashes and two restarts preserve reports/pins. Earlier 0.5.1 output probes independently establish exact fixture frame-1 healthy/repeated/faulty pixels and a no-blit negative control, distinct from the source's embedded frame-2 screenshot (I-050/I-051); no generic frame/event/timing inference is made. Final complete/pinned bundle-1c59130aafc0cb1a946cd9e0c4da869c retains all 741 verified payloads, nine independently pinned native bundles and the earlier probe evidence. The installed importer remains 0.4.1; the tested binary/source is 0.5.2. See docs/REPLAY.md and docs/BUILD_VALIDATION.md; I-047/I-048/I-052 retain development corrections. R-024–R-026 remain Pending and need actual documented evidence improvements.
```

### R-023

```text
Status: Done
Area: capture/platform/test
Title: Restore reliable fresh Graphics Capture
Goal: Make the current unattended Graphics Capture path reliably produce inspectable captures from fresh Vulkan application launches.
Scope: Reproduce I-038 with the explicit matching-driver environment; isolate the connection timeout using controlled documented CLI launches and implement the demonstrated backend, launch or prerequisite correction. Keep the separately working C++ capture path qualified independently.
Acceptance: Fresh reference, repeated-reference and faulty fixture captures complete with matching tools, readable exports and independently verified selected-frame images; reference repeats agree and the fault differs. Timeout/cancellation/shutdown checks preserve owned cleanup and GPU reservations. Exact runtime/build identities, failed and successful evidence, source pins and restart checks are retained.
Notes: Priority 1, accepted by the owner on 2026-10-15. Follow-up to I-038 and R-019; historical R-001 qualification remains intact. The default NVML mismatch has a process-local workaround, but matching-library capture attempts still time out; neither a server regression nor the underlying cause is established. Driver, desktop and permission changes are not implicit prerequisites authorized by this item. Completed at 0.5.1 on 2026-10-15. I-046 identifies an owned low-space dialog in a fresh 0.5.0 reproduction; the adapter now supplies the exact help-advertised unattended control, preserving the older interface without it. Three fresh 2026.3 MCP captures complete in 6.354–6.655 seconds, with distinct targets and exact frame-2 application-readback pixels. All 36 serial CPU checks pass before the final narrow producer-schema correction (I-049); focused MCP/HTTP checks and 20 actual schema-valid retained results then pass, including graphics/compute producer variants, four capture-store restarts and unchanged pins/payloads. Complete qualification bundle-ab49ccec89e6f4584051359b650e7f41 is pinned with all 407 payloads verified. See docs/BUILD_VALIDATION.md. This does not retroactively establish the cause of I-038 or new 2026.2 GPU support.
```

### R-022

```text
Status: Done
Area: profile/capture/mcp/test
Title: Qualify correlated draw timing and counter scopes
Goal: Expose supported action/marker durations and counter scopes using actual same-profile exported evidence.
Scope: Add typed retained per-action/HES settings; probe labelled repeated/reused fixture workloads, audit exported associations/units and implement bounded queries for demonstrated scopes.
Acceptance: Actual exported rows support unambiguous correlation and independently checked timing/units/overhead; each counter has verified scope; unavailable documented evidence receives a retained limitation outcome; two-release claims require both releases.
Notes: Authorized by the owner on 2026-10-15. Step E; no private trace decoder, implicit permission changes or fabricated draw counters. Application witnesses remain a separate evidence source. The first slice at 0.4.8 adds nullable per-action/HES controls and producer-qualified unavailable regime cells. All 35 serial CPU checks and two fresh matching 2026.3 profiles pass; 202 MCP results validate against actual schemas and restart checks pass. Qualification is pinned as bundle-193d06c97b109c55fb8a30e35f12b8f9, with source profiles independently pinned. At 0.4.9, six baselines and fifteen fresh 2026.3 profiles pass, with 400 independent API/timestamp submission checks, 1,615 schema-validated MCP results, restart pins and all 35 serial CPU checks. The single-frame marker candidate has scene-unit/timing sanity; I-043 retains wider identity/counter limits. The 0.4.9 qualification is pinned as bundle-40d1c8f983a49246a26d5294f20636bb, with all fifteen source profiles independently pinned. At 0.4.10, profile_marker qualifies unique single-frame exported marker ranges, with explicit original-identity/draw/scaling limits. All 36 serial CPU checks, 391 retained-profile stdio results and eight HTTP results pass; values/refusals and source pins remain stable after restart. Qualification is pinned as bundle-b8d3515b5cd6079e2ba2c8cb5637067e; source profiles retain 0.4.9 collection identities. At 0.4.11, two baselines and eight alternate-order off/on profiles pass after 30 warmup presents, with 326 private submission checks, 32–96 ns scene agreement, all 1,168 raw frame cells and 913 schema-valid MCP results. All eight sources remain independently pinned; qualification is pinned as bundle-8d667f47f660fc46a582f440ee786c5e. I-044/I-045 retain inconclusive causal overhead and unresolved counter attribution. At 0.5.0 the conditional scope report and full A–E completion audit pass, with 36 CPU checks, 691 schema-valid final-server results, 28 recorded draws, 31 new pinned reads and verified retained evidence. R-018–R-022 complete as one minor-version group; unavailable physical/execution/overhead fields remain explicit. Final qualification is pinned as bundle-bc107b133c527aa81f11c17fa93c9743. See docs/PROFILE_SCOPE_FEASIBILITY.md and docs/CAPABILITY_FOLLOWUP_COMPLETION.md. See docs/PROFILE_OVERHEAD.md and docs/PROFILE_MARKERS.md and docs/PROFILE_CORRELATION.md. See docs/PROFILE_CONTROLS.md, docs/BUILD_VALIDATION.md and docs/CAPABILITY_FOLLOWUP_PLAN.md.
```

### R-021

```text
Status: Done
Area: inspection/capture/mcp/test
Title: Qualify selected-draw state in bounded evidence slices
Goal: Expose evidence-supported pipeline/shader, depth/culling and viewport/scissor state for identified draw execution occurrences, then assess descriptors/constants/values.
Scope: Qualify documented interfaces and retained source grammar, add a separate state interpreter/adapter with capture-local execution selection, per-field provenance and deterministic refusals; qualify later state/value slices only where evidence establishes them.
Acceptance: Independent fixture API/execution records, static/dynamic disagreement and lifecycle/refusal matrices pass; retained and fresh matching-tool evidence supports every advertised field; unresolved executed-state/value correspondence remains explicitly unavailable.
Notes: Authorized by the owner on 2026-10-15. Step D; documented interfaces only. The first recorded-state slice at 0.4.5 adds capture-local submitted draw references, pipeline/shader, depth/culling and viewport/scissor fields, input hashes and refusals; its four-capture/eight-read qualification is pinned as bundle-556efc8c7f65c8ebdb96e360197fc015. At 0.4.6, per-stage push writes, compatible layouts and explicit opaque-byte reconstruction pass 34 serial CPU checks, two fresh 2026.3 captures with nine draw occurrences, 41 schema-validated MCP results and restart pins. The 0.4.6 qualification is pinned as bundle-754e06a3afa984d58e2d073bac3b590f; source/read bundles are independently pinned. At 0.4.7, recorded descriptor bindings, layout compatibility/disturbance and dynamic offsets pass 35 serial CPU checks and two fresh matching 2026.3 captures covering 15 draw occurrences, 46 schema-validated state responses and MCP restart stability. The 0.4.7 qualification is pinned as bundle-b59b7343be911eec0e49b85ffe5767ef; its source captures are independently pinned. The conditional remaining-field feasibility outcome is complete: descriptor contents, execution-time values and general GPU correspondence remain unavailable, with the two-producer/fresh-input audit pinned as bundle-7269c28d30326b8f1fa3ef64ebd4ed36. See DRAW_STATE_FEASIBILITY.md and I-039. This does not claim full effective GPU state; 2026.2 state support remains unqualified. See DRAW_STATE.md and BUILD_VALIDATION.md. Wider proprietary hydration closure still requires a separate owner scope decision. I-038 graphics timeouts and R-022 remain separate work.
```

### R-020

```text
Status: Done
Area: profile/inspection/mcp/test
Title: Preserve valid header-only event and regime exports
Goal: Keep frame metrics usable when a qualified GPU Trace export has no action/regime rows.
Scope: Accept producer-qualified header-only event/regime tables; make metadata, queries and comparisons safe with explicit missing coverage; retain malformed-export rejection.
Acceptance: Retained header shapes and frame rows parse; stand-in jobs, restart, queries and comparisons preserve provenance and never fabricate zero counters or durations.
Notes: Authorized by the owner on 2026-10-15. Step C completes at 0.4.4: 32/32 serial CPU checks pass. Actual retained header shapes and all 146 frame rows pass managed stand-in/typed queries; actual MCP empty/escaped-header paging and restart checks pass. Empty comparisons refuse explicitly. Qualification is pinned as bundle-2051be7525227bb35f9e959f9d96c9ca; no new GPU profiling, raw import or draw correlation is claimed. See docs/CAPABILITY_FOLLOWUP_PLAN.md.
```

### R-019

```text
Status: Done
Area: core/capture/mcp/cli/test
Title: Expose controlled per-job launch environments
Goal: Reproduce explicit library/ICD/backend launch conditions through the existing job lifecycle.
Scope: Add a bounded allowlisted environment overlay shared by capture, C++ capture, profile and ngm-capture; apply it to discovery/collection/export before reserved-directory isolation; retain requested/applied provenance.
Acceptance: CPU stand-ins and actual stdio/HTTP prove propagation, validation, queued-job isolation, defaults, cleanup and unchanged resource-worker/parent environments; separately qualify the runtime fixture path.
Notes: Authorized by the owner on 2026-10-15. Step B; first product increment completes at 0.4.3. All 32 serial CPU checks pass; actual stdio/HTTP/CLI and process-boundary propagation/isolation/refusal checks pass. Two independent selected-environment fixture baselines pass; three current Nsight capture attempts time out with confirmed cleanup. Fresh capture compatibility remains unqualified (I-038). Implementation/validation is pinned as bundle-2a69ab0ef0bd54fd2df1fdeadc6fc423; docs/LAUNCH_ENVIRONMENT.md records scope. Machine permissions/clocks are outside scope. See docs/CAPABILITY_FOLLOWUP_PLAN.md.
```

### R-018

```text
Status: Done
Area: inspection/test/configuration/docs
Title: Activate and requalify the existing resource-byte reader
Goal: Make the matching 2026.3 worker usable by the operator and verify current-server resource bytes against independently retained inputs.
Scope: Build the fingerprint-qualified existing worker, run confinement and retained MCP checks in an isolated store, support one configured profile in the retained harness, configure the verified worker path, and retain pinned qualification evidence.
Acceptance: Exact shader bytes/hash and split ranges match the independent oracle; incorrect references/profiles refuse; helper/database/source/worker provenance and pins survive restart; configuration advertises the reader without claiming runtime enforcement from discovery alone.
Notes: Authorized by the owner on 2026-10-15 as step A of docs/CAPABILITY_FOLLOWUP_PLAN.md. No new resource API, generated helper closure, raised limits or GPU capture is required. Completed at 0.4.2: 31/31 CPU checks pass; actual confinement and two retained one-profile MCP runs verify exact shader bytes/ranges/provenance/refusal/restart pins. The verified worker is configured for new Codex server processes; existing connections need reconnecting. Qualification is pinned as bundle-133f94fc5a57cc411bc5eb00a8f1a5b6 in the external configured store; docs/RESOURCE_QUERIES.md records exact paths and scope.
```

### R-017

```text
Status: Done
Area: analysis/capture/profile/mcp/docs
Title: Assess draw-state, resource-reader, timing and launch-environment follow-up
Goal: Prepare a prioritized implementation and feasibility plan for the owner's selected-draw state, existing resource-byte reader, correlated timing/counters and per-job launch-environment needs.
Scope: Read current policy/implementation and retained eternity-forge 4K-band evidence; separate existing configuration from new implementation; document evidence limits, prerequisites, acceptance checks and the smallest useful increment in docs/CAPABILITY_FOLLOWUP_PLAN.md; record supporting stack evaluation in docs/TECH_STACK.md.
Acceptance: The plan covers all four requested needs, preserves executed-state and cross-run correlation limits, identifies a concrete matching retained helper input and current configuration gaps, and specifies CPU/retained/future GPU validation without changing application code or eternity-forge.
Notes: Planning-only assessment ran 2026-10-14–15 and completed at unchanged 0.4.1. Live capability/help observations, retained helper hashes/pin/producer checks and raw game-table inspection were performed; no build/tests, reader execution, live capture, profiling or machine controls were run. Resource reads are implemented but unconfigured; header-only profile exports and the retained reader harness's old 22-tool assertion need follow-up. The proposed first product increment is allowlisted per-job launch environment; effective draw state remains the main objective and requires documented evidence qualification. No implementation items or new capability claims are created by this completion.
```

### R-016

```text
Status: Done
Area: build/packaging/docs
Title: Package the server for Arch Linux
Goal: Build an installable Arch Linux package from the repository's pinned sources.
Scope: A package CMake preset, runtime install rules, a source-complete archive and checksummed PKGBUILD/.SRCINFO, local makepkg integration, installed-file checks and user documentation.
Acceptance: The generated recipe builds from its accompanying sources without dependency downloads; CPU checks pass; the package includes the server, capture CLI, documentation and required license notices; extracted binaries report the product version and serve MCP; packaged ELF linkage matches declared runtime dependencies.
Notes: Requested by the owner after 0.4.0 and implemented at 0.4.1. The local makepkg build passes all 31 CPU checks, including staged installation; the extracted package passes CLI/MCP checks with 26 tools and an ELF dependency audit matching glibc/gcc-libs. See docs/ARCH_PACKAGE.md and docs/BUILD_VALIDATION.md. Nsight and optional generated resource workers remain separately supplied. The fixture remains a development/validation target. No AUR publication or host installation was performed.
```

### R-009

```text
Status: Done
Area: profile/analysis/shaders/test
Title: Analyze GPU and shader performance bottlenecks
Goal: Add performance investigation after visual and compute correctness workflows are established.
Scope: Controlled performance workloads in the test app, Nsight GPU Trace collection and supported metric exports, bounded metric queries, and repeatable comparisons with capture/build/hardware provenance.
Acceptance: A known inefficient workload yields usable profiling evidence; Codex can identify a bottleneck supported by the available metrics; a corrected variant preserves output and shows a repeatable measured change; reports include warmup/repetition policy, units, variability, and relevant clock/replay settings.
Notes: Third workflow priority, following R-008 and outside the first release by round 6's decision. Verify metric availability and data semantics against the installed Nsight version and GPU. Keep performance configurations separate from shader-debug configurations and account for replay reset work. Detailed per-source-line claims require a verified export path. The owner approved temporary per-user capability access on 2026-09-25. Five real GPU Trace probes pass across matching 2026.3/2026.2 tools, with permissions restored after each batch. A private equivalent-output workload shows improved lane utilization and shorter dispatch times; application validation and Nsight counters remain distinct evidence. The 0.3.3 slice adds bounded observed-export parsers; 0.3.4 adds shared asynchronous profiling and bounded retained metric queries, with 28 CPU checks and actual MCP protocol calls on both releases; permissions are restored after the service batch. Independent service acceptance passes, and the 124-payload snapshot is pinned as bundle-9de7695e0225eb604775a6fe52c5a4dd in artifacts/performance-evidence. The 0.3.5 fixture slice integrates controlled performance shaders, explicit warmup and all-frame numerical/timestamp evidence through the ordinary experiment runner; 29 CPU checks and ten real launches/566 exact-output frames pass, with measured variability retained. Independent fixture acceptance passes; its 990-payload snapshot is pinned as bundle-a5fbe932759bd51e5785cbc361b26b3a in artifacts/performance-evidence. At 0.3.6, bounded repeated profile comparison is implemented and independently reviewed; all 30 CPU checks and the expanded focused comparison check pass. The R-009 performance group completes at 0.4.0: actual selected-scenario source edit/rebuild, exact numerical validation and four fresh traces per variant on each release pass independent acceptance. The final 30-check aggregate, focused numeric-guard checks and final-binary offline comparisons pass. Permissions are restored after every batch. The 2914-payload acceptance snapshot is pinned as bundle-d4db174806f33a707b908900599404d5 in artifacts/performance-evidence, with every payload verified and the pin checked after restart. Hardware acceptance retains its exact 0.3.6 development-build identities. See docs/PERFORMANCE.md.
```

### R-004

```text
Status: Done
Area: mcp/platform/docs
Title: Add a persistent Streamable HTTP service
Goal: Expose the server's tools through a persistent process using MCP Streamable HTTP after the local stdio release.
Scope: An HTTP transport adapter over the shared core, service startup/shutdown and configuration, client connection behavior, and deployment documentation.
Acceptance: A supported HTTP client discovers and invokes the same implemented tools with equivalent results; service lifecycle and client-disconnection behavior are documented and checked; listener exposure and access controls are explicit; local stdio integration continues to pass its checks.
Notes: Accepted later work in interview round 3, outside the first release. Depends on R-003's shared MCP/core boundary. Implementation selects local loopback access with a required private-file bearer token, one shared workspace for trusted clients, and service-owned jobs that survive client disconnect/session deletion. Service shutdown cancels and cleans up jobs. Remote deployment and multi-tenant isolation are not selected. Completed 2026-09-18 at 0.3.2 after fresh-context transport review, corrections and independent acceptance. The shared 22-tool core serves independent HTTP sessions; executable stand-ins verify capture, cross-client cancellation, session deletion, restart retention and partial-request shutdown. The GCC Debug aggregate passes 26/26 CPU checks, including stdio. Actual Codex CLI 0.154.0 HTTP discovery and capability/artifact calls pass; service persistence and clean SIGTERM exit verify. Qualification is complete/pinned as bundle-237e85b8b723970aeb424b0e98b19ae8 in artifacts/http-qualification, with all 233 payload hashes and restart pin state verified. See docs/HTTP.md.
```

### R-008

```text
Status: Done
Area: analysis/capture/shaders/test
Title: Diagnose incorrect Vulkan compute results
Goal: Extend the evidence-based investigation workflow to compute dispatches after visual correctness is established.
Scope: Deterministic compute scenarios in R-005 with known inputs and reference outputs, controlled numerical/indexing defects, dispatch and shader association, input/output resource inspection where supported, and capture boundaries for workloads without presentation.
Acceptance: A supported compute scenario without presentation runs reproducibly, is captured using an explicit documented boundary strategy, and exposes enough evidence for Codex to localize its defect; numerical outputs are checked against an independent reference with declared tolerances; correct variants pass; application-readback evidence is distinguished from capture-derived evidence.
Notes: Second workflow priority, following R-007 and outside the first release by round 6's decision. Reuses R-006's inspection boundary and extends it for compute; extends R-002's controls where application-side capture control is required, including no-presentation boundary validation. Access to arbitrary buffer contents and dispatch state must be proven rather than inferred from metadata availability. Work began after the 0.3.0 visual milestone. A privately reviewed deterministic C++ compute probe runs without presentation and passes independent integer readback checks with synchronization validation. The first real 2026.3 capture succeeds using the documented VK_EXT_frame_boundary delimiter and exports a dispatch inventory. The product fixture/runner now include three compute scenarios, all-frame readback validation and an explicit frame-boundary capture delimiter. The initial numerical matrix passes 12 launches/30 frames, and the real 2026.3 MCP matrix retrieves source/SPIR-V plus application readback for reference and two localized defects. Application evidence stays distinct from dispatch/object exports. The corrected 2026.2 boundary path is unsupported on the measured injected device (I-031); generated-source compute capture remains unqualified. Both actual GLSL repairs are recompiled and recaptured with unchanged scenario selections; all repaired outputs match the independent reference exactly. The 0.3.1 aggregate passes 25/25 CPU checks. Fresh-context acceptance review independently passes 1,143 assertions. Supporting evidence is published and pinned as bundle-1ed0b5e17d49e24b6333d6ae4b10ffc1 in artifacts/compute-evidence; all 1,127 payload hashes and restart pin state verify. Original numerical/MCP results retain their 0.3.0 development identities, while repair and unsupported MCP runs are 0.3.1. Completed 2026-09-18 at 0.3.1. See docs/COMPUTE.md and I-027 through I-031.
```

### R-007

```text
Status: Done
Area: analysis/replay/shaders/mcp/test
Title: Diagnose visual defects and verify source fixes
Goal: Let Codex investigate incorrect rendered output, identify the responsible shader, resource binding, or pipeline state, and verify a source fix using inspectable before/after evidence.
Scope: Bounded queries over available events, pipeline/shader associations and resource data; output previews/comparison; evidence references; and integration checks covering diagnosis, source/shader edits, rebuild, recapture, and fix verification against R-005's basic and R-010's advanced visual-defect scenarios. Reuse R-005's isolated experiment runner to select equivalent inputs/frames and link before/after reports with explicit evidence origins. Codex's normal development tools perform edits and builds.
Acceptance: For each supported scenario, Codex identifies the implicated draw/pass and explains the defect using retrievable evidence, edits the responsible source/shader, rebuilds, and obtains a new capture; the repaired output matches the correct reference within declared tolerances; before/after captures, build identities, and the source change are linked; unavailable state is reported explicitly; expected-answer metadata or selecting a prebuilt correct variant does not substitute for diagnosis and repair.
Notes: First workflow priority from round 4, expanded to the edit/rebuild/recapture/verify loop in round 5 and selected as the first-release boundary in round 6. Depends on R-003, R-005, R-010, R-012's evidence store, and a sufficient documented inspection path from R-006. Protect inputs during comparison and explicitly pin retained verification baselines. Repair validation uses a source-available fixture; unmodified applications still support capture and available diagnosis without promising source repair. Live breakpoints/stepping/variable inspection, compute correctness, and performance profiling are outside first-release scope. R-006 investigation is complete with generated source and fixed-capture shader/resource evidence plus explicit gaps. At 0.2.4, bounded artifact image comparison and the basic shader-calculation source edit/rebuild/recapture case pass on both releases: six fresh captures, exact repaired/reference RGB equality, linked source/compiler/build identities, cleanup, and pins/comparison after restart. docs/SHADER_REPAIR.md records the separate diagnosis/edit/build evidence and harness scope. At 0.2.5, bounded image previews and PNG comparison are implemented with 18 retained PNG/BMP/PPM inputs from both releases and 36 full/cropped previews; docs/IMAGE_PREVIEWS.md records scope and evidence. At 0.2.6, capture_cpp_source/capture_cpp_draws expose numbered source and qualified draw/pipeline/shader references with explicit coverage limits; retained MCP qualification passes 36 C++ captures and 48 draws across both producers, including restart and source-hash checks (docs/CPP_INSPECTION.md). At 0.2.7, the opt-in fixed-capture descriptor hydration runner qualifies setup array-element mappings on the two-release combined reference/postpass-fault cases; docs/DESCRIPTOR_HYDRATION.md records its exact trusted inputs and limits. This is not a generic product extractor. At 0.2.8, the source-repair harness verifies an isolated one-line C++ postpass control fix while preserving faulty scenario names and common shaders; Both combined/standalone postpass cases pass on both releases, with 12 fresh captures, 12 application baselines, exact repaired/reference equality, fresh source queries, pins and restart. docs/SOURCE_REPAIR.md records the evidence and limits. At 0.2.9, independent binding and pipeline-state source fixes pass on both releases with 12 fresh captures and 12 application readbacks; repaired/reference RGB equality is exact. docs/STATE_REPAIR.md records the separate diagnosis, unchanged-helper descriptor experiment, source/build records, and qualification limits. Five of nine defect scenarios now have verified source repairs. At 0.2.10, all four remaining standalone/combined resource-index and indirect-parameter source repairs pass both releases, with 24 fresh captures, 24 independent readbacks and exact repaired/reference pixels. docs/ADVANCED_REPAIR.md records capture-derived push/indirect values, matched shader evidence, separate source builds and limits. All nine scenarios now have verified source repairs. The final 12-run matrix (including four prior-profile regressions) passes 36 captures and 36 baselines, independent acceptance, and the 22-check CPU aggregate. Complete qualification bundle-1e6ebc236bc4232d22303d0be4ca3583 is pinned with 91 referenced pins. At 0.2.11, optional fingerprint-qualified workers and Linux confinement implement the byte-reader boundary, with no fixed capture allowlist. The 23-check CPU aggregate and independent acceptance pass; 636 retained-input invocations cover both profiles, with complete evidence pinned as bundle-78b10c057ee352241bb5eb276b22ff11. Source-reference resolution, leased immutable snapshots, response validation and MCP resource queries remain unfinished (docs/RESOURCE_WORKER.md). The completed investigation and internal worker do not satisfy those remaining product checks. At 0.2.12, capture_cpp_resources/capture_cpp_resource implement literal references, leased streaming snapshots, qualified worker supervision and strict response validation (22 tools). The 25-check CPU aggregate passes. MCP qualification passes 43 product captures, including six newly generated combined captures on both releases, with 159 independent resource byte oracles (636 reads), six large range-only resources (30 reads), five explicit historical-probe refusals, pins and restart. Fresh-context review corrected publication-failure snapshot retention and explicit-section input validation. Every read retains its exact input snapshots under artifact quotas. Qualification bundle bundle-6365322f8eb3922893e1d2256983f732 is complete/pinned with 788 referenced pins and independently verified payload/manifest identities. docs/RESOURCE_QUERIES.md and docs/BUILD_VALIDATION.md record scope and evidence; R-007/R-015 remain active for complete workflow acceptance. Completed 2026-09-18 in the visual first-release group at 0.3.0. Fresh-context final acceptance independently verifies all 18 scenario/release pairs across 28 retained repair reports, 84 captures and 140 image comparisons, plus the qualified resource-query evidence. No remaining implementation gate was found within the declared scope. docs/VISUAL_RELEASE.md consolidates exact capability and repair coverage; executed event state, generic descriptor interpretation and GPU replay remain explicitly unavailable/unqualified. Acceptance audit and source-installation snapshot bundle-49217a19ae75f41fbe607152bbbd3699 is complete/pinned, with 902 referenced pins verified.
```

### R-015

```text
Status: Done
Area: test/platform/capture/replay/docs
Title: Validate locally against two Nsight Graphics releases
Goal: Establish first-release compatibility through reproducible local checks and real GPU workflows on at least two distinct Nsight releases.
Scope: Local CMake/CTest commands for first-party checks, opt-in hardware integration runs using R-005's isolated experiment runner, explicit Nsight installation selection, matching capture/replay/SDK configuration per release, small sanitized export fixtures and capability differences, and a versioned results matrix for the basic and advanced visual workloads. Start with the installed baseline and select a compatible second release from documented prerequisites.
Acceptance: Local non-GPU checks cover the relevant parser, job, artifact, and MCP behavior; on each selected Nsight release, real runs demonstrate unmodified capture, optional SDK capture, available evidence queries, and a source-edit/rebuild/recapture verification case; the basic and advanced scenario matrix records pass/fail/unsupported/skipped separately; each result identifies the exact tools, SDK, app/shader build, GPU/driver, desktop, and retained evidence; missing prerequisites and unsupported operations return explicit errors; two-release support is not claimed from mocks, parser fixtures, or skipped runs.
Notes: Validation location and at least two releases selected in round 10. Uses R-013's build/test entry points and the implemented first-release fixture, MCP, capture, inspection, job, and artifact paths. R-006 supplies per-version capability evidence and R-014 publishes results. Only the 2026.3 installation was observed in /opt/nsight-graphics during planning; a second release must be obtained and validated, not assumed present. Test each release with its own matching tools; cross-version capture-file replay is not required or presumed. Hosted CI, GPU CI runners, and additional GPU generations are outside this item. Failed or unavailable required cases remain incomplete rather than being counted as successful support. Qualification of 2026.2 is now underway: the official 2026.2.0.26134 Linux archive was downloaded and its vendor payload extracted under ignored build/nsight-2026.2/installation, without running package-maintenance scripts or changing the installed driver. The basic hardware harness passes on both 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608, with matching tools, all five exports, fresh targets, and pins after restart. docs/NSIGHT_VALIDATION.md identifies the exact evidence and remaining gaps. At 0.2.1, all nine basic/advanced workload pairs pass on both releases: 54 fresh captures with all five exports, cleanup, repeats/variant differences, and pins after restart. The batch is pinned as bundle-a4bce77c1a8d615412702da25e33fa75. A separate typed query matrix passes on all 54 captures and is pinned as bundle-a1af3b8bb46feebabcc66a3bd821ab80. At 0.2.2, optional SDK capture is qualified for the basic workload on matching SDK 0.9.2/0.9.0 toolchains: 12 SDK captures plus 6 default regressions, with independent decoded frame correspondence; complete matrix bundle-7b6c2aa712deb108b66af94d86b50e75 is pinned. At 0.2.3, the separate basic generated-C++ MCP matrix passes six captures across the two releases, including source retrieval and pins/indexes after restart; report bundles and exact identities are in docs/NSIGHT_VALIDATION.md. The later 24 advanced generated-C++ captures pass and fixed-capture resource/source comparisons are reviewed; snapshot bundle-cb069965dca0cc90e2b672194195beb5 records exact scope and unrun variants. R-006 investigation is complete. At 0.2.4, the basic shader repair passes six fresh captures on both matching releases with exact image comparison and retained source/build evidence in docs/SHADER_REPAIR.md. At 0.2.8, the combined/standalone postpass C++ source repair passes both releases with 12 fresh captures, exact repaired/reference RGB equality, and separately matched application baselines (docs/SOURCE_REPAIR.md). At 0.2.9, binding and pipeline-state repairs also pass the two-release matrix with exact image equality and retained source/build evidence (docs/STATE_REPAIR.md). At 0.2.10, the four standalone/combined resource-index and indirect-parameter repairs also pass both releases with 24 fresh captures and exact reference equality (docs/ADVANCED_REPAIR.md). At that milestone, generic bounded product resource access and standalone GPU replay remained unqualified; full workflow qualification stayed active. At 0.2.12, two-release generated-resource MCP qualification adds 43 supported captures (six fresh), 666 successful byte-range reads with distinct oracle/range-only coverage, five provenance refusals and a passing 25-check CPU aggregate. Full workflow acceptance remains open; see docs/RESOURCE_QUERIES.md. Completed 2026-09-18 in the visual first-release group at 0.3.0. Independent final acceptance confirms matching real two-release uninstrumented capture, optional basic SDK control, evidence queries and all nine actual source repairs. The consolidated versioned pass/limitation matrix is docs/VISUAL_RELEASE.md; no cross-version replay or additional host coverage is inferred. Audit and pinned manifest identities are retained in bundle-49217a19ae75f41fbe607152bbbd3699.
```

### R-014

```text
Status: Done
Area: docs/build/platform/test
Title: Validate and document Linux source installation
Goal: Let another Linux user build the project from source and reproduce the supported first-release visual-debugging workflow without workstation-specific assumptions.
Scope: Clone/submodule initialization and local configure/build/CTest instructions, compiler/build-tool and runtime prerequisites, Nsight discovery/overrides, Codex stdio configuration, fixture launch/capture examples, retention/pinning behavior, troubleshooting, and R-015's tested matrix for at least two Nsight releases with explicit capability limits.
Acceptance: The documented source-build procedure succeeds in a clean build environment using only declared prerequisites and initialized source dependencies; configure/build require no dependency downloads; documented runtime dependencies match binary inspection and the static-vendored-library policy; on the recorded compatible desktop/GPU setup, the instructions lead from a built server and fixture through capture, diagnosis, source repair, rebuild, recapture, and comparison; readers can locate known gaps and investigation evidence.
Notes: First-release source distribution for other Linux users was selected in round 9. Depends on R-013, R-015's local two-release validation, and the completed first-release MCP, artifact, capture, and visual-diagnosis work (R-003, R-012, R-011, R-001, R-002, R-005, R-010, R-006, R-007). Clean build validation and real GPU workflow validation are distinct checks; do not claim untested distro/driver coverage. Prebuilt release publishing, fully headless operation, and CI setup are outside this item. Completed 2026-09-18 in the visual first-release group at 0.3.0. docs/INSTALL.md gives the exercised source-build-to-repair walkthrough. A separate clone of 15df044 (0.2.12), exact source submodules and no copied outputs/cache passes network-isolated configure/build and all 25 CPU checks, including linkage. On the recorded 2026.3 desktop/GPU stack, three fresh captures, three experiment-runner baselines and three module byte correlations verify the actual one-line shader edit, rebuild and exact repaired/reference match. A real Codex 0.154.0 two-tool smoke verifies the clean stdio build. Fresh-context review independently checks 510 payloads, build/source identities, images and nine pins; final qualification publication verifies 121 files and 902 referenced pins. Complete snapshot bundle-49217a19ae75f41fbe607152bbbd3699 is pinned in artifacts/source-install-validation; docs/BUILD_VALIDATION.md records exact commands, source-mirror acquisition, environment and coverage limits. This is a clean checkout on the existing host, not a new distribution image. No public release upload or prebuilt package is claimed.
```


### R-006

```text
Status: Done
Area: analysis/replay/shaders/artifacts
Title: Establish the detailed capture-inspection interface
Goal: Determine and demonstrate which pipeline, shader, and resource evidence can be retrieved programmatically for the visual-debugging workflow.
Scope: Representative R-005 and R-010 captures, documented Nsight interfaces and real export schemas, event/object identity, draw-to-pipeline and shader association, bindings, and selected resource contents; probes and a capability matrix for R-015's selected Nsight releases, distinguishing native capture data, application-provided evidence, and unavailable information; a record of every failed integration/extraction attempt in docs/INVESTIGATIONS.md. Cover pass relationships, bindless resource selection, and indirect draw evidence where documented interfaces expose them.
Acceptance: Each required evidence category has a reproducible extraction example or a demonstrated gap tied to the tested backend/version; a minimal extraction path is exercised on a correct/faulty visual pair; each failed attempt records its documented entry point, reproduction steps, expected/observed results, evidence, and revisit condition; unresolved gaps and the chosen next action are recorded before dependent diagnostic features are claimed complete.
Notes: Depends on R-001 and R-005; initial probes can start with basic fixtures, then the matrix must cover R-010 before release. Store evidence through R-012 and explicitly pin bundles referenced by investigation records. Round 5 limits integration to documented Nsight interfaces and accepts explicit limitations; do not add private RPC, GUI automation, reverse-engineered formats, or a complementary backend to bypass a gap. Detailed state must not be inferred from function names or object counts alone. A documented gap completes investigation of that category, not the missing inspection capability needed by R-007; insufficient evidence leaves the dependent capability incomplete. Early work on 2026-09-18 inspects a pinned correct/faulty pair from Nsight 2026.3.1.0 and adds bounded metadata/event/object parsers with sanitized regression inputs. docs/INSPECTION.md records actual inventory schemas and missing state; the integrated parser CPU check and independent review pass. Typed MCP metadata/event/object queries now pass integrated CPU checks, a fresh independent review, and actual retained-capture pagination/restart queries on the pinned correct/faulty pair; validation report bundle-e8bedf549310ff13da710678546d0a9f is pinned. The C++ capture connection timeout and GPU replay initialization stall are retained in investigations I-006/I-007. At 0.2.1, two exact producer profiles and optional opaque indirect_index pass focused parser/inspection/MCP checks and fresh-context reviews. All 54 basic/advanced captures from matching 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 tools pass typed queries, exact raw-field comparison, and metadata after restart; report bundle-a1af3b8bb46feebabcc66a3bd821ab80 is pinned. docs/INSPECTION.md records the per-workload inventories and remaining state gaps on both producers. I-013 retains the initial 2026.2 C++ capture connection failure and later successful diagnostic/ordinary repeats with the same frozen inputs. Pinned bundle-cd9faf11bccbc42e3da46f1275b39073 supplies generated draw/pipeline/shader/pass evidence; the restricted generated-helper reader extracts shader bytes matching the application and is pinned as bundle-44c491ce96fef4b2b53817c5ee677c93. Both generated screenshots match the frame-2 baseline. The initial connection failure cause remains unresolved. Both releases now pass experimental correct/faulty image and shader-byte comparisons, pinned in bundle-4d0ca74d88003db698ae2dca0b952e11. Product 0.2.3 implements separate capture_cpp source/data retention, with all six basic MCP captures, source reads, repeat/fault differences, and pin/index restart checks passing on the matching releases; pinned reports are bundle-d2579d201899eba43073dfdcecf2387f and bundle-8287081bf6c6f039ced6341fd9aa59e4. The 20-check CPU aggregate and focused directory-correction checks pass after fresh-context review cycles; I-016 retains the real adapter failure and correction. docs/CPP_CAPTURE.md records the contract and limits. Completed 2026-09-18 at 0.2.3 after independent acceptance audit: 30 product C++ captures cover basic shader, standalone multipass/bindless/indirect, and combined pass-defect pairs across both releases. Pinned advanced snapshot bundle-cb069965dca0cc90e2b672194195beb5 retains the exact matrix and combined resource comparison; independent review verifies 52 resource lengths/hashes and 16 shader matches. docs/INSPECTION.md consolidates each required evidence category, demonstrated limits, and chosen next actions. Descriptor array-slot/offset/range interpretation, arbitrary after-event contents, generic typed deep-state APIs, and actual source repair remain unimplemented. These explicit gaps complete the investigation category, not R-007 functionality. Next R-007 work begins with bounded image comparison and a source-isolated shader-calculation edit/rebuild/recapture case, then extends to every basic/advanced defect; descriptor helper-contract validation precedes claims requiring slot mapping. No minor increment: the complete visual-workflow group remains unfinished.
```


### R-002

```text
Status: Done
Area: capture/platform
Title: Add optional application-side capture controls
Goal: Give applications whose source is available precise native capture control at meaningful workload boundaries while preserving the unmodified-application path.
Scope: A C++ integration with the NGFX SDK, initialization before Vulkan instance creation, explicit workload boundaries, and per-launch capture configuration/result reporting so the server can request and observe a capture within a fresh application invocation.
Acceptance: A newly launched instrumented graphics fixture captures a chosen visual workload boundary and returns a usable artifact through the job lifecycle; a second request uses a fresh instance; the unmodified capture path continues to work without this integration.
Notes: Accepted application mode and native integration priority in rounds 1–2. Depends on R-003, R-005, R-011, R-012, and the capture contract also used by R-001. Add an instrumented fixture mode while retaining a run without NGFX integration. Round 7 makes a reusable application control session unnecessary for this release; choose a per-launch configuration/result mechanism and validate SDK synchronization. Round 6 moves compute-without-presentation validation to R-008. Moved into implementation after R-010 completion. The first slice uses documented SDK initialization before Vulkan and explicit frame boundaries inside each fresh CLI-launched fixture, selected by per-launch arguments; it does not self-inject or create a reusable application session. Optional SDK source builds use explicitly selected, fingerprinted headers from the matching separately installed Nsight toolchain. Shared capture requests select present or graphics_capture_api delimiters, retaining the request separately from application-reported SDK observations. Completed 2026-09-18 at 0.2.2 after fresh-context implementation and acceptance reviews, corrections, the default GCC Debug 20-check aggregate, and focused GCC/Clang fixture checks. The C++ MCP harness passes 12 SDK-controlled basic captures and 6 default-mode regressions on matching Nsight 2026.3.1.0/build 38722833 with SDK 0.9.2 and 2026.2.0.0/build 37991608 with SDK 0.9.0. Each request starts a fresh target; all five exports, cleanup, exact application/build/SDK observations, and pins after restart pass. Independent RGB comparison confirms two distinct chosen workload boundaries against separate application readbacks. Complete matrix/source/input/comparison snapshot bundle-7b6c2aa712deb108b66af94d86b50e75 is pinned; docs/SDK_CONTROL.md records the exact scope and importer/payload identities. I-012 preserves the corrected hardware-harness failure. This qualifies the selected basic windowed workload, not advanced SDK scenarios, headless compute, detailed state, or source repair.
```

### R-010

```text
Status: Done
Area: test/platform/shaders
Title: Add multipass, bindless, and indirect rendering scenarios
Goal: Make the development application representative of the advanced visual workloads selected for the first release while retaining reproducible defect isolation.
Scope: Extend R-005 with multiple passes, offscreen render targets, post-processing, bindless resource access, and indirect draws; selectable feature/scenario configurations, a combined workload, correct references, and controlled visual defects involving pass output, resource selection, or indirect draw parameters.
Acceptance: Each feature can be exercised in a reproducible scenario and the combined workload runs on the validated development configuration; correct and faulty outputs have documented references/tolerances; requested feature support is checked explicitly; the configuration and required API features accompany each run; capture compatibility and inspection gaps are recorded through R-001/R-006 without silently substituting simpler scenarios.
Notes: Required first-release renderer coverage selected in interview round 6; depends on R-005. Basic fixtures may enable earlier capture experiments, but they do not satisfy this item's advanced coverage. Exact Vulkan feature requirements and explicit support checks are recorded in docs/FIXTURE.md. Fixtures should use valid, deterministic API usage for visual defects; arbitrary out-of-bounds access or undefined behavior cannot serve as a stable reference. Standalone compute diagnosis and profiling remain later work. Implementation and fresh-context review now cover ten advanced scenarios alongside the four basic cases. The full application-readback matrix passes all 57 fresh launches with synchronization validation on the recorded GPU/driver/desktop, deterministic repeats, and the independent oracle. Complete evidence is pinned as bundle-a5e2bf5a70cfdff49bb8705afff9aba5; docs/FIXTURE.md records exact commands and feature requirements. Completed 2026-09-18 at 0.2.1 after all nine basic/advanced workload pairs pass capture/export on matching Nsight 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 tools, totaling 54 fresh captures. Capture batch bundle-a4bce77c1a8d615412702da25e33fa75 and typed query report bundle-a1af3b8bb46feebabcc66a3bd821ab80 are pinned; docs/NSIGHT_VALIDATION.md and docs/INSPECTION.md record exact compatibility, per-workload inventories, and missing detailed state. Workload/profile/indirect-field changes pass focused CPU checks and fresh-context reviews. This completes the fixture item, not the larger visual workflow group or source repair.
```

### R-012

```text
Status: Done
Area: artifacts/core/mcp/test
Title: Store capture evidence with configurable retention and pinning
Goal: Keep capture evidence inspectable across server sessions while automatically managing eligible artifact storage and preserving explicitly retained evidence.
Scope: Server-managed artifact bundles with stable IDs, raw captures/exports/logs/reports, provenance manifests and derived indexes; staging and atomic publication on the destination filesystem, explicit failed-attempt bundles, and interrupted-publication recovery; configurable maximum age and storage budget, oldest-eligible-first pruning, persistent pin/unpin state, temporary usage leases for active readers/writers, storage-usage reporting, and small records explaining expired references. Coordinate protection checks with deletion claims and prune whole bundles. Expose bounded artifact queries and retention controls through MCP.
Acceptance: Artifacts and pin state remain discoverable after restart; readers never observe partial bundles as complete, interrupted publication is reconciled, and failed attempts retain available evidence with an explicit failure status; fake-clock/filesystem tests demonstrate age and budget pruning of completed unpinned bundles, protection of pinned/in-use data including a pin/lease acquired while pruning is choosing candidates, and cleanup confined to managed artifacts; related files are retained or expired coherently; expired references report their status; quota exhaustion or disk-full failures are actionable without deleting protected data; a before/after capture comparison and an investigation entry can explicitly pin and retrieve their evidence bundles.
Notes: Retention policy selected in round 8. This is a first-release prerequisite shared by R-011, R-001, R-002, R-006, and R-007; core storage tests do not need a GPU and R-003 supplies the MCP adapter. Pinning exempts data from automatic pruning; references alone do not. Pinned bytes still count toward reported usage, so a storage budget is not a guarantee when protected data exceeds it. Choose and document default limits and age semantics after measuring fixture artifacts. Index format and exact cleanup scheduling remain implementation choices; this item does not require SQLite. Completed 2026-09-18 in the R-012/R-011/R-001 capture/evidence group at 0.2.0. JSON manifests/directories, atomic publication and recovery, protected pin/lease retention, quota errors, and expiration records have CPU coverage. Twenty integrated CPU checks pass. All six real basic captures and their two baseline/report pairs are pinned and retrievable after restart; I-005 retains a pinned comparison and investigations retain failed evidence. The selected defaults are 2 GiB/30 days, with measured basic sizes and limitations in docs/ARTIFACTS.md. Independent acceptance review found no blocking defect.
```

### R-011

```text
Status: Done
Area: core/platform/mcp/test
Title: Manage one application launch per capture job
Goal: Give each capture a fresh, owned application instance and a predictable lifecycle without leaving job processes running after completion or cancellation.
Scope: Job IDs and state transitions, executable/argument/working-directory inputs, process ownership, captured subprocess logs, configurable deadlines, cancellation, and bounded cleanup on success, failure, or server shutdown; a single coordinator applies worker completions after checking job/capture/attempt identity and expected state. Add test-only C++ command-line stand-ins at the real process boundary, controlling output streams, exits, files, delays, and child processes. Expose job status and cancellation through the MCP adapter.
Acceptance: Consecutive capture jobs use separate application instances and separately identified outputs; executable stand-ins exercise launch failure, early exit, malformed/missing exports, output-stream handling, hangs, timeout, cancellation, and shutdown without requiring a GPU or Nsight; stale completions cannot revive terminal jobs or cross capture identities; success/cancellation/timeout races resolve once, and GPU reservations remain held until owned-process cleanup is confirmed; owned child processes are cleaned up while unrelated processes remain untouched; a real Nsight capture exercises the same lifecycle; subprocess output stays separate from MCP protocol stdout.
Notes: First-release lifecycle selected in round 7. Core process handling is independent of MCP; R-003 exposes it and R-001/R-002 use it. Use R-012 for output bundles and protect artifacts while a job is writing or reading them. Real capture acceptance is checked with those integrations and R-005. Validate Nsight launcher/target process behavior before choosing the cleanup mechanism. This work does not introduce application-session reuse or attachment to existing processes. Completed 2026-09-18 in the R-012/R-011/R-001 capture/evidence group at 0.2.0. Typed coordinator completions, process ownership, deadlines/cancellation/shutdown, stale identity rejection, GPU reservations through cleanup, and protocol-output isolation are covered by the passing 20-check CPU aggregate. Both real basic hardware matrices demonstrate three distinct fresh targets, separate bundles, confirmed cleanup, released reservations, and normal MCP shutdown. The independent acceptance audit found no blocking defect; docs/JOBS.md and docs/NSIGHT_VALIDATION.md record scope and evidence.
```

### R-001

```text
Status: Done
Area: capture/platform
Title: Capture unmodified Vulkan applications
Goal: Let agents capture supported Vulkan workloads without requiring changes to the application's source or an application-side integration.
Scope: A fresh application launch for each capture through Nsight's documented interface, with executable, arguments, working directory, supported capture triggers, and a managed output destination; expose capture results and useful failure information through R-011's job lifecycle.
Acceptance: A reproducible Vulkan application produces a saved capture without source changes; the matching replayer reads its metadata; repeating the request launches a new application instance and produces a separate capture; launch/capture failures are reported without claiming success; applications lacking a usable capture delimiter receive an explicit limitation.
Notes: Accepted application mode in round 2 and fresh-launch lifecycle in round 7. Depends on R-003 for MCP access, R-011's process supervisor, and R-012's artifact store; use R-005 with NGFX integration disabled as the primary fixture and extend capture validation to R-010 before release. Round 8 requires an existing desktop session; verify its display prerequisites and actual window-system path. Exact API features remain to validate. Existing-process attachment, application-session reuse, and fully headless operation are outside first-release scope; compatibility with every Vulkan feature is not promised. Completed 2026-09-18 in the R-012/R-011/R-001 capture/evidence group at 0.2.0. The uninstrumented fixture is captured through the shared C++ service and MCP on 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608. Each matching-tool matrix passes reference/repeat/faulty launches, all five exports, metadata readability, cleanup, and retained evidence access. CPU stand-ins cover failures. The presented-frame delimiter requirement is explicit. Independent acceptance review passes. See docs/CAPTURE_VALIDATION.md and docs/NSIGHT_VALIDATION.md. At the 0.2.0 milestone, GPU replay and advanced capture compatibility remained separate unfinished work; the 0.2.1 R-010 matrix subsequently qualifies the selected advanced capture/export workloads on both releases. GPU replay remains unqualified.
```

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
