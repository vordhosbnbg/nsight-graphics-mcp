# Capability follow-up completion audit

Scope: the owner's **2026-10-15** implementation request for the complete
[A–E plan](CAPABILITY_FOLLOWUP_PLAN.md), with a commit/version increment after
each finished capability. The **R-018–R-022 group** is assigned **0.5.0** under
the existing minor-version rule. This is a conditional capability milestone;
full effective GPU state and isolated-draw counters remain unavailable.

Final verification: **passed** — 36 CPU checks, 691 actual schema-valid tool
results, 10 expected tool-error refusals, 28 distinct recorded draws, 31 pinned
new read artifacts, 14 earlier qualification/failure snapshots and 87 native
bundles. The table below traces the
original requirements to code, executed checks and retained evidence. Historical
qualification keeps its actual product/tool identity; a final offline query
does not relabel an old GPU collection.

## Requirement audit

| Requirement | Implementation or accepted conditional outcome | Authoritative evidence |
| --- | --- | --- |
| A: reuse the existing matching reader | Existing five-file helper closure and byte API; no hydration extension or raised bounds. | RESOURCE_QUERIES.md, RESOURCE_WORKER.md, pinned 0.4.2 worker/helper fingerprints; final confinement/resource checks. |
| A: exact bytes, ranges and refusals | The listed handle-22 reference yields the independent 2,700-byte shader hash; whole/two split/end reads agree. Wrong references/profiles refuse. | `ngm_retained_resource_integration`, `ngm_resource_read_check`, `ngm_resource_reference_check`; final 0.5.0 retained run and original 0.4.2 qualification. |
| A: provenance, restart and activation | Producer/source/database/helper/worker identities and source/read pins survive restart. Stable optional worker remains in Codex launch arguments. Discovery is configuration evidence; actual confinement/read execution is checked separately. | RESOURCE_QUERIES.md; current operator configuration/hash observation; two-process retained verifier and `ngm_worker_confinement_check`. |
| B: bounded shared launch input | Eight-key allowlist, 4 KiB/value, sixteen path entries, 16 KiB total; ICD/library identity validation and conflicting-selector rejection. Same API/CLI validator. | LaunchEnvironment.cpp, CaptureService.cpp, capture/Main.cpp, Workflow.cpp; `ngm_launch_environment_check`, CLI checks. |
| B: propagation/isolation/lifecycle | Overlay reaches discovery/graphics/C++/profile/export, freezes at submission, precedes HOME/XDG/temp isolation and preserves parent/other-job/reader environments. Queue mutation/deadline/cancel/shutdown checks pass. | `ngm_capture_service_check`, `ngm_nsight_check`, `ngm_profile_workflow_check`, `ngm_mcp_check`, `ngm_http_check`; LAUNCH_ENVIRONMENT.md. |
| B: actual runtime qualification | Fresh matching-tool C++ captures and GPU Trace profiles work on the recorded XCB/Xwayland path. I-038 graphics-capture timeouts stay explicitly unqualified. SDL Wayland remains a separate future target. | 0.4.3 failed probes, 0.4.4 C++ probe, 0.4.5–0.4.7 native state captures and 0.4.8/0.4.9/0.4.11 profiles in BUILD_VALIDATION.md. |
| C: header-only compatibility | Qualified empty event/regime tables preserve exact headers and zero rows; all 146 retained frame metrics stay usable. Empty/malformed/ragged/nonfinite timing data still fails. | ProfileEvidence.cpp; `ngm_profile_evidence_check`, `ngm_profile_workflow_check`; sanitized fixtures and 0.4.4 qualification. |
| C: bounded consumers/restart | Header paging/escaping remains bounded; metadata coverage is explicit; empty comparisons refuse instead of inventing zero. | ProfileInspection.cpp; `ngm_profile_compare_check`, `ngm_mcp_check`, process-boundary/restart receipts. |
| D: documented backend/identity | Separate pure state interpreter and inspection adapter use capture-local generated submission references. Existing source relationships keep their contract. No inventory-ID or cross-capture numeric join is made. | DrawState.cpp, Inspection.cpp; `capture_draw_executions`/`capture_draw_state`, CppEvidence checks and DRAW_STATE.md. |
| D: pipeline/shaders/raster | Qualified direct/indexed recorded pipeline/stage, depth/culling and viewport/scissor fields retain source spans/hashes; static/dynamic disagreement and setter lifetime are checked. Shader bytes match separate application oracles. | Four native 0.4.5 captures, eight shader reads; final C++ `--verify-mcp`; `ngm_draw_state_check`. |
| D: push constants | Per-stage ordered partial writes and layout compatibility; opaque bytes resolve only on explicit request through the confined reader with one shared deadline. Missing/unread/unsupported data stays unavailable. | Nine native 0.4.6 occurrences; final C++ `--verify-push`; `ngm_push_state_check`, read/provenance checks. |
| D: descriptors/dynamic offsets | Literal allocation/layout/bind identities, prefix/push-range compatibility, disturbance and array-element offset mappings. Partial-write witnesses do not claim descriptor contents. | Fifteen native 0.4.7 occurrences; final C++ `--verify-descriptors`; `ngm_descriptor_state_check`. |
| D: lifecycle/refusal coverage | Unknown relevant calls invalidate coverage; foreign/stale refs, duplicate annotations, unsubmitted recordings, unsupported creation forms, source/payload mutation, indirect/secondary/multipart limits refuse explicitly. | DrawState/CppEvidence/CppInspection/Inspection/resource check suites, recorded native witnesses and restart transcripts. |
| D: remaining state/value feasibility | Descriptor contents, execution-time uniform/material values, actual shader access and general captured GPU correspondence remain unavailable. Full effective-state and correspondence flags remain false. No extra proprietary helper executes. | DRAW_STATE_FEASIBILITY.md, I-039, two-producer/fresh-input 56-identity audit and current result/schema checks. |
| D: provenance/bounds/qualification | Bounded capture-local pages, leases, field reasons and hashes remain explicit. New state support targets the measured 2026.3 producer; unsupported 2026.2 cases refuse. | DRAW_STATE.md, final CPU suite and retained 0.5.0 state/resource verifier; no new two-release claim. |
| E: controls and unavailable cells | Nullable per-action/HES requests, documented help gates and exported-setting verification; GB20x activation remains unqualified. Only qualified lowercase regime `nan` becomes unavailable; comparison never silently drops it. | PROFILE_CONTROLS.md; `ngm_profile_evidence_check`, `ngm_nsight_check`, `ngm_profile_workflow_check`, `ngm_profile_compare_check`; two fresh 0.4.8 profiles and CPU old-request compatibility. |
| E: actual export probes | Direct/indexed/reused workloads, repeated/nested markers, frame/submit intervals and timing off/on are exercised before query design. | Fifteen native 0.4.9 profiles, six baselines and 400 independent timestamped API submissions; PROFILE_CORRELATION.md. |
| E: correlated duration/identity | Unique exact same-profile paths qualify exported marker ranges in ms. Ambiguous paths, missing keys and wider collection shapes refuse; original queue/submission/action/draw identities stay null. | ProfileMarkers.cpp/ProfileInspection.cpp; PROFILE_MARKERS.md, pure/workflow checks, 360 retained marker responses and HTTP parity; final 0.5.0 reader checks. |
| E: timing sanity/overhead | Independent whole-scene timestamps use declared max(0.025 ms, 25%) tolerance. Four alternate-order fresh pairs after 30 warmup presents retain every observation; causal correction remains unresolved. | 0.4.9 and 0.4.11 numerical audits, PROFILE_OVERHEAD.md, I-044, 92 telemetry probes; no correction in product results. |
| E: counters/scopes/units | Raw frame/regime membership and exact cells are verified; physical normalization, application isolation and fragment/draw attribution remain unavailable. Null units are unknown scaling; no frame-to-draw division. | PROFILE_SCOPE_FEASIBILITY.md, I-043/I-045, all 1,168 frame cells and 80 selected raw facts; exact-name/position/hash audits. |
| E: conditional outcome/releases | Wider per-draw/physical claims receive a retained limitation outcome. New timing/control/state qualification remains 2026.3-only; old two-release evidence keeps its original scope. | PROFILE_SCOPE_FEASIBILITY.md and the versioned qualification records below. |
| Shared evidence/process invariants | Fresh owned launches, serialized jobs, bounded cleanup, managed atomic publication, coordinated leases/pins, restart and fail-closed hashes remain intact. Every failed/inconclusive integration is retained. | All 36 CPU checks; I-037–I-045, original native pins and final retention/hash audit. |
| Build/delivery boundaries | C++20/CMake/Ninja and seven exact submodule pins remain unchanged; static vendored linkage and staged installation pass. No dependency download, proprietary closure extension, machine-setting or game modification is introduced. | DEPENDENCIES.md, TECH_STACK.md, final linkage/install checks, submodule comparison and source diff. |
| Versioning/commits | Capability commits increment 0.4.2 through 0.4.11; documentation-only feasibility closure retains 0.4.7. Completing R-018–R-022 increments MINOR once to 0.5.0, resetting PATCH. Protocol/artifact/Nsight versions remain separate. | Git commit history, top-level CMake project version, shared version/CLI/MCP/manifest checks and the completing commit. |

The conditional rows implement the plan's specified fallback assessments; they
do not count unavailable fields as functional capabilities. Future game runs,
SDL/native Wayland qualification, additional hardware/producers, live shader
debugging, native trace decoding and a wider hydration worker are not deliverables
of this plan. No new priority is inferred from an empty roadmap.

## Qualification references

| Step/slice | Complete pinned qualification |
| --- | --- |
| A, reader 0.4.2 | `bundle-133f94fc5a57cc411bc5eb00a8f1a5b6` |
| B, launch 0.4.3 | `bundle-2a69ab0ef0bd54fd2df1fdeadc6fc423` |
| C, empty tables 0.4.4 | `bundle-2051be7525227bb35f9e959f9d96c9ca` |
| D, raster/shaders 0.4.5 | `bundle-556efc8c7f65c8ebdb96e360197fc015` |
| D, push bytes 0.4.6 | `bundle-754e06a3afa984d58e2d073bac3b590f` |
| D, descriptor bindings 0.4.7 | `bundle-b59b7343be911eec0e49b85ffe5767ef` |
| D, conditional feasibility | `bundle-7269c28d30326b8f1fa3ef64ebd4ed36` |
| E, controls 0.4.8 | `bundle-193d06c97b109c55fb8a30e35f12b8f9` |
| E, timing matrix 0.4.9 | `bundle-40d1c8f983a49246a26d5294f20636bb` |
| E, marker query 0.4.10 | `bundle-b8d3515b5cd6079e2ba2c8cb5637067e` |
| E, repeated assessment 0.4.11 | `bundle-8d667f47f660fc46a582f440ee786c5e` |
| Final 0.5.0 audit/reader | `bundle-bc107b133c527aa81f11c17fa93c9743` |

Native source captures, profiles and important read results are independently
pinned, with their exact stores/IDs in BUILD_VALIDATION.md and the final audit
inventory. Generic import manifests identify the installed 0.4.1 importer;
payloads identify the tested server, collection, helper and source versions.

Final checks and exact commands are recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
The installed server remains **0.4.1**; completion of the source implementation
does not install or replace the running MCP connection. The optional verified
0.4.2 worker configuration is preserved for new operator server processes.
