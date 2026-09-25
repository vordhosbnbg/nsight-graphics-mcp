# Persistent Streamable HTTP service

R-004 adds an optional local HTTP transport over the same shared tool implementation (25 tools at 0.3.4)
as stdio. R-004 completes at 0.3.2 after independent acceptance and pinned evidence publication.
Stdio remains the default. HTTP is a persistent process: stdin EOF has no effect,
and each HTTP client has an independent MCP initialization/session state.

## Selected deployment policy

The listener binds only to IPv4 `127.0.0.1`; there is no all-interfaces or remote
bind option. Every request requires a bearer token from a private file owned by
the server user. The token grants all tools and access to the shared workspace;
it does not provide per-client permissions or separate tenants. Do not forward
this endpoint through a proxy or tunnel to untrusted clients. Host headers must
name the local endpoint. Browser Origin headers are refused, and CORS is not
provided. TLS is unnecessary for this selected loopback boundary and is disabled
in the pinned dependency configuration.

All clients with the token act for the same trusted user. They can inspect each
other's jobs, cancel them and access the same managed artifacts. Job IDs belong
to the service lifetime; the existing coordinator retains at most 4,096 jobs
and requires a service restart when that limit is reached; artifact IDs and pins survive restarts. A TCP disconnect,
session expiry, or session DELETE does not cancel jobs. Use `job_cancel` and poll
for confirmed cleanup. Losing a submission response does not undo submission;
clients must not blindly retry capture requests as though they were idempotent.

SIGINT or SIGTERM stops accepting connections, interrupts partial HTTP reads,
finishes already executing tool calls, then cancels and cleans up service-owned
jobs. Tool calls share the existing serialized core; captures themselves remain
asynchronous. Unconfirmed cleanup produces a nonzero server exit. No daemonization,
automatic restart, service installation, or persistence of live job execution is
provided. Restart recovery uses the existing artifact reconciliation rules.

## Start the service

Build with the normal source presets. Generate a private token once in a directory
you control; this example creates a new file and refuses to overwrite an existing
one. The token is not printed or included in process arguments.

```sh
python3 - <<'PY'
import os, secrets
fd = os.open('http-token', os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'w') as output:
    output.write(secrets.token_hex(32) + '\n')
PY
build/linux-gcc-debug/nsight-graphics-mcp \
  --transport http --http-port 18080 \
  --http-token-file /absolute/path/to/http-token \
  --artifact-root /absolute/path/to/managed-evidence \
  --nsight-root /absolute/path/to/Nsight-installation
```

Token files must be regular, not symlinks or hard-linked files, owned by the server
user, and inaccessible to group/other users. Allowed token content is 32–128 ASCII
letters/digits/hyphens/underscores, with an optional final newline. Supply a random
value with at least 256 bits of entropy. The server reads the file once at startup;
rotate it by stopping the service, replacing the file and restarting. It never
logs token contents. Protect it like permission to run applications as your user.

`--http-port 0` selects a free local port. The listening URL is printed to stderr;
stdout stays empty in HTTP mode. Without `--artifact-root`, discovery works and
workflow tools report missing configuration. HTTP options are rejected in stdio
mode; HTTP requires a token file. Bind and token failures exit nonzero before
accepting clients. A second process cannot open the same managed artifact store.

## Connect Codex

Start the service, then configure its URL and the name of an environment variable
containing the same token:

```toml
[mcp_servers.nsight_http]
url = "http://127.0.0.1:18080/mcp"
bearer_token_env_var = "NGM_HTTP_TOKEN"
required = true
```

Supply `NGM_HTTP_TOKEN` to the Codex process from your private token file; do not
put the token itself in checked-in configuration. This is the documented
[Codex Streamable HTTP bearer-token configuration](https://developers.openai.com/codex/mcp#streamable-http-servers).
The local server must already be running. OAuth login is not used by this service.

## MCP transport contract

The single endpoint is `/mcp`. POST requests carry a single bounded JSON-RPC
message. The server returns JSON responses; notifications receive HTTP 202 with
no body. GET returns 405 because this implementation does not offer an SSE stream.
DELETE with a valid session ends that session and returns 204. There are no
server-initiated requests, streaming progress events, or resumable event streams.
Long work uses the existing asynchronous job tools.

Initialization negotiates MCP `2025-06-18` or `2025-11-25`, with the latter offered
for other requested revisions. A successful initialization returns a random
192-bit `MCP-Session-Id`. Subsequent requests carry it and the negotiated
`MCP-Protocol-Version`; missing version headers can use the version already
recorded in the session. Unknown/expired session IDs return 404; unsupported or
contradictory protocol headers return 400. Sessions expire after 30 minutes of
inactivity and are limited to 128. Session expiry never unpins or deletes evidence.

POST requires Content-Type `application/json` and Accept listing both
`application/json` and `text/event-stream`. Normal HTTP whitespace, media-type
casing and positive quality values are supported. The pinned fastmcpp HTTP client
emits Content-Type twice; equivalent JSON declarations are accepted, conflicting
ones are refused. Duplicate identity/authentication headers are refused.

Limits: 64 KiB JSON bodies, 64 nested containers, 16 KiB request-line/header bytes,
128 KiB total wire bytes per request, and 1 MiB encoded JSON responses. Four HTTP
workers and a 16-connection waiting queue bound listener concurrency. Active connections
have a 30-second processing lifetime (excluding time in the bounded waiting queue) in addition to five-second read/write inactivity
timeouts. These connection limits do not cancel an already submitted job or undo
an executing tool's side effects. Tool-specific evidence budgets remain unchanged.

The adapter uses pinned cpp-httplib for HTTP parsing/routing and the shared fastmcpp
handler/tool registry for MCP. First-party framing retains the existing strict
stdio validation and adds per-client lifecycle state. A small pinned-library
connection hook tracks owned sockets, interrupts partial requests at shutdown,
and bounds bytes before upstream request/header allocation. No vendored source
changes or new dependencies are needed.

The transport choices follow the [MCP transport specification](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports).

## Qualification

The focused CPU command is:

```sh
cmake --build --preset linux-gcc-debug --target ngm_http_check_run
```

The check launches the real server on an ephemeral loopback port and uses the
pinned fastmcpp StreamableHttpTransport/Client as well as raw HTTP requests.
The test uses the client's raw-result `call` API: its optional high-level result
coercion assumes string-only schema types and fails on valid nullable union types
in this server's tool schemas. The server preserves those schemas. It
checks authentication, Host/Origin rejection, independent negotiated sessions,
media-type handling, limits, tool discovery/invocation, stand-in capture jobs,
shared cancellation, surviving session deletion, shutdown with incomplete HTTP
requests, process cleanup, and pinned evidence after restart. It uses executable
Nsight stand-ins and does not establish any new Nsight/GPU compatibility.

The focused HTTP check passes in GCC Debug (3.81 seconds). Codex CLI 0.154.0
also connects to the real HTTP server, negotiates MCP 2025-06-18, discovers the
22-tool surface and successfully calls `capabilities` and `artifact_info` for
an existing pinned compute qualification bundle. The server remains alive after
the client exits; SIGTERM then exits cleanly with empty stdout. This run uses an
isolated command-line configuration, without changing the user's Codex config.
Its transcript and verified fields are in `build/http-qualification`.

Independent transport review identified and corrected partial-request shutdown,
upstream header-allocation bounds, and valid HTTP media-type handling. Regression
checks cover those paths, including a server-acknowledged unfinished request body.
The existing GPU evidence remains tied to its original transport, product, build,
and Nsight identities; the HTTP checks do not relabel earlier capture runs.

The full 0.3.2 GCC Debug aggregate passes 26/26 CPU checks, with no skips, in
254.13 seconds. Independent acceptance review finds no implementation blocker;
all qualification payload hashes and pin state after restart verify.

The complete qualification snapshot is pinned as
`bundle-237e85b8b723970aeb424b0e98b19ae8` in `artifacts/http-qualification`.
It contains 234 files (233 hashed payloads plus the manifest), including the
full aggregate, actual Codex transcript, reviewed source/binaries and both
independent reviews. The snapshot preserves pre-publication status wording;
this document and the roadmap record closure.

Independent publication verification and final closure documents are separately
pinned as `bundle-4615affc3e6a70d1499c82aacce19a2e` in the same store. That review
confirms every payload, the persisted pin/restart transcript and final status docs.
