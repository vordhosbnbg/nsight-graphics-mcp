# nsight-graphics-mcp

A C++ MCP server under development for NVIDIA Nsight Graphics on Linux.
The source-build foundation, a local stdio capability tool, and a deterministic
windowed Vulkan fixture are implemented. Capture and Nsight evidence inspection
remain subsequent [roadmap items](docs/ROADMAP.md). R-003/R-005 passed independent
review/correction cycles, CPU checks, real Codex interoperability, and local GPU
fixture validation; the R-013/R-005/R-003 group is complete at version 0.1.0.

## Build on Linux

Host prerequisites:

- A C++20 compiler and matching standard library (`std::span` and
  `std::source_location` are used). The qualification baseline is GCC 15.3 or
  newer, or Clang 22.1 with libstdc++; see the exact
  [tested configurations](docs/BUILD_VALIDATION.md). Older compilers are unqualified.
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
GPU runs use KDE Wayland with Xwayland; this is a windowed application. Nsight
capture compatibility remains unverified. The process runner requires Linux
`/proc`, `close_range` (Linux 5.9+), and a platform with the `fork` system call;
the currently validated architecture is x86-64.

The server runs on stdin/stdout with one implemented `capabilities` tool. See
[MCP.md](docs/MCP.md) for the verified Codex integration, protocol boundaries,
and explicit distinction between discovered tools and tested capture support.

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
| `ngm_image_check` | Bounded PPM parsing, channel comparisons, malformed images, and nonblocking FIFO rejection. |
| `ngm_process_check` | Actual executable boundary, timeout/cancellation, descendants, process ownership, and isolated logs/environment. |
| `ngm_mcp_check` | Real stdio protocol, initialization, capability query, invalid requests, discovery, and shutdown. |
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
and image parsing. `ngm_experiments` owns the isolated fixture runner. The server
and fixture have separate entry points; later job coordination, artifact storage,
and Nsight backend adapters build on these boundaries. Vendored warnings and flags
stay separate from first-party C++20 targets.

The authoritative product version is `project(... VERSION ...)` in
[CMakeLists.txt](CMakeLists.txt), exposed through `ngm::project_version()`.
MCP protocol, artifact schema, Nsight, and dependency versions are separate
identifiers. Fixture results retain shader artifacts and build identities;
Nsight capture provenance arrives with the capture backend.

Planning and integration limits are in [AGENTS.md](AGENTS.md),
[TECH_STACK.md](docs/TECH_STACK.md), and [INVESTIGATIONS.md](docs/INVESTIGATIONS.md).
