# Generated-resource queries

Version 0.2.12 adds `capture_cpp_resources` and `capture_cpp_resource`, bringing
the stdio surface to 22 tools. The former lists source references; the latter
reads bounded opaque bytes through the separately built qualified worker.
These are serialized resource inputs from a generated C++ capture, not arbitrary
buffer/image contents after a GPU event or executed descriptor selection.

## Configuration

Build the optional profiles following [RESOURCE_WORKER.md](RESOURCE_WORKER.md),
then configure their absolute executable paths when launching the server:

```sh
build/linux-gcc-debug/nsight-graphics-mcp \
  --artifact-root /absolute/managed-evidence \
  --resource-worker-2026-3 /absolute/build/ngm-resource-worker-2026_3 \
  --resource-worker-2026-2 /absolute/build/ngm-resource-worker-2026_2
```

Either profile may be omitted. Source listing needs neither worker nor a current
Nsight installation. Byte reads require the worker matching the retained exact
producer: 2026.3.1.0/build 38722833 or 2026.2.0.0/build 37991608. Each call verifies
all five retained helper fingerprints against compiled-in profiles, then verifies
the worker's profile before reading. Operator-configured worker executables are
trusted code; MCP callers cannot choose a binary or supply their own profile.
Capability discovery reports configuration only and executes no worker.

## Tool contracts

`capture_cpp_resources` requires `capture_id`; optional `section` is `resources`
(default) or `unsupported`, `offset` defaults to zero, and `limit` defaults to 50
(maximum 100). Pages stop at the shared 256 KiB response budget; follow
`next_offset` until null. Each section has its own total and pagination.

The parser examines indexed `CommandList*`, `Resources*`, and `Frame*` C++ source
files, up to 64 files, 4 MiB each and 16 MiB combined. Generated helper
implementations are excluded. It identifies literal `NV_GET_RESOURCE`,
`NV_GET_RESOURCE_CHECKED`, and `NV_GET_RESOURCE_STATIC` occurrences, ignoring
comments and quoted text. It accepts canonical decimal handles and byte counts
with the qualified integer suffix forms. Type expressions are retained as text.
Unsupported macros/expressions and resource-like text inside preprocessor
directives produce coverage records; malformed source fails explicitly.

Each reference includes the capture-local handle, macro, declared size (nullable),
expected size from consistent literal declarations (nullable), preprocessor
conditional flag, readability/reason, and exact source path/hash/line/byte span.
`resource_ref` hashes the source path, complete source hash and occurrence span.
Conflicting, zero or oversized literal size declarations make that handle
unreadable. Listing a conditional or runtime-controlled occurrence does not
assert execution. The `conditional` field describes preprocessor nesting only.

`capture_cpp_resource` requires `capture_id` and a listed `resource_ref`.
Optional `offset` defaults to zero, `length` defaults to 65536 (range 1–65536),
and `pin` defaults to false. The resource is limited to 16 MiB. The service
reparses sources and resolves the reference; arbitrary caller-supplied handles,
sizes or source files are not accepted. The response contains hexadecimal data,
range/total size, `next_offset`, byte hash, source reference, exact input hashes,
worker identity, producer observations, and a retained result artifact/report.
Reading at the resource end returns empty bytes and no next offset.

## Input and process boundary

The source capture stays leased through the operation. The artifact store copies
inventoried database files into exclusive read-only files in the attempt's staging
directory, using component-wise no-follow traversal, regular-file/single-link
checks, bounded streaming, deadlines and checks for observed source mutation.
Limits are 256 MiB for `data.bin` and 16 MiB for `data.bin.rec`. Hashes identify the
copied bytes, not a later reopening of the original capture.

The process supervisor supplies an empty environment, null stdin, separate
regular stdout/stderr logs and a ten-second deadline for fingerprinting,
snapshots and worker invocations. Filesystem calls are not preemptible. The
worker's Landlock/seccomp/resource restrictions are described in
[RESOURCE_WORKER.md](RESOURCE_WORKER.md); missing enforcement fails closed.
The parent rejects signals, unsuccessful exits, timeouts, unconfirmed cleanup,
diagnostics, duplicate JSON keys, wrong profile/handle/range/size, truncation,
extra output and observed input/worker changes. It never treats partial output
as successful evidence.

## Retention and provenance

Every started read attempt creates a managed bundle containing
`raw/resource-read.json`, profile/read logs, and available `raw/input` snapshots.
Success also retains the exact database snapshots, so publication failures cannot
discard the input evidence. This can consume up to 272 MiB per call, in addition
to reports and logs, even for a small byte range. All bytes count toward artifact
retention limits. Use bounded requests deliberately and unpin evidence when it
is no longer needed; protected evidence is never deleted to satisfy quota.

The read's `pin` protects its bundle only. Source-capture pinning is independent.
Reports distinguish successful extraction from failed attempts; unconfirmed
process cleanup quarantines the attempt. Failures identify the evidence artifact
when one was allocated. Helper profiles identify supported reader code and hashes
identify observed bytes; neither authenticates a capture's provenance.

## Validation

The CPU checks `ngm_resource_reference_check`, `ngm_resource_read_check`,
`ngm_cpp_inspection_check`, and `ngm_mcp_check` cover the parser, process boundary,
snapshot handling, quotas, producer/report checks and protocol registration.
The command-line stand-in is test-only and establishes no Nsight compatibility.

`ngm_retained_resource_integration` is an explicitly invoked C++ qualification
runner. It uses actual MCP listing/byte queries, independent byte-hash oracles,
range reconstruction, snapshot/source hashes, pins and server restart. It does
not run GPU work. Qualification passes 43 product captures (including six freshly
generated on the two matching Nsight releases): 159 independent byte oracles,
636 oracle reads, 30 separately counted large-resource range checks, five expected
historical-probe refusals, and pins/restart. The full CPU aggregate passes 25/25.
Exact results and limitations are in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
The complete evidence is pinned as `bundle-6365322f8eb3922893e1d2256983f732` in
`artifacts/nsight-resource-query-evidence`, including independent acceptance and
788 referenced pins.
