# Draw timing validation at 0.5.6

This qualification joins the 0.5.5 retained source-call reader with independent
per-call Vulkan timestamps, new inputs and an actual shader repair. The measured
producer is **Nsight 2026.3.1.0/build 38722833** on **RTX 3080 Ti**, kernel driver
**615.71.09**, KDE Wayland/Xwayland and GCC **16.2.1**. Other producers or GPU
generations are not qualified by these runs. The shared surface stays **32 tools**.

## Optional fixture evidence

The existing `--timing-witness NEW_FILE` defaults to its scene-only, schema-1
witness with two timestamp queries. Add `--timing-scope draw` to select a
schema-2 witness with fourteen queries: scene boundaries plus one pair around
each of six direct/indexed calls. The timing scenarios remain `timing-direct`,
`timing-indexed` and `timing-reuse`; the last submits the same recording twice.
The witness must stay outside application output and the destination must be
fresh. These validation options are not server requirements for arbitrary
applications; existing unmodified-application labels can supply literal paths.

Draw scope adds `ngm.fixture.draw.N` leaves beneath the existing nested,
repeated and multi-draw human labels. Queue labels include queue family, frame
and submission occurrence. Indexed-bind state stays inside the one-call leaf.
Each owned submission waits for its fence before copying query results, so
reusing the query pool cannot overwrite the preceding witness. Records retain
query indices, raw ticks, queue valid bits, timestamp period, call parameters,
marker paths, scene bounds and executable/shader hashes. Draw intervals are
checked for valid bits, bounded wrap and order within the scene; zero elapsed
ticks remain possible at timestamp resolution.

Both endpoints use `BOTTOM_OF_PIPE`. Vulkan permits timestamp writes inside a
render pass and defines their prior-stage dependency and wrap behavior in the
[Khronos command reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html).
These are instrumented call envelopes, including state and timestamp effects.
They are not isolated shader costs or a correction for profiling overhead.

## Independent numerical and occurrence checks

Five fresh standalone controls cover scene/default and draw/reuse at seed 42,
192×128/frame 7; direct at seed 73, 224×160/frame 5; indexed at seed 137,
224×144/frame 6; and reuse at seed 987, 256×160/frame 9. Wide integer modulo
arithmetic and Decimal period conversion independently verify **294** per-call
records. Scene and draw variants produce byte-identical images at equal inputs.
I-075 retains the earlier unsupported 160×96 window request; no silent resize
or relaxed numerical criterion is used.

Before measurement, the plan declares the same-live-profile tolerance:

```text
abs(Nsight_ms * 1e6 - Vulkan_ns)
    <= 256 ns + 0.05 * max(Nsight_ns, Vulkan_ns) + 2 * timestamp_period_ns
```

Four fresh serial GPU Traces use Time Every Action, HES requested false,
Throughput Metrics, one frame after five presents and unchanged clocks. Their
**36** selected direct/indexed/reused call ranges agree with the application's
timestamps from those **same live executions** to **0–32 ns**. All **362** actual
MCP results pass their schemas, including two restarts of all selected marker
queries and pins. The HES request is retained; its active implementation state
is not inferred from these observations.

Fresh saved-capture analyses independently verify the literal source/trace
association:

| Workload | Source intervals | Available | Unavailable |
| --- | ---: | ---: | ---: |
| Direct, seed 73, 224×160 | 1 | 6 | 0 |
| Indexed, seed 137, 224×144 | 1 | 6 | 0 |
| Reused mixed calls, seed 987, 256×160 | 3 | 36 | 0 |
| Direct scene labels without new leaves, seed 42, 192×128 | 1 | 2 | 4 |

The last control refuses repeated full paths and multi-draw leaves. These four
sources and six analyses pass **586** actual schemas and two restarts. Original
fixture API witnesses verify source paths and call parameters; raw replay cells
verify returned values. Each selected interval is profiled in its own replay
process. **Original live timestamps are not the durations of these saved
replays.** Original numeric Graphics Capture IDs stay null; generated-C++
selections and unique original label paths provide the qualified association.

## Actual shader repair

An isolated editable copy of `scene.frag` uses `palette.tint.bgr` in the first
build. Pinned source-built glslang uses the existing diagnostic Vulkan 1.3
`-g -Od` settings; each variant retains source, SPIR-V, compiler arguments and
hashes. At seed 987, 256×160 and application frame 9, the Graphics Capture differs
from the correct independent application reference in **12,286 pixels**, with
maximum channel difference **75**.

An ordinary C++ capture's numbered source links its bound graphics pipeline to
the fragment module. The confined, unchanged **0.4.2** five-file resource worker
extracts all **1,356** module bytes, exactly matching the first compiled shader
and its retained debug source. Shader/module/pipeline numeric IDs are scoped to
that C++ capture; no cross-capture numeric equality is used.

The actual source edit restores `.rgb`, then rebuilds a second frozen shader
bundle and launches fresh captures. The repaired output has **zero differing
pixels** and the same RGB hash as the correct reference. Its extracted module
exactly matches the newly built SPIR-V. Both saved inputs return **12** available
draw ranges with equivalent paths/functions/parameters and distinct evidence
references. The before/after stages pass **319** schemas, source reviews **20**,
and two further restart/read/image checks **40**. New read artifacts and their
source captures remain independently pinned. Descriptor contents and typed
execution-time uniform values are not established by shader-byte extraction.

The first/repaired fragment SPIR-V SHA-256 values are:

```text
e951bdc426e1425c1a0bff42bc213f257552ba637d24171d2e40beeadb1254aa
b7dd0e0292ee9bf6ec8fd02730c677e2e74ff68b8915e5b680b1650878cb77d5
```

## Process and runtime boundaries

Every actual GPU operation is serial, uses a fresh target and confirms owned
cleanup. The explicit C++ process supervisor owns standalone controls; product
jobs own capture/profile/replay phases. No GUI automation, private interfaces,
generated replay compilation or wider helper closure is used.

I-074 records that the earlier temporary runtime disappeared and default NVML
615.78 mismatches the running 615.71.09 kernel. The matching
[official NVIDIA archive](https://download.nvidia.com/XFree86/Linux-x86_64/615.71.09/)
is checksum-verified and extracted only into the ignored validation directory.
Explicit process-local library/ICD selection passes matching NVML and the actual
standalone controls. The installer is never run. Existing profiler-device and
trace-device access remains sufficient; before/after capability controls and
access match exactly. The global driver, desktop, clocks, permissions, installed
0.4.1 server and optional 0.4.2 worker are unchanged.

All **40 CPU checks** pass in **402.41 seconds**, including the expanded fixture
contract. Overall actual transport qualification totals **1,327** schema-valid
results. [BUILD_VALIDATION.md](BUILD_VALIDATION.md) records retained bundle and
build identities. Physical counter scaling/application attribution remains
R-026 work; descriptor/resource/type lifetimes remain R-024 work.

Complete/pinned **bundle-168c040e6fa84e7efd645b16a5413735** retains **329**
payloads (**83,527,614 bytes**). Its `snapshot.tar.gz` losslessly contains all
**2,640** experiment/prerequisite members, including every declared native file,
all **27** complete/pinned native bundles (**2,152 files**), successful/failed
controls, isolated configurations, oracles, actual schemas/transcripts, shader
builds and frozen executables. `snapshot-inventory.json` records each restored
member's length and SHA-256. The GPU-free portable verifier extracts into a
temporary directory and rechecks every member, native manifest, pin, timestamp,
raw source/table association, shader read, image comparison and actual result
schema. The imported verifier passes; all imported payload lengths/hashes match.
The source snapshot precedes only the final documentation/pin bookkeeping.
This completes **R-025** at **0.5.6**; it does not complete R-024 or R-026.
