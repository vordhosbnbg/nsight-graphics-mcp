# Local stdio MCP server

The server exposes one read-only tool, `capabilities`, through a local stdio
connection. It reports the project version, negotiated MCP revision, implemented
operations, and prerequisite observations. Capture, export inspection, profiling,
and launching the separate Vulkan fixture through MCP are not implemented.

`capabilities` accepts `{}` or omitted `arguments`. Unknown object properties are
tool input errors: the call returns a tool result with `isError: true` and text
explaining how to correct it. Malformed call parameters, including non-object
`arguments`, produce JSON-RPC error `-32602`. Successful results contain both
`structuredContent` and an equivalent JSON text block, with an advertised output
schema. This distinction follows the MCP
[2025-11-25 tool error contract](https://modelcontextprotocol.io/specification/2025-11-25/server/tools#error-handling)
and uses the invalid-input tool error mechanism also present in
[2025-06-18](https://modelcontextprotocol.io/specification/2025-06-18/server/tools#error-handling).

Executable discovery observes regular executable files; it does not run Nsight,
read a tool version, probe a GPU, or validate a display connection. The report
keeps those states `not_verified` or `not_probed` and leaves unobserved versions
and GPU identities null. A visible `DISPLAY` or `WAYLAND_DISPLAY` environment
variable is only a hint. The fixture's application readback is separate from
Nsight evidence and is not exposed by this tool.

## Build and run

Use the source setup in [README.md](../README.md). The MCP implementation uses
the existing pinned, statically linked fastmcpp and nlohmann/json sources.

```sh
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug --target nsight-graphics-mcp ngm_mcp_check_run
./build/linux-gcc-debug/nsight-graphics-mcp --version
```

Without arguments, `nsight-graphics-mcp` reads line-delimited JSON-RPC from stdin
until EOF. Stdout contains only protocol replies. Negotiation/shutdown diagnostics
use stderr; `--version` and `--help` are separate standalone commands, never
startup banners.

Default discovery searches `PATH` for `ngfx`, resolves symlinks, then searches for
`ngfx-capture` and `ngfx-replay` beside it (or in its installation's
`host/linux-desktop-nomad-x64` directory), with `PATH` as a fallback. Findings can
belong to different releases and do not establish compatibility. Select a single
installation explicitly with an existing absolute directory:

```sh
/absolute/path/to/nsight-graphics-mcp --nsight-root /absolute/path/to/Nsight-installation
```

An explicit root searches `host/linux-desktop-nomad-x64` and the root itself,
without falling back to other `PATH` installations. A directory with missing
executables still permits discovery and reports them as absent. An invalid root
exits with code 2 and an actionable stderr message.

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
env_vars = ["DISPLAY", "WAYLAND_DISPLAY"]
required = true
```

If needed, select an installation with
`args = ["--nsight-root", "/absolute/path/to/Nsight-installation"]`. Forwarding
desktop environment variables makes their presence observable; this still does
not verify a desktop connection. Server names, arguments, and timeouts use the
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

- The library's opt-in input validator ignores `additionalProperties`; the
  `capabilities` handler explicitly requires an empty object and returns tool
  input errors as `isError: true`, rather than the library's JSON-RPC mapping.
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
error, then the server exits normally. There is no HTTP listener or background
job lifecycle in this slice.

`ngm_mcp_check` is a C++ client that starts the actual executable through
`posix_spawn`, with isolated environment and fake installation files. It checks
discovery, identity, initialization, tool listing/calling, structured/text result
agreement, invalid arguments and envelopes, notification silence, input limits,
stdout isolation, and bounded EOF shutdown. Regressions include raw NUL after a
valid initialize prefix (which must not change lifecycle state), 16,384 nested
arrays, deep objects, the exact nesting boundary, and healthy requests following
rejection. It also checks negotiation for both supported revisions and fallback
from `2024-11-05`, `2025-03-26`, and an unknown revision. Fake files
exercise path discovery only and are never executed. These checks do not validate
future asynchronous jobs, cancellation, image/resource responses, or real Nsight
capture compatibility.

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
