# nsight-graphics-mcp

A C++ MCP server under development for NVIDIA Nsight Graphics on Linux.
The build foundation (R-013) is implemented. The server and Vulkan fixture currently
support `--version` and `--help`; MCP serving, rendering, and capture are subsequent
[roadmap items](docs/ROADMAP.md). Running either executable without an option
reports that limitation on stderr and exits unsuccessfully.

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

The current build and all ordinary checks need no display, GPU, Vulkan loader, or
Nsight installation. The eventual rendering/capture workflow requires a compatible
NVIDIA GPU/driver, a system Vulkan loader, an existing Linux desktop session, and a
separately installed Nsight Graphics release. Windowing selection and real capture
compatibility remain in R-005/R-015; no GPU configuration is qualified by these
CPU checks.

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
| `ngm_cli_check` | Both versions match CMake; invalid/startup requests fail on stderr without contaminating stdout. |
| `ngm_shader_failure_check` | A bad compile removes an earlier shader output, reports failure, and recovers after a fix; paths include spaces. |
| `ngm_missing_source_check` | Each missing dependency gives an actionable configure error. |
| `ngm_check_isolation` | The registration helper excludes hardware executables from default builds, CPU aggregates, and CTest. |
| `ngm_linkage_check` | Real ELF consumers use only documented runtime libraries and vendored archives are nonempty. |

The registration helpers in [cmake/Checks.cmake](cmake/Checks.cmake) give ordinary
checks a focused `<name>_run` target and a CTest entry. Hardware checks use a
separate helper, are excluded from default builds, and have explicit run targets
only. No real hardware check is implemented yet; the isolation check uses a tiny
CPU stand-in to verify the build rules.

## Build boundaries

`ngm_core` owns shared first-party code. The server and fixture have separate entry
points; later MCP handlers, job/process coordination, artifact storage, parsers,
and backend adapters can build on this boundary. Vendored warnings and flags stay
separate from first-party C++20 targets.

The authoritative product version is `project(... VERSION ...)` in
[CMakeLists.txt](CMakeLists.txt), exposed through `ngm::project_version()`.
The initial version is `0.0.1`; R-013 alone does not finish the roadmap's
R-013/R-005/R-003 group or trigger a minor increment. MCP protocol, artifact schema,
Nsight, and dependency versions are separate identifiers. The source-edit and
capture provenance records will be added with the corresponding features.

Planning and integration limits are in [AGENTS.md](AGENTS.md),
[TECH_STACK.md](docs/TECH_STACK.md), and [INVESTIGATIONS.md](docs/INVESTIGATIONS.md).
