# Linux source installation and first repair

This walkthrough builds the server and its deterministic Vulkan fixture, captures
a shader defect, inspects the evidence, and verifies an actual shader edit. Use a
dedicated checkout: the tutorial changes an intentionally faulty test shader.
The server supplies capture/evidence tools; Codex uses ordinary file and build
tools for the edit. Applications without source access can still be captured,
but this repair exercise requires the fixture's source.

## Prerequisites and tested scope

- Linux x86-64, a C++20 compiler and matching standard library, CMake 3.25+,
  Ninja, Python 3, Git, GNU binutils, normal C development headers/linker,
  pkg-config, and XCB development headers/library (1.13+).
- Recorded builds use GCC 15.3.0/16.2.1 and Clang 22.1.8 with libstdc++.
  The current 0.2.12 full suite is qualified with GCC 16.2.1; older foundation
  and focused portability runs have narrower scope. See
  [BUILD_VALIDATION.md](BUILD_VALIDATION.md). Other versions are unqualified.
- For rendering/capture: an existing X11/Xwayland desktop with `DISPLAY`, a
  Vulkan 1.3 device/driver, the system Vulkan loader, and a compatible separately
  installed NVIDIA Nsight Graphics release. Native Wayland and fully headless
  operation are outside this walkthrough.
- For resource-byte inspection: Linux UAPI headers and a running kernel providing
  Landlock ABI 3 (Linux 6.2+) with seccomp filtering, plus the optional worker built
  below. Unsupported confinement produces an error; it is never silently disabled.
- For the agent workflow: Codex with local stdio MCP support. Recorded client
  interoperability uses Codex CLI 0.154.0; the server itself needs no Python runtime.

Vendored libraries and the glslang shader compiler are built from pinned sources.
The binaries also need OS/compiler runtimes and, for rendering, XCB, the Vulkan
loader and GPU/desktop libraries. They are not fully static binaries.
[DEPENDENCIES.md](DEPENDENCIES.md) records exact pins, linkage checks and observed
runtime-loaded libraries. Nsight, the driver, and optional generated helpers are
separate toolchain inputs and are not distributed in this repository.

The visual qualification covers Nsight **2026.3.1.0 / build 38722833** and
**2026.2.0.0 / build 37991608**, each using its own matching tools. The recorded
GPU is an RTX 3080 Ti with driver 615.71.09, on KDE Wayland through Xwayland/XCB.
Other GPUs, drivers, desktops and producer builds are not established by these
results. See [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) and the capability limits
at the end of this guide.

## Acquire and build

Obtain access to the repository, then clone it or use a supplied source checkout.
Select the revision you intend to validate before initializing its submodules.
The commands below do not install packages or alter machine settings.

```sh
git clone https://github.com/vordhosbnbg/nsight-graphics-mcp.git
cd nsight-graphics-mcp
git submodule update --init --recursive
git submodule status --recursive
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target ngm_check
build/linux-gcc-debug/nsight-graphics-mcp --version
build/linux-gcc-debug/ngm-vulkan-fixture --version
build/linux-gcc-debug/ngm-capture --version
```

The clone/submodule commands acquire sources; configure/build use only those
initialized sources. There are no build-time dependency downloads. Four parallel
compile jobs are the preset default; adjust with `--parallel N`. Keep local changes
in ignored `CMakeUserPresets.json`, and use separate build trees for different
compilers. The alternative presets are `linux-clang-debug` and `linux-gcc-release`.
`ngm_check` builds test prerequisites and runs the CPU suite, including binary
linkage inspection. It launches no GPU integrations. A confinement check reported
as skipped does not qualify resource-byte workers on that host.

Run binaries from the build tree; this project does not currently provide a
system installation or prebuilt release package. Shader debug information is
preserved by the shared presets. A Release build is not automatically a qualified
GPU performance measurement configuration.

## Check the desktop and retain application baselines

In the same terminal/session used for Codex, set an absolute checkout path:

```sh
ngm_checkout="$(pwd -P)"
build/linux-gcc-debug/ngm-experiment \
  --fixture "$ngm_checkout/build/linux-gcc-debug/ngm-vulkan-fixture" \
  --output-root "$ngm_checkout/build/first-repair-baselines" \
  --scenario reference --seed 42 --width 192 --height 128 --frame 2 \
  --shader-dir "$ngm_checkout/build/linux-gcc-debug/shaders/fixture" \
  --validation false
```

Require exit status zero and `status: pass`. The returned report points to an
isolated fresh run, its input/build/shader identities, logs, and `image.ppm`.
Repeat with `--scenario shader-error` to retain the original faulty baseline.
These are application readbacks, separately labeled from later Nsight screenshots.
`--validation false` avoids requiring a separately installed Vulkan validation
layer for this tutorial; the development fixture's synchronization-validation
matrix is recorded in [FIXTURE.md](FIXTURE.md).

## Connect Codex and capture

Configure a local stdio server using absolute paths. The following uses the
documented `mcp_servers` command, arguments and forwarded environment fields in
the [official Codex MCP guide](https://developers.openai.com/codex/mcp).
Replace every `/absolute/...` value. Use a new, dedicated artifact directory,
outside your source files and ordinary application data.

```toml
[mcp_servers.nsight_graphics]
command = "/absolute/checkout/build/linux-gcc-debug/nsight-graphics-mcp"
args = ["--artifact-root", "/absolute/managed-evidence", "--nsight-root", "/absolute/Nsight-installation"]
env_vars = ["DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS", "XDG_DATA_DIRS"]
required = true
```

Restart/reconnect the client after configuration changes. Ask Codex to call
`capabilities`; expect product identity and 22 advertised tools. Discovery reports
observed prerequisites, not proof that capture works. [MCP.md](MCP.md) explains
configuration locations, isolated CLI configuration, and the complete contracts.
An explicit Nsight root prevents accidental mixing of installed tool releases.

Ask Codex to call `capture_cpp` with this JSON, using your actual absolute paths:

```json
{
  "executable": "/absolute/checkout/build/linux-gcc-debug/ngm-vulkan-fixture",
  "working_directory": "/absolute/checkout",
  "arguments": ["--scenario", "reference", "--seed", "42", "--width", "192", "--height", "128", "--frame", "120", "--shader-dir", "/absolute/checkout/build/linux-gcc-debug/shaders/fixture"],
  "wait_frames": 2,
  "timeout_ms": 120000,
  "application_output_option": "--output",
  "pin": true
}
```

Save the returned artifact ID as the reference capture. Poll `job_status` using
its job ID until terminal, with `worker_running: false` and
`finalization_pending: false`. Require `state: succeeded` and
`cleanup_confirmed: true`. Repeat with only the scenario changed to `shader-error`;
save that distinct artifact ID as the original faulty capture. Every submission
launches a fresh target. The target's final frame 120 leaves time for capture;
`wait_frames: 2` is the Nsight C++ capture control, not that final frame number.
The recorded tutorial validates the screenshot against frame-2 readback.

Use `artifact_read` on `derived/cpp-project.json` in each capture. Its
`project_directory` gives the bundle-relative generated project path. Preview
`<project_directory>/screenshot.bmp` with `artifact_preview_image`, and compare
the reference/faulty screenshots with `artifact_compare_images` at
`channel_tolerance: 0`. Expect a difference. Keep all capture IDs: job IDs are
session-local; artifact IDs survive restart.

## Inspect the responsible shader

Call `capture_cpp_draws` for the faulty capture. Follow its source references with
`capture_cpp_source`, retaining the implicated draw, pipeline and fragment module
association. These are generated-source relationships, not reconstructed GPU
event state. Source queries work without a resource worker.

To correlate the fragment module bytes, build the matching optional reader. For
2026.3, use the absolute generated project directory inside the pinned capture:

```sh
cmake --preset linux-gcc-debug \
  -DNGM_RESOURCE_HELPERS_2026_3=/absolute/managed-evidence/bundles/CAPTURE_ID/PROJECT_DIRECTORY
cmake --build --preset linux-gcc-debug --target ngm-resource-worker-2026_3
build/linux-gcc-debug/ngm-resource-worker-2026_3 --profile
```

Substitute the real capture ID and `project_directory`; do not run any generated
replay build script. CMake accepts only the fingerprint-qualified five-file helper
closure. For 2026.2 use `NGM_RESOURCE_HELPERS_2026_2` and
`ngm-resource-worker-2026_2`. Add `"--resource-worker-2026-3",
"/absolute/checkout/build/linux-gcc-debug/ngm-resource-worker-2026_3"` to the server's
`args` (or the corresponding `2026-2` pair), then restart/reconnect Codex.
[RESOURCE_WORKER.md](RESOURCE_WORKER.md) explains build/runtime requirements.

Call `capture_cpp_resources` and page through `resources` using `next_offset`.
Identify the source reference used by the implicated fragment module, then call
`capture_cpp_resource` with that `capture_id`, `resource_ref`, and `pin: true`.
Keep its returned read-artifact ID and hashes. The fixture's SPIR-V is smaller
than the default 64 KiB page; otherwise fetch successive ranges until complete.
Compare the returned bytes/hash with the capture-retained application shader
under `raw/application/shaders/`, not with a subsequently overwritten build file.
Keep the accompanying retained GLSL and compiler provenance as the source link.

For this defect the fragment module matches `shader-error.frag.spv`. Its retained
GLSL writes `vertex_color.bgr * palette.tint.rgb`: red and blue are exchanged.
The reference shader uses `vertex_color * palette.tint.rgb`. The screenshot
difference and captured module/source correlation support the channel-order
diagnosis. Record the source excerpts, hashes and comparison before editing.
Opaque resource bytes alone do not establish every binding or executed state.

## Edit, rebuild, recapture and compare

In the dedicated tutorial checkout, change only the shader expression in
`shaders/fixture/shader-error.frag` from `vertex_color.bgr` to `vertex_color`.
Keep the scenario name `shader-error`. Save the source patch and build output:

```sh
git diff -- shaders/fixture/shader-error.frag > build/first-repair.patch
cmake --build --preset linux-gcc-debug > build/first-repair-build.log 2>&1
```

Require a successful build. Repeat the frame-2 `ngm-experiment` command using
`shader-error`; retain this new application baseline. Repeat the original faulty
`capture_cpp` request with the same seed, size, paths and `wait_frames`, still
using `shader-error`. Require successful cleanup and save the new artifact ID.
The captured shader/build identities must show the rebuilt input.

Compare the repaired screenshot with the original correct reference using
`artifact_compare_images`:

```json
{
  "reference": {"artifact_id": "REFERENCE_ID", "path": "REFERENCE_PROJECT_DIRECTORY/screenshot.bmp"},
  "candidate": {"artifact_id": "REPAIRED_ID", "path": "REPAIRED_PROJECT_DIRECTORY/screenshot.bmp"},
  "channel_tolerance": 0
}
```

Require equal dimensions and zero differing pixels/channel error. Also compare
each screenshot with its corresponding independent application baseline: import
the baseline run directory with `artifact_import`, `pin: true`, then compare its
inventoried PPM path. Keep comparison responses, the patch/build log, capture IDs,
and original/repaired shader identities together in a record directory; import
that record with `pin: true`. This links actual source repair to before/after
evidence. The comparison tool evaluates pixels; it does not infer workload
equivalence or diagnose a fix by itself.

Leave the tutorial checkout separate from normal development. Restoring the
intentional defect later requires undoing only your tutorial edit and rebuilding;
do not discard unrelated source changes. Fixture regression expectations assume
the repository's deliberate faults remain present.

## Retention, troubleshooting and limits

- Default retention is 2 GiB and 30 days. Pins persist and protect bundles; a
  reference alone does not. Each resource read retains its input snapshots,
  potentially up to 272 MiB. Use `artifact_usage`; configure a larger
  `--artifact-max-bytes` if required. Protected evidence can exhaust the quota.
  [ARTIFACTS.md](ARTIFACTS.md) documents pruning and expiration.
- Only one server/process may own a store at a time. Stop the owning server before
  using `ngm-capture` on that same store; separate stores are independent.
- Missing submodules: run the explicit initialization command. Missing display:
  launch from the existing desktop and forward its environment. Do not interpret
  a capture timeout alone as proof of a missing display or frame delimiter.
- Failed captures retain reports/logs. Check `artifact_info`/`artifact_files` and
  `raw/report.json`; a returned job ID is not capture success. The known
  `ngfx --help`/version exit-1 discovery quirk is handled only for discovery.
- Unsupported producer/helper profiles and source forms are explicit errors or
  coverage gaps. A configured worker path does not prove kernel confinement.
  [RESOURCE_QUERIES.md](RESOURCE_QUERIES.md) documents qualified byte semantics.
- General after-event resource state, executed descriptor selection and live
  shader stepping are unavailable. Standalone GPU replay remains unqualified
  after recorded timeouts. Metadata export success does not qualify replay.
- Optional SDK control is separately qualified in [SDK_CONTROL.md](SDK_CONTROL.md).
  Compute correctness, performance profiling and HTTP service are later roadmap
  work. [INVESTIGATIONS.md](INVESTIGATIONS.md) retains failed probes and revisit
  conditions; [ROADMAP.md](ROADMAP.md) records accepted scope and status.
