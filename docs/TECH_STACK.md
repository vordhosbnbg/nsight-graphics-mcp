# Technology Stack Evaluation

Status: **Build/basic-fixture group complete at 0.1.0; capture/evidence group complete at 0.2.0; R-010 advanced fixture complete at 0.2.1; R-002 optional SDK control complete at 0.2.2 for the qualified basic workload. The recorded CPU aggregate passes 20 checks. Basic/advanced capture and retained typed queries pass across 54 captures on two Nsight releases, with current focused CPU checks passing. Application readback passes 57 launches. GPU replay and the complete visual-debugging workflow remain unqualified.**
Last updated: **2026-09-18**.

This evaluation is separate from [ROADMAP.md](ROADMAP.md). The earlier Python
recommendation has been superseded by the user's preference for C++ throughout.
Keep the selected direction, remaining choices, and interview answers here.

At 0.2.3, a separate `capture_cpp` activity retains generated API source and
resource files through the existing service/artifact path (16 current tools).
It uses the same pinned dependencies. [CPP_CAPTURE.md](CPP_CAPTURE.md) records
its contract; fixed-capture generated-helper experiments remain separate from a
generic resource-extraction API. Full visual repair remains unfinished.

## Confirmed context

- The first target is Vulkan graphics and compute on Linux with NVIDIA Nsight
  Graphics.
- R-013 provides the build/check foundation. R-003/R-005 completed the 0.1.0 stdio
  capability query, windowed Vulkan fixture, and isolated C++ experiment runner.
  The 0.2.0 capture/evidence milestone adds managed storage, asynchronous jobs,
  `CaptureService`, a native `ngm-capture` command, and initially 15 MCP tools, including
  bounded retained metadata/event/object queries.
  [MCP.md](MCP.md) and [FIXTURE.md](FIXTURE.md) distinguish current evidence from
  the still-pending Nsight workflows. Exact source pins and build evidence are in
  [DEPENDENCIES.md](DEPENDENCIES.md) and [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
- The implemented backend uses documented Nsight CLI interfaces for capture and
  replay exports. Corrected production environment handling preserves explicit
  `XDG_DATA_DIRS` and supplies standard defaults when absent or empty. A C++ client
  drove 54 successful basic/advanced captures across nine workload pairs and two
  releases through the real MCP server without a data-directory override. Typed
  retained queries also pass on every capture. The matching
  2026.3.1.0 and 2026.2.0.0 results and limits are in
  [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). Actual GPU replay stalled during
  initialization on both releases (I-007/I-011) and remains unqualified. Profiling
  remains planned. R-002 now implements optional application-side SDK boundaries
  using explicitly selected, fingerprinted headers from the separately installed
  Nsight toolchain. At 0.2.2, 12 SDK-controlled basic MCP captures and 6 default
  captures pass on matching 2026.3.1.0/SDK 0.9.2 and 2026.2.0.0/SDK 0.9.0 pairs,
  including independently decoded frame correspondence and persistent pins. See
  [SDK_CONTROL.md](SDK_CONTROL.md).
- The first-release client is Codex, launching a local stdio server process on the
  GPU machine. Persistent Streamable HTTP is accepted later work.
- Each first-release capture starts a fresh application instance; reusable target
  sessions and attachment to existing processes are outside that release.
- The first release runs a windowed target in an existing Linux desktop session;
  fully headless operation is outside scope. The actual window-system path must
  be validated with the fixture and Nsight.
- Artifact storage uses configurable age/storage limits and automatic pruning,
  protecting explicitly pinned bundles and data in active use. Pin state persists
  across server restarts; investigation evidence must be explicitly retained.
- The first release provides documented source builds for other Linux users.
  Dependencies must be pinned as git submodules and built from source with CMake,
  using `../lava-chan-viewer/` as the reference. The user clarified static linking
  to mean static vendored libraries with documented system/runtime/GPU exceptions,
  matching the reference's Linux policy.
- Validation is local: build/tests and explicitly invoked GPU integration checks,
  with no first-release CI setup. Compatibility validation must cover at least two
  distinct Nsight Graphics releases; record the exact tested environment for each.
- The artifact store uses JSON manifests and directories. Provisional retention
  defaults are 2 GiB and 30 days, informed by fixture-directory sizes; they have
  not yet been qualified against representative Nsight captures. See
  [ARTIFACTS.md](ARTIFACTS.md).
- The user's highest priority is extensive native Vulkan/tooling integration.
- The project should support both applications with source access and unmodified Vulkan
  applications; application-side integration is optional.
- The user prefers C++ throughout and has previously had success with fastmcpp.
  That makes fastmcpp the preferred MCP library for this project.
- The user selected visual correctness, then compute correctness, then performance
  analysis, and requested a dedicated Vulkan test application for development.
  These requirements are tracked in the roadmap.
- The first visual-debugging release includes source/shader repair and verification
  through rebuild and recapture. Codex handles edits/builds through its normal
  development tools; the MCP server provides capture and inspection evidence.
- The first release ends at visual correctness; compute and performance follow
  later. Its test renderer includes basic rasterization, multiple passes,
  offscreen rendering, post-processing, bindless resources, and indirect draws.
  Supporting API/compiler choices must accommodate that selected workload.
- The user delegated shader-language selection to the agent. Use GLSL and glslang
  to produce SPIR-V for the test app. R-013 pins/builds glslang 16.4.0 and checks
  embedded source/line information; capture/source correlation still needs validation.
- Backend integration is limited to documented Nsight interfaces. Record each
  failed integration/extraction attempt in [INVESTIGATIONS.md](INVESTIGATIONS.md)
  for future investigation; accept and report gaps within that boundary.

## What should drive the decision

1. Fit for extensive native Vulkan/tooling integration, the user's stated priority.
2. Maintainability for someone most familiar with C++ and preferring it throughout.
3. How quickly we can build and refine useful capture and analysis workflows.
4. Installation, release expectations, and large-artifact processing needs.
5. Clear process ownership, cancellation, data validation, and testability.

For the selected CLI-based architecture, the server delegates GPU work to Nsight.
My working assessment is that process control, artifact access, and analysis
ergonomics matter more initially than server-language execution speed. This is an
architectural judgment, not a benchmark. Actual parser costs and artifact sizes
must be measured before making performance claims.

## Alternatives considered

As checked on 2026-09-17, Python, TypeScript, Go, and Rust all have official Tier 1
MCP SDKs. C++ is not listed in the official SDK catalog, so fastmcpp remains a
community dependency whose compatibility must be validated for our selected clients.
[Official MCP SDK catalog](https://modelcontextprotocol.io/docs/sdk)

The following tradeoffs are project-specific judgments. The user's native
integration priority and C++ experience give C++ greater weight in this comparison:

| Candidate | Why it fits | Main tradeoff | Prefer it when |
| --- | --- | --- | --- |
| Python | Concise process orchestration, flexible export parsing, and convenient analysis workflows. | The usual deployment manages a Python runtime and dependencies; large parsing workloads need deliberate memory use and possibly workers. | Iteration speed and analysis ergonomics dominate. |
| Rust | Explicit ownership and error handling, a compiled executable, and a good fit for substantial binary parsing or native interfaces. | More implementation and compilation overhead for exploratory code, especially without existing Rust familiarity. | Distribution, resource control, and a durable systems implementation dominate. |
| Go | A compiled executable and a straightforward model for concurrent jobs and service code. | Native bindings and analysis integrations may favor separate helpers; SQLite driver selection affects build and runtime requirements. | Simple deployment and maintainable orchestration dominate. |
| TypeScript on Node.js | Typed MCP interfaces, asynchronous orchestration, and potential sharing with future web tooling. | The usual deployment includes a JavaScript runtime; Python/native analysis integrations need another process or binding. | TypeScript familiarity or web-tool integration is a strong advantage. |
| C++ | Direct fit with native Vulkan development and the header-only NGFX SDK. | MCP library support needs additional investigation, alongside more manual process, lifetime, and parsing infrastructure. | Existing C++ expertise or extensive native components outweigh the protocol/tooling cost. |

No language makes missing Nsight interfaces available. Likewise, a standalone MCP
executable would still depend on an appropriate Nsight installation and driver.

## Plausible component combinations

These combinations preserve the alternatives considered. C++ throughout is now
the selected direction. The C++ build dependencies are now pinned in
[DEPENDENCIES.md](DEPENDENCIES.md); the other columns are historical alternatives,
not installed components. R-003 validates Codex CLI 0.154.0 over local stdio;
other clients and HTTP remain outside that validation.

| Layer | Python | Rust | Go | TypeScript | C++ |
| --- | --- | --- | --- | --- | --- |
| MCP | Official `mcp` SDK | Official `rmcp` SDK | Official Go SDK | Official TypeScript server SDK | Pinned fastmcpp with first-party protocol validation |
| Jobs/processes | `asyncio` subprocesses | Tokio processes/tasks | `os/exec`, contexts, goroutines | Node child processes/streams | First-party Linux process supervisor and job coordinator |
| Data contracts | Typed models plus runtime validation | Serde and schema generation | Typed structs plus schema validation | Runtime schemas plus TypeScript types | nlohmann/json, typed core records, and first-party validation |
| Artifact index | Files plus standard-library SQLite if needed | Files plus a SQLite crate if needed | Files plus a selected SQLite driver if needed | Files plus a selected SQLite binding if needed | JSON manifests and directories |
| Development tooling | Locked Python environment, formatter/linter, type checker, test runner | Cargo, rustfmt, Clippy, cargo test | Go modules, gofmt, vet, go test | Lockfile, TypeScript checks, formatter/linter, test runner | CMake/Ninja, clang-format, CTest, and project-owned checks |

Python documents asynchronous subprocess control directly. RMCP uses Tokio, Serde,
and schema generation. The current TypeScript SDK separates server and client
packages. These provide starting points for the combinations above.
[Python subprocess documentation](https://docs.python.org/3/library/asyncio-subprocess.html),
[RMCP documentation](https://github.com/modelcontextprotocol/rust-sdk),
[TypeScript SDK documentation](https://github.com/modelcontextprotocol/typescript-sdk),
[Go SDK documentation](https://github.com/modelcontextprotocol/go-sdk)

## Keep the native boundary separate

Even with C++ throughout, the server and an application's capture integration
are separate components. NGFX is a header-only SDK used inside the target
application; initialization must occur before Vulkan instance creation. Its
application-side calls do not become an external inspection API merely because
the MCP server is also written in C++.
[NGFX SDK guide](https://docs.nvidia.com/nsight-graphics/UserGuide/sdk.html)

RenderDoc was considered as a possible additional backend and has a Python API.
It is excluded by the round 5 decision to use documented Nsight interfaces only.
Private interfaces and GUI automation are also outside the selected scope. A
missing documented inspection path must remain an explicit capability gap, with
failed attempts recorded for future investigation.
[RenderDoc Python API](https://github.com/baldurk/renderdoc/blob/v1.x/docs/python_api/index.rst)

## Selected direction and remaining choices

**Use C++ throughout, with fastmcpp as the preferred MCP library.** This follows
the user's language preference and previous successful use of that library. A
Python/TypeScript protocol front end and a Rust server are not the planned
architecture.

| Component | Current position |
| --- | --- |
| Implementation language | C++ throughout; user preference recorded. |
| MCP library | fastmcpp 3.4.7.1 pinned and source-built; real Codex stdio capability query validated at 0.1.0. The 0.2.0 workflow has synthetic checks and real capture/retained-inspection validation through C++ MCP clients. First-party framing/schema checks handle documented local library gaps. |
| C++ standard | First-party C++20 selected and built with GCC/Clang; dependencies retain upstream language levels. |
| Formatting | Root .clang-format copied byte-for-byte from lava-chan-viewer; its formatter settings do not choose the compiler language level. |
| Build | CMake 3.25 minimum, Ninja configure/build/test presets, isolated build trees; exact tested compilers in BUILD_VALIDATION.md. |
| Project versioning | Kiln policy adopted: one CMake version, patch per commit changing first-party code, minor per completed roadmap group, major only on explicit owner instruction. |
| Dependencies | Exact-commit git submodules under external/, including required transitive sources; build from source with no configure/build downloads. |
| Linkage | Static vendored libraries; documented system/runtime/GPU exceptions allowed, as clarified by the user. |
| Distribution | Documented Linux source builds; prebuilt releases are not required for v1. |
| Validation | Local CMake/CTest and opt-in GPU integration checks; no hosted CI or GPU runner requirement. |
| Test harness | Small project-owned C++ checks with focused/aggregate CMake targets, executable tooling stand-ins, and an isolated C++ experiment runner; adopted from the sibling review. |
| Nsight compatibility | At least two distinct releases, with matching tools/SDK configuration and real GPU workflow evidence for each. |
| JSON | Reuse fastmcpp's nlohmann/json types where appropriate; avoid an additional JSON stack. |
| MCP client | Codex CLI 0.154.0; a real 0.1.0 capability query negotiated MCP 2025-06-18. |
| MCP transport | Local stdio for the first release; persistent Streamable HTTP later. |
| Display | System XCB desktop dependency, Vulkan XCB surface; actual KDE Wayland/Xwayland path exercised by the fixture. |
| Test shaders | Source-built glslang 16.4.0; Vulkan 1.3/SPIR-V 1.6 debug compilation, seven-shader provenance, and basic/advanced fixture rendering exercised. Nsight source correlation remains pending. |
| Nsight integration | Documented capture/replay CLI adapter and shared CaptureService implemented; native ngm-capture and MCP workflow use it. At 0.2.1, focused CPU checks and 54 real basic/advanced captures pass on matching 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 tools. Typed retained queries pass on all 54 captures using two explicit producer profiles. Actual GPU replay stalls during initialization on both releases. See NSIGHT_VALIDATION.md and INVESTIGATIONS.md. |
| Jobs/processes | Linux process supervisor uses argv/environment arrays, deadlines, cancellation, and bounded descendant cleanup. JobCoordinator serializes state transitions and GPU reservations, validates completion identities, and retains ownership until cleanup is confirmed. See JOBS.md. |
| Storage | ArtifactStore implements staged/atomic bundles, age/budget pruning, persistent pins, coordinated leases, and restart recovery using JSON manifests and directories. CPU checks pass; real attempts, captures, controls, and image comparisons are pinned, with capture pins verified after restart. See ARTIFACTS.md. |

### Source-build and linkage policy

Round 9 selected source installation for other Linux users, CMake, exact-commit
git submodules, source-built dependencies, and static linking. The user clarified
the linking boundary to match the reference: static vendored libraries with
documented system/runtime/GPU exceptions. Dependency source acquisition is
explicit through `git submodule update --init --recursive`.
After initialization, configure/build must work without dependency downloads or
prebuilt project dependency packages. The host compiler/build tools and installed
Nsight/GPU stack are documented prerequisites.

The requested sibling reference was inspected read-only on 2026-09-17:

- It uses submodules under `external/`, Ninja presets, isolated build directories,
  and ignored local preset overrides. Its first-party language level is C++23;
  that observation did not dictate this project's independently selected C++20 level.
- Its policy prefers static third-party libraries but allows OS/compiler-runtime,
  Vulkan-loader, and driver dependencies. Its Linux README also lists system
  OpenSSL and desktop libraries, and an installed shader compiler.
- Inspection of both existing Linux MinSizeRel binaries with `readelf -d` found
  dynamic dependencies including `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1`,
  `libssl.so.3`, and `libcrypto.so.3`. These are reference-project observations,
  not dependencies selected for this server or evidence of a build run here.

[Reference dependency/linking policy](../../lava-chan-viewer/AGENTS.md#dependency-policy),
[reference Linux build](../../lava-chan-viewer/README.md#linux-build),
[reference presets](../../lava-chan-viewer/CMakePresets.json),
[reference submodules](../../lava-chan-viewer/.gitmodules)

The user's clarification accepts the reference's static-vendored-code boundary.
Required OS/compiler-runtime, desktop, Vulkan-loader, and GPU-driver dependencies
may remain external and must be documented. Audit final ELF linkage and libraries
loaded at runtime; `BUILD_SHARED_LIBS=OFF` alone does not establish the dependency
boundary. Do not copy the reference's optional dependencies merely because it
uses them, or describe these executables as having no dynamic dependencies.
R-013's build validation is recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md);
R-014 owns the complete visual-workflow installation guide.

For fastmcpp, the reviewed CMake creates a static core library and accepts existing
`nlohmann_json::nlohmann_json` and `httplib::httplib` targets; without them it can
fetch sources during configuration. Supply pinned local submodule targets before
adding fastmcpp. Keep optional curl/TLS/sampling features disabled for stdio and
validate the whole dependency graph with downloads unavailable. R-013 now supplies
those local targets, disables optional features, and has passed a clean configure,
build, and CPU suite in a network namespace without network access.
[fastmcpp CMake source](https://github.com/0xeb/fastmcpp/blob/main/CMakeLists.txt)

### Project versioning

Use **Kiln's versioning policy**, selected by the user's clarification. This
supersedes the earlier viewer-based versioning recommendation. The viewer remains
the reference for the adopted build/harness practices and `.clang-format`.

Kiln was inspected read-only on 2026-09-17:

- Its authoritative version is the top-level `project(Kiln VERSION 0.19.1 ...)`
  declaration. `target_compile_definitions` passes it to a C++ version accessor;
  package metadata derives from the same `PROJECT_VERSION`.
- A registered CTest checks that `kiln --version` reports the CMake version.
  This is source inspection, not a test run in this session.
- Its `AGENTS.md` requires a patch increment in every commit changing first-party
  code, a minor increment when an identified group of related roadmap items is
  completed, and a major increment only on explicit owner instruction. Major or
  minor increments subsume the patch increment and reset lower components.
- Documentation, roadmap, architectural-decision, and benchmark-result-only
  commits do not trigger a patch increment unless requested. Bumps belong in the
  same commit as the triggering change. These are contributor rules, not an
  automatic Git-derived version counter.

[Kiln versioning policy](../../kiln/AGENTS.md#project-versioning),
[version declaration and compile definition](../../kiln/CMakeLists.txt),
[version accessor](../../kiln/src/command/Version.cpp),
[version check](../../kiln/tests/CMakeLists.txt),
[package version metadata](../../kiln/cmake/packaging.cmake)

The policy is adopted in this repository's `AGENTS.md`, including shaders among
first-party code. R-013 established CMake version `0.0.1` and the shared
`ngm::project_version()` accessor; both entry points reported it in checked standalone
queries. R-003 verified matching MCP server identity, and the R-013/R-005/R-003
group completed at 0.1.0. The R-012/R-011/R-001 capture/evidence group completes
at 0.2.0 after reviewed implementation and real validation. Its acceptance
evidence records the tested pre-milestone version 0.1.1; the final minor bump
subsumes the patch increment. Artifact manifests and capture reports derive
their product version from the same accessor. Keep version output off protocol
stdout during normal stdio service. Kiln's package machinery does not add binary
packaging to our first-release scope.

For reproducible investigations, extend the application version with a separate
build record: source revision, dirty/unknown checkout state, dependency revisions,
and executable/shader hashes. Different development builds can share the same
project version. Source archives without Git metadata need an explicit unknown
value or supplied provenance; Git must not become an unconditional build
prerequisite. Keep product version, negotiated MCP protocol revision, artifact
schema version, and Nsight/tool/SDK versions separate. Document compatibility
changes explicitly: Kiln's chosen bump policy does not infer a major increment
from a breaking change and therefore does not by itself promise semantic-version
compatibility for the MCP tool surface or stored artifacts.

### Test shader choice

Use GLSL with the glslang command-line compiler for the fixture. This is the
agent's choice under the user's round 7 delegation, not a user-stated language
preference or a restriction on applications the MCP server may capture.

The rationale is a direct GLSL-to-SPIR-V build path with documented Nsight source
information support. glslang is Khronos's GLSL reference front end and includes a
SPIR-V generator. NVIDIA documents embedding source/debug information with
`glslangValidator -g` for Vulkan analysis.
[glslang upstream](https://github.com/KhronosGroup/glslang),
[NVIDIA shader configuration](https://docs.nvidia.com/nsight-graphics/UserGuide/configure-application.html)

Local inspection on 2026-09-17 found `glslangValidator` on PATH reporting 16.4.0.
This is an observed installation, not a dependency pin or a successful compilation
test. R-013 independently pins and builds glslang 16.4.0 from source, using
`-V --target-env vulkan1.3 -g -Od` for the diagnostic build probe. CPU checks inspect
embedded GLSL source and line instructions and prove that compilation failure
removes stale output. R-005 added the real fixture's compilation/provenance
records and shader hashes. Verify useful source correlation through the selected
documented capture/export path; embedded debug information alone does not establish
that an MCP query can retrieve it.

### fastmcpp findings

Initial documentation/source-interface review on 2026-09-17, followed by R-013
source builds and a CPU library-call check. On 2026-09-18, R-003's real Codex CLI
0.154.0 capability query negotiated MCP 2025-06-18 and passed; see [MCP.md](MCP.md).
The following records the initial library findings:

- Upstream is `0xeb/fastmcpp`, which describes itself as beta. The README currently
  reports version 3.4.7.1; R-013 pins the exact revision in DEPENDENCIES.md.
- The documented surface includes stdio, Streamable HTTP, tools, resources,
  prompts, and JSON Schema validation.
- The README states a default MCP revision of 2025-11-25 with version negotiation.
  This does not establish support for every feature in newer revisions.
- It uses CMake and nlohmann/json. Optional HTTP-related components and build
  defaults should be reviewed when choosing the required transport footprint.

[fastmcpp upstream documentation](https://github.com/0xeb/fastmcpp),
[upstream CMake configuration](https://github.com/0xeb/fastmcpp/blob/main/CMakeLists.txt)

Validate the pinned fastmcpp revision against the intended MCP clients:
version negotiation, tool/schema handling, needed structured/image/resource
results, concurrent requests, progress/cancellation, and clean stdio shutdown.
Also review its dependency footprint, license, maintenance, and tests. Protocol
support claims or a successful hello-world exchange alone are insufficient.

## Interview record

Round 1 — answered on 2026-09-17:

1. Highest priority: **Extensive native Vulkan/tooling integration.**
2. Language familiarity: **C++ is most familiar; open to suggestions.**

Round 2 — answered on 2026-09-17:

1. Applications: **Both applications with source access and unmodified Vulkan applications.**
2. Server composition: **C++ throughout preferred; prior success with fastmcpp.**

Round 3 — answered on 2026-09-17:

1. First-release MCP client: **Codex.**
2. Deployment: **Local stdio process first; persistent Streamable HTTP later.**

The initial requirements interview is complete through round 10. Round 4
established workflow order and the dedicated test application. Round 5 selected
the source-repair/verification loop and documented Nsight interfaces only, with a
record of failed attempts. Round 6 set the first release at visual correctness and
selected the advanced renderer coverage tracked in R-010. Round 7 selected a fresh
application instance per capture and delegated the shader-language decision to the
agent, who chose GLSL/glslang. Round 8 selected an existing desktop session and
automatic age/budget retention with pinned artifacts protected. Round 9 selected
Linux source distribution, CMake, and source-built pinned submodules. The user
clarified static linking to match the sibling's static vendored libraries with
documented system/runtime/GPU exceptions. Round 10 selected local-only validation
and coverage of at least two Nsight releases. See the roadmap's Planning Decisions
and R-005 through R-015. Resolve remaining component/version choices within the
assigned implementation items; do not reopen settled requirements without new
evidence or a changed user preference.

## Decision

Use C++ throughout, with fastmcpp preferred, targeting Codex over local stdio in
the first release, with a fresh application launch for each capture and GLSL/glslang
for the test shaders. Use CMake/Ninja with exact-commit git submodules and build
required dependencies, including the shader compiler, from source. Provide a
documented Linux source-installation path; persistent Streamable HTTP follows
later. Link vendored libraries statically and document the allowed system/runtime/
GPU dependencies. C++20 and the initial source revisions are established by R-013;
the process supervisor, job coordinator, and JSON-manifest artifact store are
implemented. Basic/advanced capture and typed inventory queries through MCP are
verified on the recorded 2026.3.1.0 and 2026.2.0.0 setups; broader compatibility,
source repair, and performance retention measurements remain outstanding.
Run validation locally, including real GPU workflows on at least two Nsight
releases. R-013 owns the build/toolchain choices, R-005 the fixture/windowing setup,
R-011 process control, R-012 storage, and R-015 the exact compatibility matrix.
Revisit the library only if validation reveals a material gap; do not reopen
settled preferences without new evidence or a change in user priorities.
