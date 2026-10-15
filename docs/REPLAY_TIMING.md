# Same-input timing evidence collection

At **0.5.4**, `capture_analyze_timing` adds a shared stdio/HTTP operation for
collecting generated C++ and GPU Trace evidence from one retained Graphics
Capture. This is an R-025 implementation slice; **R-025 remains In Progress**.
A successful collection does not itself associate a draw with a timing.

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

Literal original labels can be compared between the same-input C++ and trace
exports. That observation is not yet a public correlation query. Original
Graphics Capture event IDs, unannotated draw correspondence, independent
per-draw timing validation, original live performance and physical counter
normalization remain unqualified. No generated replay code is compiled, no
helper closure is widened and no machine profiling policy is changed.

[BUILD_VALIDATION.md](BUILD_VALIDATION.md) records actual CPU, transport, GPU,
restart and evidence-retention checks with their exact build identities.

All **38 CPU checks** and **353** final-build actual MCP schemas pass. Complete/
pinned **bundle-d95a9298e94a1dfc4b1e598bc2445d3f** retains **2,302** verified
payloads and **15** native pins, including the failed development controls.
The imported verifier passes; final source/build identities remain distinct
from earlier 20/20 and 60/60-loop experiments and the 0.4.1 importer.
