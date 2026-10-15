# Timing and counter scope outcome

Date: **2026-10-15**. Implemented slices: **0.4.8–0.4.11**. This report closes
step E/R-022's conditional assessment from
[CAPABILITY_FOLLOWUP_PLAN.md](CAPABILITY_FOLLOWUP_PLAN.md). It distinguishes
available export facts from unavailable physical attribution. It does not claim
isolated draw time, exact application counters or a causal overhead correction.

## Available evidence and limits

| Requested information | Verified outcome |
| --- | --- |
| Collection controls | Nullable per-action/HES requests are retained with exact argv. Explicit per-action off/on values match the reproduction export. Ampere HES enable requests refuse; actual GB20x HES activation remains unqualified. |
| Valid empty event/regime exports | Qualified header-only tables preserve headers and zero rows; all frame metrics stay usable. Empty comparisons refuse. No event or zero-valued measurement is invented. |
| Same-profile marker duration | `profile_marker` qualifies a unique exact single-frame event/regime path on 2026.3.1.0/build 38722833 with exported Time Every Action Yes. Original raw text, positions and hashes are retained; `time_ms` establishes ms. |
| Action/draw identity | **Unavailable.** Event rows are markers without original API action/queue/submission IDs. Numeric graphics-capture IDs and equal counts do not create a join. A multi-draw marker remains a range. |
| Repeated markers/reused recordings | Raw rows remain queryable. Cross-table occurrence association **refuses** when full paths repeat. A matching row order cannot supply missing occurrence keys. |
| Several collected intervals | Raw positions remain available. Repeated time headers do not identify original frames/samples; interval counts do not establish timing-column IDs. The correlated query refuses those shapes. |
| Timing sanity | The 0.4.9 matrix compares scene envelopes to independent timestamps within 32–160 ns. The 0.4.11 repeated cohort agrees within 32–96 ns. Both use the predeclared max(0.025 ms, 25%) scope/tolerance. Private timestamps stay outside product evidence. |
| Instrumentation overhead | **Causal cost unresolved.** Four alternate-order fresh off/on pairs after 30 warmup presents have mixed changes of about −0.54% to +0.98%. All observations and 92 telemetry probes are retained. No stable per-marker estimate or subtraction follows. |
| Frame/regime metric cells | Exact finite raw cells and qualified unavailable `nan` positions are retained. Table/path membership is established; physical scaling, aggregation and application-only attribution remain unresolved where definitions/evidence are absent. |
| Isolated draw or fragment/depth work | **Unavailable.** No counter is advertised with isolated-draw or fragment-only scope. Instruction totals are not fragment instructions; a raw hardware draw-count name does not establish API-call count. |
| New two-release support | **Unqualified.** These new controls/associations target the measured 2026.3 producer. Historical two-release profiling keeps its original scope and build identities. |

The fifteen-profile [correlation matrix](PROFILE_CORRELATION.md) covers direct,
indexed and reused recordings, nested/repeated labels, one-/three-frame and
submit boundaries, with timing off/on. The [marker query](PROFILE_MARKERS.md)
qualifies the available subset and deterministic refusals. The
[repeated cohort](PROFILE_OVERHEAD.md) adds eight fresh runs, 326 independent
timestamped submissions, all 1,168 frame cells, 80 selected counter facts and
913 actual schema-validated MCP results. No source is discarded as an outlier.

## Counter facts and physical interpretation

The following exact names are independently audited in every repeated-cohort
frame export. MCP pages preserve all numeric text, values and source positions.
Their `.sum` spelling is retained without inventing a physical aggregation
definition. Units and normalization stay null in product results.

| Requested subject | Exact raw metric name |
| --- | --- |
| Draw count | `FE_A.TriageSCG.fe__draw_count.sum` |
| ZCull input | `GPC_A.TriageSCG.raster__zcull_input_samples_realtime.sum` |
| ZCull accepted | `GPC_A.TriageSCG.raster__zcull_input_samples_op_accepted_realtime.sum` |
| PROP input | `GPC_A.TriageSCG.prop__input_pixels_type_3d_realtime.sum` |
| Pixels to depth ROP | `GPC_A.TriageSCG.prop__prop2zrop_pixels_realtime.sum` |
| Depth-passed pixels | `GPC_A.TriageSCG.prop__prop2zrop_pixels_op_passed_realtime.sum` |
| Pixels to color ROP | `GPC_A.TriageSCG.prop__prop2crop_pixels_realtime.sum` |
| FMA instructions | `SM_C.TriageSCG.smsp__inst_executed_pipe_fma.sum` |
| FMA-heavy instructions | `SM_C.TriageSCG.smsp__inst_executed_pipe_fmaheavy.sum` |
| SM instructions | `SM_A.TriageSCG.sm__inst_executed_realtime.sum` |

Six direct API calls occur in every private selected-scene record. Frame draw
cells are 10 in six runs and 71 in two; timing-on scene regime cells are 7, 7,
7 and 3. The export/fixture difference has no established cause. The
[NVIDIA GPU Trace guide](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html)
describes collection of other GPU activity and a per-action timing penalty;
that description does not diagnose these particular cells. The scoped exact-name
documentation review establishes no version/architecture-bound normalization for
this qualification. It does not prove definitions cannot exist elsewhere.

Thus `exported_marker_regime` means an exact associated producer cell, with
explicit unresolved physical semantics. It does not assert a hardware isolation
boundary. The frame table is also an export domain, without a claim that every
cell measures only this application's six draws. Even the raw `GPU frame time`
row lacks the `time_ms` unit header used by event durations; its unit is not
silently inferred. A finite zero stays zero, and missing data remains null with
coverage reasons. Raw imports never become service-produced typed profiles.

## Retention and revisit conditions

The complete qualification snapshots remain pinned:

- Controls/undefined cells: `bundle-193d06c97b109c55fb8a30e35f12b8f9`.
- Graphics matrix: `bundle-40d1c8f983a49246a26d5294f20636bb`.
- Marker query: `bundle-b8d3515b5cd6079e2ba2c8cb5637067e`.
- Repeated controls/counter facts: `bundle-8d667f47f660fc46a582f440ee786c5e`.

Native source profiles are independently pinned in the stores recorded in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md). I-040–I-045 preserve failures,
ambiguities and inconclusive results. The final [completion audit](CAPABILITY_FOLLOWUP_COMPLETION.md)
verifies the retained identities and final reader separately. Installed generic
imports report 0.4.1; payloads retain the actual tested collection/reader versions.

Broader claims require documented occurrence/sample identifiers, exact
version/architecture metric definitions and independent new-input association
checks. A controlled causal-overhead experiment would require separately
accepted machine/background conditions. Neither a private trace decoder nor
implicit clock/permission/other-application changes are selected. Future game
qualification needs its own current run/build/shader/marker evidence; the old
pre-capture band labels do not prove the later selected frame's state.

R-022's accepted conditional outcome is the implemented qualified export subset
plus retained unavailability for the wider scopes. The final release does not
turn those unavailable fields into completed diagnostic capabilities.
