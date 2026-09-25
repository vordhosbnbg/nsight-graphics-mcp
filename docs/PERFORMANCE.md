# Performance investigation (R-009)

The **R-009 performance group completes at 0.4.0**, with 26 shared stdio/HTTP
tools. Asynchronous GPU Trace jobs, retained metric queries, controlled fixtures
and bounded repeated comparisons support a reviewed source-repair workflow.
Hardware acceptance uses the explicitly retained **0.3.6 development build** on
two matching Nsight releases. The final 0.4.0 build passes all 30 CPU checks,
focused numeric-guard checks and identical offline comparisons on the real
cohorts. Independent acceptance and the pinned snapshot are detailed below.

## Profiling service

`profile` launches a fresh target through the documented GPU Trace CLI. It shares
the capture job coordinator, GPU reservation, owned-process cleanup and managed
artifact store. Poll `job_status` and cancel with `job_cancel`; client lifetime
follows the existing stdio/HTTP service policy. Failed attempts retain available
logs/evidence; only complete successful bundles support profile inspection.

Required inputs are absolute `executable`, absolute `working_directory`, and
`settings.architecture` (an architecture configuration name advertised by the
selected `ngfx` help, such as `Ampere GA10x` on the tested installation). This
selector is requested configuration, not independent GPU identification.
Optional `arguments`, `pin`, `timeout_ms` and `application_output_option` have the
same ownership/evidence separation as capture. Target argument tokens currently
use the existing restricted CLI serialization; arguments requiring spaces or
quoting are rejected.

Settings default to `delimiter: submits`, `start_after: 30`, `limit: 3`,
`duration_ms: 1000`, and `metric_set: Throughput Metrics`. The delimiter can also
be `frames`. Bounds are 0–1,000,000 start intervals, 1–1,000 collected intervals,
1–10,000 ms trace duration, and 1–600,000 ms job deadline (default 120,000).
Skipping intervals does not establish adequate warmup. GPU clocks are always
`unaltered`, screenshots are disabled, and exported settings must confirm that
multi-pass metrics are disabled. The service never changes profiling permissions.

`profile_metadata(profile_id)` returns observed producer/GPU/driver, requested
and exported settings, and hashed trace/export references. `profile_metrics`
accepts `profile_id` and one of `frame_duration`, `frame_metrics`,
`event_durations`, or `regime_metrics`. Row pagination uses `offset`/`limit`
(default 0/50, maximum 100); independent numeric-column pagination uses
`column_offset`/`column_limit` (default 0/32, maximum 64). Both offsets are bounded
at 4096, and pages have a 240 KiB row budget. Values retain original text,
finite numeric value and export position. `unit` is `ms` only for explicit
`time_ms` event-duration columns; otherwise it is null. Repeated labels/headers
remain distinct positions, with no inferred event IDs or statistical meaning.

Queries lease the bundle, check index/report/manifest and producer consistency,
validate inventoried references, and verify hashes of text exports they read.
The proprietary trace is retained as a reference, not decoded. Queries work after
restart without Nsight or GPU access; caller imports do not become qualified
service-produced profiles. Successful trace output is bounded to 1 GiB, 256
entries and three levels of recursion; each parsed table is bounded separately.

## Repeated profile comparison

`profile_compare` accepts `baseline` and `candidate` arrays of 2–8 distinct
service-produced profile IDs each, a `table`, exact row `label`, zero-based
numeric `column_index`, and required `workload_policy` / `warmup_policy` strings
(up to 2048 bytes each). Each profile must come from a distinct collection job.
The policies are returned as `caller_unverified` declarations; the tool does not
establish equivalent inputs, output correctness or adequate warmup.

Producer, exported GPU name, driver, exported settings and requested collection
settings must match across both groups. Matching device names do not establish
physical GPU identity. The process deadline may differ; application builds and
arguments may differ for a repair. Each run includes its job identity and hashed
source table/report references; the report retains application executable hashes,
launch arguments and tool provenance for audit. Build/source correctness needs
separate evidence. All bundles remain leased for the entire comparison.

Every trace contributes one median of its exact-label rows at the selected
column. Groups report the minimum, maximum, mean and median of those run medians,
plus each trace's row count and corresponding within-trace statistics. Unequal
row counts do not give one fresh run more weight. Original rows remain available
through `profile_metrics`; no outliers are removed. Headerless columns remain
positions without inferred sample or min/mean/max semantics. Only event durations
have explicit `ms` units; other units remain null.

`median_difference` is candidate minus baseline. `candidate_over_baseline` is
the ratio of group medians; zero denominators or overflowing arithmetic produce
null. `run_median_range_order` reports strictly lower, strictly higher, or
overlapping/touching observed run-median ranges. This is descriptive evidence,
not a confidence interval, a significance test or a claim that higher/lower is
better. It says nothing about unobserved clock variation. Collection profiles
are live-target GPU Traces, without replay.

The CPU check `ngm_profile_compare_check` exercises actual service jobs through
CLI stand-ins and an actual offline MCP comparison after restart. It checks equal
run weighting, provenance/setting/column/label mismatches, duplicate IDs, bounds,
explicit versus unresolved units and finite arithmetic at extreme values. These
stand-ins establish product behavior, not real GPU compatibility or repair
acceptance. The fresh-context static review is retained at
`build/profile-comparison-review/review.md`.

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
and full acceptance remained unfinished at that prototype milestone; the later
completion evidence is recorded below.

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
its unit. Queries report unresolved units rather than infer physical
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

## Production service slice — 0.3.4

The full GCC Debug CPU aggregate passes **28/28** checks (262.14 seconds).
After the final reviewed inventory-bound correction, the focused
`ngm_profile_workflow_check` passes (9.35 seconds), including the 520-file
application-output regression. Backend and query-surface reviews have no
remaining blockers. CPU stand-ins establish lifecycle and validation behavior;
the following separate runs establish actual tool compatibility for this slice.

The C++ MCP protocol client in `build/profile-service-integration/Probe.cpp`
submits one private local-size-1 workload on each matching release. Both jobs
succeed with confirmed cleanup, released GPU reservations and completed
publication. `profile_metadata` and all four `profile_metrics` table queries
succeed; metadata remains identical after an offline server restart. All server
exits are 0. The approved access wrapper verifies exact ACL/DeviceFileModify
restoration after the batch. No failed integration attempt occurred in this batch.

| Nsight release | Pinned service bundle | Store under `build/profile-service-integration` |
| --- | --- | --- |
| 2026.3.1.0 / 38722833 | `bundle-4d8ede41ce24ccba7b9b2f564edb28a5` | `run-2026.3/store` |
| 2026.2.0.0 / 37991608 | `bundle-4770884ecff671d4e20a79a9a13d1f6c` | `run-2026.2/store` |

These runs use the 0.3.4 server and the previously retained 0.3.2 private target;
source, binary and shader identities remain separate. They exercise an actual
MCP protocol client, not a new Codex-client acceptance workflow. Each table query
reads the first two rows/columns, proving interface retrieval rather than a
complete repeated-performance comparison. Collection settings and hardware match
the probe configuration above. Product workload integration and repeated
source-repair/measurement acceptance remain required before completing R-009.

Independent fresh-context acceptance review finds no service-slice blocker. It
checks both completed jobs, 54 manifest file sizes, all 12 trace/export hashes,
all eight actual metric-table calls and all 18 returned numeric cells against raw
exports, metadata equality after restart, pins and recorded permission
restoration. Separate fresh MCP `artifact_info` calls also confirm both original
service bundles remain complete and pinned after restart. This review retains
the scope limits above; it does not complete R-009.

The service-slice snapshot is complete and explicitly pinned as
**`bundle-9de7695e0225eb604775a6fe52c5a4dd`** in
`artifacts/performance-evidence`. All 124 payload hashes verify, and a fresh server
confirms the persisted pin. It retains the two original stores, integration
reports/runner and permission records, final service/check binaries, source
snapshot, CPU logs and independent reviews. Publication/restart receipts are in
`build/profile-service-publication`; the snapshot's documents predate this
publication reference. Imported snapshot contents remain archival evidence;
profile tools use the original service-produced bundles listed above.

## Integrated performance fixture — 0.3.5

The ordinary `ngm-vulkan-fixture` now includes `performance-underfilled` and
`performance-reference`. They use 1- and 64-invocation workgroups for the same
2,048-iteration uint32 transform. The fixture derives dispatch size from the
retained SPIR-V literal LocalSize so a shader repair can keep the same scenario.
Width × height is bounded at 16,384. Explicit `--warmup` selects excluded submits;
the existing zero-based `--frame` selects the last submit. Every frame retains
input/output arrays, including warmup. The seed-derived input is identical across
frames, unlike the correctness fixture's frame-varying affine inputs.

The twelve-shader bundle records per-shader compilation profile and arguments.
Only the two performance shaders use `-g0` without `-Od`; the diagnostic shaders
retain `-g -Od`. Source, bytecode, compiler and executable identities are retained
independently. Older override bundles must be rebuilt to supply the complete
inventory and per-shader compilation metadata. See [FIXTURE.md](FIXTURE.md).

Application timestamps retain their own evidence origin, raw counters, valid
bits, nanosecond period, elapsed ticks and converted nanoseconds. The fixture and
runner reject invalid counter bits and ambiguous wrap intervals. The measured
GPU interval brackets dispatch and can include scheduling/pipeline overhead;
it excludes host readback. Every dispatch is followed by a fence wait, JSON
readback serialization and hashing, so this is an isolated-dispatch workload,
not continuous throughput. No GPU clock or profiling permission changes occur.

The opt-in C++ hardware runner is separate from CPU CTest:

```sh
cmake --build --preset linux-gcc-debug --target ngm_performance_integration_run
```

It first runs both variants with synchronization validation at 33×35 elements,
seed UINT32_MAX, one warmup and two measured submits. It then disables validation
for four fresh launches per variant in alternating order, at 128×128, seed 42,
30 warmup and 40 measured submits. A wide-integer CPU oracle independently checks
every output element of every frame with zero tolerance. Build/GPU and per-variant
shader identities must remain stable across the launch group. The report records
per-launch duration statistics, first/second-half medians, across-launch ranges
and paired median ratios. `status: pass` means successful numerical/timing
validation; `comparison_status` separately reports observed range separation or
an inconclusive comparison. Neither outcome substitutes for actual source repair
or Nsight profiling acceptance, which remained subsequent work at 0.3.5.

The final 0.3.5 batch is
`build/linux-gcc-debug/tests/performance-integration/run-97579-35035408232136`.
It passes ten fresh launches and **566 frames**, with all elements matching the
independent oracle exactly. The 320 measurement samples (eight launches × 40)
exclude warmup; the two validation launches contribute four additional timed
samples. Hardware is RTX 3080 Ti / driver 615.71.09, Vulkan 1.4.351, 64-bit
timestamps, GCC 16.2.1 Debug host code and source-built glslang 16.4.0 performance
shaders. The existing KDE Wayland/Xwayland desktop remains active. No repository
CPU checks ran concurrently with this final batch.

| Final application-timestamp evidence | One-thread workgroups | 64-thread workgroups |
| --- | --- | --- |
| Median dispatch duration, range across four launches | 638.256–638.816 µs | 50.576–50.832 µs |
| Full observed range across 160 measured submits | 636.800–3321.376 µs | 50.016–230.976 µs |
| Per-launch mean duration range | 638.278–1230.104 µs | 50.691–88.194 µs |

Paired launch-median ratios are **12.561–12.620**. This is a measured difference
between known variants on this machine, not a fixed speedup guarantee. Variability
is material: two one-thread launches have second-half medians near 1681 µs versus
first-half medians near 638 µs, and some 64-thread submits reach 231 µs. The data
do not establish stationary clock/load conditions. The active desktop, uncontrolled
clocks and readback cadence remain interpretation limits; no particular cause for
the spikes is established. Full samples and first/second-half summaries are kept.

At this slice, GCC Debug passes **29/29 CPU checks** (284.71 seconds). The final
36–64-bit/upper-bit validation correction then passes the focused
`ngm_performance_evidence_check` (15.29 seconds) and the final real batch above.
The aggregate predates that narrow correction and was not repeated afterward.
The initial GPU batch ran partly alongside CPU checks and is superseded by the
final batch; its numbers are not the qualification table above. These application
measurements remain distinct from the earlier Nsight service probes.

Fresh-context acceptance independently verifies all **566 frames / 9,181,970
output elements**, input generation, timestamp conversions, warmup exclusion,
compiler/shader/executable identities, synchronization-validation records and
reported statistics. Its 7,774 assertions and 1,018 hash checks pass with no
fixture-slice blocker. The review explicitly preserves the variability above.
The verifier and results are in `build/performance-fixture-review/acceptance`.
A separate actual `ngm-experiment` CLI smoke run with `--warmup 0 --frame 0`
also passes its execution/evidence contract; it is outside the ten-run table.

The final fixture snapshot is complete and explicitly pinned as
**`bundle-a5fbe932759bd51e5785cbc361b26b3a`** in
`artifacts/performance-evidence`. All **990 payload hashes** verify, and a fresh
server confirms the persisted pin. It retains all ten final launches, the
zero-warmup CLI smoke run, source/binaries, CPU/build logs and independent reviews.
Receipts are in `build/performance-fixture-publication`; the snapshot's documents
predate their own publication reference. The superseded initial GPU batch is not
part of this qualification snapshot.

## Actual selected-scenario source repair, 0.3.6 validation

The normal `performance-underfilled` fixture now passes real GPU Trace collection
on both qualified releases. Two initial smoke traces established the exact event
label and exported columns. Codex inspected the actual MCP `profile_metrics`
results and separately retained application source/setup: the dispatch-specific
`smsp__thread_inst_executed_per_inst_executed.ratio` was 1, with its `.pct` value
3.125, while the selected shader declared `local_size_x = 1`. These observations
support underfilled warp lanes as a bottleneck. Whole-frame values differed and
were not substituted for dispatch-specific evidence. General counter scaling
remains unresolved in the product API.

The [NVIDIA compute architecture discussion](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-system-architecture.html)
describes CTA allocation constraints and increasing small thread groups to 64.
An actual edit changes `performance-underfilled.comp` from local size 1 to 64
in an isolated retained shader bundle. The same scenario, executable, transform,
inputs and bounds guard remain selected. The pinned glslang binary recompiles
that edited source using `-V --target-env vulkan1.3 -g0`; its manifest is updated
from the resulting bytes. The fixture reads actual SPIR-V LocalSize and changes
dispatch group count from 16,384 to 256. This is not a selection of the prebuilt
`performance-reference` scenario. The resulting bytes match that reference, as
expected for this one-line repair without shader debug information.

`build/performance-repair/{source.patch,rebuild.json,diagnosis/diagnosis.md}`
retain the diagnosis, actual patch and compiler command/hashes. Source identities
are `c3319c655bb280522c517c2ff94292f5447922206d93f96c15d9f5ad0c461bb0`
(before) and `2b5b828ec2e07fdfdfd78d533ad44dfdcc3d3526de7e3bd90c23e12ed2024c72`
(after); SPIR-V identities are
`7566b21171a4ab97b3bae32a2afb494894e5583416d6e4310b0353dea95549c7` and
`4aab2dde6637932aaec8b98631792634b01d1febc568dc82afaa4fbee75f221a`.

The C++ numerical harness runs two synchronization-validation launches
(33×35, seed UINT32_MAX, one warmup and two measured submits), then four launches
per variant at 128×128, seed 42, 30 warmup and 40 measured submits each. Alternating
pair order limits a simple order bias. Its independent wide-integer oracle checks
all **566 frames / 9,181,970 output values** exactly, including warmup frames.
The same `performance-underfilled` scenario uses the baseline or edited shader
bundle explicitly. All ten runs pass. Application timestamps remain distinct
from Nsight measurements. Report:
`build/performance-repair/numerical-validation/run-103840-36713159012608/report.json`.

Application launch medians range from **638.032–638.432 µs** before to
**50.624–51.136 µs** after; paired median ratios are 12.4784–12.6113. All 160
measured samples per variant remain retained: before **636.768–3130.592 µs**,
after **50.016–147.264 µs**. Some second-half medians drift materially, reaching
1483.312 µs before and 128.128 µs after. Medians do not erase these tails or prove
stationary clocks. The application cadence still fence-waits, reads back,
serializes and hashes each frame; these are isolated dispatches, not continuous
throughput measurements.

The C++ MCP batch then collects four fresh baseline and four fresh repaired traces
per release, alternating pair order. All use 30 skipped submits, three collected
submits, 1000 ms collection cap, `Ampere GA10x`, `Throughput Metrics`, unaltered
clocks, disabled multi-pass metrics/screenshots and no replay. The actual
`profile_compare` tool summarizes each trace's three exact-label rows before
comparing the four trace medians. Comparisons remain separate per producer.

| Producer | Original trace medians (ms) | Repaired trace medians (ms) | All original rows (ms) | All repaired rows (ms) | Repaired/original group median |
| --- | --- | --- | --- | --- | --- |
| 2026.3.1.0/build 38722833 | 0.602656–0.604064 | 0.060512–0.061824 | 0.602080–0.648288 | 0.059872–0.063584 | 0.101249 |
| 2026.2.0.0/build 37991608 | 0.602336–0.604160 | 0.059872–0.062560 | 0.601760–0.606976 | 0.059552–0.063648 | 0.099995 |

In both cohorts, all dispatch-specific lane-ratio rows change from 1 to 32, and
the corresponding `.pct` rows from 3.125 to 100. This supports the diagnosis;
no source-line timing, general executed-state inspection or universal speedup is
claimed. There are four independent application launches per group, not twelve
independent repetitions. The desktop remains active and clocks are uncontrolled;
collection settings and observed variability are reported without claiming
statistical significance. Application timestamps and Nsight durations use
different instrumentation and are not pooled.

All runs use the recorded RTX 3080 Ti / driver 615.71.09 on KDE Wayland/Xwayland,
GCC 16.2.1 Debug host build and glslang 16.4 performance shaders. Exact context,
server/fixture identities and harness commands are in
`build/performance-repair/{host-build-context.json,harness-build.json}`.
`repeated-2026.3/report.json` and `repeated-2026.2/report.json` contain every
request/response, job, source identity, metric page and comparison. All 16 jobs
confirm cleanup and release the GPU reservation. Their original service bundles
are persistently pinned in the adjacent `store` directories, verified after
server restart. The two smoke profiles are pinned separately.

Approved temporary capability access is restored after the smoke batch, the
failed harness batch and the corrected repeated batch; the exact journals are
under `access-smoke`, `failed-attempts/access-repeats-nonexecutable`, and
`access-repeats`. Actual final ACL inspection confirms root-only 0400 with no
extended ACL. No driver reload, desktop restart or clock change occurred.
I-035/I-036 preserve the two harness setup failures and their corrections.
Independent acceptance and final publication are recorded below.

The independent fresh-context acceptance report is
`build/performance-repair-review/review.md`. It finds no remaining code or
acceptance blocker. Its main verifier passes 16,298 assertions and 1,437 hash
checks; its separate diagnosis verifier passes 899 assertions and 62 hash checks.
Together they verify 1,255 retained frames / 20,470,546 exact output values,
including every retained application readback from all eighteen real profiles.
These readbacks remain application evidence, separate from the audited raw Nsight
exports. Both verifier scripts and complete initial/final outputs are retained.
The frozen server and CMake contents preserve exact 0.3.6 hardware identities.

The final 0.4.0 aggregate passes all 30 CPU checks; focused checks and a further
fresh-context review cover the final pre-conversion numeric range guard. The
0.4.0 binary reproduces all six real-cohort comparison responses exactly using
retained evidence without GPU access. See BUILD_VALIDATION.md for commands and
logs. Managed archival publication completes this acceptance record.

The complete **2914-payload** qualification snapshot is complete/pinned as
`bundle-d4db174806f33a707b908900599404d5`, store `artifacts/performance-evidence`.
Every payload hash verifies, and an actual fresh-server `artifact_info` call
confirms the persistent pin after restart. Receipts and the payload manifest are
in `build/performance-repair-publication`. The archive includes all raw captures,
application evidence, source edits/build commands, failed attempts, permission
journals, independent audits, source/binary snapshots and checks; isolated tool
caches are excluded. Snapshot documents predate their own publication reference
and final roadmap bookkeeping. Original service-produced profile bundles remain
separately pinned in their respective stores and remain the profile-query targets;
the archive is an imported qualification record, not a new service-produced trace.

This completes the R-009 performance group and its minor increment to **0.4.0**.
No roadmap items remain In Progress, Pending or Blocked. The observed scope is a
source-available isolated-dispatch repair on the recorded hardware; source-line
profiling, stationary clocks, generic metric scaling and continuous-throughput
performance are not inferred from these results.
