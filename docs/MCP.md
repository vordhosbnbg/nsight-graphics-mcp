# MCP reference

Use tools/list for exact input/output schemas and capabilities for detected
prerequisites. A listed tool may still refuse an unsupported installation,
missing worker, incomplete artifact or unqualified data shape. Both transports
share these contracts.

## Jobs and identities

Capture, profile, replay and timing analysis return identity.job_id and artifact_id.
Poll job_status until the state is terminal and worker_running and
finalization_pending are false. Check cleanup_confirmed; submission or a terminal
state alone does not prove success/cleanup. Unconfirmed cleanup retains GPU ownership.

Jobs belong to the server process; artifacts survive restart until pruned. EOF on
stdio or service shutdown cancels owned jobs and waits for cleanup. Cancellation
only signals owned processes. Capture/profile/replay share one GPU reservation.
Use job_cancel explicitly; do not blindly retry a lost submission response.

## Tools

| Tool | Purpose |
| --- | --- |
| `capabilities` | Discover product identity, observations and coverage limits. |
| `capture` | Fresh native Graphics Capture; supports presented or qualified application-delimited boundaries. |
| `capture_cpp` | Fresh generated C++ capture with separate source/resource indexes. |
| `replay` | Finite standalone replay of a native service capture. |
| `capture_analyze_timing` | Collect C++ and GPU Trace evidence for an interval of the same saved capture. |
| `capture_draw_timings` | Associate qualified unique source-call ranges with saved replay durations. |
| `profile` | Collect a fresh live-target GPU Trace. |
| `profile_metadata` | Read collection settings and available exported tables. |
| `profile_metrics` | Page raw timing/counter cells by table, row and column. |
| `profile_marker` | Read a uniquely associated exported marker duration and requested raw metrics. |
| `profile_compare` | Compare repeated fresh profile groups with matching settings. |
| `capture_metadata` | Read typed capture metadata and explicit coverage. |
| `capture_events` | Page capture-scoped function-stream events. |
| `capture_objects` | Page the object inventory, without inferring event bindings. |
| `capture_cpp_source` | Read bounded generated-source lines. |
| `capture_cpp_draws` | List source-local draw/pipeline/shader relationships. |
| `capture_draw_executions` | List supported source recording/submission/draw occurrences. |
| `capture_draw_state` | Read supported recorded pipeline, push-constant and descriptor-binding state. |
| `capture_cpp_resources` | List source-referenced serialized resources. |
| `capture_cpp_resource` | Read qualified serialized bytes through an optional confined worker. |
| `capture_cpp_descriptor_writes` | List/decode source-local descriptor writes. |
| `capture_draw_values` | List supported draw occurrences and reconstruct selected typed uniform values. |
| `capture_shader_trace` | Inspect bounded invocation values reported by an instrumented application. |
| `job_status` | Inspect progress, terminal state, cleanup and reservation state. |
| `job_cancel` | Request bounded cancellation of owned work. |
| `artifact_list` | List managed bundles and expiration records. |
| `artifact_info` | Inspect status, provenance, files and retention. |
| `artifact_files` | Page a bundle's inventoried files. |
| `artifact_read` | Read bounded UTF-8 text or obtain a local file reference. |
| `artifact_preview_image` | Return a bounded PNG preview/crop. |
| `artifact_compare_images` | Compare equivalent RGB images at a specified channel tolerance. |
| `artifact_pin` | Set or release retention protection. |
| `artifact_import` | Copy caller-provided results into a managed bundle. |
| `artifact_usage` | Report managed/protected bytes and quota state. |
| `artifact_prune` | Apply configured limits to eligible completed bundles. |

## Queries

- Follow returned offsets/references rather than constructing IDs or guessing
  cross-capture relationships. Inventory pages may stop at a byte budget before
  their requested count. Empty pages, unsupported coverage and missing data remain explicit.
- artifact_read accepts up to 65536 inline bytes. Larger text is reference-only
  with text: null; use the returned local path rather than parsing null as JSON.
- Resource reads return at most 64 KiB per call and require a listed source-bound
  resource_ref. They expose serialized captured bytes, not arbitrary post-event memory.
- Descriptor update_ref and value execution_ref come from their listing calls;
  the service revalidates source/database/helper identity. Values are a bounded
  generated-source model with captured_gpu_execution_correspondence: false.
  Unknown shader effects, layout, selection or lifetimes refuse the selected query.
- Selected value queries share a 20-second deadline, 32 parent reads, 1 MiB of
  returned child payloads and 64 KiB shader/uniform-prefix reads. Pages are
  bounded to 256 KiB; larger proof reports use artifact_read's file-reference contract.
- capture_shader_trace reads the optional application trace contract below.
  It fingerprints retained trace/module bytes and reports application claims;
  it does not read live registers or associate records with Nsight capture events.
- Image tools accept bounded P6/PNG/BMP inputs (up to 16 MiB); previews have a
  maximum edge of 384. Comparisons require equal dimensions and caller-established
  workload equivalence. Transparent/unsupported formats fail explicitly.
- GPU Trace .xls exports are UTF-8 TSV. Raw profile zeros remain zero; unavailable cells remain null with reasons.
  Duplicate labels/columns retain positions. Event time_ms establishes milliseconds;
  other units are unresolved unless the result explicitly establishes them.

## Instrumented shader trace contract

A source-owning application may write `shader-trace.json` and its instrumented
SPIR-V module inside the directory supplied through `application_output_option`.
The capture service retains these files under `raw/application/`. Instrumentation
and producing this output are optional; ordinary capture needs neither.
The query accepts complete native service Graphics Captures. C++ captures,
imports and failed bundles remain available through the ordinary artifact tools.

The trace is a JSON object with exactly these fields:

| Field | Meaning |
| --- | --- |
| `schema_version` | `1`, the current producer contract. |
| `evidence_origin` | `application_reported_instrumented_shader_trace`. |
| `original_execution`, `capture_event_correspondence` | Both must be `false`. |
| `shader` | `path` relative to the application output directory, lowercase SHA-256 in `sha256`, `entry_point`, and `stage: "fragment"`. |
| `extent` | `width` and `height`, each from 1 to 65536. |
| `selection` | In-bounds `pixel_x` and `pixel_y`. |
| `capacity`, `attempted_records`, `dropped_records` | Bounded record capacity and explicit collection counts, consistent with the retained records. |
| `probes` | Unique numeric `id`, `name`, `type` (`float32`, `uint32` or `int32`) and `components` from 1 to 4. |
| `records` | Application-declared unsigned 32-bit `frame`, `draw`, `pixel_x`, `pixel_y`, `sample`, nullable `primitive`, `probe` ID and component `bits` as unsigned 32-bit words. Every record belongs to the reported selected pixel. |

The query accepts at most 1 MiB of trace JSON, nesting depth 16, 64 probes and
4096 records/capacity. Duplicate or unexpected keys, invalid counts, unknown
probes, invalid word types and paths outside the application output refuse.
The retained module is capped at 16 MiB and must have a supported SPIR-V header
and match the declared hash. That check establishes file
identity, not that the application executed the declared module or invocation.

Use `capture_shader_trace` with `capture_id`, `offset` (default 0) and `limit`
(default 50, maximum 100). Optional `frame`, `draw`, `pixel_x`, `pixel_y`, `sample` and `probe`
selectors filter application records. Raw bits preserve NaN, infinity and signed
zero; nonfinite floating-point values have null numeric representations.
`records_total` counts all reported rows and `matched_records` counts rows after
filtering; `offset` and `next_offset` page that filtered sequence. Each returned
row includes `probe_name`, `type`, `bits_hex` and decoded `values`.
Results identify their origin as application-reported instrumentation and keep
`producer_claims_verified`, `original_execution` and
`capture_event_correspondence` false. Application frame/draw values do not become
Nsight event identifiers. Instrumentation can change compilation and behavior;
trace equality cannot establish values from an unchanged original shader.

[ShaderTrace.hpp](../include/ngm/ShaderTrace.hpp) provides an optional C++
producer helper, `make_shader_trace`, with a 32-byte header and 48-byte std430
record layout. Applications still own shader probes, feature admission,
descriptor binding, capacity/overflow handling, completed GPU readback and host
memory visibility. The helper does not instrument or launch a shader.

## Artifacts

The store uses JSON manifests and directories, with separate raw/ and derived/
files. Complete/failed publication is atomic; interrupted staging is reconciled
on startup. Imports are labeled caller_provided_import and do not become native
service captures. Source snapshots used by jobs are protected while queued/running.

Retention defaults to 2 GiB/30 days. Pins persist; active leases also prevent
pruning. Quota exhaustion preserves protected data and reports the problem.
Pruning removes whole eligible bundles and leaves expiration explanations.

Use pins only for a user retention request or a concrete ongoing regression/bug
investigation. Release development pins and remove temporary run output after
validation. Central imports and permanent archives are not completion requirements.
Do not mutate a live managed store directly; artifact_prune uses its current policy.

## HTTP

POST /mcp accepts one JSON-RPC message with Content-Type application/json and
Accept including application/json and text/event-stream. Initialize first;
subsequent requests carry MCP-Session-Id and the negotiated MCP-Protocol-Version.
Supported protocol revisions are 2025-06-18 and 2025-11-25. Notifications return
202; GET returns 405. DELETE ends a session without cancelling its jobs.

Sessions expire after 30 idle minutes and are limited to 128. Job history is
limited to 4096 entries per service lifetime. Requests are capped at 64 KiB JSON,
responses at 1 MiB; long work uses job polling rather than streaming progress.
Bearer authentication is required for every request. All clients share the
trusted user's workspace; there are no per-client artifact permissions.
