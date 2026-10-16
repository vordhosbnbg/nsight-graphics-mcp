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
- Draw timing uses qualified original literal paths and generated-source calls.
  Ambiguous/repeated/multi-draw ranges refuse. Saved replay durations do not
  establish original live cost, replay-reset cost or causal instrumentation overhead.
- GPU Trace counters can include other GPU activity. Exported names/positions do
  not establish application-only draw/fragment counts, units or scaling. Raw cells
  stay available; physical interpretation remains the blocked roadmap item.
- Product profiling is single-pass. A native multi-pass probe supplied additional
  raw counters but did not qualify their semantics or all-pass marker correspondence.
- Compute numerical data comes from the application; Nsight inventories are
  separate. Application-side instrumentation remains optional.
- Live shader breakpoints, stepping and variable inspection have no qualified
  public server interface. A single GPU also does not meet the reviewed local
  shader-debugger setup. Fully headless capture and attachment are outside scope.

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
