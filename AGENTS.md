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

The source is **0.5.9**, with **34 tools** shared by stdio and HTTP. Read
[docs/ROADMAP.md](docs/ROADMAP.md) for current accepted work; keep this file
focused on working rules rather than a release-by-release evidence ledger.

- R-023 fresh capture reliability, R-027 standalone replay and R-025 draw timing
  association are complete within their documented matching 2026.3 scope.
- R-024's bounded `capture_draw_values` source-visible uniform model passes the
  actual descriptor-source edit/rebuild/recapture, independent value/image
  checks, two-restart pin checks and all **51 CPU checks**. It reconstructs
  supported generated-source values; GPU invocation/register observation and
  numeric Graphics Capture event correspondence remain unavailable. See
  [docs/DESCRIPTOR_WRITES.md](docs/DESCRIPTOR_WRITES.md).
- R-026 remains blocked on documented Graphics counter definitions and
  application scope. Native multi-pass collection exports more raw cells but
  does not establish their physical interpretation or application attribution.
  On 2026-10-16 the owner explicitly chose **Nsight Graphics only** and declined
  the separate Perf SDK proposal. Do not evaluate or integrate that backend.
- On 2026-10-16 the owner rejected performative evidence archival. Routine
  validation outputs are temporary. Do not make archival, central import,
  permanent pins or storage-budget increases completion requirements. The
  proposed 2 GiB to 4 GiB archival budget increase is withdrawn.
- The installed server remains **0.4.1** with the separately installed **0.4.2**
  resource worker. Development binaries and installed configuration are distinct;
  cleanup must preserve configured executables and genuine build prerequisites.
- The optional descriptor worker has the owner-approved seven-file closure:
  the five resource helpers plus `VulkanStructHydrator.cpp/.h`, with confined
  execution and recorded bounds instrumentation. No generated replay/application
  code or wider proprietary closure is authorized.

The historical visual release qualifies all nine fault-repair scenarios on
matching Nsight **2026.3.1.0/build 38722833** and
**2026.2.0.0/build 37991608**, with optional matching SDK **0.9.2/0.9.0**.
Compute correctness, performance analysis, loopback HTTP and Arch packaging have
their own measured scopes in [docs/COMPUTE.md](docs/COMPUTE.md),
[docs/PERFORMANCE.md](docs/PERFORMANCE.md), [docs/HTTP.md](docs/HTTP.md) and
[docs/ARCH_PACKAGE.md](docs/ARCH_PACKAGE.md). Later 2026.3-only results do not
extend those older matrices. Validation summaries and reproducible commands
remain in docs; transient run directories may have been cleaned up.

The development host has an RTX **3080 Ti** and matching **615.71.09** kernel
runtime, KDE Wayland/Xwayland, and separately installed Nsight 2026.3 under
`/opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3`. A process-local matching
library/ICD overlay is used where needed; it does not change the installed
system driver. Discover installations and support explicit path overrides.
Recheck compatibility when diagnosing problems; do not hard-code this host's
paths or GPU into runtime behavior.

## Nsight integration boundaries

- Use only documented Nsight interfaces and exports for backend integration.
  GUI automation, private RPC, reverse-engineered formats, and complementary
  backends such as RenderDoc are outside the selected scope. Accept and report
  documented-interface limitations instead of silently substituting another path.
- Record failed Nsight integration or extraction attempts concisely in
  [docs/INVESTIGATIONS.md](docs/INVESTIGATIONS.md): reproduction steps, relevant
  versions, expected/observed results and a concrete reason to revisit. Preserve
  inconclusive results as inconclusive. Routine build typos and private harness
  setup mistakes do not need permanent investigation entries or archived logs.
  A failed probe does not establish that a capability is universally unavailable.
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

- **Validation artifacts are temporary by default.** After completing a check,
  retain a concise record of the command, relevant versions/configuration and
  result in the existing documentation; clean up generated captures, traces,
  copied binaries, logs, scratch scripts and run directories.
- **Do not perform evidence archival for its own sake.** Do not create lossless
  snapshots, tarballs of working trees, exhaustive payload inventories, frozen
  executable collections or archive-verification programs merely to prove that
  validation happened. Do not duplicate completed runs into a central store.
- Completion depends on working behavior and meaningful validation. Archival,
  publication of validation packages, central imports, permanent pins and
  artifact-budget increases are not acceptance gates. Test persistence/pin
  behavior with temporary test data and clean it up afterward.
- Retain a minimized artifact only when it has a concrete ongoing purpose: an
  input used by an automated regression test, a difficult unresolved bug, or an
  explicit user request. State that purpose and when the data can be removed.
  A successful validation run or a documentation reference alone is not a reason
  for indefinite retention. Prefer small sanitized fixtures under tests/fixtures.
- Product captures, traces and exports stay on disk while the user or an active
  workflow needs them; return bounded structured results, pagination, previews
  and artifact references. Keep raw data and derived indexes separate. Record
  tool/build/shader identities, capture settings, GPU/driver and units where
  needed for interpretation, without creating a parallel archival project.
- Apply configurable age/storage limits to completed, unpinned managed bundles.
  Preserve explicit user pins and data in active use. Release development-only
  pins after validation. Do not increase a quota to accommodate disposable
  development history; clean up that history instead. Preserve unrelated user
  captures, installed executables, dependency sources and working toolchains.
- Assemble product bundles in unique staging directories, validate required
  outputs and publish atomically. Partial attempts must remain visibly failed.
  Keep failure output for current diagnosis, subject to the same temporary
  retention policy; reconcile interrupted publication on restart.
- Use temporary leases for active writers, inspection and comparison. Check
  leases/pins under the same coordination used to claim a bundle for deletion;
  prune whole bundles and perform slow deletion after releasing the protection
  lock. Use store APIs for a live managed store; do not unlink its files directly.
  Keep small expiration records so old references explain what happened.
- Scope event/object IDs to their capture. Keep measured facts distinct from
  inferred diagnoses. Use reproducible inputs/debug labels and source/SPIR-V
  correlation during diagnosis. Compare equivalent workloads after warmup with
  repeated measurements; distinguish debug and performance builds and replay
  overhead.
- Keep generated captures, proprietary binaries, application data, credentials
  and large logs out of version control. Historical run IDs and pin statements
  describe the original validation, not a promise that data is still retained.
  When cleaning up, note that fact in the relevant existing validation summary;
  do not create another cleanup archive or ledger.

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
   Record relevant tool/SDK, GPU/driver, desktop, build and shader identities
   plus pass/fail/unsupported/skipped outcomes in a concise validation summary.
   Clean up run data after validation under the temporary-artifact policy above.
   Compare equivalent inputs and frames within declared tolerances. Reuse this
   runner for the source-edit/rebuild/recapture workflow, keeping edits/builds in
   normal development tools. Keep expected diagnoses private to the harness and
   label Nsight exports and application readback as distinct evidence sources.

The first end-to-end success criterion is an agent identifying a deliberately
introduced visual defect, editing the responsible shader/source, rebuilding,
recapturing, and verifying the fix against the correct reference. Keep the
before/after data through comparison, record build identities/results, then
clean up temporary outputs. Compute correctness and repeatable
performance measurements follow. Live shader breakpoints, stepping, and variable
inspection are outside first-release scope.

## Primary references

- [Capture and replay CLI](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
- [GPU Trace](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html)
- [NGFX SDK](https://docs.nvidia.com/nsight-graphics/UserGuide/sdk.html)
- [Shader debugger setup](https://docs.nvidia.com/nsight-graphics/UserGuide/shader-debugger-setup.html)
- [NVIDIA SIGGRAPH MCP preview description](https://www.nvidia.com/en-us/events/siggraph/)
- [Official MCP Python SDK](https://github.com/modelcontextprotocol/python-sdk)
