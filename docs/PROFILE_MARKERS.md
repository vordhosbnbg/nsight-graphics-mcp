# Retained GPU Trace marker ranges

At **0.4.10**, `profile_marker` brings the shared stdio/HTTP surface to **29
tools**. It returns a duration and requested raw regime cells for a uniquely
associated exported marker range. It works offline on a complete service-produced
profile and creates no capture, job or derived artifact.

## Select a marker

Collect with `settings.delimiter: frames`, `limit: 1` and
`time_every_action: true`. Use the existing qualified launch environment and
hardware/tool prerequisites for collection. Query `profile_metrics` with
`table: event_durations`, then pass the selected `row_index` to `profile_marker`.
Use the profile ID returned by your collection:

```json
{
  "profile_id": "<PROFILE_ID>",
  "event_row_index": 4,
  "metrics": ["FE_A.TriageSCG.fe__draw_count.sum"]
}
```

`metrics` defaults to an empty array for a duration-only query. It accepts at most
**eight distinct exact names**, each 1–4096 bytes. Available names come from the
regime table's paginated `profile_metrics.columns`. Missing or duplicated metric
names return individual unavailable values/reasons. They never select a guessed
column. Out-of-range event rows and malformed arguments are tool errors.

## Evidence and coverage

The source must be **Nsight 2026.3.1.0/build 38722833**, with one collected frame,
one frame-data/time column and exported `Time Every Action: Yes`. The qualified
hierarchy uses eight-space indentation, at most **32 components**, **256 bytes**
per visible-ASCII component and **4096 bytes** per joined path. Slash-containing
names, edge spaces, malformed levels and unconfirmed paths receive explicit
coverage. These are qualification bounds, rather than general restrictions on
Vulkan labels.

Every event path must have an exact same-profile regime anchor before the
indentation interpretation is accepted. The selected full path must occur
exactly once in each export. Repeated sibling markers and reused recordings
cannot be joined by row order. Multiple timing columns do not supply frame/sample
identity. Other producers, collection shapes or spellings remain unsupported;
raw positional queries keep their existing contract.

`coverage.status` is `available`, `unavailable` or `unsupported`, with a reason.
It describes the marker association and duration. Each requested metric has its
own availability: a missing metric, ambiguous column or qualified `nan` cell
can remain unavailable while the duration is available. A finite exported zero
remains zero. Refused associations have null duration/counter values, not zero.

The result retains the profile ID, producer/GPU/driver, source product version,
hashed report/export references, exact raw label, exported path and event/regime
row/column positions. Its selection domain is **`gpu_trace_exported_marker`**.
Paths name exported ranges; original frame, queue, submission, action IDs and
draw membership remain null. Numbers from graphics-capture inventories do not
identify these ranges.

Duration units come from the `time_ms` header. Scope is
**`exported_marker_range`**: a marker may contain several draws and include
pipeline/synchronization work. Instrumentation overhead is retained explicitly
and is not subtracted. No isolated draw execution time or causal overhead
correction is inferred.

Counter scope is **`exported_marker_regime`**, associated by the exact path.
Raw name, text, numeric value and source positions are retained. Unit, aggregation,
normalization and shader-stage isolation remain unresolved where the export does
not establish them. Null units mean unknown scaling. Hardware draw counts do
not establish application call counts, and instruction totals do not establish
fragment-only work. `metric_semantics.isolated_draw` is always false.

## Validation and retained scope

The [0.4.9 graphics matrix](PROFILE_CORRELATION.md) supplies fifteen real
profiles, independent marker API inputs and timestamp sanity on the recorded
RTX 3080 Ti / 615.71.09 configuration. The **0.4.10 reader** verifies every
exported row through actual MCP calls in two fresh processes: **360** marker
responses comprise **32 available**, **48 ambiguous/unavailable** and **280
unsupported** selections. These are 180 selections repeated after restart.
The one-draw and two-draw fixture markers both retain null product draw membership.

All **391** tool results, including capabilities and source inventory checks,
pass actual JSON Schema 2020-12 contracts and text/structured parity. Exact
duration/raw counter text, source hashes/positions, producer/build identities,
unresolved units and missing metric reasons agree with independent retained
exports. Source pins and file inventories remain unchanged after restart.
No new Nsight/GPU process is launched by this validation.

An actual authenticated loopback HTTP process discovers the same 29 tools and
returns identical available, ambiguous and unsupported selections. All **eight**
HTTP tool results pass their actual schemas and text/structured parity; source
inventories/pins remain stable and the owned service exits 0 after session
deletion. The temporary private token is removed and is absent from evidence.

The opt-in C++ verifier is separate from ordinary CPU checks:

```sh
cmake --build --preset linux-gcc-debug --target ngm_profile_marker_integration
/absolute/build/tests/ngm_profile_marker_integration \
  /absolute/build/nsight-graphics-mcp /absolute/source-correlation-run \
  /absolute/validation-parent
```

The pure check uses a sanitized real single-frame subset; process-boundary
checks use C++ Nsight stand-ins for collection-shape/type/provenance behavior,
undefined/duplicate/missing metrics, older-producer refusal and stdio restart.
Stand-ins establish no hardware compatibility. Source captures remain separately
pinned; [BUILD_VALIDATION.md](BUILD_VALIDATION.md) records the current reader's
qualification, pinned as `bundle-b8d3515b5cd6079e2ba2c8cb5637067e`. All **36** serial CPU checks
pass with zero skips. New 2026.2 marker support remains unqualified. The final
**0.5.0** reader repeats retained stdio/HTTP checks; its
[scope report](PROFILE_SCOPE_FEASIBILITY.md) completes R-022's conditional
assessment without adding isolated draw, physical counter or overhead claims.
