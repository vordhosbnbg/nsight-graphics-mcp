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

At initialization on 2026-09-17, this repository had no implementation, dependency
manifest, or test suite. R-013 now supplies CMake/Ninja presets, pinned source
dependencies, a source-built glslang compiler, CPU-only checks, and server/fixture
entry points with version/help reporting. See [README.md](README.md) and
[docs/BUILD_VALIDATION.md](docs/BUILD_VALIDATION.md) for verified commands and limits.
R-003/R-005 complete the first build/basic-fixture group at version 0.1.0 with a
real Codex capability query, a windowed Vulkan fixture, and an isolated C++
experiment runner. Fresh-context reviews, CPU checks, and local GPU validation
are recorded in docs/BUILD_VALIDATION.md, docs/MCP.md, and docs/FIXTURE.md.
The R-012/R-011/R-001 capture/evidence group completes at **0.2.0** after
independent acceptance review. It provides the job coordinator, managed artifact
storage, documented Nsight CLI adapter, and shared `CaptureService`, used by the
native `ngm-capture` command and stdio tools. The 0.2.0 surface had **15 MCP
tools**, including bounded retained metadata/event/object queries. The integrated
GCC Debug CPU suite passes **20 checks**. Real basic MCP capture/export matrices
pass on matching Nsight **2026.3.1.0/build 38722833** and
**2026.2.0.0/build 37991608** tools, with three fresh targets per release and
persistent pins. The service preserves explicit `XDG_DATA_DIRS` and supplies
standard defaults when unset or empty. Typed queries pass on both exact producer
profiles at **0.2.1**. The basic and advanced matrix passes 54 fresh captures
across nine workload pairs on matching tools; all 54 retained captures also pass
typed metadata/event/object pagination and restart queries. The full fixture
passes 57 fresh application-readback launches with synchronization validation.
R-010 is complete with recorded advanced capture compatibility and export gaps.
R-002 completes at **0.2.2** with optional per-launch SDK control, tested matching
SDK 0.9.2/0.9.0 builds, 12 real basic SDK captures and 6 default-mode regressions
through MCP, independently decoded frame correspondence, and retained pins.
The default GCC Debug aggregate passes 20 checks; a subsequent portability
annotation passes focused fixture checks on GCC and Clang. Exact SDK evidence,
source/build identities, and limits are in docs/SDK_CONTROL.md. Detailed state
and source repair remain unfinished. At 0.2.3, the separate `capture_cpp` mode
retains generated source and resource files with a derived project index, bringing
the current surface to 16 tools. See docs/CPP_CAPTURE.md for its validated scope,
producer restrictions, and limits; it is not a typed deep-state query or replay.
R-006 completes its investigation/interface acceptance at 0.2.3 after 30 real
basic/advanced C++ captures and independently reviewed selected resource/shader
extraction on both releases. Its capability/gap/next-action matrix is in
docs/INSPECTION.md. R-007 remains active. At **0.2.4**, bounded artifact image comparison brings the
surface to 17 tools. The basic shader source edit/build/recapture case passes on
both releases, with exact repaired/reference RGB equality and retained pins; see
docs/SHADER_REPAIR.md. At **0.2.5**, PNG comparison and bounded P6/PNG/BMP
image previews bring the surface to 18 tools; docs/IMAGE_PREVIEWS.md records
retained-file validation. At **0.2.6**, numbered generated-source queries and qualified draw/pipeline/shader
relationships bring the surface to 20 tools. Retained MCP queries pass for 36
C++ captures and 48 draws on both exact profiles; see docs/CPP_INSPECTION.md.
At 0.2.7, the opt-in fixed-capture descriptor hydration experiment qualifies setup
array-element mappings for four combined captures; docs/DESCRIPTOR_HYDRATION.md
records the trusted helper/data contract. Generic descriptor/resource extraction,
executed GPU state, and the remaining visual defect families remain unfinished.
At 0.2.8, an isolated C++ source repair passes combined/standalone postpass cases
on both releases with 12 fresh captures and 12 application baselines; exact image
comparison, build/source identities, queries, pins, and restart are verified.
See docs/SOURCE_REPAIR.md. Binding, pipeline, resource-index, and indirect-parameter
repairs remain R-007 work.
Actual GPU replay times out on both releases. These results are
separate from the historical 0.1.0 Codex capability query. Exact versions,
evidence, and limits are in docs/BUILD_VALIDATION.md, docs/NSIGHT_VALIDATION.md,
docs/FIXTURE.md, docs/INSPECTION.md, and docs/INVESTIGATIONS.md. Component contracts
are in docs/ARTIFACTS.md, docs/JOBS.md, docs/NSIGHT_BACKEND.md, and docs/MCP.md.
Update these instructions as the project develops.

At 0.2.9, binding and pipeline-state C++ source repairs also pass on both releases,
with 12 fresh captures matching application baselines and exact repaired/reference
pixels. Five of nine visual defects now have verified source repairs. The four
resource-selection/indirect-parameter cases and generic bounded resource access
remain unfinished; see `docs/STATE_REPAIR.md`. The MCP surface remains 20 tools.

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
