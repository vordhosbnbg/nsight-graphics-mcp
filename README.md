# nsight-graphics-mcp

Capture Vulkan workloads, diagnose rendering defects, and verify source changes.

This MCP server connects Codex to **NVIDIA Nsight Graphics on Linux**. Inspect
Vulkan captures, preview rendered images, investigate compute results, and compare
GPU performance before and after a change. Graphics capture requires no
application instrumentation; source edits and builds use standard
development tools.

**34 tools · C++20 · Local stdio or authenticated loopback HTTP**

The [0.5.0 capability follow-up](docs/CAPABILITY_FOLLOWUP_COMPLETION.md) adds
controlled launches, retained reader activation, bounded recorded draw state
and exported marker ranges. Execution-time values and isolated draw counters
remain unavailable under the documented evidence limits.

The **0.5.1** capture fix prevents the qualified 2026.3 tool's low-space warning
from blocking unattended jobs. Fresh reference/repeat/fault captures and exact
readback comparisons pass; [validation details](docs/BUILD_VALIDATION.md#051-fresh-graphics-capture-reliability-r-023-2026-10-15)
retain the tested environment and evidence.

At **0.5.2**, [retained-capture GPU replay](docs/REPLAY.md) uses matching
2026.3 tools without relaunching the original application. Source snapshots,
shared GPU scheduling and cleanup are qualified separately from the fixture's
independent replay-image comparisons. The older 2026.2 replay path remains
unqualified.

The **0.5.3** [capture interval option](docs/MULTI_INTERVAL_CAPTURE.md) retains
several consecutive intervals in one file on the qualified matching 2026.3 path.
Raw range metadata is available, with qualified draw association described below.

At **0.5.4**, [same-input timing evidence collection](docs/REPLAY_TIMING.md)
collects generated C++ and a selected-interval GPU Trace from a retained capture.
The collection has its own source/trigger checks, separate from the association query.

At **0.5.5**, `capture_draw_timings` associates unique same-input literal marker
ranges with submitted direct/indexed source calls. It checks every source
interval and preserves raw replay durations, provenance and ambiguity refusals.
At **0.5.6**, independent per-draw timestamps, new direct/indexed/reused inputs
and a shader edit/rebuild/recapture repair complete **R-025** on the measured
2026.3 path. See [draw timing validation](docs/DRAW_TIMING_VALIDATION.md).
Original live performance and physical counter meanings remain unavailable.

At **0.5.8**, `capture_cpp_descriptor_writes` lists and optionally decodes
qualified source-local buffer/image writes through the confined optional worker.
Fresh uniform, storage/image and dynamic-array captures pass independent source
checks and restart/pin checks. See [descriptor writes](docs/DESCRIPTOR_WRITES.md).

At **0.5.9**, `capture_draw_values` supplies a bounded model of
source-visible uniform values at selected C++ submission/draw occurrences. It
checks descriptor writes/copies, coherent uploads, completion boundaries and
actual SPIR-V layouts before returning typed bits and provenance. This remains
source evidence; GPU invocation/register observation and numeric Graphics
Capture event correspondence are unavailable. R-024 completes for this supported
source interface after independent value/image, source-repair and restart checks.
Validation run outputs are temporary; compact results and reproduction steps are
recorded in [BUILD_VALIDATION.md](docs/BUILD_VALIDATION.md).

## Example workflows

**Diagnose a rendering defect**

> “Capture the triangle demo, identify the shader responsible for the incorrect
> colors, and verify the output after the fix.”

Use `capture_cpp` to retain generated source and resources, follow the draw's
shader references, then preview and compare the captures. The included Vulkan
demo shows a verified shader repair:

| Before: incorrect color channels | After: corrected shader |
| --- | --- |
| ![Captured triangle with blue at the upper left and red at the bottom](docs/images/shader-before.png) | ![Recaptured triangle with red at the upper left and blue at the bottom](docs/images/shader-after.png) |

These Nsight screenshots were retrieved through `artifact_preview_image`. The
repaired capture matches the reference pixel for pixel.
[Repair walkthrough](docs/INSTALL.md)

For qualified 2026.3 C++ captures, `capture_draw_executions` and
`capture_draw_state` expose recorded pipeline/shader, depth/culling and
viewport/scissor state with field-level provenance, recorded descriptor bindings
and dynamic offsets, plus explicit bounded push-constant input reads.
GPU-observed state and descriptor/material values remain unavailable.
[Recorded-state scope](docs/DRAW_STATE.md)

**Validate compute results**

> “Capture the compute workload, compare its output against the CPU reference,
> and identify the indexing error.”

Inspect dispatch and object inventories with `capture_events` and
`capture_objects`, alongside retained shader sources and application readback.
The [compute walkthrough](docs/COMPUTE.md) demonstrates two shader repairs;
numerical validation uses application-provided output.

**Evaluate a performance change**

> “Collect four GPU traces before and after the shader change, using the same
> warmup. Compare dispatch times and report variation between runs.”

Use `profile`, `profile_metrics`, and `profile_compare` to compare repeated GPU
traces. In the included performance workload, correcting the workgroup size
reduced measured dispatch times from approximately **0.60 ms to 0.06 ms** on the
tested RTX 3080 Ti.
[Measurement details](docs/PERFORMANCE.md#actual-selected-scenario-source-repair-036-validation)

Profiles can select [per-action timing and HES collection controls](docs/PROFILE_CONTROLS.md).
For qualified single-frame traces, `profile_marker` reads
[unique exported marker ranges](docs/PROFILE_MARKERS.md) and associated raw
regime metrics. Unavailable cells and wider draw/counter scope remain explicit.

## MCP tools

| Task | Tools and functionality |
| --- | --- |
| Inspect configuration | `capabilities` lists tools and observed prerequisites. |
| Capture an application | `capture` saves a frame for metadata inspection; `capture_cpp` exports a C++ capture with source and serialized resources. |
| Inspect a capture | `capture_metadata` describes the capture; `capture_events` lists recorded calls; `capture_objects` lists Vulkan objects. |
| Inspect shader associations | `capture_cpp_draws` identifies draw, pipeline, and shader relationships in generated source; `capture_cpp_source` reads that source. |
| Inspect resource data | `capture_cpp_resources` lists serialized resource references; `capture_cpp_resource` reads byte ranges through an optional resource worker. |
| Preview and compare images | `artifact_preview_image` returns an image or crop; `artifact_compare_images` measures pixel differences with a specified tolerance. |
| Profile GPU workloads | `profile` records a GPU Trace; `profile_metadata` describes its configuration; `profile_metrics` reads exported timings and counters. |
| Inspect marker ranges | `profile_marker` associates unique single-frame exported paths, with marker duration and requested raw regime metrics. |
| Compare performance | `profile_compare` compares repeated baseline and candidate traces, reporting medians and variation. |
| Manage jobs | `job_status` reports progress; `job_cancel` requests cancellation. |
| Browse artifacts | `artifact_list` lists bundles; `artifact_info` describes a bundle; `artifact_files` lists its files; `artifact_read` retrieves text. |
| Import and retain evidence | `artifact_import` imports existing results; `artifact_pin` sets or removes protection from pruning. |
| Manage storage | `artifact_usage` reports storage use; `artifact_prune` applies retention limits to eligible unpinned bundles. |

Captures and profiles run asynchronously and return job IDs. Saved artifacts
remain available across server restarts, subject to retention settings.
[Tool reference](docs/MCP.md#workflow-tools)

Each capture/profile can specify validated library, Vulkan ICD, desktop-backend
and layer settings through a [per-job launch environment](docs/LAUNCH_ENVIRONMENT.md).

## Getting started

Building requires Linux, a C++20 compiler, CMake 3.25+, Ninja, Python 3, and the
[build prerequisites](docs/INSTALL.md#prerequisites-and-tested-scope). Capturing
requires a compatible NVIDIA GPU/driver, Nsight Graphics, and a desktop session.

From a source checkout:

```sh
git submodule update --init --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
```

On Arch Linux, use the [package preset and PKGBUILD](docs/ARCH_PACKAGE.md) to build
an installable package with the server, capture CLI, and documentation.

Configure your MCP client to launch the server with absolute paths:

```sh
/path/to/checkout/build/linux-gcc-debug/nsight-graphics-mcp \
  --artifact-root /path/to/managed-evidence \
  --nsight-root /path/to/Nsight-installation
```

Use a dedicated directory for managed evidence. Start with the
[Codex setup and first capture](docs/INSTALL.md#connect-codex-and-capture), or use
the optional [persistent HTTP service](docs/HTTP.md).

## Limitations

- Tested on Linux x86-64 with Nsight **2026.3.1.0** and **2026.2.0.0**. See the
  [compatibility matrix](docs/VISUAL_RELEASE.md#qualified-scope) for exact builds
  and hardware; other configurations are unverified.
- Visual capture requires an X11/Xwayland desktop and launches a fresh application
  each time. Attachment to running applications and fully headless operation
  are unsupported.
- Inspection is limited to Nsight's exports: no live shader stepping or complete
  GPU state at an event. Standalone GPU replay has not passed validation.
- Reading resource bytes requires an [optional worker](docs/RESOURCE_WORKER.md).
  The validated compute workflow requires application source and Nsight 2026.3.
- Profiling requires GPU counter access; the server does not change system
  permissions. Available metrics and their units depend on the exported data.

## License

Original project code is licensed under the [MIT License](LICENSE).
Dependencies retain their [respective licenses](docs/DEPENDENCIES.md).
