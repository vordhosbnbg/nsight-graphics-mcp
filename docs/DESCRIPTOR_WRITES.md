# Source descriptor writes

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
