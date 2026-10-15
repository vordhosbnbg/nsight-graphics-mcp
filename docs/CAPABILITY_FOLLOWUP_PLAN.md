# Draw-state and profiling capability follow-up

Assessment dates: **2026-10-14–15 (Europe/Sofia)**. Product assessed: **0.4.1**.
Status at assessment: **Planning and feasibility assessment only.**

Implementation authorized by the owner on **2026-10-15**, with a commit and
applicable version increment after each completed capability. R-018–R-022 in
[ROADMAP.md](ROADMAP.md) track execution. The assessment below retains its original
evidence and proposals; implementation/qualification updates are recorded there
and in the capability-specific documents.

Implementation update: A–C complete at **0.4.2–0.4.4**. At **0.4.5**, step D
adds its first qualified recorded-state subset through two separate tools;
[DRAW_STATE.md](DRAW_STATE.md) defines the four-case 2026.3 qualification and
explicit execution/value limits. R-021 stays In Progress for the remaining
assessment. Step E/R-022 remains Pending. The assessment below preserves its
original scope; it is not a statement that all proposed state is now available.

At **0.4.6**, step D also reconstructs bounded per-stage push input bytes through
explicit existing-reader calls, including partial writes and compatible layout
rebindings. Descriptor contents, uniform/material interpretation and general
captured-execution correspondence remain unqualified; R-021 remains In Progress.

At **0.4.7**, step D adds recorded descriptor-set/allocation/layout identities,
prefix/push-range compatibility, disturbance and dynamic-offset mappings. Two
fresh matching 2026.3 captures cover partial set rebinding, descriptor arrays and
reused command buffers; 15 occurrences pass private API comparisons and exact
MCP restart checks. Descriptor contents, uniform/material values and general
captured-execution correspondence still need their feasibility outcome. This
does not extend the proprietary helper closure or complete R-021/R-022.

The subsequent [state feasibility outcome](DRAW_STATE_FEASIBILITY.md) completes
step D/R-021's conditional assessment at **0.4.7**: implemented recorded-state
slices remain qualified, while descriptor contents, execution-time values and
general GPU correspondence remain explicitly unavailable. The two-producer/
fresh-input audit is pinned as `bundle-7269c28d30326b8f1fa3ef64ebd4ed36`.
This documentation-only closure does not add a full-state capability or extend
the helper policy. Step E/R-022 is now In Progress. Earlier versioned implementation
notes above retain their status at those increments.

At **0.4.8**, step E's first implementation slice adds nullable per-action/HES
controls, exported-setting verification and producer-qualified unavailable regime
cells. All 35 serial CPU checks and two fresh matching 2026.3 off/on profiles
pass, including restart and 202 schema-validated MCP results. This qualifies
collection controls and nullable counter handling, not draw correlation or counter
scope. R-022 stays In Progress for the repeated/nested direct/indexed timing
matrix, independent timestamp/overhead sanity and supported counter associations.
See [PROFILE_CONTROLS.md](PROFILE_CONTROLS.md) and BUILD_VALIDATION.md.
Qualification is pinned as `bundle-193d06c97b109c55fb8a30e35f12b8f9`, with both
native source profiles independently pinned. The installed/live server remains
0.4.1; these results bind the source-built 0.4.8 binaries.

At **0.4.9**, step E's graphics timing workloads and export assessment pass six
baselines, fifteen fresh 2026.3 profiles, 400 independent timestamp/API submission
checks, 1,615 schema-validated MCP results and all 35 serial CPU checks. Nested/
repeated markers, direct/indexed draws and reused recordings expose a qualified
single-frame unique-marker candidate and concrete wider identity/scope limits.
Scene totals agree within 32–160 ns under the predeclared tolerance; causal
instrumentation overhead remains inconclusive. No new product query is added.
R-022 stays In Progress for the bounded marker query and remaining scope/overhead
qualification. See [PROFILE_CORRELATION.md](PROFILE_CORRELATION.md), I-042/I-043
and BUILD_VALIDATION.md for exact identities and retained evidence.
The complete qualification is pinned as
`bundle-40d1c8f983a49246a26d5294f20636bb`, with native sources independently pinned.

At **0.4.10**, step E adds `profile_marker` for qualified single-frame unique
exported marker paths, with duration in ms, up to eight exact raw regime metrics
and explicit identity/scaling/ambiguity limits. All **36** serial CPU checks,
**391** retained-profile stdio results and **eight** HTTP results pass, including
restart parity and source pin/inventory stability. The source profiles retain
their 0.4.9 collection identities; no new GPU collection is implied.
Qualification is pinned as `bundle-b8d3515b5cd6079e2ba2c8cb5637067e`. See
[PROFILE_MARKERS.md](PROFILE_MARKERS.md) and BUILD_VALIDATION.md. R-022 remains
In Progress for the remaining scope/overhead assessment. The installed/live
server remains 0.4.1; the source-built surface now has **29 tools**.

At **0.4.11**, step E's repeated control assessment passes two baselines and
eight fresh profiles in four alternate-order off/on pairs after 30 warmup
presents. The independent audit verifies 326 timestamped submissions, 32–96 ns
scene agreement, all 1,168 frame cells and 913 schema-valid MCP results with
restart pins. Mixed timing changes retain an inconclusive causal-overhead
outcome; varying raw counter totals retain unresolved application attribution.
See [PROFILE_OVERHEAD.md](PROFILE_OVERHEAD.md), I-044/I-045 and pinned
`bundle-8d667f47f660fc46a582f440ee786c5e`. R-022 stays In Progress for its final
scope and conditional-outcome audit. No new product tool or installed-server
change occurs.

The owner's priorities are effective state at a selected captured draw, activation
of the existing resource reader, correlated draw timing/counters, and controlled
per-job launch environments, in that order. Draw state remains the main objective.
Reader activation and launch control are prerequisites for collecting and inspecting
useful evidence; they do not establish executed state by themselves.

This assessment changes documentation only. It runs no application, GPU capture,
GPU replay or profiling collection, changes no server configuration, permissions,
clocks or other applications, and leaves eternity-forge untouched. Historical
qualification remains scoped to its recorded product/tool versions. Proposed tool
names and result fields below are design examples, not implemented interfaces.

## Current implementation and feasibility

| Owner priority | Existing behavior | Missing behavior / next action |
| --- | --- | --- |
| 1. Effective draw state | `capture_metadata`, `capture_events` and `capture_objects` provide inventories. `capture_cpp_source` and `capture_cpp_draws` provide hashed generated-source relationships. | No general executed-state query. First qualify a documented evidence path and the identity of the selected execution occurrence; then implement bounded state interpretation with per-field coverage. Full coverage is conditional on evidence availability. |
| 2. Resource bytes | `capture_cpp_resources` and `capture_cpp_resource` already list references and read bounded serialized bytes through qualified optional workers. | The running server has neither worker configured. Build/configure the existing 2026.3 worker and validate retained inputs offline before considering any reader API changes. |
| 3. Draw timing / fragment and depth work | `profile`, `profile_metadata`, `profile_metrics` and `profile_compare` collect/query/compare GPU Trace exports. Event-duration and regime tables are recognized. | The game exports have no event/regime rows. The current parser rejects that shape. Per-action collection settings and verified draw/marker correlation are additional work; frame counters cannot supply missing draw counters. |
| 4. Controlled launch environment | `ProcessOptions` and the core `CaptureServiceOptions` already accept a complete environment map and use `execve`, without a shell. | Public MCP requests and `ngm-capture` expose no per-job environment. `capture_environment()` drops the required library/ICD/SDL variables. Add a validated request overlay over the service's existing isolated environment. |

The current C++ draw parser accepts single-part, straight-line generated recordings
and recognizes `CmdDraw` and `CmdDrawIndirect`. It does not recognize indexed
draws, viewport/scissor setters or general dynamic-state commands. Such recordings
are refused rather than interpreted. This is a material qualification prerequisite
for a larger renderer, independently of the new state fields.

Implementation anchors:

- [CaptureRequest and service configuration](../include/ngm/CaptureService.hpp),
  [launch isolation](../src/capture/CaptureService.cpp),
  [public schemas/handlers](../src/server/Workflow.cpp),
  [native CLI](../src/capture/Main.cpp).
- [Generated-source parser](../src/inspection/CppEvidence.cpp),
  [inspection/provenance validation](../src/inspection/Inspection.cpp),
  [resource read boundary](../src/inspection/ResourceRead.cpp),
  [worker build fingerprints](../cmake/ResourceWorker.cmake).
- [GPU Trace adapter](../src/nsight/Nsight.cpp),
  [export parser](../src/nsight/ProfileEvidence.cpp),
  [metric queries/comparison](../src/inspection/ProfileInspection.cpp).

### Documented interface boundary

The installed `ngfx --version` reports **2026.3.1.0/build 38722833**.
Its version/help calls return status 1 with valid output, consistent with the
existing discovery exception. Installed capture/replay help returns status 0.
The replay help exposes metadata, functions, objects, logs and the embedded
screenshot, without an advertised selected-event state/descriptor/value export.
NVIDIA's [CLI documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
describes those exports. The [API Inspector and Descriptors documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-ui.html)
describes detailed selected-event inspection in the UI; that is not a verified
machine-readable backend for this server.

NVIDIA documents [generated C++ source and the ReadOnlyDatabase helper](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html).
That is the permitted path for qualified source/serialized-input analysis.
It is not an arbitrary after-event memory reader. Reconstructing recorded API
state requires new interpretation and validation; the current source relationship
query does not already provide it.

The public NVIDIA pages/help reviewed during this assessment do not establish an available
official MCP state-export interface. The [SIGGRAPH description](https://www.nvidia.com/en-us/events/siggraph/)
still describes an in-development preview. Recheck a documented NVIDIA release/API
before investing in a broader backend. This is a scoped search result, not proof
that no such interface can exist. Private RPC, binary-format decoding and GUI
automation remain outside the repository's selected integration boundary.

## Retained game evidence and its limits

The supplied [game investigation](../../eternity-forge/docs/SHADOW_FILTERING_RESULTS.md#nsight-investigation-of-the-two-4k-bands-2026-10-14)
and local `analysis.json`, command arrays, summaries and raw exports were read.
The evidence root is
`/home/vordhosbn/.local/share/eternity-forge/profiles/r119-4k-2026-10-14/`.
`analysis.json` has SHA-256
`5e4b676f7b72870f551375e09c234456b13900882f4d23a615406cf5ebb5050f`.

| Evidence | Fast | Slow | Scope |
| --- | ---: | ---: | --- |
| Systems GPU-workload median | 2.242104 ms | 2.861695 ms | `systems-training-box-1` / `systems-training-box-2`; 1,001 correlated workloads each. |
| Graphics profiled frame | 2.333440 ms | 2.931780 ms | `graphics-training-box-3` / `graphics-training-box-2`; separate single-frame launches. |
| Graphics hardware draw count | 94 | 94 | Frame-wide exported counter, not 94 identified timing rows. |
| Graphics ZCull input samples | 143.246 million | 143.246 million | Frame-wide counter. |
| Graphics ZCull accepted samples | 123.509 million | 131.568 million | Frame-wide counter. |
| Graphics pixels sent to depth ROP | 33.751 million | 41.615 million | Frame-wide counter. |
| Graphics depth-passed pixels | 27.0097 million | 27.0098 million | Approximately equal; exported numerical precision retained. |
| Graphics pixels sent to color ROP | 53.940 million | 53.940 million | Frame-wide counter; output images were not newly compared. |
| Graphics reported FMA instructions | 384.807 million | 499.043 million | About 29.69% more in the slow frame; not a fragment-stage-only or draw-local measurement. |

The Systems launches have identical completed frozen Training metadata. Graphics
band labels use native pre-capture samples 301–400; the CLI terminates its target
after collecting the requested frame after 1,500 presents. There is no completed
Graphics application receipt establishing identical state at that later frame.
Equivalent commands/builds do not prove equivalent executed draw state.

Both selected Graphics exports have:

- `D3DPERF_EVENTS.xls`: one header line (`event_text`, `time_ms`), zero data rows.
- `GPUTRACE_REGIMES.xls`: one header line with 147 fields, zero data rows.
- `GPUTRACE_FRAME.xls`: 146 frame-metric rows.
- `REPRO_INFO.xls`: matching producer/GPU/driver, `Time Every Action: Yes`,
  shader-binding/pipeline collection enabled, multi-pass metrics disabled,
  GPU clocks unaltered and VSync enforced off.

The command arrays already include `--time-every-action`, `--hes-enabled 0` and
`--trace-timeout 30`. Enabling per-action timing alone therefore has not solved
these empty exports. Their cause remains unknown: absent markers, trace collection
behavior and export limitations are possibilities to test, not findings.
The observed counter differences support investigation of depth rejection and
extra shading work. They do not establish a responsible draw, state or driver
behavior. Systems and Graphics results remain separate; no event IDs or times
are joined across their launches.

The game records are externally retained local evidence, not newly imported or
MCP-pinned by this assessment. Imported raw artifacts would remain raw evidence:
the current typed capture/profile queries require complete service-produced
bundles and do not promote an `artifact_import` result into a qualified capture.

## Priority 1: selected-draw state

### Feasibility gate and first bounded state slice

First inspect retained generated captures and document exactly which calls, setup
data and submission identities the two qualified producers expose. Check for a
new documented NVIDIA state interface at the same time. An available documented
selected-event export is preferable to building a wider source interpreter, but
must pass actual schema/provenance checks before becoming a backend.

If generated replay source remains the available path, introduce a separate pure
state interpreter and inspection adapter; preserve `capture_cpp_draws`' existing
source-relationship contract. A proposed `capture_draw_state` request would use
an explicit capture-local reference containing recording, annotated event,
submission/execution occurrence and, when applicable, indirect subdraw. Its
selection domain must distinguish graphics function-inventory events from
generated-source events. A matching integer alone is not a join.

The first useful state implementation covers qualified direct/indexed draws,
pipeline/stage identities, depth test/write/compare, culling/front face and
viewport/scissor. Start with static state, then the observed Vulkan dynamic-state
forms. Retain relevant creation/bind/setter source spans and hashes. Extend the
accepted grammar only from observed, independently checked producer output.

State interpretation must follow the qualified Vulkan semantics, including
[dynamic-state lifetime](https://docs.vulkan.org/guide/latest/dynamic_state.html)
and [descriptor/pipeline-layout compatibility and offset rules](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html).
These references guide interpretation; they do not prove that a particular
Nsight export contains the required evidence.

| Requested field | Evidence required for an available value | Refusal / remaining scope |
| --- | --- | --- |
| Pipeline and shaders | Unique pipeline creation and effective bind in the selected command-buffer execution; stage/module, entry point and specialization evidence; shader bytes/hash through the existing reader where available. | A literal source bind alone stays a source relationship. Pipeline libraries, shader objects, ambiguous creation, unsupported specialization or missing execution occurrence receive explicit coverage reasons. SPIR-V identity does not claim a driver machine-code hash. |
| Depth test/write/compare, culling | Qualified pipeline depth/raster creation data plus dynamic-state enablement, setter order and Vulkan state lifetime/invalidation rules at the selected draw. | Never substitute static creation defaults when dynamic state controls the field. Unknown extensions/setters cannot leave a stale value marked available. |
| Viewport/scissor | Static arrays or complete dynamic setter arrays/counts applicable to the selected draw, including the qualified count variants. | Missing setters, unsupported count forms and relevant inherited state are unavailable. |
| Bound descriptors and dynamic offsets | Pipeline/set layouts, all relevant set binds, compatibility/disturbance rules, array elements, descriptor writes/copies and their ordered visibility; mapping of each dynamic offset to its binding/element. | Setup inventories do not establish after-update state. GPU descriptor buffers, update-after-bind races, unsupported descriptor forms and unresolved write history remain unavailable. Binding an array does not prove which element a shader accessed. |
| Push constants | Layout/stage/range compatibility and ordered partial byte writes with source/resource identities; byte validity tracked separately for each stage/range. | Missing writes and unsupported compatibility paths remain unavailable. The latest single call is not necessarily the complete constant block. |
| Uniform/material values | Proven descriptor buffer/range plus dynamic offset; bytes valid at the selected execution; verified type/layout schema or shader reflection with its own hash/provenance. | Serialized initial bytes alone cannot prove bytes after host/GPU writes. Return opaque bytes when layout is unknown; material names/meaning need an explicit schema. Actual bindless access and GPU-written values remain unavailable without additional evidence. |

Proposed results carry capture/producer/build identity, selection domain, source
and payload hashes, recording/submission occurrence, and field-level evidence.
Each field has an explicit status such as `qualified_recorded_state`,
`observed_event_state`, `unavailable`, `unsupported` or `not_applicable`, a nullable
value, a reason and bounded evidence references. Report whether correspondence
to the captured execution was established. Recorded-state interpretation stays
labelled as such; it does not become observed GPU state by changing a status name.
Incomplete coverage must not satisfy the full effective-state acceptance claim.

### Descriptor/value follow-up

Assess the existing opaque reader first. Literal source arrays can be interpreted
only under a qualified grammar. For serialized Vulkan descriptor structures,
[the current hydration experiment](DESCRIPTOR_HYDRATION.md) qualifies four exact
trusted inputs, not a generic descriptor API. Its pointer hydration has incomplete
payload bounds. Do not cast opaque bytes into native structures or introduce a
private packed-descriptor decoder.

A generic hydration worker would need a separate feasibility/security experiment,
bounded input/output validation and confinement, exact helper/ABI fingerprints,
object-registration mapping, independent new-input tests and source-closure policy
review. The production worker exception currently permits only five helper files;
adding `VulkanStructHydrator` to that closure requires an explicit owner scope
decision. This plan does not assume that extension is accepted or safe.

No documented selected-event snapshot means arbitrary post-GPU uniform/material
contents can remain unavailable. Optional source-available application witnesses
can help validate fixture expectations but must use a distinct evidence origin;
they do not supply general Nsight support for unmodified applications.

### Acceptance and validation

- For every supported selected draw, compare each available state field against
  an independent fixture-side record of the Vulkan API inputs and execution
  occurrence. Keep that record in the private harness, separate from MCP evidence.
- Test static/dynamic disagreement, rebinding, partial constant writes, nonzero
  dynamic offsets, partial descriptor-array updates and reused command buffers.
  Include indexed and indirect draws, secondary command buffers and multi-part
  recordings as either explicitly qualified cases or deterministic refusals.
- A missing/unknown relevant call invalidates affected coverage; it cannot inherit
  an earlier value. Wrong capture IDs, duplicate annotations, unsubmitted
  recordings, unsupported creation extensions and source/payload mutation refuse
  associations. Validate lifecycle and submission identity, not only lexical order.
- Bounded paging, lease protection, restart stability and hashes follow the existing
  inspection contract. Extend `ngm_cpp_evidence_check`,
  `ngm_cpp_inspection_check`, `ngm_resource_reference_check`,
  `ngm_resource_read_check` and `ngm_mcp_check` as applicable; names of additional
  state checks are not documented commands until implemented.
- Retained-source qualification comes first. Fresh windowed fixture capture on
  matching 2026.3 tools is a later, explicitly invoked integration step; repeat on
  matching 2026.2 before advertising the new capability on both releases.
  No assertion about the game follows from fixture-only qualification.

If the documented path cannot establish execution correspondence or value
lifetime, finish the feasibility report with those fields unavailable. A useful
recorded-state slice is progress, not completion of the entire priority.

## Priority 2: activate the existing reader

### Verified prerequisites and configuration

Live MCP discovery reports **0.4.1**, 26 tools, an artifact root of
`/home/vordhosbn/.local/share/nsight-graphics-mcp/artifacts`, a 2 GiB / 30-day
retention policy, `resource_bytes.available: false`, and both reader paths null.
Discovery does not execute the reader or establish kernel support.

The installed **2026.3.1.0/build 38722833** exactly matches the implemented
`nsight-2026.3.1-build-38722833-linux-x86_64` reader profile.
The default/package presets intentionally omit optional helper inputs and the
package does not supply the proprietary helper worker. No populated current
build tree was found during this assessment; the dependency submodules are present.

A concrete existing helper source directory is:

```text
/home/vordhosbn/code/nsight-graphics-mcp/artifacts/nsight-resource-query-evidence/bundles/bundle-0ae9ebf56c4cbaa146c955c127786616/raw/cpp/CppCaptures/fixture__2026_09_18__15_26_43
```

Its manifest is complete/unquarantined, its pin is present, and CLI/capture/replay
producer observations all match 2026.3.1.0/build 38722833. All five helper files
were hashed and match `cmake/ResourceWorker.cmake` exactly. The database is
706,564 bytes and its record file 416 bytes, within existing bounds. This is
retained 0.2.12 evidence; hashing it is not a new 0.4.1 reader qualification.

The follow-up can use existing targets without new reader APIs:

```sh
cmake --preset linux-gcc-debug \
  -DNGM_RESOURCE_HELPERS_2026_3=/home/vordhosbn/code/nsight-graphics-mcp/artifacts/nsight-resource-query-evidence/bundles/bundle-0ae9ebf56c4cbaa146c955c127786616/raw/cpp/CppCaptures/fixture__2026_09_18__15_26_43
cmake --build --preset linux-gcc-debug \
  --target nsight-graphics-mcp ngm-resource-worker-2026_3 ngm_worker_confinement_check_run ngm_resource_read_check_run
build/linux-gcc-debug/ngm-resource-worker-2026_3 --profile
```

Then add the absolute `--resource-worker-2026-3` executable path to the chosen
server's operator configuration and restart that server. Keep 2026.2 optional.
Use a separate offline validation server/store for historical bundles: the current
live artifact root does not automatically contain bundles from the repository's
older stores. Do not change an active server's artifact root just to test a reader.

The running kernel reports **7.2.7-zen1-1-zen**, x86-64. Runtime Landlock ABI 3+
and seccomp enforcement still need the CPU confinement check; a kernel version
or `--profile` result does not prove enforcement. Refer to
[RESOURCE_WORKER.md](RESOURCE_WORKER.md) and [RESOURCE_QUERIES.md](RESOURCE_QUERIES.md).

### Acceptance and validation

Validate an isolated copy of the retained producer-bound bundle and its source pin
with the current server. List references, select the listed reference for shader
handle 22, and read it through `capture_cpp_resource` rather than accepting a raw
handle from the caller. Its independent retained SPIR-V oracle is 2,700 bytes with
SHA-256 `cdc87c7eed07e6909af5e182cbaf46636002634279f598a26b42a64064b1cc9b`.
The oracle/case inventory is in the pinned qualification snapshot
`bundle-6365322f8eb3922893e1d2256983f732`, `raw/imported/fresh-cases.json`.

Require exact bytes/hash, a split-range reconstruction, source/database/helper/
worker provenance, refusal of an incorrect profile/ref, pins for both source
and read bundles, and retrieval after restart. Keep reader processes on their
existing empty-environment confinement path. This qualification needs no Nsight
launch, desktop or GPU permission. Publish it as current-server retained-input
qualification, separately from historical hardware capture support.

`ngm_retained_resource_integration` exists, but its current source still asserts
**22** tools and requires both worker arguments. It cannot be advertised as a
passing 0.4.1 runner. Either use direct current MCP calls for the one-profile
qualification or update that test harness to check the required tools and support
the chosen qualification inputs; the latter is a first-party test-code change
requiring normal versioning/checks. No harness update was made here.

Limits remain: 256 MiB database, 16 MiB record file, 16 MiB resource and 64 KiB
returned range. Each read can retain up to 272 MiB of database snapshots, in
addition to logs/results. Measure a future game C++ export before deciding whether
it fits. Its reported 401,080,320 application target bytes do not equal serialized
database size, but warn against assuming a 4K capture fits the current reader.
If it exceeds bounds, report that measured prerequisite and assess a separate
bounded-reader change; do not raise limits during activation. A GPU Trace file
does not supply the C++ resource database.

## Priority 3: timing, counters and correlation

### Small compatibility fix before timing expansion

`parse_profile_table()` currently rejects a header-only event/regime table as
`GPU Trace table has no values`. `run_nsight_profile()` requires all four tables
to parse before succeeding. Source inspection therefore predicts rejection of
the retained game's export shape in a managed job; this was not reproduced with
a new job during this assessment.

Implement acceptance of valid, producer-qualified header-only **event/regime**
exports, preserving headers and returning zero rows with explicit missing coverage.
Keep empty files, malformed headers, ragged tables and invalid numbers as errors.
Frame-duration/frame-metric validation remains separate. `profile_metrics` and
`profile_compare` currently dereference `table.rows.front()`; update both consumers
with the parser change. Empty-table queries must be safe and comparison must
return an actionable unavailable-selection error, never a zero duration/counter.

Acceptance: both retained header shapes parse with exact column counts/names and
zero rows; all 146 frame rows and frame duration remain retrievable; synthetic
malformed exports still fail; managed stand-in jobs publish valid partial coverage;
query/compare/restart checks exercise the empty tables without GPU access.
Use `ngm_profile_evidence_check`, `ngm_profile_workflow_check`,
`ngm_profile_compare_check` and `ngm_mcp_check` for the affected paths.
This fix does not create absent rows or qualify a new raw-profile import API.

### Collection settings and feasibility probes

Add typed, retained profile settings for `time_every_action` and explicit hardware
event selection, gated by matching installed help. The installed help documents
`--time-every-action`; it adds instrumentation overhead. It limits the Hardware
Event System to GB20x or newer, so the RTX 3080 Ti's working command explicitly
uses `--hes-enabled=0`. Preserve the existing unaltered-clock policy and qualify
every new selector against exported settings where exported. Hardware-event
enablement is not a route to draw timing on this Ampere GPU.
See NVIDIA's [GPU Trace settings](https://docs.nvidia.com/nsight-graphics/UserGuide/gpu-trace-overview.html).

After controlled launches are implemented, a later bounded fixture probe should
exercise several direct/indexed draws inside unique nested Vulkan debug labels,
including repeated labels and a reused command buffer. Test the same workload
with/without per-action timing and with the existing frame/submission boundaries.
Compare raw exports before designing new correlated queries. Log missing rows or
unsupported settings in [INVESTIGATIONS.md](INVESTIGATIONS.md), preserving exact
commands/tool/driver/build identities and concrete revisit conditions.

### Correlated query contract and acceptance

Expose a draw timing only when the same profile's documented export identifies
the draw or a uniquely verified draw-containing marker occurrence. Preserve the
raw table hash, row/column position, label hierarchy, queue/submission/frame and
execution occurrence where available, and describe whether a duration covers a
single action, a marker range or coalesced work. Units from `time_ms` are ms.
An unresolved marker span is not a draw duration. Numeric source event IDs and
matching draw counts cannot correlate independent graphics captures and profiles.

For supported counters, retain exact metric names, raw text, value, unit source,
aggregation, hardware/producer identity and scope (`frame`, `regime`, `marker` or
qualified `draw`). Start with the observed ZCull input/accepted samples, PROP
input/depth/color pixels and instruction totals as frame facts. Unit/normalization
rules require a version/architecture-qualified documented definition; preserve
null units when unresolved. Instructions summed across SM work are not implicitly
fragment-shader instructions, and sampled regime metrics are not exact isolated
draw counters. Do not divide frame totals by 94 to fabricate draw-local work.

Acceptance requires at least one fixture profile with actual exported action
timings and unambiguous same-profile correlation, repeated/nested marker cases,
explicit units and overhead, and an independent fixture timestamp sanity check
with declared scope/tolerance. For each counter advertised per draw, demonstrate
that the documented export supplies that scope and compare its association/value
against an independent export audit. Unsupported counters return reasons.
Retain data from both matching Nsight releases before claiming support for both.

If the documented auto-export continues to provide only frame totals or unkeyed
rows, keep per-draw timing/counters unavailable and report the limitation.
Consider optional application timestamps/pipeline-statistics only as separately
labelled source-available evidence, requiring explicit scope acceptance and
fixture validation; do not silently substitute them for Nsight counters.
No broader native trace decoder or private export is selected by this plan.

Future game qualification must retain a new run/build/shader/marker identity and
evidence from the profiled interval. Pre-capture band medians can label a launch
but cannot prove the selected later frame's execution state. Repeat equivalent
workloads and report variability; the existing two diagnostic frames are not a
performance acceptance cohort or proof of the depth-rejection hypothesis.

## Priority 4: per-job launch environment

### Smallest useful first product increment

Add optional `launch_environment` to `CaptureRequest` and the `capture`,
`capture_cpp` and `profile` MCP schemas. Use the same validator in `ngm-capture`
with a repeatable proposed `--launch-env KEY=VALUE` option. Merge the validated
request map into a copy of service defaults **before** attempt isolation replaces
HOME/XDG per-user directories/temp paths. Freeze the request at submission.
Apply it to Nsight discovery, capture/profile and export subprocesses so matching
libraries apply to the tooling as well as the freshly launched application.
Do not mutate the parent process environment or the resource worker environment.

This is a request/service change; the process supervisor already supports an
environment array. Exporting `LD_LIBRARY_PATH` around the current server alone
does not fix it because the public launch path filters that variable out.
Nsight's documented target-only `--env` also does not configure the launcher
process's own library selection.

Use a closed initial allowlist with at most eight keys, 4 KiB per value,
16 entries per path list and 16 KiB combined text:

| Key | Validation / purpose |
| --- | --- |
| `LD_LIBRARY_PATH` | Bounded colon list of existing absolute library directories; reject empty/relative entries. |
| `VK_DRIVER_FILES` or `VK_ICD_FILENAMES` | Existing absolute ICD JSON file list; reject simultaneous selectors and validate the selected manifests/referenced paths. Record exact selection and file hashes. |
| `SDL_VIDEODRIVER` | `wayland` or `x11`; actual backend/device compatibility remains a runtime qualification. |
| `QT_QPA_PLATFORM` | Explicit qualified `xcb` or `wayland` for tooling prerequisites. |
| `DISABLE_VK_LAYER_LUNARG_override` | Literal `0`/`1`, matching the known unwanted-layer control in the successful collector. |
| `VK_LOADER_LAYERS_DISABLE` | Permit only the observed explicit unwanted layer names `VK_LAYER_LUNARG_override` / `VK_LAYER_KHRONOS_validation`; reject patterns or Nsight injection-layer suppression. |
| `VK_LOADER_DEBUG` | Optional bounded documented diagnostic selectors; initially only the observed `layer` value needs qualification. |

The [Khronos loader driver interface](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md#overriding-the-default-driver-discovery)
documents the Linux selector list and the newer `VK_DRIVER_FILES` name; supporting
the collector's older `VK_ICD_FILENAMES` preserves its explicit input. The
[loader layer interface](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#layer-disable-filtering)
documents disabling by layer-name patterns. This proposed policy deliberately
accepts only the two qualified exact names.

Reject unknown keys, NULs, excessive values and invalid paths before allocating
a job or launching a process. HOME/XDG isolation overrides, arbitrary `PATH`,
`LD_PRELOAD` and injection controls outside the closed policy are not accepted
request fields. These are deliberately trusted local launch inputs, consistent
with the server's existing executable/argument authority.

The successful game collector supplied matching driver libraries, an ICD, SDL
Wayland, QT XCB and explicit unwanted-layer controls. The driver directory and
`nvidia_icd.json` still exist at
`/tmp/ef-procedural-retirement-20261013/nvidia-61571`; the ICD references
`libGLX_nvidia.so.0`, which resolves to the matching 615.71.09 library there.
The running kernel driver also reports 615.71.09. Those temporary paths are
operator inputs, never hard-coded runtime defaults. Recheck them for future jobs
or use an owner-maintained stable copy; this assessment does not install libraries.

Example proposed request overlay:

```json
{
  "LD_LIBRARY_PATH": "/tmp/ef-procedural-retirement-20261013/nvidia-61571",
  "VK_ICD_FILENAMES": "/tmp/ef-procedural-retirement-20261013/nvidia-61571/nvidia_icd.json",
  "SDL_VIDEODRIVER": "wayland",
  "QT_QPA_PLATFORM": "xcb",
  "DISABLE_VK_LAYER_LUNARG_override": "1",
  "VK_LOADER_LAYERS_DISABLE": "VK_LAYER_LUNARG_override,VK_LAYER_KHRONOS_validation",
  "VK_LOADER_DEBUG": "layer"
}
```

The current service unconditionally erases `VK_LOADER_LAYERS_DISABLE`; replace
that behavior only for explicit validated job requests. Preserve the default
injection policy and Nsight's own required layer environment.

### Acceptance and validation

Retain requested overrides and the final applied map separately in the job report,
with path/ICD identity observations labelled as selection evidence. Exported GPU/
driver observations remain separate from assumptions about which library loaded.
Capture failures retain the same provenance. Advertise the allowlist/coverage in
capability discovery without executing a job.

Executable stand-ins must observe the requested values in discovery,
graphics/C++ capture, profile and metadata exports. Run successive queued jobs
with different overlays and verify isolation, absent-key defaults and an unchanged
parent environment. Test rejected keys/paths, argument values containing spaces,
timeout/cancellation/shutdown cleanup, reserved-directory isolation, layer policy
and empty resource-worker environments. Verify actual stdio and HTTP request
schemas against the shared core.

Extend `ngm_capture_service_check`, `ngm_profile_workflow_check`, `ngm_nsight_check`,
`ngm_mcp_check`, `ngm_http_check`, and relevant CLI checks. Use one GCC Debug preset
and the CPU aggregate for the shared launch/schema change. A later fresh windowed
fixture run through MCP with the same driver/ICD settings qualifies the runtime
path. The fixture uses XCB/Xwayland; it cannot by itself qualify SDL Wayland.
A source-available SDL target or owner-authorized game launch is a distinct later
check. Reproducing the environment does not imply reproducing the old clock/worker
guard conditions; those machine controls are not part of this increment.

## Proposed execution order and prerequisites

| Step | Deliverable | Exit criterion |
| --- | --- | --- |
| A. Configuration/retained evidence | Build and activate the existing 2026.3 reader; inspect retained state and marker exports; record documented-interface coverage. | Current-server offline byte oracle/ranges/provenance/restart pass; state export feasibility recorded without new GPU work. |
| B. First product increment | Per-job allowlisted environment across capture/C++/profile and native capture CLI. | CPU stand-ins and stdio/HTTP prove propagation/isolation/rejection; documentation shows the exact operator inputs. A later runtime qualification is labelled separately. |
| C. Separate compatibility increment | Preserve valid header-only event/regime exports safely, including queries and comparison. | Retained-shape and lifecycle/provenance checks pass; frame metrics remain usable and draw coverage is explicitly unavailable. |
| D. Main state increment | Qualified pipeline/shader, depth/culling and viewport/scissor interpretation for selected draw occurrences. | Independent state oracle and refusal matrix pass; only demonstrated coverage is advertised. Extend descriptors/constants/values in subsequent bounded slices. |
| E. Correlated timing/counters | Typed per-action/HES controls, documented export probes, then queries only for demonstrated scopes. | Real exported rows correlate within one profile; units and supported counters are qualified. Otherwise retain a documented unavailable outcome. |

Steps A and the retained feasibility work for priority 1 can begin without a GPU
launch. Step B is the smallest useful **product implementation**: it enables the
known library/ICD/backend launch conditions through the MCP job lifecycle without
inventing evidence APIs. Step C is independently small and should precede live
game profiling because the currently observed valid exports would otherwise fail.
Neither completes effective state or draw timing.

Prerequisites for A/B are initialized pinned submodules, the documented source-build
toolchain, the fingerprint-matching retained helper directory, enforceable Linux
worker confinement for byte reads, a writable isolated validation store and enough
quota for snapshots. Capability discovery/configuration alone does not qualify
extraction. The current package excludes optional workers; keep their distribution
separate rather than copying proprietary helpers into tracked source.

Fresh validation later additionally requires matching Nsight tools, a compatible
driver/library/ICD selection, the actual desktop backend and a reproducible target.
Profiling requires applicable counter access; if unavailable, report it and seek
an explicit owner decision for any permission change. No prior temporary profiling,
clock or worker-pause authorization is carried into this plan.

Use an external managed root such as the configured
`/home/vordhosbn/.local/share/nsight-graphics-mcp/artifacts` for future captures,
traces and extraction bundles; build directories hold ignored build outputs.
Pin source captures and qualification/read/report bundles independently, account
for snapshot storage, and preserve failed attempts. Never hand-edit manifests to
make external CLI outputs appear service-produced. Retain exact version/compiler/
driver/desktop/settings/build/shader identities for every qualification.

Implementation candidates are not new committed roadmap priorities yet. This
assessment is tracked as R-017; a follow-up implementation task should select its
bounded increment and add the appropriate accepted item(s). First-party code/test/
build commits increment PATCH from the then-current CMake version under AGENTS.md;
this documentation-only assessment leaves **0.4.1** unchanged.

## Validation performed for this assessment

Read repository policy/roadmap/stack and the implementation cited above; called
live MCP `capabilities`; inspected installed CLI help/version and primary NVIDIA
documentation; read the game records/commands/raw tables; checked running driver,
kernel and staged driver paths; verified the retained helper source fingerprints,
bundle status, producer observations and pin; reviewed this plan and its links.

No configure/build/CTest, confinement test, reader execution, MCP resource read,
fresh capture, GPU replay or profile was run. Parser empty-table behavior and the
stale harness assertion are source-inspection findings, not newly executed
failures. Existing historical pins were inspected, without modifying their stores.
