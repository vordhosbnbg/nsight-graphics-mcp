# Nsight Graphics MCP

A C++ MCP server for capturing and inspecting Vulkan applications with NVIDIA
Nsight Graphics on Linux. Agents can diagnose a rendering defect, edit its
source, rebuild, recapture and compare the result.

The server provides:

- Fresh application captures, generated C++ exports and standalone GPU replay.
- Bounded event/object queries, shader/resource reads and supported source-state
  and uniform-value reconstruction.
- Image previews/comparisons, GPU Trace collection and repeated-run comparison.
- Asynchronous jobs, cancellation and managed artifacts over stdio or local HTTP.

It uses documented Nsight interfaces. Application instrumentation is optional;
source edits use the agent's ordinary development tools. See
[compatibility and limitations](docs/COMPATIBILITY.md) before relying on a result.

## Build

Requires Linux x86-64, a C++20 compiler, CMake 3.25+, Ninja, Python 3, Git,
pkg-config, binutils and XCB development files.

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
```

Capture also needs a compatible Nsight installation, NVIDIA driver and an existing
X11/Xwayland desktop. CPU checks do not launch GPU workloads.

## Documentation

- [Install and connect](docs/INSTALL.md): prerequisites, client setup, HTTP and packaging.
- [Workflows](docs/WORKFLOWS.md): capture, source repair, compute, profiling and replay.
- [MCP reference](docs/MCP.md): tools, job lifecycle and artifact handling.
- [Compatibility](docs/COMPATIBILITY.md): tested interfaces and known limitations.
- [Development](docs/DEVELOPMENT.md): architecture, dependencies, tests and optional helpers.
- [Roadmap](docs/ROADMAP.md): remaining accepted work.

## Example repair

The fixture's shader defect swaps red and blue. A source edit followed by rebuild
and recapture restores the reference image; selecting a different fixture scenario
is not a source repair.

| Before | After |
| --- | --- |
| ![Shader defect](docs/images/shader-before.png) | ![Repaired shader](docs/images/shader-after.png) |

The project is MIT licensed; dependency notices are in their source trees.
