# GPU Trace collection controls

At **0.4.8**, the existing `profile` tool accepts two additional nullable booleans
inside `settings`: `time_every_action` and `hes_enabled`. Omission or `null`
preserves the tool default. The shared surface remains 28 tools; profile
artifact/index schemas remain version 1.

```json
{"settings":{"architecture":"Ampere GA10x","time_every_action":true,"hes_enabled":false}}
```

These fields are shared by stdio/HTTP and the C++ `CaptureRequest::profile` API.
Both are frozen with the job and retained in its provenance, report and index.
They do not change permissions, clocks or another job's environment.

## Requested and observed settings

- `time_every_action: true` passes documented `--time-every-action`. False omits
  that enable switch and requires `Time Every Action: No` in `REPRO_INFO.xls`.
  Both explicit values require matching help and matching exported settings;
  missing or mismatched reproduction data fails the job. Null retains the
  observed tool default without asserting a requested value.
- `hes_enabled` explicitly passes `--hes-enabled=0` or `1`, gated by matching
  help. Enable requests require the documented `Blackwell GB20x` or `T25x GB20x`
  architecture selector. The selector does not verify the actual GPU. Ampere
  requests can explicitly disable HES; enabling it on Ampere is rejected before
  launching a target. Hardware enablement on GB20x is not newly qualified.

The [NVIDIA GPU Trace documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html)
describes per-action instrumentation and its overhead. Installed matching
2026.3.1.0 help limits HES to GB20x or newer. Help discovery accepts option
declaration lines, preventing a flag-shaped diagnostic path from satisfying a
missing-option gate.

`profile_metadata.collection_controls` separates requested values from exported
ones. Per-action timing has `verified_export` for an explicit matched request,
`observed_export` for a reported tool default, or `not_exported` if absent.
HES retains its request and invocation evidence with a null exported value;
active HES state is not established by the qualified export fields.

Old ten-field profile requests remain inspectable. New requests retain both
controls, including nulls. Comparisons normalize old omissions to null, while
different explicit selections remain different collection settings. Exported
settings must still agree; an unspecified default is not assumed equivalent to
an explicit false value.

## Undefined regime values

The exact **2026.3.1.0/build 38722833** regime export can contain literal `nan`
cells. Those positions retain their raw text and return `value: null`,
`status: unavailable` and a reason. Adjacent finite cells remain available.
Metadata and metric-page coverage count available/unavailable values over the
whole table and report `partial` when both occur. A comparison selecting an
unavailable cell refuses with an actionable error; it never substitutes zero
or silently drops a run.

Only this producer, regime table and exact lowercase token are qualified.
Other nonfinite spellings, empty/ragged cells and nonfinite timing/frame values
remain errors. The pure parser defaults to finite-only validation; its explicit
producer policy enables the qualified case. `ProfileValue::value` is now an
optional number, so C++ consumers must check availability before dereferencing.
The nullable metric value is also an MCP response-contract change; clients must
check `status`/`value`. Artifact/index versions remain unchanged because retained
raw tables and producer provenance already distinguish this case.

Two fresh native profiles pass with per-action timing off/on. The off profile
has 146 undefined regime cells; the on profile has none. The earlier failed
attempt had undefined cells with timing on. These observations do not establish
that the collection control causes undefined counters. The sanitized regression
subset and pinned I-040 evidence retain the exact observed input.

These controls do not establish draw correlation or convert frame counters into
draw-local metrics. [PERFORMANCE.md](PERFORMANCE.md) and
[CAPABILITY_FOLLOWUP_PLAN.md](CAPABILITY_FOLLOWUP_PLAN.md) define the remaining
timing/counter acceptance. Exact validation and pinned evidence are recorded in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md).
