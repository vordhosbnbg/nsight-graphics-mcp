# Repeated GPU Trace collection-control assessment

The **0.4.11** runner adds an opt-in `--repeat-overhead` mode to
`ngm_profile_correlation_integration`. This is a validation workload, with no
new product tool or performance correction. R-022 remains In Progress until
the recorded run and final scope assessment are complete.

## Declared measurement plan

Run **four pairs / eight fresh profiles** of `timing-direct`. The explicit
per-action timing order is **off, on, on, off, off, on, on, off**. Reverse each
alternate pair to expose ordering/drift instead of relying on one off/on launch.
Use the identical fixture/shaders, seed **42**, **1024×512**, **30 warmup
presents**, one collected frame, **1000 ms** duration bound, Ampere GA10x /
Throughput Metrics, HES false, unaltered clocks and a **120000 ms** job deadline.
Every launch receives isolated configuration/output/log paths and the same
explicit matching-driver environment. Source bundles are independently pinned.

An isolated synchronization-validation baseline and an ordinary baseline run
first. Actual marker inputs and two whole-scene Vulkan timestamps remain a
private fixture witness. They do not enter product evidence. The runner retains
every raw timestamp and compares no isolated draw timing. The export audit uses
the original declared scene sanity tolerance: **max(0.025 ms, 25% of the private
scene duration)**. Select the actual exported frame label against that launch's
private API witness; do not guess an original frame ID from a table column.

Each launch supplies **one independent observation** for the collected scene.
Retain all eight observations, four paired on/off ratios, group medians and
ranges, with no selective removal or correction. Other private warmup/post-window
samples remain separate. Validate actual MCP schemas, exact exported cells and
hashes, cleanup, restart parity and source pins. Record read-only clock/power/
temperature telemetry and before/after profiling-access observations.

Clocks, desktop load and power state remain uncontrolled. Timestamp commands,
host fence/JSON cadence and monitoring belong to this experiment. Repetition
can describe variation; it cannot by itself establish stable causal per-marker
instrumentation cost. No overhead is subtracted from product durations.
NVIDIA documents a per-action timing penalty and collection of other GPU work
in its [GPU Trace guide](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html).
Machine settings and other applications are not changed by this assessment.

## Run

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_correlation_integration
/absolute/build/tests/ngm_profile_correlation_integration \
  /absolute/build/nsight-graphics-mcp /absolute/build/ngm-vulkan-fixture \
  /absolute/build/shaders/fixture /absolute/Nsight-installation \
  /absolute/validation-parent /absolute/launch-environment.json --repeat-overhead
```

Without the flag the original fifteen-profile direct/indexed/reuse matrix is
unchanged. The repeated mode retains frame metrics and the actual scene query
as well as event pages. Timing-off scene queries explicitly refuse the qualified
per-action association; timing-on queries retain marker-range scope. Source
versions/tool identities are recorded for each run; earlier 0.4.9 and 0.4.10
qualification is not relabelled.

## Native result, 2026-10-15

Both baselines and all **eight** profiles pass on matching Nsight
**2026.3.1.0/build 38722833**, RTX **3080 Ti** / **615.71.09**, with diagnostic
shaders and the recorded KDE Wayland/Xwayland-XCB desktop. Synchronization
validation is active/clean. All selected exports correspond to private fixture
frame **30**, with identical actual marker inputs and six direct calls.
The independent audit verifies **326** private timestamped submissions. Exported
scene durations differ from the corresponding timestamps by **32–96 ns**, within
the declared tolerance.

| Pair | Order | Private scene off (ms) | Private scene on (ms) | On / off |
| --- | --- | ---: | ---: | ---: |
| 1 | Off, on | 0.333184 | 0.333056 | 0.999616 |
| 2 | On, off | 0.331456 | 0.334688 | 1.009751 |
| 3 | Off, on | 0.334112 | 0.334816 | 1.002107 |
| 4 | On, off | 0.333664 | 0.331872 | 0.994629 |

Off/on group medians are **0.333424 / 0.333872 ms**, ratio **1.001344**.
The median paired ratio is **1.000861**; individual changes range from about
**−0.54% to +0.98%**. Every observation is retained. These are descriptive
whole-scene measurements, not a stable causal per-marker overhead estimate.
The **92/92** successful read-only telemetry observations span multiple power/
clock states over the batch; their one-second cadence does not establish clock
stability inside a selected interval. Profiling access matches before/after.
I-044 retains the inconclusive causal outcome and revisit conditions.

All **913** actual MCP tool results pass their advertised JSON Schema 2020-12
contracts and text/structured parity. This includes **16** scene queries across
two server processes: eight available timing-on results and eight explicit
timing-off refusals. Metadata/event/scene results and all eight source pins
survive restart, with owned process cleanup confirmed and GPU reservations
released. The complete **146 × 8** raw frame-metric cells agree with MCP pages.
Ten selected exact counter names yield **80** additional raw frame facts with
null physical units/aggregation definitions.

Those facts strengthen the counter-scope limitation: frame
`FE_A.TriageSCG.fe__draw_count.sum` is **10** in six runs and **71** in two,
despite six private API calls in every selected scene. Available timing-on scene
regime cells are **7, 7, 7, 3**. Their cause and correspondence to application
calls remain unresolved; no record is discarded or reinterpreted as an exact
draw count. I-045 retains these observations. ZCull, PROP and instruction names
remain literal export facts, without isolated-draw/fragment claims or inferred
physical normalization.

Qualification is complete/pinned as **bundle-8d667f47f660fc46a582f440ee786c5e**, with all
eight native sources independently pinned. [BUILD_VALIDATION.md](BUILD_VALIDATION.md)
records exact source paths, hashes and the CPU receipt. No installed-server,
permission, clock, driver, display or helper-closure update is performed. New
2026.2 support remains unqualified. R-022 stays In Progress for its final
scope/conditional-outcome audit.
