# Performance investigation (R-009)

R-009 remains **In Progress**. Version 0.3.3 adds bounded, pure parsers for
observed GPU Trace text exports. No profiling submission, metric-query or
performance-comparison MCP tool is implemented yet; the existing 22-tool surface
is unchanged. The experiments below establish interfaces and a useful workload,
not completion of the product workflow.

## Real interface probes, 2026-09-25

The owner approved temporary per-user access to the three NVIDIA profiling
capabilities. On this session the device nodes were absent, so the documented
`nvidia-modprobe -f` procedure first recreated them with root-only mode 0400.
The runner recorded the original ACLs and `DeviceFileModify: 1`, granted only
`vordhosbn` read access, and disabled automatic mode rewriting for each bounded
batch. Its `finally` path restored the original ACLs and modify settings and
verified exact equality afterward. The default root-only nodes remain present.
No driver reload, display restart, persistent boot configuration or GPU clock
change was performed. See the
[NVIDIA capability procedure](https://developer.nvidia.com/nvidia-development-tools-solutions-err_nvgpuctrperm-permission-issue-performance-counters).

One windowed fixture trace on Nsight 2026.3 and two private compute-workload traces
on each release succeed with exit 0 and confirmed process cleanup:

- Nsight **2026.3.1.0 / build 38722833**.
- Nsight **2026.2.0.0 / build 37991608**.
- Linux x86-64, RTX 3080 Ti, driver **615.71.09**, KDE Wayland/Xwayland.
- GCC **16.2.1**, source-built glslang **16.4.0**; experimental target uses `-O2`,
  shaders use `-V --target-env vulkan1.3 -g0` without `-Od`.
- Activity `GPU Trace Profiler`, Ampere GA10x, `Throughput Metrics`,
  `--set-gpu-clocks unaltered`, `--auto-export`, screenshots disabled;
  exported reproduction records confirm clocks unaltered and multi-pass disabled.

Compute traces start after 30 submits and stop after three, with a 1000 ms trace
limit and a 70-second owned-process deadline. They profile a live application;
there is no generated GPU replay or replay-reset measurement. The existing
desktop remains active, so background GPU activity and boost variation remain
possible. The earlier I-033 counter-permission failure is resolved for these
authorized runs; it was a prerequisite failure, not evidence that tracing was
universally unavailable.

## Controlled workload and preliminary evidence

The private GLSL prototype transforms 16,384 uint32 values through 2,048 dependent
xorshift/add iterations. The inefficient and efficient variants differ only in
local size: 1 versus 64. Every invocation writes one independent output element.
The target checks the SPIR-V literal local size against its dispatch argument.
The C++ runner independently checks all final-frame outputs using wide arithmetic
and explicit modulo 2^32 reduction, with zero tolerance.

Two separate synchronization-validation launches pass with validation active and
clean. Eight measurement launches then alternate variant order across four
repeats, each with 30 warmup submits and 40 measured submits. Validation is off in
measurement launches. Final-frame numerical output passes in every run. Raw
timestamp pairs, valid bits, period, elapsed ticks and per-frame nanoseconds are
retained; the timing interval brackets dispatch with TOP/BOTTOM_OF_PIPE and does
not include host readback. It can include scheduling and pipeline overhead.

| Evidence | Local size 1 | Local size 64 |
| --- | --- | --- |
| Application median dispatch time, range across four launches | 637.920–638.256 µs | 50.560–50.720 µs |
| 2026.3 Nsight event durations, three exported rows | 0.602560, 0.603040, 0.602592 ms | 0.061536, 0.059424, 0.060064 ms |
| 2026.2 Nsight event durations, three exported rows | 0.602336, 0.602816, 0.602464 ms | 0.060928, 0.059168, 0.060544 ms |
| Nsight `smsp__thread_inst_executed_per_inst_executed.ratio`, both releases | 1 | 32 |
| Corresponding exported `.pct`, both releases | 3.125 | 100 |

The lane-use metrics support a diagnosis of wasted warp lanes in the one-thread
workgroup variant. Changing local size preserves independently checked output
and lowers measured duration. These are preliminary private experiments: each
Nsight variant/release has one fresh trace, and its event rows are not independent
fresh-process repetitions. Nsight terminates these targets after capture; only
their setup records survive. Numerical correctness comes from the separately
labeled application-validation runs, not the trace inventories. Product fixture
integration, repeated profiling through MCP, diagnosis/edit/build verification
and a complete performance acceptance report remain unfinished.

## Export contract and parser boundaries

The documented `--auto-export` produces a binary `.ngfx-gputrace` plus text files
named `.xls`. Only the text exports are parsed. No private binary format is read.

| Export | Observed shape |
| --- | --- |
| `REPRO_INFO.xls` | Unique key/value TSV entries: producer, hardware and collection settings. |
| `FRAME.xls` | One `GPU frame time` row and positional numbers; no value header. |
| `GPUTRACE_FRAME.xls` | Unique metric names with positional numbers; no value header. |
| `D3DPERF_EVENTS.xls` | `event_text` header followed by `time_ms` columns; repeated event names are preserved. |
| `GPUTRACE_REGIMES.xls` | `flattened_event_name` followed by metric columns; repeated headers and event names are preserved. |

The graphics trace has three unnamed numeric positions, while the compute traces
have one. No general mapping to frames, repetitions or min/mean/max is established.
The parser preserves column position, original numeric spelling and a finite
double value. It performs no unit conversion or statistical aggregation. In
particular, the observed `cycles_elapsed.avg.per_second` values near 1930 do not
establish Hz: exporters can scale display values. Only `time_ms` explicitly names
its unit. A later query must report unresolved units rather than infer physical
scaling from metric suffixes.

Parsers enforce a 16 MiB input limit, 4,096 rows, 4,096 columns including the label,
262,144 total cells and 4,096 bytes per field. They reject invalid UTF-8, control
characters, ragged/empty tables, nonfinite/malformed numbers, duplicate frame
metrics, duplicate reproduction keys and missing required reproduction settings.
LF/CRLF and a final row without newline are accepted. Reproduction parsing gates
on the two exact observed producer strings and Vulkan, and returns selected
settings while excluding host/process identity and command lines. These checks
validate a format; they do not authenticate caller-supplied provenance.

Sanitized real subsets are in `tests/fixtures/profile-2026.3.1` and
`tests/fixtures/profile-2026.2.0`, with raw/fixture hashes. The configured check is:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_evidence_check_run
```

The private full-export probe additionally exercises all four parsers and the
reproduction parser on all five complete exports. It establishes broader input
coverage than the selected regression subsets.

## Validation and retention

At 0.3.3, GCC Debug passes `ngm_profile_evidence_check` (0.06 seconds), the existing
`ngm_nsight_evidence_check` (0.96 seconds) and `ngm_check_isolation` (0.16 seconds).
The complete 27-check suite was not rerun for this isolated parser slice. The last
full aggregate remains the 26/26 result at 0.3.2. Independent fresh-context review
found no blocking parser defect; its fixture attribution and regression-coverage
notes were corrected and re-reviewed. All 22 sanitized fixture files were checked
as exact ordered projections of their raw inputs.

The experiment/parser snapshot is complete and explicitly pinned as
**`bundle-d345e0dadab7961610746f6f2d4845e2`** in `artifacts/performance-evidence`.
All 284 payload hashes verify, and a fresh server confirms the persisted pin.
It retains all five traces/exports, three permission grant/restore records,
private C++ runners and shaders, ten application runs, reviews, test logs and
parser source/binaries. The private workload binaries report 0.3.2; the parser
checks/server publication use 0.3.3. Those identities are not interchangeable.
The snapshot's documentation predates its own publication reference.

Publication receipts and restart verification are in
`build/performance-20260925-publication`. The earlier failed probes remain pinned
separately as `bundle-8d30dab0cf976915a34adff7b483ad10` in the same store.

A separate fresh-context publication audit passes: all 284 payloads, 22 fixture
projections, three permission-restoration records, five traces and ten application
launch records agree. It independently recomputes all 324 retained timestamp
conversions and the documented median ranges. No slice blocker remains; this
audit does not mark the remaining R-009 product work complete.
Its report, verifier, results and publication/restart receipts are separately
pinned as `bundle-2c7403abbb1b1bef4fa54e1b3e51583e` in the same store; all 12 review
payload hashes and the persistent pin after restart verify.
