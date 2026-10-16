# AGENTS.md

## Scope

Build a C++ MCP server for Vulkan capture, shader diagnosis, compute correctness
and GPU performance analysis using NVIDIA Nsight Graphics on Linux.
Windows support is planned for a future release; Linux is the current supported platform.

- Use documented Nsight CLI/SDK interfaces and exports only. GUI automation,
  private RPC, reverse-engineered formats and complementary backends are excluded.
  Nsight Perf SDK is outside the approved scope.
- Support applications with or without source access; instrumentation stays optional.
  Each capture launches a fresh application. Attachment and reusable target sessions
  are outside scope. Windowed capture uses an existing X11/Xwayland desktop.
- Prioritize visual correctness, then compute correctness, then performance.
  Use the repository's deterministic Vulkan fixture and GLSL/SPIR-V workloads.
- Prove behavior against actual exports. Schemas, stand-ins, object inventories
  and function names alone do not establish GPU state or tool compatibility.
- Keep measured facts distinct from inferred diagnoses. Scope IDs to their
  capture; never join runs by numeric ID or assume sampled counters are API counts.
  [COMPATIBILITY.md](docs/COMPATIBILITY.md) defines the qualified boundaries.
- Do not change drivers, display configuration, GPU clocks, profiling permissions
  or other machine settings as an implicit development step.

## Workflow and documentation

- Inspect git status and relevant code before editing; preserve unrelated work.
  Keep implementation inside this repository. Use the checked-in clang-format
  for first-party C++, with no unrelated or vendored formatting churn.
- Read [ROADMAP.md](docs/ROADMAP.md) for remaining accepted work. Preserve its
  item fields and matching status sections. Remove completed entries; Git keeps
  history. An empty roadmap does not authorize inventing priorities.
- Put technical choices in [DEVELOPMENT.md](docs/DEVELOPMENT.md). Keep user scope
  decisions distinct from implementation choices; do not reopen settled choices
  without new evidence or a changed preference.
- Documentation describes current behavior, working commands and relevant limits.
  Consolidate into the existing guides. Do not add a page per feature, calendar
  stamps, maintenance metadata, progress logs, release narratives, test-count
  ledgers or artifact inventories. Git records change history.
- Record meaningful unresolved integration findings briefly under Compatibility
  or the relevant roadmap item: reproduction, expected/observed behavior and what
  would justify revisiting it. Routine build/harness mistakes need no permanent log.
- Verify unfamiliar flags against installed help and primary documentation.
  Keep version-specific assumptions in backend adapters and report unsupported
  operations explicitly. A failed probe is not a universal limitation.

## Implementation

- Use C++20, CMake/Ninja and fastmcpp; share the core between stdio and loopback HTTP.
  Separate handlers, job coordination, storage, parsers and backend adapters.
- Long operations are asynchronous. Workers return typed completions; one
  coordinator validates job/capture/attempt identity and expected state before
  changing job state. Stale completions cannot revive terminal work.
- Serialize capture/profile/replay work on the GPU. Resolve success, cancellation
  and timeout races once; release the reservation only after owned cleanup is confirmed.
- Launch argument arrays without shell interpolation. Track process ownership
  and bound deadlines/cleanup. Never signal processes belonging to another job.
- Keep MCP stdout protocol-only; diagnostics go to stderr and child logs to files.
  HTTP uses IPv4 loopback, a private bearer-token file and one shared trusted-user workspace.
- Publish complete/failed bundles atomically from unique staging directories.
  Protect active readers/writers with leases; coordinate pin/lease checks with
  deletion claims. Reconcile interrupted publication on restart. Use store APIs
  for a live managed store, not direct file deletion.
- Return bounded queries, pagination and previews rather than large raw payloads.
  Preserve unavailable data and raw zero distinctly; refuse unknown type,
  lifetime, selection or provenance rather than inventing values.

## Build and dependencies

- Dependencies are exact-commit submodules under external/, built from source.
  Initialize explicitly with git submodule update --init --recursive. Configure
  and build must then work without downloads, prebuilt substitutions or network fallbacks.
- Build the pinned glslang compiler from source. Preserve diagnostic shader debug
  information; keep performance compilation separate and correlate source/SPIR-V hashes.
- Statically link vendored code and inspect actual binaries. Document OS/compiler,
  XCB, Vulkan-loader and driver runtime exceptions; do not claim fully static binaries.
- Use shared presets and ignored build/<preset>/ directories; local overrides
  belong in CMakeUserPresets.json. Scope warnings/options to first-party targets.
- Optional NGFX headers come from a separately installed, fingerprint-qualified
  SDK. The default build is SDK-free; never download SDK inputs during a build.
- Optional resource workers may compile only the exact five qualified files:
  ReadOnlyDatabase.cpp/.h, DataScope.cpp/.h and DllCommon.h. The descriptor worker
  may additionally compile VulkanStructHydrator.cpp/.h with the approved bounds
  instrumentation. Copy/verify inputs in isolation; retain build-required original/
  derived fingerprints and the instrumentation patch. Do not compile generated
  replay/application code, generated build scripts or a wider proprietary closure.
- Keep proprietary helper sources outside version control. Confinement is required
  before payload parsing: Landlock/seccomp failure must refuse, never fall back.

## Validation

- Run configured checks appropriate to the change. Use one relevant preset and
  focused checks; broaden for shared behavior, build/portability or release work.
  Documentation-only edits need content/link review, not an automatic full rebuild.
- Ordinary CPU checks use project-owned C++ stand-ins at real process boundaries.
  Cover malformed exports, failures, hangs, deadlines, cancellation, cleanup and
  protocol isolation. Stand-ins do not establish real Nsight/GPU support.
- Invoke hardware integration separately. Use each Nsight release's matching
  tools/SDK and record pass, fail, unsupported and skipped distinctly. Preserve
  the exact tested scope; do not imply additional release/GPU support.
- Use the C++ experiment runner with explicit scenario, seed, dimensions and frame.
  Give runs isolated configuration/output directories and a fresh application.
  Compare equivalent workloads after warmup with independent oracles/tolerances.
  Check requested Vulkan features explicitly; never substitute a simpler scenario
  or count an unsupported prerequisite as a pass.
- Source-repair validation must edit the responsible source, rebuild, recapture
  the same faulty workload and compare with the reference. Keep expected diagnoses
  private to tests; application readback and Nsight evidence remain distinct.

## Temporary artifacts

- Validation outputs are disposable after checks finish. Keep only a concise
  current conclusion or useful reproduction instruction; clean up captures,
  traces, copied binaries, logs, scratch scripts and run directories.
- Do not create archival snapshots, tarballs of working trees, exhaustive payload
  inventories, frozen executable collections or archive-verification programs
  merely to prove that validation happened. Do not duplicate completed runs into a central store.
- Archival, central import, permanent pins and quota increases are not completion
  gates. Test persistence/pin behavior with temporary data and clean up afterward.
- Retain a minimized input only for an automated regression, a difficult unresolved
  bug with a concrete ongoing need, or an explicit user request. Prefer sanitized
  fixtures under tests/fixtures; release development-only pins after validation.
- Preserve user-requested artifacts, unrelated captures, active work, installed
  executables, dependency sources and genuine build prerequisites. Respect managed
  retention and locks. Do not increase quotas to accommodate disposable history.

## Versioning

The root CMake project VERSION is authoritative; derive CLI/MCP identity, logs,
manifests and packaging from ngm::project_version().

- Increment MAJOR only on explicit owner instruction.
- Increment MINOR once when a related roadmap group completes; do not count each
  item or session as a new minor. A major/minor increment subsumes patch and resets lower fields.
- Increment PATCH in every commit with first-party source, headers, shaders,
  tests, build/packaging logic or installation scripts; include it in that commit.
- Documentation, planning and benchmark-result-only commits do not change version
  unless the owner requests it. Product, MCP protocol, artifact schema and
  Nsight/SDK versions are independent; version bumps do not prove compatibility.
