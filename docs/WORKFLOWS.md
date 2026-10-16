# Workflows

Use [Install](INSTALL.md) to build/connect and [Compatibility](COMPATIBILITY.md)
to check scope. Tools expose their exact schemas through tools/list. All long
operations require polling job_status through confirmed cleanup.

## Capture and inspect

Start with a standalone fixture run to verify the desktop and application:

```sh
build/linux-gcc-debug/ngm-experiment \
  --fixture build/linux-gcc-debug/ngm-vulkan-fixture \
  --output-root build/fixture-runs --scenario reference \
  --seed 42 --width 192 --height 128 --frame 2 --validation false
```

Application readback is separate from Nsight evidence. For a generated C++ capture,
call capture_cpp using absolute paths:

```json
{
  "executable": "/absolute/checkout/build/linux-gcc-debug/ngm-vulkan-fixture",
  "working_directory": "/absolute/checkout",
  "arguments": ["--scenario", "shader-error", "--seed", "42", "--width", "192", "--height", "128", "--frame", "120", "--shader-dir", "/absolute/checkout/build/linux-gcc-debug/shaders/fixture"],
  "wait_frames": 2,
  "application_output_option": "--output",
  "timeout_ms": 120000
}
```

Use capture for a native Graphics Capture instead. Its capture_frame counts
Nsight delimiters; capture_cpp uses wait_frames. Neither is a universal mapping
to an application frame index. Every capture launches a fresh target instance.

For native captures, query metadata/events/objects and preview exported images.
For C++ captures, read derived/cpp-project.json, then follow project_directory
and indexed sources with capture_cpp_source/capture_cpp_draws. The project
screenshot is normally screenshot.bmp. Optional resource workers enable listed
source resource_ref reads; opaque bytes alone do not prove descriptor selection.

## Diagnose and verify a source repair

1. Capture a correct reference and the faulty workload with equivalent inputs.
2. Inspect source/shader relationships and compare images. The fixture's
   shader-error.frag swaps red and blue; trace the selected module rather than
   relying on an expected answer supplied to the agent.
3. Edit the responsible source and rebuild normally. For that shader defect,
   replace vertex_color.bgr with vertex_color while keeping scenario shader-error.
4. Recapture the same faulty scenario/settings with the rebuilt input. Confirm
   that the build/shader identity changed and compare it with the correct reference.
5. Use artifact_compare_images with reference/candidate artifact_id and path,
   and channel_tolerance: 0 for the deterministic exact-image case. Compare
   equivalent frames/dimensions; screenshot equality alone does not diagnose a cause.

Keep before/after data through comparison, then clean up temporary output.
The capture CLI shares the service; run ngm-capture --help for its argument form.

## Source state and uniform values

Use capture_draw_executions and capture_draw_state for supported recorded source
occurrences. capture_cpp_descriptor_writes lists updates; pass a listed update_ref
to decode through the descriptor worker. It does not alone establish draw-time visibility.

For a matching direct C++ capture, call capture_draw_values with capture_id and
section: "executions", then select a returned execution_ref. Selection requires
both matching resource and descriptor workers. The model checks supported
update/copy, upload/completion, resource-range and actual SPIR-V type/lifetime
relationships before decoding. Unknown effects discard provisional values.
These are source-visible values, not invocation/register observations or numeric
Graphics Capture event joins.

## Compute

The compute-reference, compute-index-error and compute-arithmetic-error fixture
scenarios run without presentation. Width × height is limited to 16384 uint32
values. Numerical outputs are application readback; Nsight dispatch/object
inventories are a separate evidence source.

For qualified capture boundaries, use capture delimiter: "vk_frame_boundary"
and fixture arguments --compute-boundary vk_frame_boundary. The target must
support/enable VK_EXT_frame_boundary. The independent CPU oracle lives in the
C++ integration harness; selecting a correct scenario is not a source repair.

## Profiling

Call profile with executable, working_directory, arguments and settings:

```json
{"architecture":"Ampere GA10x","delimiter":"submits","start_after":30,"limit":3,"duration_ms":1000,"metric_set":"Throughput Metrics"}
```

Choose the actual GPU's architecture. The product requests unaltered clocks and
single-pass collection. Nullable time_every_action/hes_enabled controls are
version/prerequisite sensitive. Per-action timing can perturb execution.

Read profile_metadata, page profile_metrics and use profile_marker only for
uniquely associated exported marker ranges. profile_compare takes 2–8 fresh
runs per group, matching producer/GPU-name/driver/settings and an exact row/column.
Declare workload/warmup policies; the tool does not verify them. It compares
run medians and variation, without outlier removal or a causal speedup claim.
Raw hardware counter names do not establish application draw/fragment counts.

## Saved replay and draw timing

replay accepts a complete native service capture_id and finite loop_count (1–600,
default 1). It uses matching ngfx-replay without relaunching the original target.
It verifies saved-input identity and owned cleanup; success does not establish
original-live timing or pixel/frame correspondence.

capture_analyze_timing collects C++ and GPU Trace evidence for a saved interval,
using architecture and interval_index. Qualified inputs contain one or three
intervals. For capture_draw_timings, pass one analysis artifact ID in analysis_ids for a
single-interval source or all three interval analyses for a three-interval source.
Only supported unique literal one-call ranges associate; ambiguous/repeated or
multi-draw paths refuse. Durations belong to the saved replay execution.
