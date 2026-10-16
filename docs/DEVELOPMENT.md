# Development

Read [AGENTS.md](../AGENTS.md) for project rules and [ROADMAP.md](ROADMAP.md)
for remaining accepted work. Documentation describes current behavior; Git carries history.

## Layout and dependencies

| Path | Responsibility |
| --- | --- |
| src/server | Shared MCP registry, stdio and loopback HTTP. |
| src/capture | Capture/profile/replay jobs and their coordinator. |
| src/nsight | Discovery, CLI adapters and export parsers. |
| src/inspection | Bounded queries, source models and confined workers. |
| src/artifacts | Atomic publication, leases, retention and pins. |
| src/fixture, shaders/fixture | Deterministic Vulkan application and GLSL inputs. |
| tests | CPU stand-ins, sanitized fixtures and explicit hardware integrations. |

The exact gitlinks under external/ are authoritative; inspect them with
git submodule status --recursive. CMake uses local targets and disables
network fallbacks, dependency examples/tests and unused TLS features.

| Submodule | Use |
| --- | --- |
| fastmcpp | Static MCP implementation. |
| json | Header-only nlohmann JSON. |
| cpp-httplib | Header-only local HTTP transport. |
| glslang | Source-built GLSL-to-SPIR-V compiler and static compiler libraries. |
| vulkan-headers | Vulkan API headers. |
| volk | Static Vulkan entry-point loader. |
| lodepng | Static PNG codec with its own deflate implementation. |

The fixture uses GLSL/SPIR-V for Vulkan 1.3. Diagnostic shaders use -g -Od;
performance shaders use -g0. Per-shader provenance is authoritative. Runtime
OS/compiler, XCB, Vulkan-loader and driver libraries remain external. Dependency
licenses stay in their source trees and installed package notices.

## Checks

```sh
cmake --build --preset linux-gcc-debug --target ngm_check
cmake --build --preset linux-gcc-debug --target ngm_mcp_check_run
cmake --build --preset linux-gcc-debug --target ngm_http_check_run
```

The aggregate builds prerequisites and runs CPU checks, including real-process
stand-ins and ELF/static-linkage inspection. For a focused check use its _run
target, or build first and invoke CTest with -R. Hardware checks are explicit and
are not part of the CPU aggregate. Missing hardware/confinement support is
unsupported/skipped, not a compatibility pass. Remove temporary output after checking it.

The fixture exposes small selectable raster, multipass, bindless, indirect,
compute, performance, state and timing scenarios. Run ngm-vulkan-fixture --help
and ngm-experiment --help for options. [tests/SCENARIOS.md](../tests/SCENARIOS.md)
contains private expected diagnoses/oracles; never insert those answers into MCP
inspection results or diagnosis prompts. A source repair must edit/rebuild and
recapture the same workload.

## Optional Nsight helpers

Default builds need no generated proprietary sources. For resource reads, obtain
a matching Nsight C++ export containing ReadOnlyDatabase.cpp/.h, DataScope.cpp/.h
and DllCommon.h. Descriptor reads additionally require VulkanStructHydrator.cpp/.h
(the header may be under include/). Only fingerprint-qualified inputs are accepted.

```sh
cmake --preset linux-gcc-debug \
  -DNGM_RESOURCE_HELPERS_2026_3=/absolute/CppCapture/project \
  -DNGM_DESCRIPTOR_HELPERS_2026_3=/absolute/CppCapture/project
cmake --build --preset linux-gcc-debug \
  --target ngm-resource-worker-2026_3 ngm-descriptor-worker-2026_3
```

The 2026.2 resource option/target use suffix 2026_2; descriptor hydration is
qualified only for the matching 2026.3 profile. CMake copies/checks the five or
seven files, applies the descriptor bounds instrumentation in isolation and
compiles only that approved closure. Never build generated application/replay
sources or generated build scripts. --profile reports a worker's compiled identity.

Workers require Landlock ABI 3 and seccomp before database parsing, with bounded
memory, CPU, file output and parent wall time. Unsupported confinement refuses.
The parent validates the source-bound request, full response and cleanup.
Input caps are 256 MiB database/16 MiB records; byte reads return at most 64 KiB.
The build-required fingerprints/patch are part of the toolchain, not validation archives.

For optional application SDK boundaries, select separately installed matching headers:

```sh
cmake --preset linux-gcc-debug -DNGM_NSIGHT_SDK_ROOT=/absolute/NsightGraphicsSDK/0.9.2
cmake --build --preset linux-gcc-debug
```

The SDK-free default is unchanged. Only qualified header bundles are accepted;
no SDK download occurs. Initialize before Vulkan instance creation. The fixture's
--sdk-first-boundary-frame N enables SDK calls for that launch; capture uses
delimiter: "graphics_capture_api". Leave at least two later application frames.
SDK compilation does not prove a runtime boundary works for an arbitrary workload.
