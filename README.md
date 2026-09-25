# nsight-graphics-mcp

Capture Vulkan workloads, diagnose rendering defects, and verify source changes.

This MCP server connects Codex to **NVIDIA Nsight Graphics on Linux**. Inspect
Vulkan captures, preview rendered images, investigate compute results, and compare
GPU performance before and after a change. Graphics capture requires no
application instrumentation; source edits and builds use standard
development tools.

**26 tools · C++20 · Local stdio or authenticated loopback HTTP**

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
| Compare performance | `profile_compare` compares repeated baseline and candidate traces, reporting medians and variation. |
| Manage jobs | `job_status` reports progress; `job_cancel` requests cancellation. |
| Browse artifacts | `artifact_list` lists bundles; `artifact_info` describes a bundle; `artifact_files` lists its files; `artifact_read` retrieves text. |
| Import and retain evidence | `artifact_import` imports existing results; `artifact_pin` sets or removes protection from pruning. |
| Manage storage | `artifact_usage` reports storage use; `artifact_prune` applies retention limits to eligible unpinned bundles. |

Captures and profiles run asynchronously and return job IDs. Saved artifacts
remain available across server restarts, subject to retention settings.
[Tool reference](docs/MCP.md#workflow-tools)

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
