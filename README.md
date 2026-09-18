# nsight-graphics-mcp

A C++20 MCP server for investigating Vulkan rendering defects with NVIDIA Nsight
Graphics on Linux. Codex uses **22 local stdio tools** to capture a fresh application,
inspect retained evidence, preview/compare images and verify source fixes. Codex's
normal development tools perform source edits and builds.

The visual first-release group completes at **0.3.0** after independent review.
All nine fixture defect scenarios have actual source-repair verification on Nsight
**2026.3.1.0/build 38722833** and **2026.2.0.0/build 37991608**. The clean 0.2.12
installation walkthrough passes **25 CPU checks** and a fresh shader repair;
[ROADMAP.md](docs/ROADMAP.md) records the completed group and later work.
The 0.3.0 GCC Debug aggregate also passes all 25 checks.

Start with [INSTALL.md](docs/INSTALL.md) for the source-build-to-repair walkthrough.
[VISUAL_RELEASE.md](docs/VISUAL_RELEASE.md) consolidates capability/repair evidence
and exact limits; [BUILD_VALIDATION.md](docs/BUILD_VALIDATION.md) preserves versioned
milestones and tested configurations.

The server provides asynchronous captures, managed/pinned evidence, metadata/event/
object queries, generated-source draw/pipeline/shader relationships, and optional
confined serialized-resource readers. Those relationships and bytes do not establish
arbitrary executed GPU state. Standalone GPU replay remains unqualified after
recorded timeouts. Compute correctness, performance profiling and HTTP are later work.

## Build on Linux

Host prerequisites:

- A C++20 compiler and matching standard library (`std::span` and
  `std::source_location` are used). Recorded compilers are GCC 15.3.0/16.2.1 and Clang 22.1.8
  with libstdc++; the current full suite uses GCC 16.2.1. See the exact
  [tested configurations](docs/BUILD_VALIDATION.md); other versions are unqualified.
- CMake 3.25 or newer, Ninja, Python 3, and the normal Linux C development
  headers/linker. Python runs glslang's header generator; it is not a server runtime.
- Git for source/submodule acquisition, and GNU binutils (`ar`, `readelf`) for
  the linkage check. Configuration itself does not require Git metadata.
- pkg-config and XCB development headers/library (xcb 1.13+). XCB is the selected
  system desktop dependency; Vulkan headers and volk remain pinned source builds.

From the repository root:

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
build/linux-gcc-debug/nsight-graphics-mcp --version
build/linux-gcc-debug/ngm-vulkan-fixture --version
build/linux-gcc-debug/ngm-capture --version
```

All dependency acquisition happens in the explicit submodule step. Configure and
build use the sources under `external/`, including the shader compiler. Do not run
upstream dependency download scripts or substitute installed dependency packages.
Missing source trees produce a configure error naming the submodule command.
The [dependency record](docs/DEPENDENCIES.md) lists exact commits and runtime
exceptions. Initial submodule downloads need network access; later builds do not.

`linux-clang-debug` and `linux-gcc-release` also have configure/build/test presets.
Every build uses `build/<preset-name>/`. Four compile jobs are the shared default;
use `--parallel N` or an ignored `CMakeUserPresets.json` for local overrides.
Do not reuse a configured build tree when changing compilers. The Release preset
still preserves shader source and line information; it is not yet a GPU performance
measurement configuration.

The build and ordinary checks need no display, GPU, Vulkan loader, or Nsight
installation. The rendering fixture requires a Vulkan 1.3 device/driver, a system
Vulkan loader, and an existing X11/Xwayland desktop through `DISPLAY`. The local
GPU runs use KDE Wayland with Xwayland; this is a windowed application. Basic
and advanced capture/export are verified on the recorded Nsight 2026.3.1.0 and
2026.2.0.0 setups; broader
compatibility remains under validation. The process runner requires Linux
`/proc`, `close_range` (Linux 5.9+), and a platform with the `fork` system call;
the currently validated architecture is x86-64.

The server runs on stdin/stdout with `capabilities`, capture submission, job
status/cancellation, and managed-artifact tools. Workflow tools require an
explicit `--artifact-root`. The native `ngm-capture` command uses the same
`CaptureService` to run one attempt and report its job and retained artifact.
See [MCP.md](docs/MCP.md) for configuration, tool contracts, and the distinction
between the historical Codex capability query and current synthetic workflow
checks and real C++ MCP capture validation. [NSIGHT_BACKEND.md](docs/NSIGHT_BACKEND.md)
records adapter limits.

## Focused checks

```sh
cmake --build --preset linux-gcc-debug --target ngm_dependency_check_run
cmake --build --preset linux-gcc-debug --target ngm_check
```

`ngm_check` builds its prerequisites and runs the complete first-party CPU suite.
CTest alone expects those prerequisites to have been built. The C++ check harness
uses explicit failures rather than `assert`, so Release checks remain effective.

| Check | Evidence |
| --- | --- |
| `ngm_dependency_check` | Source-built fastmcpp/JSON calls and uninitialized volk calls, with no GPU or loader initialization. |
| `ngm_shader_check` | Valid SPIR-V 1.6 with embedded GLSL source and source-line instructions. |
| `ngm_hash_check` | SHA-256 standard known answers for content identities. |
| `ngm_image_check` | Bounded PPM/BMP/PNG decoding, PNG encoding, crop/downsampling, channel comparisons, malformed images, and FIFO rejection. |
| `ngm_process_check` | Actual executable boundary, timeout/cancellation, descendants, process ownership, and isolated logs/environment. |
| `ngm_worker_confinement_check` | Worker-only filesystem/syscall restrictions and memory, CPU and output limits; unavailable Landlock enforcement is explicitly skipped after checking refusal. |
| `ngm_jobs_check` | Serialized state transitions, GPU reservations, completion identities, deadlines, cancellation, and cleanup ownership using CPU workers. |
| `ngm_artifacts_check` | Atomic publication, persistent pins, leases, retention, quota exhaustion, imports, and restart recovery in temporary directories. |
| `ngm_nsight_check` | CLI discovery, argument delivery, export validation, failures, and owned-process cleanup using executable Nsight stand-ins. |
| `ngm_nsight_evidence_check` | Bounded parsing of sanitized observed metadata/event/object exports, invalid schemas, duplicate IDs/keys, and input limits. |
| `ngm_capture_service_check` | Shared job/storage/backend workflow, retained success/failure evidence, timeout, and cancellation using executable stand-ins. |
| `ngm_cpp_evidence_check` | Bounded generated-source associations, unsupported forms, malformed input, and result budgets. |
| `ngm_resource_reference_check` | Literal resource references, declaration conflicts, source spans and unsupported forms. |
| `ngm_resource_read_check` | Qualified worker supervision, leased input snapshots, bounded byte results, failures and publication retention. |
| `ngm_cpp_inspection_check` | C++ bundle provenance/index completeness, source pagination, coverage, limits, and leases. |
| `ngm_inspection_check` | Retained capture provenance, producer profiles, pagination, response bounds, and leased artifact image comparisons. |
| `ngm_capture_validation_check` | Hardware-harness timeout/cancellation classification and valid empty logs exports. |
| `ngm_mcp_check` | Actual stdio protocol and capture/job/artifact tools through executable stand-ins, including input validation, output isolation, and shutdown. |
| `ngm_fixture_contract_check` | Standalone option and shader-provenance validation without initializing Vulkan. |
| `ngm_experiment_check` | Isolated repeated runs and retained failed/malformed results using a CPU executable stand-in. |
| `ngm_cli_check` | Versions match CMake; standalone options and clean server EOF keep diagnostics separate. |
| `ngm_shader_failure_check` | A bad compile removes an earlier shader output, reports failure, and recovers after a fix; paths include spaces. |
| `ngm_missing_source_check` | Each missing dependency gives an actionable configure error. |
| `ngm_check_isolation` | The registration helper excludes hardware executables from default builds, CPU aggregates, and CTest. |
| `ngm_linkage_check` | Real ELF consumers use only documented runtime libraries and vendored archives are nonempty. |

The registration helpers in [cmake/Checks.cmake](cmake/Checks.cmake) give ordinary
checks a focused `<name>_run` target and a CTest entry. Hardware checks use a
separate helper, are excluded from default builds, and have explicit run targets
only. `ngm_fixture_integration_run` explicitly launches the real windowed fixture
and compares all four scenarios across repeated fresh launches and two input
configurations. It is excluded from ordinary CTest and default check targets.

## Build boundaries

`ngm_core` owns shared versioning, process execution, discovery, content hashes,
and image parsing. `ngm_experiments` owns the isolated fixture runner.
`ngm_jobs`, `ngm_artifacts`, and `ngm_nsight` own the coordinator, managed storage,
CLI adapter, and export parsers. `ngm_capture` composes the capture workflow as
`CaptureService`, shared by the MCP server and `ngm-capture`. `ngm_inspection`
queries retained capture bundles independently of the transport. The fixture
remains a separate application. Vendored
warnings and flags stay separate from first-party C++20 targets. Storage and job
contracts are in [ARTIFACTS.md](docs/ARTIFACTS.md) and [JOBS.md](docs/JOBS.md).

The authoritative product version is `project(... VERSION ...)` in
[CMakeLists.txt](CMakeLists.txt), exposed through `ngm::project_version()`.
MCP protocol, artifact schema, Nsight, and dependency versions are separate
identifiers. Fixture results retain shader artifacts and build identities;
capture-attempt reports retain executable hashes, tool observations, and launch
settings, with caller-provided application metadata labelled separately.

Planning and integration limits are in [AGENTS.md](AGENTS.md),
[TECH_STACK.md](docs/TECH_STACK.md), and [INVESTIGATIONS.md](docs/INVESTIGATIONS.md).
