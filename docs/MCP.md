# Local stdio MCP server

The visual release completes at **0.3.0**, preserving the 22-tool surface qualified
at 0.2.12. [INSTALL.md](INSTALL.md) provides the exercised source-build and repair
walkthrough; [VISUAL_RELEASE.md](VISUAL_RELEASE.md) consolidates acceptance scope.
The clean 0.2.12 build also passes a real Codex CLI 0.154.0 capability/artifact
retrieval check; exact evidence is in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

The local stdio server exposes capability discovery, asynchronous fresh-process
capture, job status/cancellation, and bounded access to managed artifact bundles.
Retained capture metadata and paginated event/object inventories are implemented
for the observed Nsight 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608
Vulkan export profiles. The separate `capture_cpp` tool retains generated API
source, metadata, and binary resource files through artifact access. Executed GPU state and profiling remain pending. At **0.2.12**, the surface has
**22 tools**, including `capture_cpp_resources` and `capture_cpp_resource`. These
provide literal references and bounded serialized input bytes; see
[RESOURCE_QUERIES.md](RESOURCE_QUERIES.md) for exact scope and configuration.
The following versioned milestones record earlier stages. At 0.2.4, the seventeenth tool,
`artifact_compare_images`, supports the independently prepared basic shader repair
qualified in [SHADER_REPAIR.md](SHADER_REPAIR.md); broader diagnosis/fix coverage remains pending. At 0.2.5 the eighteenth tool,
`artifact_preview_image`, returns bounded PNG image content for retained
P6/PNG/BMP sources; see [IMAGE_PREVIEWS.md](IMAGE_PREVIEWS.md). A capture submission returns job and artifact IDs; it does not
assert that capture or replay succeeded. At **0.2.6**, `capture_cpp_source` and
`capture_cpp_draws` bring the surface to **20 tools** with bounded generated-source
inspection; [CPP_INSPECTION.md](CPP_INSPECTION.md) defines its exact grammar and limits.

`capabilities` accepts `{}` or omitted `arguments`. It reports the product version,
negotiated MCP revision, implemented tools, configured artifact limits, and
prerequisite observations. It never creates artifact directories, opens the
artifact store, runs Nsight, or probes the GPU. Executable paths and desktop
variables are observations only: compatibility remains `not_verified`, the GPU
remains `not_probed`, and unobserved versions/identities remain null. Capture is
reported as `prerequisites_observed` only when an artifact root, executable paths,
and a desktop environment hint are present; the real connection and release
compatibility still require validation.
Inspection reports `retained_exports_only` when an artifact root is configured;
each inventory query separately validates the retained bundle and producer. Reading these
exports needs no current Nsight installation, desktop, or GPU execution.

Every tool advertises closed input and structured output schemas. Unknown
properties, incorrect field types, out-of-range values, unsafe paths, missing
configuration, and operational failures return tool results with `isError: true`
and actionable text. Malformed protocol parameters, including non-object
`arguments`, produce JSON-RPC error `-32602`. Successful results contain both
`structuredContent` and an equivalent JSON text block. This distinction follows
the MCP [2025-11-25 tool error contract](https://modelcontextprotocol.io/specification/2025-11-25/server/tools#error-handling)
and the equivalent mechanism in
[2025-06-18](https://modelcontextprotocol.io/specification/2025-06-18/server/tools#error-handling).

## Workflow tools

| Tool | Inputs and behavior |
| --- | --- |
| `capture` | Required absolute `executable` and `working_directory`; optional `arguments` (up to 256 strings, each at most 4096 bytes), `capture_frame` (default 2, minimum 2), `delimiter` (`present`, the default, or `graphics_capture_api`), `timeout_ms` (default 120000, range 1–600000), `pin` (default false), and `application_output_option` (one option of at most 64 bytes). Captures one selected delimiter interval in a fresh target. The SDK delimiter requires application-side initialization/boundaries; its exact two-release basic-workload qualification is in SDK_CONTROL.md. Returns `identity` and `artifact_id`. |
| `capture_cpp` | Required absolute `executable` and `working_directory`; optional `arguments`, `wait_frames` (default 2, range 2–1000000), `timeout_ms`, `pin`, and `application_output_option` with the same bounds as `capture`. Generates a C++ project in a fresh target. Read `derived/cpp-project.json` and its source paths through artifact tools. Does not accept `capture_frame` or `delimiter`; see CPP_CAPTURE.md. |
| `capture_cpp_source` | Required `capture_id` and indexed `source_path`; optional `start_line` (default 1, range 1–4194304), `max_lines` (default 100, range 1–200). Returns numbered generated replay source lines, content hash and `next_line`; source cap 4 MiB, page cap 256 KiB. |
| `capture_cpp_draws` | Required `capture_id`; optional `offset` (0–100000), `limit` (default 50, range 1–100), and `section` (`draws`, default; `unsupported_recordings`; `unsupported_objects`). Returns literal draw/pipeline/shader references and separate coverage totals for qualified generated source. Page selected sections with `next_offset`; no executed GPU-state or resource-byte claim. |
| `job_status` | Required `job_id`; returns state, stop reason, cleanup/reservation flags, elapsed milliseconds, a bounded diagnostic error, and evidence IDs. IDs belong to this server session. |
| `job_cancel` | Required `job_id`; requests cancellation and reports whether it was already requested or terminal. Poll status for cleanup completion. |
| `artifact_list` | Optional `after_id` and `limit` (default 50, range 1–100); includes staging, complete, failed, and expired summaries with `next_after`. |
| `artifact_info` | Required `artifact_id`; returns retention state, bounded provenance, required outputs, file count, or an expiration explanation. |
| `artifact_files` | Required `artifact_id`; optional `offset` (0–4096) and `limit` (default/max 100); returns inventoried relative paths/sizes with `next_offset`. |
| `artifact_read` | Required `artifact_id` and `path`; optional `max_bytes` (default/max 65536, minimum 1). Returns a complete UTF-8 text file, or metadata and `local_path` for a larger file. NUL or malformed UTF-8 is a tool error. |
| `artifact_compare_images` | Required `reference` and `candidate`, each containing `artifact_id` and inventoried `path`; optional integer `channel_tolerance` (0–255, default 0). Compares bounded P6/PNG/BMP RGB8 images, returning hashes, source statuses, and pixel/channel errors. Imports and readable failed artifacts are allowed; workload equivalence and repair are not inferred. See IMAGE_COMPARISON.md. |
| `artifact_preview_image` | Required `artifact_id` and inventoried `path`; optional `max_edge` (1–384, default 384) and `region` with required integer `x`, `y`, `width`, `height`. Returns text/structured source and sampling metadata plus base64 `image/png` MCP image content. P6, opaque RGB/RGBA8 PNG, and supported BMP; same 16 MiB input cap. Crops must be in bounds; nearest-neighbor downsampling never enlarges. See IMAGE_PREVIEWS.md. |
| `artifact_pin` | Required `artifact_id` and boolean `pinned`; persists protection across server restarts. Quarantined evidence cannot be unpinned before cleanup is confirmed. |
| `artifact_usage` | Empty arguments; reports managed bytes, protected subsets, configured size limit, and quota exhaustion. |
| `artifact_prune` | Empty arguments; applies configured limits to unpinned, unused completed bundles. Returns at most 100 expired IDs/errors with total counts, `truncated`, and usage. |
| `artifact_import` | Required absolute `source` directory outside the store; optional source-relative `required_outputs` (up to 128 paths) and `pin` (default true). Copies without following links, leaves the source unchanged, and labels provenance `caller_provided_import`. |
| `capture_metadata` | Required `capture_id` (the capture's `artifact_id`); returns selected typed metadata, capture scope, exact producer observations, raw evidence references, and unavailable evidence categories. Missing optional metadata is null. Process environment and command line are omitted. |
| `capture_events` | Required `capture_id`; optional `offset` (default 0, range 0–100000) and `limit` (default 50, range 1–100). Returns `events` in exported order with capture-scoped `event_index`, `function_name`, `thread_index`, and nullable opaque `sequence_id` and `indirect_index`, plus `offset`, full `total`, and `next_offset`. |
| `capture_objects` | Same inputs/pagination as `capture_events`; returns `objects` with capture-scoped `uid`, `api`, `object_name`, `type_name`, and opaque `access_flags`. No event associations, object definitions, or resource contents are inferred. |
| `capture_cpp_resources` | Required `capture_id`; optional `section` (`resources` or `unsupported`), `offset`, `limit` (default 50, maximum 100). Lists literal source references and qualified byte declarations with hashes/spans; no worker required. |
| `capture_cpp_resource` | Required `capture_id` and listed `resource_ref`; optional `offset`, `length` (1–65536, default 65536), `pin` (default false). Uses the configured qualified worker and returns hex bytes/hashes plus a retained attempt artifact. Each call retains database snapshots up to 272 MiB. No GPU event-state reconstruction. |

Strings reject NUL bytes; input byte limits are enforced in addition to the
advertised schemas. Absolute application/import paths are limited to 4096 bytes;
inventoried file paths to 512 bytes. File paths must remain below `raw/` or
`derived/`, without absolute paths or traversal. Tools do not accept arbitrary
caller-provenance or expected-diagnosis fields. All requests remain subject to
the protocol line/nesting limits below. Structured results are bounded to 1 MiB;
text evidence reads are capped at 64 KiB and do not truncate or repair evidence.
Inventory pages also stop before their encoded structured JSON exceeds 256 KiB;
`next_offset` advances only by the records returned, and becomes null at the end.
Follow that offset even when fewer than `limit` records were returned. An offset
beyond the end returns an empty terminal page with the unchanged total.
Inspection checks the serialized structured result and escaped text fallback,
reserving protocol overhead within 1 MiB. If one record or a metadata summary
cannot fit, the query returns an actionable `inspection_limit` error and points
to the retained raw export; fields are never silently truncated.

The three graphics-inventory inspection tools reject C++ capture bundles; use
artifact reads for their separate [generated-project index](CPP_CAPTURE.md).
All three inspection tools lease their bundle through loading and querying.
They require a complete server capture with matching schema-1 manifest/report,
successful capture and cleanup, inventoried outputs, and a successful relevant
export. Events and objects additionally validate metadata from that same bundle
before interpreting their unversioned arrays. Both explicit profiles require
Vulkan metadata version 1. Capture and replay must match the same complete tuple:
CLI `2026.3.1.0` / build `38722833`, metadata `2026.3.1` / build `38722833`; or
CLI `2026.2.0.0` / build `37991608`, metadata `2026.2.0` / build `37991608`.
Mixed releases/builds are rejected. These distinct strings are returned unchanged
in `producer`. This preserves recorded observations; it does
not authenticate the producer or establish successful GPU replay execution.
Imports cannot substitute for server captures. Unsupported producers, failed or
missing exports, invalid schemas, and analysis limits produce explicit tool
errors. Raw evidence remains available separately through the artifact tools.
See [INSPECTION.md](INSPECTION.md) for the core contract and evidence limits.

Each capture uses a fresh application launch and retains stdout/stderr under its
artifact bundle. The documented CLI backend checks matching tool versions/help,
saves a capture, and attempts replay metadata plus optional export views. The
retained report distinguishes each export outcome; readable capture metadata
does not establish full event state, descriptor contents, shader inspection, or
resource extraction. `application_output_option` appends that option and a fresh
bundle-local output directory to the application argv. This optional convention
labels its files as application-provided evidence, separate from Nsight exports.
The service defaults to the documented present delimiter and one frame. Its
optional `graphics_capture_api` delimiter selects the documented SDK boundary
path described in [SDK_CONTROL.md](SDK_CONTROL.md), qualified for its recorded
basic fixture workloads on the two matching tool/SDK pairs. Capture frame ordinals refer to the selected delimiters, not
application frame indices. A timeout alone does not identify a missing delimiter
as its cause; inspect the retained process logs and report. Compute without
presentation is outside the current qualification.

Poll until the state is terminal **and** `worker_running` and
`finalization_pending` are both false before reading finalized evidence. Check
`cleanup_confirmed`; a terminal result alone cannot certify process cleanup.
Queued cancellation never launches the target. Running cancellation retains the
GPU reservation until cleanup is confirmed. EOF requests job shutdown and waits
for owned work to finish; the server reports failed cleanup on stderr and exits
nonzero. Completed artifact references survive restarts; job snapshots do not.

Pin investigation evidence and retained verification baselines explicitly: a
reference alone does not protect data. Default retention is 30 days and 2 GiB;
zero disables the corresponding limit. Protected bundles can leave the quota
exceeded. Failed/interrupted attempts remain identifiable rather than appearing
as complete captures. Imported investigation/baseline directories provide a way
to bring existing evidence under the same retention policy. Import checks both
lexical and resolved source/store containment before opening storage, so rejecting
an overlapping source does not create a store inside that source. The artifact
core repeats its containment and no-follow checks at import time. Job diagnostics
are bounded to their advertised schema; inspect retained report/log files for the
full evidence. Raw captures/images
remain disk files; `artifact_preview_image` returns a bounded derived PNG preview
while preserving the original file and its identity.

## Build and run

Use the source setup in [README.md](../README.md). The MCP implementation uses
the existing pinned, statically linked fastmcpp and nlohmann/json sources.

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target nsight-graphics-mcp ngm_mcp_check_run
./build/linux-gcc-debug/nsight-graphics-mcp --version
```

Without arguments, `nsight-graphics-mcp` permits discovery and reads line-delimited
JSON-RPC from stdin until EOF. Workflow tools require an explicit artifact root;
there is no implicit HOME-backed store. Stdout contains only protocol replies.
Negotiation/shutdown diagnostics use stderr; `--version` and `--help` are separate standalone commands, never
startup banners.

Default discovery searches `PATH` for `ngfx`, resolves symlinks, then searches for
`ngfx-capture` and `ngfx-replay` beside it (or in its installation's
`host/linux-desktop-nomad-x64` directory), with `PATH` as a fallback. Findings can
belong to different releases and do not establish compatibility. Select a single
installation explicitly with an existing absolute directory:

```sh
/absolute/path/to/nsight-graphics-mcp \
  --nsight-root /absolute/path/to/Nsight-installation \
  --artifact-root /absolute/path/to/managed-evidence \
  --artifact-max-bytes 2147483648 \
  --artifact-max-age-seconds 2592000
```

An explicit root searches `host/linux-desktop-nomad-x64` and the root itself,
without falling back to other `PATH` installations. A directory with missing
executables still permits discovery and reports them as absent. An invalid root
exits with code 2 and an actionable stderr message. Artifact-root creation and
validation are deferred to the first valid workflow call; configuring it alone
has no storage side effects. Its path must be absolute and below the filesystem
root. Retention values are nonnegative integers, may appear once each, and require
`--artifact-root`. The managed store claims an exclusive lock and rejects a
nonempty unrelated directory.

A minimal capture tool call is:

```json
{"executable":"/absolute/path/to/application","arguments":["--scenario","reference"],"working_directory":"/absolute/path/to/work","capture_frame":2,"timeout_ms":120000,"pin":true}
```

Arguments must be representable by the installed documented Nsight CLI; rejected
argument combinations are retained as a failed attempt with their reason. No
shell command string is accepted by MCP. The backend's argument conversion and
version-specific interface checks are documented in [NSIGHT_BACKEND.md](NSIGHT_BACKEND.md).

## Codex configuration

Codex CLI 0.154.0 is installed on the development machine. Its `exec --help` and
the [official MCP configuration guide](https://developers.openai.com/codex/mcp)
describe local stdio commands and per-server options. Set the command to an
absolute path because Codex's working directory need not be the repository.

For a regular local setup, add the following to the relevant Codex configuration
file after substituting your checkout path:

```toml
[mcp_servers.nsight_graphics]
command = "/absolute/path/to/nsight-graphics-mcp/build/linux-gcc-debug/nsight-graphics-mcp"
args = ["--artifact-root", "/absolute/path/to/managed-evidence"]
env_vars = ["DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "DBUS_SESSION_BUS_ADDRESS", "XDG_DATA_DIRS"]
required = true
```

If needed, add `"--nsight-root", "/absolute/path/to/Nsight-installation"` to
`args`. Forwarding desktop session variables permits the capture worker to use
the existing desktop; it still does not verify a desktop connection. Each attempt
gets isolated HOME/XDG configuration/cache/data/state and temporary directories
inside its managed bundle. `XDG_DATA_DIRS` preserves the caller's system data
search path; unset/empty values use `/usr/local/share:/usr/share` before Nsight
adds its layer location. This keeps normal Vulkan driver discovery available.
No authentication-file contents are copied into the MCP protocol. The core
forwards only its documented desktop session and data-search environment.
Server names, arguments, and timeouts use the
[official configuration fields](https://learn.chatgpt.com/docs/config-file/config-reference).

The interoperability run uses CLI `-c` overrides instead of changing any
persistent user or project configuration. It supplies an absolute server path,
requires startup, and asks Codex to call only `capabilities`. Run it from the
repository root after building:

```sh
codex exec --ignore-user-config --ephemeral --json --sandbox read-only \
  -c 'mcp_servers.nsight_graphics.command="/absolute/path/to/nsight-graphics-mcp/build/linux-gcc-debug/nsight-graphics-mcp"' \
  -c 'mcp_servers.nsight_graphics.required=true' \
  -c 'mcp_servers.nsight_graphics.env_vars=["DISPLAY","WAYLAND_DISPLAY"]' \
  -c 'features.shell_tool=false' -c 'features.unified_exec=false' \
  -c 'agents.enabled=false' -c 'features.apps=false' -c 'features.hooks=false' \
  -c 'web_search="disabled"' \
  'Call the nsight_graphics capabilities tool exactly once with {}. Use no other tools. Report the server version, negotiated protocol_version, implemented_tools, and capture availability. Distinguish discovered executable paths from verified compatibility.'
```

This is an explicit integration check using the installed, authenticated Codex
client and its model service. Ordinary CPU checks do not require Codex, network
access, an Nsight installation, or a GPU.

## Protocol boundary and validation

The pinned fastmcpp revision is
`29144985f51f41247584efe0c9cd2064de01b6fa` (3.4.7.1). The server uses its documented
`Tool`, `ToolManager`, and `make_mcp_handler` APIs for tool metadata, handshake
construction, invocation, and result construction. Source-inspected gaps are
handled by a small first-party adapter without modifying the vendored source:

- The library's opt-in input validator ignores `additionalProperties` and maps
  its failures to protocol errors. First-party workflow validation enforces exact
  fields, required values, types, ranges, path constraints, and byte limits before
  performing an operation. `capabilities` separately requires an empty object.
  Tool input failures return `isError: true`.
- The library's `StdioServerWrapper` maps malformed JSON to an internal error
  and has no input-size limit. First-party framing returns parse error `-32700`,
  validates request envelopes and IDs, and limits each line to 65,536 bytes.
  It rejects raw NUL bytes before parsing. A parser callback rejects nesting
  beyond 64 open JSON containers before building a deeper DOM, so later copies
  and destruction cannot encounter unbounded recursive structures.
- The library recognizes older protocol revisions whose batching contract this
  adapter does not implement. Negotiation is restricted to `2025-06-18` and
  `2025-11-25`. Any other requested revision receives the supported latest
  `2025-11-25` in the initialize response; a client unable to use that revision
  should disconnect, following the
  [MCP negotiation rules](https://modelcontextprotocol.io/specification/2025-06-18/basic/lifecycle#version-negotiation).

Invalid envelopes receive `-32600`, malformed method parameters receive `-32602`,
and unknown methods receive `-32601`. Tool input errors use `isError: true` for
both supported revisions. Tools require an `initialize` request followed
by `notifications/initialized`; premature tool requests receive `-32002`.
Notifications receive no response. An oversized line is drained before the next
request; oversized or overdeep requests receive `-32600`. The server recovers
after rejected input. A complete final JSON
request without a newline is processed on EOF; incomplete JSON receives a parse
error, then the server shuts down owned jobs and exits. There is no HTTP listener.
Capture work runs through the shared job coordinator; MCP handlers submit, poll,
cancel, and query retained evidence without running capture inline.

`ngm_mcp_check` is a C++ client that starts the actual executable through
`posix_spawn`, with isolated environment and fake installation files. It checks
discovery, identity, initialization, tool listing/calling, structured/text result
agreement, invalid arguments and envelopes, notification silence, input limits,
stdout isolation, and bounded EOF shutdown. Regressions include raw NUL after a
valid initialize prefix (which must not change lifecycle state), 16,384 nested
arrays, deep objects, the exact nesting boundary, and healthy requests following
rejection. It also checks negotiation for both supported revisions and fallback
from `2024-11-05`, `2025-03-26`, and an unknown revision. Fake files
exercise path discovery only and are never executed.

The same check also copies the C++ Nsight stand-in as matching discovery/capture/
replay executables and launches a test-owned CPU application through the actual
server/process boundary. It exercises asynchronous submission, fresh PIDs,
serialization, queued/running cancellation, timeout and EOF cleanup, evidence
publication, pagination, bounded text reads, binary/UTF-8 rejection, import,
persistent pins, unpin/prune, quota exhaustion, unknown session jobs, and argument/
CLI rejection. Discovery-only and invalid-input sessions must leave the configured
artifact root absent. These synthetic checks do not establish real Nsight or GPU
compatibility. New real Codex/capture validation must be recorded separately from
the historical 0.1.0 result below.

The inspection cases use synthetic observed-shape exports from the C++ stand-in
through real MCP calls. They exercise metadata selection and omitted process
context, event/object identities and pagination, encoded-byte page limits and
complete traversal, metadata validation before inventories, malformed/failed
exports, staging/failed/import rejection, and retained inspection after restart
without tool paths. They also check that complete inspection protocol replies
remain below 1 MiB and the raw MCP read cap remains 64 KiB. The independent
inspection-core regression adds report/manifest provenance and producer checks,
the same-bundle schema gate, parser/read bounds, raw-file references, and
over-limit metadata/individual-record errors. Executed validation results belong
in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

## Initial interoperability, 2026-09-18

The integrated `linux-gcc-debug` build and the focused command
`ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^ngm_mcp_check$'`
passed on the Linux development machine. The focused subprocess check completed
in 0.05 seconds. The standalone query reported `nsight-graphics-mcp 0.0.2`.

The Codex command above was then run with this checkout's absolute executable
path. Codex CLI **0.154.0** launched the server and completed exactly one real
`nsight_graphics.capabilities({})` call. The CLI exited 0 with a completed turn.
Both the JSON text content and structured content contained the same report:

| Observed field | Result |
| --- | --- |
| Server identity | `nsight-graphics-mcp`, project version `0.0.2` |
| Actual negotiated protocol | `2025-06-18` |
| Implemented tools | `capabilities` |
| Capture, inspection, profiling, fixture through MCP | `available: false`, `status: not_implemented` |
| Nsight executable observations | All three executable paths found under the installed 2026.3 directory |
| Nsight compatibility/version | `not_verified` / null |
| Desktop variables / connection | Both variables present / `not_verified` |
| GPU / driver | `not_probed` / null |

The protocol revision is taken from that connection's actual `initialize`
response and returned in `server.protocol_version`. It is distinct from the
library's default `2025-11-25` and the product version. The real Codex query
establishes tool discovery and invocation; the C++ subprocess check verifies
invalid request handling, stderr separation, and EOF shutdown directly.

Raw local results are retained in ignored
`build/mcp-acceptance/codex-query.jsonl` and `codex-query.stderr`. These are ordinary
development logs, outside the future server-managed artifact store; no capture
bundle or retention pin was created. The integration run changed no persistent
Codex configuration. It did not execute any Nsight command or GPU workload, so
the installation path is not a claim of Nsight release support.

This initial result predates the nesting/NUL, revision-negotiation, and tool-input
error corrections described above.

## Correction validation, 2026-09-18

The corrected GCC Debug build passes `ngm_mcp_check` and the complete 13-check
CPU suite. Fresh-context review additionally exercised malformed UTF-8, numeric
overflow, raw NUL, nesting/line limits, revision fallback, and recovery using
independent subprocess probes; no further substantive protocol finding remained.

Codex CLI 0.154.0 then repeated the real query against product version **0.1.0**,
negotiated **2025-06-18**, invoked `capabilities` exactly once, and exited 0 with
a completed turn. The version matched the standalone binary and initialize
identity. Capture, inspection, profiling, and fixture invocation remained
explicitly `not_implemented`. Logs are retained as
`build/mcp-acceptance/codex-query-0.1.0.jsonl` and
`build/mcp-acceptance/codex-query-0.1.0.stderr`. No persistent client configuration
was changed, and this query executed no Nsight or GPU workload.

## Capture/workflow CPU validation, 2026-09-18

The shared GCC Debug build, using **GCC 16.2.1** and product version **0.1.1**,
passed the expanded `ngm_mcp_check` in **6.57 seconds**. The configured focused
entry point is `ngm_mcp_check_run`. This run used the actual stdio server,
copied C++ Nsight stand-ins, and a test-owned CPU target in temporary directories.
It covered the protocol regressions and capture/job/artifact workflow cases
described above, including confirmed process cleanup and persistent pins.

The check used no real Nsight installation, GPU workload, network, or Codex
invocation. It establishes the adapter/process contracts under synthetic tool
behavior; it does not establish release compatibility or the real visual
capture/diagnosis workflow.

## Retained inspection and capture/evidence milestone

The R-012/R-011/R-001 group completes at **0.2.0**. The surface at that milestone had
**15 tools**. The full GCC Debug aggregate passes 20 CPU checks at the
pre-milestone 0.1.1 version; after the minor increment, the rebuilt
`ngm_mcp_check_run` passes in 14.39 seconds at 0.2.0. It covers the retained
inspection handlers alongside the existing capture/job/artifact tools. See
[BUILD_VALIDATION.md](BUILD_VALIDATION.md) for exact logs and review boundaries.

A separate C++ MCP client exercised all three typed queries against real,
pinned 2026.3.1.0 correct/faulty capture bundles. Each returns 22 events and
32 objects through five-record pages. Metadata remains identical after a fresh
server process opens the store without desktop variables or a usable tool PATH.
The complete responses, source, and report are explicitly pinned in
`bundle-e8bedf549310ff13da710678546d0a9f`. A retained 2026.2 capture returns the
expected `unsupported_producer` error in that historical 0.2.0 profile.
This validates inspection of retained exports; it does not establish detailed
event state, a rendered replay, source repair, or new Codex client coverage.

Basic real capture/export passes independently on both selected Nsight releases;
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) records the exact scope and evidence.

At **0.2.1**, both exact producer profiles are implemented and reviewed. The
focused MCP regression passes in **14.44 seconds**, including nullable/zero
`indirect_index` values and byte-bounded pagination, and again in **14.10 seconds**
after the final capability-description correction. A separate retained C++ MCP
probe passes all three query tools on **54 basic/advanced captures**, checking
every paginated event/object field against the corresponding raw export and
identical metadata after restart. It uses no desktop variables and
`PATH=/nonexistent`. Report, input expectations, probe/client sources, and complete
transcripts are explicitly pinned as `bundle-a1af3b8bb46feebabcc66a3bd821ab80`.
This broadens verified retained inventory coverage, without establishing detailed
state, GPU replay execution, source repair, or a new Codex client run.
