# Source descriptor writes

The **0.5.9 development slice** additionally implements the
[source-visible draw uniform model](#source-visible-draw-uniform-values-059-development)
below. Its verified qualification package awaits the owner storage-budget
decision before publication. The following source-write qualification retains
its 0.5.8 identity and scope.

At **0.5.8**, `capture_cpp_descriptor_writes` brings the shared stdio/HTTP surface
to **33 tools**. It lists literal descriptor-update calls in retained matching
**Nsight 2026.3.1.0/build 38722833** C++ captures, then optionally decodes one
listed call through the owner-approved confined seven-file worker.

Results describe source-scoped writes. They do not establish which update was
visible to a selected draw, an effective dynamic-buffer address, GPU-written
contents or a shader's typed material value. **R-024 remains In Progress**;
copies, resource/update lifetimes and actual shader type/layout evidence still
need their own qualification. Earlier recorded descriptor bindings and dynamic
offsets retain their separate [contract](DRAW_STATE.md).

## Query and configuration

First list calls, without a worker or active Nsight installation:

```json
{"capture_id":"<native-cpp-bundle-id>","limit":50}
```

Call `capture_cpp_descriptor_writes` again with that capture's listed
`update_ref` to read its source write:

```json
{"capture_id":"<native-cpp-bundle-id>","update_ref":"<listed-64-character-reference>","limit":50,"pin":true}
```

These examples show argument shapes; the placeholders must be replaced with
actual native IDs/references. `section` selects `updates` (default) or
`unsupported` when listing. `offset` defaults to zero (maximum 100000), and
`limit` defaults to 50 (range 1–100). A decoded result uses section `writes`
and `next_offset` for write pagination. Every read, including another page,
creates a separately retained attempt. `pin` defaults to false and requires
`update_ref`; it protects that attempt rather than implicitly changing the
source capture's retention policy.

Build the matching optional worker using the verified commands in
[DESCRIPTOR_WORKER.md](DESCRIPTOR_WORKER.md), then configure the server:

```sh
build/linux-gcc-debug/nsight-graphics-mcp \
  --artifact-root /absolute/managed-artifacts \
  --descriptor-worker-2026-3 /absolute/ngm-descriptor-worker-2026_3
```

The worker executable is trusted operator configuration, never a tool argument.
The existing `--resource-worker-2026-3` byte reader is independent and cannot
decode descriptor writes. The parent requires its exact compiled-in original,
instrumented, bounds, patch and recipe identities; the earlier 0.5.7 prototype
worker refuses against this parent because the recipe fingerprint changed.
Capabilities report configuration separately from successful qualification.

## Evidence and bounds

Only complete native C++ captures with the matching producer and complete
capture-time source/database fingerprints are eligible. The query rechecks
those fingerprints, leases the source and binds each reference to the capture,
all input identities and the exact literal source span. A foreign, missing or
stale reference refuses. The existing source scan limit is 64 files / 16 MiB.

The parser accepts literal wrapped `VulkanHelper_UpdateDescriptorSetsFromDatabase`
calls with zero descriptor copies. Conditional updates remain listed with a
reason and cannot be decoded. Unsupported helper forms remain visible in the
separate coverage section. Typed `VulkanHelper_RegisterObject` declarations
associate serialized IDs with source symbols for descriptor sets, buffers,
image views and samplers. Numeric symbol suffixes are never assumed to equal
serialized IDs. Conflicting or unknown registration coverage prevents decoding.

Supported write types are uniform/storage buffers, their dynamic variants and
combined image samplers. Each write retains destination set, binding, first
array element, type, count and exact buffer offset/range or image-view/sampler/
layout fields. Object fields include their typed serialized ID, source symbol
and hashed source spans. Null resources remain null; `VK_WHOLE_SIZE` remains its
unsigned sentinel. No immutable sampler, array selection or resource extent is
inferred from those fields.

The parent validates all seven original helper fingerprints, snapshots and
hashes both database files, checks the worker profile and executable identity,
and uses one 20-second deadline across the source checks and child operations.
The child runs with an empty environment through owned-process supervision and
the existing fail-closed confinement. Its [worker bounds](DESCRIPTOR_WORKER.md)
remain unchanged. Duplicate/extra JSON fields, malformed types, unexpected IDs,
count/extent mismatches, overflow, diagnostics, nonzero exit, timeout, changed
inputs or unconfirmed cleanup refuse.

Each successful attempt retains exact input copies, process records, worker
stdout/stderr, profile/hashes and `raw/descriptor-read.json`. Failed attempts
retain the available evidence and a failed status; unconfirmed cleanup stays
quarantined. Quota exhaustion preserves protected sources and the failed
attempt. Structured results include the attempt ID and provenance; full raw
results remain available in the artifact. Response pages obey the existing MCP
budget, and oversized rows refuse with their retained attempt ID.

## Measured qualification

The actual 0.5.8 stdio client independently validates advertised input/output
schemas, JSON/text equivalence and clean EOF. Retained source reads work with an
empty selected toolchain directory. Two restored 0.5.6 sources produce **35**
successful schema-valid results, **six** expected worker/configuration refusals
and **four** repeat reads over two server restarts. All **306** original source
payload identities remain unchanged.

Three fresh original-application C++ captures use new seeds, 256×160 pixels,
`wait_frames: 7` and a fresh target per capture:

| Workload | Seed | Decoded source writes |
| --- | ---: | --- |
| `reference` | 931 | One uniform-buffer write, offset zero / range 16. |
| `combined-reference` | 137 | Two distinct storage-buffer array slots and one combined image/sampler write, layout 5. |
| `state-descriptors-reuse` | 73 | Sixteen dynamic-buffer writes across four sets, including the single partial array replacement. |

The fresh matrix passes **190** schema-valid results, two server restarts,
identical decoded fields/input hashes and persistent pins. Six additional
bounded source pages establish exact debug-name/source-symbol associations.
The dynamic-array fields match the independent private fixture API witness;
that witness stays distinct from product evidence. Seven separately supervised
reads through the existing 0.4.2 byte worker reproduce each full opaque input's
length and SHA-256. The fresh exported snapshot already incorporates the partial
update; this does not recover its original update history or establish GPU use.

The frozen server SHA-256 is
`64f72257304bf4a6dc6a925169680e53e8dd904b4b9b0eb1e815ad50bfc2c654`;
the worker is `4c31cce685f4bf6d76e61cde1b909ad0ed5647587f25efb456e17266f3ae017a`.
I-077–I-082 preserve the initial build/schema and private harness failures,
including the invalid scenario and absent toolchain directory. No driver,
desktop, profiling permission, clocks or installed server/worker is changed.
The process-local matching runtime is the separately recorded 0.5.6 prerequisite.
CPU aggregate and final retention are recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

All **43 CPU checks** pass in **433.91 seconds**. The optional real-helper
check and a clean configure with the helper option empty also pass. Complete/
pinned **bundle-52a3d6a428d29f5fad1e043e61fb9983** retains all **343** verified
payloads, including a lossless snapshot of **1,764 file paths / 468 unique
content blobs** and all **41 native bundle instances**. The imported portable
verifier reconstructs every file and passes **313** total schema-valid results
(**231** from the final qualified runs), source/input/reader checks, private
oracles and the preserved failures. The installed 0.4.1 importer identity stays
separate from the tested 0.5.8 server/worker.

## Source-visible draw uniform values (0.5.9 development)

`capture_draw_values` lists bounded draw occurrences from a matching direct C++
capture, then optionally composes source descriptor updates, buffer uploads and
shader layouts for one listed occurrence. The shared surface has **34 tools**.
The result remains generated-source evidence:
`captured_gpu_execution_correspondence: false`. No shader invocation/register
value or numeric Graphics Capture event-ID relationship is inferred. The source
domain's `frame: 0` identifies the exported captured frame; debug-label text
does not replace an original capture frame identity.

List candidates without a worker:

```json
{"capture_id":"<native-cpp-bundle-id>","section":"executions","limit":50}
```

Read a selected candidate using **both** trusted operator options
`--resource-worker-2026-3` and `--descriptor-worker-2026-3`:

```json
{"capture_id":"<native-cpp-bundle-id>","execution_ref":"<listed-64-character-reference>","pin":true}
```

References bind the capture, complete capture-time source/database fingerprints
and literal submission/recording/draw selection. Listing also supports
`section: "unsupported"`. Selected results page `values`; offsets/defaults match
the source-write query. `pin` protects the query and its child reads, and must
be requested separately for the source capture. Every selected call, including
pagination, creates a new retained attempt.

The model requires a complete supported source history. It validates queue,
device, command-pool, command-buffer and fence relationships; descriptor pool,
set, write/copy and recorded-use lifetimes; exclusive, nonaliased buffer-memory
bindings; coherent host uploads and explicit completion before later updates.
All submitted vertex/fragment modules must pass the bounded no-external-buffer-
write proof. Unknown effects anywhere in the captured frame discard all
provisional plans, including effects after the requested draw. Unsupported
GPU writes, aliases, ownership transfers, shader control/layout forms and
incomplete byte coverage refuse values.

Supported typed reads are direct constant-member loads from plain uniform
blocks in the qualified single-entry, single-function, straight-line vertex/
fragment SPIR-V subset. Values are 32-bit float, signed/unsigned integer scalars
or vectors of two to four components. Each component retains its raw bits;
nonfinite floats have null numeric values with explicit classification. Shader
names are source/debug names rather than material semantics. Buffer range,
effective offset, member offset, module hash, load position and the full
descriptor/upload/shader proof accompany the value. The selected input buffer
must have the qualified GPU-read-only usage profile; opaque initial bytes alone
never establish a later value.

Checked exported helper semantics stay read-only text. Five implementation/
interface files require exact hashes; `ReplayProcedures.cpp` additionally
permits only reordering its complete set of 15 known literal function
prototypes before the first body. All executable bytes remain fingerprinted.
Raw and canonical hashes are retained. No generated replay/application code is
compiled or executed, and the proprietary worker closure remains the approved
seven files.

Selected calls share one **20-second** deadline, at most **32 parent reads**
and **1 MiB** of returned byte/structure payloads. Each shader or uniform-prefix
read is at most **64 KiB**. Pages are bounded to **256 KiB**, and full retained
proofs to **16 MiB**. Large proofs use `artifact_read`'s existing local-file
reference contract. Successful and failed attempts retain
`raw/draw-values.json`; protected evidence is never removed to satisfy quota.

Three fresh direct C++ captures contain **54** submitted draw occurrences with
two different seeds/resolutions and descriptor copies/host updates between
three submissions. The public 0.5.9 query passes **835** schema-valid results,
all **216** typed component-bit comparisons against separate private API and
independent numeric oracles, nine missing-worker/foreign-reference refusals
and **18** repeat value reads over two server restarts. Native source payloads
remain byte-identical and source/query/child pins persist. Original application
GPU readback is checked separately against independent renderer images; it is
not product evidence of a shader invocation value.

The actual descriptor-source repair and final CPU/publication receipts are
recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md). R-024 is Blocked on the
pending storage-budget decision before final qualification publication; the
source model is not a broader GPU-state or register-debugging capability.
