# Same-input timing evidence and draw association

At **0.5.4**, `capture_analyze_timing` adds a shared stdio/HTTP operation for
collecting generated C++ and GPU Trace evidence from one retained Graphics
Capture. This is the R-025 collection foundation.
A successful collection does not itself associate a draw with a timing.

At **0.5.5**, `capture_draw_timings` adds a read-only, bounded association query
over retained analyses. It returns unique literal marker ranges containing one
submitted direct/indexed source call, with the raw exported replay duration.
At **0.5.6**, [independent per-draw timestamps, new inputs and the actual shader
repair workflow](DRAW_TIMING_VALIDATION.md) complete **R-025** on the measured
matching 2026.3 producer. Original numeric capture IDs and original live cost
remain unavailable; the qualified association uses original literal paths,
source/input hashes and generated-C++ submission selections.

## Collect an analysis

Supply a native service `capture_id` and the GPU Trace `architecture`, for
example `"Ampere GA10x"` on the qualified host. Optional arguments are:

- `interval_index`: zero based, default 0, range 0..2, smaller than the
  source's recorded interval count;
- `duration_ms`: default 1000, range 1..10000;
- `timeout_ms`: one deadline covering both launches, default 120000, range
  1..600000;
- `pin`: default false;
- `launch_environment`: the existing validated process-local overlay.

The source must be a complete native presented-frame Graphics Capture from
matching **2026.3.1.0/build 38722833**, with a retained payload hash and the
qualified singular/plural metadata shape and interval count **1 or 3**. Other
counts refuse before launch because their selection mapping is unqualified.
Imported directories, C++ captures, ordinary profiles and analysis bundles are
not eligible sources. Actual GPU qualification covers a retained single-interval
input and all three intervals of a retained count-three input. Other counts and
arbitrary Vulkan workloads are not qualified.

The job protects its source while queued, creates a private same-filesystem
snapshot and verifies its hash against the capture-time identity. It validates
fresh matching metadata before launching the installed matching replayer ELF.
It does not relaunch the original application. Both phases use the fixed
documented replay arguments `--present-app --verbose INPUT`, with 20 loops for
C++ capture and 60 for profiling, retained separately in the settings.
The 20-loop profile development build exports tables but misses its single-interval
termination deadline (I-063). A serial 60-loop control exits successfully with
the same input/trigger and literal labels; the internal cause remains unknown.

The first phase uses Generate C++ Capture at
`max(2, source_interval_count) + interval_index` presented boundaries. The second
uses GPU Trace with one selected frame, Time Every Action enabled, HES disabled,
Throughput Metrics and clocks unaltered. Index zero requires the advertised
`--start-with-ngfx-sdk` control and an exported `Replay Pass Start`; later indices
require their explicit after-frame ordinal. The exported source path, target,
producer and settings must agree. The ordinary live `profile` guard still
requires its ordinary requested trigger.

Input and replayer file hashes are checked around the launches. These checks
describe path contents; they do not authenticate the producer or prove the
identity of every running mapped page. Capture/profile work shares one GPU
reservation, including owned cleanup. Cancellation, deadline, shutdown and
failed evidence use the existing coordinated publication/quarantine path.

The resulting bundle has evidence origin `nsight_replay_timing` and contains:

- `raw/input/capture.ngfx-capture`, source identity and fresh metadata;
- generated C++ and `derived/cpp-project.json`, including input fingerprints;
- raw GPU Trace tables and `derived/replay-profile.json`;
- `derived/timing-analysis.json`, binding the source, selected interval,
  replayer hashes, job identity and both child-index hashes;
- the report and separate subprocess logs.

`timing_collected`, `generated_cpp_project` and `profiled` indicate successful
collection. `replayed` stays false because the operation has a different success
contract from standalone replay. Inspect bounded raw/index files through the
artifact tools. Existing capture/profile inspection tools reject this distinct
origin rather than silently treating a mixed analysis as an ordinary capture.

No generated replay code is compiled, no helper closure is widened and no
machine profiling policy is changed.

## Query submitted draw ranges

Pass `analysis_ids` to `capture_draw_timings`: one analysis for a one-interval
source, or three distinct analyses covering indices 0, 1 and 2 for a three-
interval source. All must come from the same native saved input, matching
producer, replay target, GPU/driver and common collection settings. Caller order
does not affect the result. A partial or mixed group refuses.

The query validates manifest/report/master identities, successful process
cleanup, source payload and copied-header/report hashes, both child indexes,
C++ input fingerprints, raw trace/table hashes and the exact retained phase
arguments and exported settings. Earlier 20/20 and 60/60 development controls
are outside its qualified 20/60 contract. Retained input and target identities
remain recorded provenance, not producer authentication. Reading needs an
artifact store; installed Nsight tools, a GPU and a resource worker are not
required.

The timing-specific source reader follows primary command-buffer recordings,
pool ownership and the submitted queue stream. Queue/command labels can span
non-draw setup buffers, but a qualified draw range must begin and end within
one submitted command-buffer occurrence. It must contain exactly one direct or
indexed call, with no child range or other GPU work. Included state commands
are listed explicitly. Pre-frame callbacks containing draws, unknown execution
forms, unsupported allocation/lifetime forms and ambiguous paths refuse.
The older recorded-state query grammar and `profile_marker` contract are
unchanged.

Each selected full path must be unique across **all** retained source intervals
and both trace views, including regime-only paths. Repeated names beneath
different unique parents can qualify; repeated full paths cannot. An unknown
source or trace hierarchy in one interval prevents available associations in
the other intervals. Row order, matching counts and numeric event IDs never
establish the join.

Results provide:

- A stable `timing_ref`, `analysis_id` and `interval_index` per source occurrence.
- A generated-C++ selection containing submission, submit-info, command-buffer,
  recording and source event identifiers, plus literal draw parameters.
- Full marker paths and source spans for the draw, submission and labels.
- `duration_ms`, the raw duration cell and event/regime row references when
  available; unavailable associations retain their reason with null timing.
- Analysis/source/target hashes, producer/GPU/driver identities and coverage.

These selection identifiers belong to the **generated C++ snapshot**.
`original_capture_event_id` is null. Original numeric Graphics Capture IDs,
unannotated draw correspondence,
original live performance and physical counter normalization remain unavailable.
The range includes state packets, replay and instrumentation; its time is not
an isolated shader cost. Different interval analyses use separate fresh replay
processes.

`offset` defaults to 0 and is bounded by 30000; `limit` defaults to 50 and is
1..100. Follow `next_offset` until null. Draw pages have a 256 KiB byte budget;
the complete result is bounded by 1 MiB. Every request revalidates its leased
inputs under a shared ten-second, 512 MiB read/fingerprint budget. Individual
saved inputs and serialized files are bounded by 256 MiB; selected C++ syntax
is bounded by 64 files and 16 MiB per analysis. Oversized evidence refuses.

The final 0.5.5 retained-data qualification returns **36** matches across the
three-interval group and **12** for the single-interval input collected at
0.5.4. Independent fixture API witnesses and raw source/table checks agree with
all 48 selections and values. **58** actual results validate against advertised
schemas over three stdio starts and two HTTP starts; **14** invalid/legacy
requests refuse. All **704** checked native payloads stay unchanged and source
pins survive restart. No new GPU collection or independent per-draw timestamp
acceptance is implied by these reads.

[BUILD_VALIDATION.md](BUILD_VALIDATION.md) records actual CPU, transport, GPU,
restart and evidence-retention checks with their exact build identities.

The **0.5.4 collector** passes all **38 CPU checks** and **353** final-build actual
MCP schemas. Complete/
pinned **bundle-d95a9298e94a1dfc4b1e598bc2445d3f** retains **2,302** verified
payloads and **15** native pins, including the failed development controls.
The imported verifier passes; final source/build identities remain distinct
from earlier 20/20 and 60/60-loop experiments and the 0.4.1 importer.

The **0.5.5 reader** passes all **40 CPU checks** in **413.34 seconds**. Its
complete/pinned qualification
**bundle-975763dfe5b2d8a5bbc97f0219e6ccec** retains **483** verified payloads,
including the new success/failure controls, exact source and eight compressed
executable identities. Its imported verifier checks all 58 schemas, 14 refusals,
48 call/path/raw-time matches and stable references against the already pinned
0.5.4 input snapshot. No raw input history is duplicated or unpinned.

The **0.5.6 completion qualification** adds 294 independent application-only
per-draw checks, 36 same-live-profile Vulkan/Nsight timestamp comparisons with
0–32 ns error under the predeclared tolerance, 48 fresh saved-input associations
and an ambiguity control returning two available/four unavailable draws. An
actual fragment-source edit, source-built shader compilation and fresh capture
repair changes 12,286 differing pixels to zero; all twelve draw associations
and exact captured shader bytes pass before/after and after two restarts.
All 40 CPU checks and 1,327 actual schema-valid results pass. Complete/pinned
**bundle-168c040e6fa84e7efd645b16a5413735** retains 329 verified payloads,
including a lossless archive of all 2,640 snapshot members and 27 native pins;
its imported portable verifier passes. See [DRAW_TIMING_VALIDATION.md](DRAW_TIMING_VALIDATION.md)
for settings, source domains, units and remaining limits.
