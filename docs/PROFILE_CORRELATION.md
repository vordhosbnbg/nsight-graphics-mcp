# Graphics timing and counter qualification (R-022)

The **0.5.0** group completes R-022 with the qualified marker query and
[conditional scope outcome](PROFILE_SCOPE_FEASIBILITY.md). The versioned
collection/assessment notes below retain their earlier identities and status.

The **0.4.9** slice adds reproducible graphics timing workloads and a separate
private validation witness. It adds no MCP query or new draw/counter claim.
`profile_metrics` still reports exact export positions with their existing
units/coverage contract. R-022 remains In Progress for bounded product queries
and the remaining scope/overhead qualification.

At **0.4.10**, [profile_marker](PROFILE_MARKERS.md) implements the qualified
single-frame unique-path query using this retained matrix. It preserves explicit
marker/regime scope and unavailable original execution identities and scaling.
Repeated paths and wider collection shapes receive reasons. R-022 remains In
Progress for its remaining scope/overhead assessment; the versioned matrix
results below retain their original 0.4.9 collection/build identities.

## Workloads and independent witness

| Scenario | Calls per submission | Submissions per present |
| --- | --- | --- |
| `timing-direct` | Six `vkCmdDraw` calls | 1 |
| `timing-indexed` | Six `vkCmdDrawIndexed` calls | 1 |
| `timing-reuse` | Three direct and three indexed calls, alternating | 2, reusing the same recording |

The calls use instance counts **8, 16, 32, 64, 128, 128**, the existing diagnostic
scene shaders and the same opaque triangle. They sit inside frame/scene/batch
labels and two branches. Two sibling `repeat` markers have the same full path;
another `repeat` belongs to the other branch. `unique.first` contains one call,
while `unique.pair` contains two. There are ten marker begins per normal recording;
the selected image-readback frame adds an eleventh marker outside the timed scene.
No label name implies a general action count.

`--timing-witness NEW_FILE` is an explicit fixture-only validation option. It
requires a timing scenario and a fresh file outside application output. Default
launches create no timestamp pool or witness. With the option, the fixture records
actual begin-label inputs, parent/occurrence ordinals, draw-call arguments and
successful submission identities. Those inputs never feed product MCP queries.

Two Vulkan timestamps bracket the whole scene render pass at BOTTOM_OF_PIPE.
The witness retains raw counters, queue valid bits, nanosecond period, elapsed
ticks, converted nanoseconds and the host submit/wait bound. It requires 36–64
valid bits and rejects invalid periods, upper bits, nonpositive durations or
possible multiple wraps. A fence completes each submission before readback and
before the next reuse resets the queries. This measures an incremental scene
envelope including clear/pipeline work. Timestamp commands and host fence/JSON
cadence are part of the validation workload. It provides no isolated draw time.
See [Vulkan timestamp semantics](https://docs.vulkan.org/spec/latest/chapters/queries.html#queries-timestamps).

## Collection matrix

The opt-in C++ `ngm_profile_correlation_integration` target is separate from
ordinary CPU checks:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_correlation_integration
/absolute/build/tests/ngm_profile_correlation_integration \
  /absolute/build/nsight-graphics-mcp /absolute/build/ngm-vulkan-fixture \
  /absolute/build/shaders/fixture /absolute/Nsight-installation \
  /absolute/validation-parent /absolute/launch-environment.json
```

It first launches each scenario with synchronization validation, then each again
without validation. Inputs are seed **42**, **1024×512**, final application frame
**3** or **11** respectively. Each gets isolated configuration, logs, output and
private witness paths, plus the explicit matching-driver launch overlay.

Five fresh profiles per scenario then exercise three frame intervals with timing
off/on, three submit intervals with timing off/on, and one frame with timing on.
Collection starts after **five** chosen intervals, uses **1000 ms**, Throughput
Metrics / Ampere GA10x, HES explicitly false, clocks unaltered, a **120000 ms**
deadline and pinned source artifacts. The target's final frame is **300**, so its
lifetime extends beyond collection. Witness samples after the profiled window
remain distinct from the selected-window samples. No permission or machine
setting is changed.

The collection report, raw exports and independent audit distinguish an export
row/column from original frame, queue, submission and action identity. Repeated
full paths prevent a unique event/regime join. Multiple identical `time_ms`
headers do not by themselves identify frames or samples. The submit-boundary
reuse case also exercises partial frame coverage.

For single-frame profiles, the independent audit compares the exact exported
marker sequence to actual API marker inputs. It checks scene totals against the
same profile's private whole-scene timestamps, using the tolerance declared before
collection: **max(0.025 ms, 25% of the private scene total)**. Reused submissions
are checked as an explicitly labelled aggregate; missing original occurrence IDs
remain unavailable. The `time_ms` header supplies the duration unit.

Per-action timing has a documented instrumentation penalty in
[NVIDIA's GPU Trace settings](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html).
Private scene samples from application frames 5–7 provide descriptive on/off and
standalone observations. One fresh launch per matrix condition and uncontrolled
desktop/clocks do not establish causal overhead or a stable speedup. Diagnostic
shader configuration is retained explicitly.

Frame and regime counter tables retain raw metric names, text and positions.
Hardware draw counts are not assumed equal to application API-call counts;
instruction totals have no inferred fragment-stage-only scope. Physical counter
scaling remains unresolved where exports/documentation do not establish it.
Neither frame totals nor a multi-draw marker provide isolated draw counters.

Exact results and retained evidence are recorded in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md). I-042 preserves the corrected private
harness failure. Broader two-release claims require actual matching toolchains;
this new matrix targets only the installed **2026.3.1.0/build 38722833** producer.

## Native result, 2026-10-15

All **six baselines** and **fifteen profiles** pass, including active/clean
synchronization validation, owned-process cleanup, independent source pins and
identical metadata/event pages after restart. The independent audit verifies
**400** timestamped submissions and **1,615** actual MCP results against their
advertised schemas and text/structured parity. Direct/indexed/reused outputs
match exactly within each baseline's selected frame. The GPU reports **64** valid
timestamp bits and a **1 ns** period; raw counters remain retained.

| Single-frame case | Nsight scene total (ms) | Private scene total (ms) | Absolute difference |
| --- | --- | --- | --- |
| Direct | 0.331200 | 0.331232 | 32 ns |
| Indexed | 0.333952 | 0.334016 | 64 ns |
| Reused submissions, aggregate | 0.667296 | 0.667456 | 160 ns |

The exact API marker sequence agrees with the export in these cases. The maximum
relative difference is **0.024%**, below the predeclared tolerance. The one-draw
and two-draw markers remain distinct facts in the private API witness. This
checks marker-containing scene duration and units, not isolated draw execution.

Observed timing-on/off private-scene median ratios are **1.002996**, **0.996351**
and **1.001543** for direct, indexed and reuse respectively. The full samples and
standalone observations remain retained. These small, mixed changes are
inconclusive as causal overhead estimates under the declared conditions.

The exports expose concrete association limits (I-043). A three-frame direct or
indexed trace has **10 timing rows / 3 time columns**, versus **30 regime rows /
438 metric columns**. Frame-numbered paths in the regime table do not supply
missing timing-column identity. A one-frame nonreuse trace has **10 / 1** timing
and **10 / 146** regime dimensions, with eight unique exact full paths and one
ambiguous repeated path. Reuse duplicates the full tree: its one-frame trace
has **20 / 1** timing and **20 / 146** regime dimensions, with no unique full path.
Three submit intervals in reuse instead yield **10 / 2** timing and **30 / 292**
regime dimensions. Thus interval counts cannot stand in for timing-column IDs.

The per-action flag does not add individual API draw IDs to these exports.
Hardware `fe__draw_count.sum` values are **10**, **10** and **17** in the three
single-frame cases, versus **6**, **6** and **12** application draw calls. This
does not diagnose the discrepancy or establish that hardware counts should equal
API calls. No inferred per-draw counters follow.

The next product slice can qualify single-frame, unique exact exported marker
paths with marker-range scope and deterministic ambiguity refusals. Original
queue/submission/action IDs, multiple-column sample identity and physical counter
scaling remain unavailable. R-022 stays In Progress.

Final qualification is complete/pinned as
**bundle-40d1c8f983a49246a26d5294f20636bb**, with all fifteen native sources
independently pinned. All **35** serial CPU checks pass in **353.28 s**; the
source/binary/imported-file hashes and retention receipts are verified. The
snapshot distinguishes the source-built 0.4.9 payload from its installed 0.4.1
generic importer.
