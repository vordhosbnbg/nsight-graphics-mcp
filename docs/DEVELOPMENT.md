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

## Instrumented shader observations

Source-owning applications can opt into the producer contract in
[MCP reference](MCP.md#instrumented-shader-trace-contract). The trace parser
validates bounded application JSON and decodes raw scalar/vector words.
Inspection uses the existing complete capture loader, artifact lease and file
inventory, then fingerprints the retained trace and declared module. Service
launch provenance and application-declared shader/invocation associations stay
distinct. This adds no debugger controller or reusable target session.

The raster fixture demonstrates a selected non-helper fragment writing a storage
buffer and publishing fenced host readback. Its tracing scenario explicitly
requires `fragmentStoresAndAtomics`; ordinary scenarios do not enable this
feature or allocate trace storage. Publish complete trace files atomically before
presentation because capture can terminate the application immediately after its
selected delimiter. An application's recorded frame is independent of that
delimiter's ordinal.

CPU checks cover malformed traces, bounds, selection, retained provenance and
MCP transport. The separate hardware integration compares the fixture's GPU
readback with an independent oracle. Its result qualifies that instrumented
fixture only; generic application claims and original shader execution remain
unverified. [Compatibility](COMPATIBILITY.md#interpretation-limits) records the
public debugger-interface and compilation limits.

Run the focused CPU checks and build the opt-in hardware check:

```sh
cmake --build --preset linux-gcc-debug \
  --target ngm_shader_trace_check_run ngm_shader_trace_mcp_check_run \
  ngm_shader_trace_integration --parallel 6
build/linux-gcc-debug/tests/ngm_shader_trace_integration \
  build/linux-gcc-debug/nsight-graphics-mcp \
  build/linux-gcc-debug/ngm-vulkan-fixture \
  build/linux-gcc-debug/shaders/fixture /absolute/NsightGraphics
```

The hardware check uses one fresh capture and one standalone same-frame
reference. Supply an existing desktop and the qualified matching Nsight tools;
this command does not configure the machine for hardware shader debugging.

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
Input caps are 512 MiB database/16 MiB records; byte reads return at most 64 KiB.
Database admission is independent of the unchanged 256 MiB worker address-space
limit: the qualified helper pages data and reads selected blobs capped at 16 MiB.
Parents stream hashes and immutable snapshots rather than loading whole databases.
Recorded-state and replay-timing validation retain a 512 MiB aggregate fingerprint/
read budget and existing query deadlines; larger files do not grant larger responses.
Use an optimized build for large-capture inspection: Debug hashing can exhaust
those deadlines even when the same inputs fit the Release budget.

The qualified Vulkan 1.3 source subset includes single-part primary recordings,
image-only Sync2 barriers, direct/indexed draws, vertex binds, zero-valued semaphore
QueueSubmit2, and bounded 64-bit timestamp host readbacks with optional availability.
Recorded draw-state inspection also accepts balanced queue debug-label scopes
within the captured frame: at most 64 literal queue symbols and 16 labels per
queue. `queue_labels` records the submission-local names and begin-call sources;
command-buffer `labels` remain separate. Missing ends, cross-queue ends, malformed
labels and unknown frame effects refuse rather than preserve a partial prefix.
Dynamic rendering requires one layer, no multiview, extensions, resolves or rendering
flags, and matching declared pipeline/attachment-view formats. Unknown forms refuse.
Shader specialization remains an opaque resource reference and bounded map, not a
resolved shader value. These associations describe recorded source, not observed
GPU execution, descriptor visibility or uniform contents. Header-only event/regime
exports explicitly report unavailable marker coverage; frame metrics remain separate.
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
