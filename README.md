# Nsight Graphics MCP

**Capture a rendering bug, fix its source, and verify the result with your coding agent.**

Nsight Graphics MCP connects agents such as Codex to **NVIDIA Nsight Graphics on
Linux** through the Model Context Protocol (MCP). It lets an agent capture your
Vulkan application, inspect rendered images and shaders, investigate compute
results, and compare GPU timings before and after a change.

Graphics capture works without adding instrumentation to your application.
When source is available, the agent can edit and rebuild it with its usual
development tools, then capture again to check the fix against actual output.

See the [full tool list](docs/MCP.md#tools) for available operations and
[compatibility guide](docs/COMPATIBILITY.md) for supported setups and inspection limits.

## What you can ask

**Find and fix a rendering defect**

> Capture the triangle demo, identify the shader responsible for the incorrect
> colors, and verify the output after the fix.

The included Vulkan demo has a shader that swaps red and blue. The agent can
inspect the captured shader, correct the code, rebuild, and compare a fresh
capture with the reference. The example below shows the verified repair:

| Before: swapped color channels | After: corrected shader |
| --- | --- |
| ![Triangle with blue at the upper left and red at the bottom](docs/images/shader-before.png) | ![Triangle with red at the upper left and blue at the bottom](docs/images/shader-after.png) |

The repaired capture matches the reference pixel for pixel.
See the [source-repair workflow](docs/WORKFLOWS.md#diagnose-and-verify-a-source-repair).

**Check whether a performance change helps**

> Collect four GPU traces before and after the shader change, using the same
> warmup. Compare dispatch times and report variation between runs.

The agent can repeat measurements for both versions and compare GPU timings
and how much they vary between runs, helping you judge whether a change improves
your workload. See the [profiling workflow](docs/WORKFLOWS.md#profiling).

## Get started

Requires Linux x86-64, a C++20 compiler, CMake 3.25+, Ninja, Python 3, Git,
pkg-config, binutils and XCB development files.

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
```

Capture also needs a compatible Nsight installation, NVIDIA driver and an existing
X11/Xwayland desktop. CPU checks do not launch GPU workloads. Follow
[Install and connect](docs/INSTALL.md) to connect your MCP client after building.

## Documentation

- [Install and connect](docs/INSTALL.md): prerequisites, client setup, HTTP and packaging.
- [Workflows](docs/WORKFLOWS.md): capture, source repair, compute, profiling and replay.
- [MCP reference](docs/MCP.md): tools, job lifecycle and artifact handling.
- [Compatibility](docs/COMPATIBILITY.md): tested interfaces and known limitations.
- [Development](docs/DEVELOPMENT.md): architecture, dependencies, tests and optional helpers.
- [Roadmap](docs/ROADMAP.md): remaining accepted work.

The project is [MIT licensed](LICENSE); dependency notices are in their source trees.
