# Compatibility and limitations

## Qualified interfaces

Use each installation's matching CLI/replay tools and SDK. Directory names alone
do not establish a version. This matrix records demonstrated scope, not a promise
about every workload or later tool release.

| Interface | Nsight 2026.3.1.0 / build 38722833 | Nsight 2026.2.0.0 / build 37991608 |
| --- | --- | --- |
| Windowed Vulkan capture/export and visual source repair | Qualified | Qualified |
| Optional basic application SDK boundaries | SDK 0.9.2 | SDK 0.9.0 |
| Generated C++/serialized byte reads | Qualified matching helper profile | Qualified matching helper profile |
| No-presentation VK_EXT_frame_boundary compute capture | Qualified | Unsupported on measured injection path |
| Live GPU Trace and repeated profile comparison | Qualified measured workload | Qualified measured workload |
| Recorded source state, descriptor hydration and typed uniform model | Qualified supported subset | Unqualified |
| Application-reported instrumented fragment traces | Qualified fixture | Unqualified |
| Standalone GPU replay and saved draw timing association | Qualified supported subset | Unqualified |

The measured host is Linux x86-64, RTX 3080 Ti, driver 615.71.09 and KDE
Wayland through Xwayland/XCB. Source builds have been exercised with GCC 16.2.1
and libstdc++; other hardware/driver/compiler combinations need their own checks.

## Interpretation limits

- Function streams and object inventories do not establish complete executed
  pipeline state, descriptor selection or resource contents at an event.
- Generated-source state/descriptor/value queries cover bounded supported forms.
  The uniform model handles proven source updates/copies, coherent uploads,
  completion and shader layouts. It reports source evidence, not observed GPU
  invocations/registers or numeric Graphics Capture event correspondence.
- Serialized bytes are capture database contents, not a general after-event
  memory API. Unknown source effects, layouts or lifetimes refuse.
- Streamed validation admits files up to 512 MiB within the unchanged 512 MiB
  aggregate query budget. Release inspection covers a 333,587,928-byte 4K Training
  database and its 434,972,120-byte saved-replay database on driver 615.78.08.
  Confined source-scoped descriptor/resource reads work on the Training input
  within the existing 256 MiB worker address-space limit. This does not qualify
  arbitrary larger inputs or the full uniform-value model for this workload.
- The Training export's 84 direct/indexed source draws use the qualified bounded
  Vulkan 1.3 dynamic-rendering/Sync2/QueueSubmit2 subset. Supported recorded state
  and source shader relationships are available, including balanced queue-label
  scopes around submissions. Queue labels carry their own source provenance;
  GPU execution correspondence and descriptor visibility remain unproved.
- Live unlabelled Training profiles with Time Every Action verified Yes export
  header-only event/regime tables on this producer/driver combination. Explicit
  application queue/command labels produce 70 event/regime ranges, usable through
  `profile_marker` under its qualified single-frame contract. These ranges may
  contain several draws and do not establish original execution IDs or exclusive
  application counters. The saved replay's 84 source draw occurrences still lack
  complete literal per-draw marker ranges and attributable per-draw durations.
- Draw timing uses qualified original literal paths and generated-source calls.
  Ambiguous/repeated/multi-draw ranges refuse. Saved replay durations do not
  establish original live cost, replay-reset cost or causal instrumentation overhead.
- GPU Trace counters can include other GPU activity. Exported names/positions do
  not establish application-only draw/fragment counts, units or scaling. Raw cells
  stay available; application attribution and physical interpretation are unsupported.
  Revisit only with documented Graphics definitions and collection boundaries.
- Product profiling is single-pass. A native multi-pass probe supplied additional
  raw counters but did not qualify their semantics or all-pass marker correspondence.
- Compute numerical data comes from the application; Nsight inventories are
  separate. Application-side instrumentation remains optional.
- `capture_shader_trace` reads optional application-reported instrumented fragment
  values, with bounded raw-word decoding, selection and retained file/launch
  provenance. A fresh 2026.3.1.0 capture on RTX 3080 Ti / driver 615.78.08 passes
  the fixture's independent value oracle and byte-exact same-frame ordinary
  reference comparison. Standalone Khronos synchronization validation reports no
  errors. This qualifies the instrumented fixture; arbitrary producer claims,
  original register values and capture-event correspondence remain unverified.
  The [workflow](WORKFLOWS.md#instrumented-fragment-values) and
  [focused checks](DEVELOPMENT.md#instrumented-shader-observations) describe use
  and reproduction.
- Live shader breakpoints, stepping and variable inspection have no qualified
  public server interface. On 2026.3.1.0, `ngfx --help-all`,
  `ngfx-capture --help` and `ngfx-replay --help` expose no debugger-control or
  invocation/register export operations. SDK 0.9.2's activity enum contains
  Graphics Capture, GPU Trace and System Profiling, without a Shader Debugger API.
  NVIDIA's documented [source-debug modes](https://docs.nvidia.com/nsight-graphics/UserGuide/shader-debugger-overview.html#debug-shaders)
  compile shaders as debug at load time or replace selected pipelines with debug
  versions. Their observations do not establish unchanged original shader code
  or preservation of a failure that disappears under instrumentation. The
  [debugger setup](https://docs.nvidia.com/nsight-graphics/UserGuide/shader-debugger-setup.html#system-setup)
  requires two GPUs for local debugging or a separate host, and an enabled Linux
  driver debugger interface. Revisit with documented public debugger controls
  and evidence that the target failure survives the selected debug mode;
  observing original optimized-code registers needs its own demonstrated support.
  Fully headless capture and attachment are outside scope.

## Prerequisites and troubleshooting

- Use an existing X11/Xwayland DISPLAY; native Wayland capture is not qualified.
  Re-run a standalone fixture before diagnosing Nsight injection problems.
- Select --nsight-root explicitly. Valid version/help text may accompany exit 1
  on the qualified ngfx discovery path; capture/profile failures still need their own handling.
- Counter collection needs operator-provided profiling access. The product never
  changes permissions, clocks, drivers or desktop settings. HES is not qualified
  on the measured Ampere GPU; requesting it can refuse.
- Driver libraries must match the loaded kernel driver. A mismatch can break
  Vulkan/NVML before capture. A validated process-local library/ICD overlay may
  be supplied through launch_environment; it does not change system configuration.
- The bounded overlay allows LD_LIBRARY_PATH, VK_ICD_FILENAMES,
  VK_DRIVER_FILES, DISABLE_VK_LAYER_LUNARG_override, VK_LOADER_DEBUG,
  SDL_VIDEODRIVER and QT_QPA_PLATFORM. Worker environments remain isolated.
- Read job diagnostics and raw/report.json for the actual operation's failure.
  Distinguish unsupported, failed and timed out from a successful empty query.

Primary interfaces: [capture/replay CLI](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html),
[GPU Trace](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html),
[application SDK](https://docs.nvidia.com/nsight-graphics/UserGuide/sdk.html).
