# Capture inspection evidence

This R-006 slice parses metadata, event inventories, and object
inventories from actual Nsight Graphics exports and exposes bounded queries over
retained server capture bundles. At product **0.2.1**, capture and typed queries
pass for **54 basic/advanced captures** from matching **2026.3.1.0/build 38722833**
and **2026.2.0.0/build 37991608** tools, on Linux with RTX 3080 Ti / driver
615.71.09 and the repository's windowed Vulkan fixture. This qualifies the two
exact inventory profiles and selected workloads, not arbitrary Nsight releases.

**R-006 and R-007 remain incomplete.** Inventory parsing does not provide the
detailed state required to diagnose a defect, edit its source, rebuild, recapture,
and verify the fix. This document records the parser contract and observed
exports; [MCP.md](MCP.md) and [BUILD_VALIDATION.md](BUILD_VALIDATION.md) own the
integrated tool surface and executed validation results.

Separate **0.2.2 experiments** demonstrate generated source and selected shader
bytes for the basic correct/faulty pair on both releases. Product **0.2.3** now
retains generated projects through `capture_cpp` and artifact reads; its six basic
MCP captures pass on both matching releases. This is a separate evidence format
from the metadata inventories. See [CPP_CAPTURE.md](CPP_CAPTURE.md) and the
experimental extraction results below.

## Observed evidence and limits

The initial 2026.3.1.0 correct/faulty pair is retained in explicitly pinned bundles
`bundle-019e2411c941dafdbfba953018451416` (`reference`) and
`bundle-3f5db7a4c55c1a3e1b6a53636b091066` (`shader-error`), under
`artifacts/nsight-evidence/bundles/`. Each bundle retains the original capture,
exports, process logs, capture report, and application-provided shader provenance.
The earlier pinned diagnostic bundle
`bundle-0863b91c8d1a178482d1cd3abebff32c` and repeated reference bundle
`bundle-55689147d2b220f4b7b72aff355e0800` have the same observed metadata shapes.
The [fixture provenance record](../tests/fixtures/nsight-2026.3.1/README.md)
identifies sanitized regression inputs and exact source hashes.

| Evidence category | Observed documented export | What it establishes and what remains missing |
| --- | --- | --- |
| Capture identity and workload summary | `--metadata`, JSON `metadata_version: 1` | Tool/build, capture UUID, process name, API/GPU/driver, frame/resolution strings, feature inventory, and collection warnings. Fields are exported facts; optional absence stays explicit. |
| Function/event inventory | `--metadata-functions`, JSON array; 22 entries in each basic capture | `event_index`, `function_name`, `thread_index`, optional `sequence_id`, and optional `indirect_index` in the tested 2026.3 indirect workloads. No API arguments, object references, or complete event state. |
| Object inventory and names | `--metadata-objects`, JSON array; 32 entries in each basic capture | `uid`, `api`, `object_name`, `type_name`, and `access_flags`. Includes application labels for pipelines, shaders, and buffers; no object definitions or event associations. |
| Capture screenshot | `--metadata-screenshot`, 192×128 PNG in each pair bundle | A capture screenshot is available as `raw/exports/screenshot.png`. This does not expose an arbitrary selected resource or framebuffer at an event. |
| Embedded capture logs | `--metadata-logs`, empty file in each pair bundle | These exports contain no log entries. The separate metadata collection contains a version warning and must not be replaced by the empty log result. |
| Pipeline state and draw-to-pipeline association | Absent from the inspected metadata exports | A `vkCmdBindPipeline` name and a pipeline object label do not identify the pipeline bound at `vkCmdDraw`, nor its raster/depth/blend state. |
| Descriptor bindings and resource selection | Absent from the inspected metadata exports | `vkCmdBindDescriptorSets`, descriptor-set names, and opaque access values do not establish binding contents or selected buffer values. |
| Shader bytes, source, and draw-to-shader association | Absent from the inspected metadata exports | Shader-module names are visible. Retained GLSL/SPIR-V under `raw/application/shaders/` is application-provided evidence, not shader extraction by Nsight. |
| Buffer/texture contents at an event | Absent from the inspected metadata exports | An object inventory and final screenshot do not reveal arbitrary resource bytes, subresources, or event-specific contents. |
| Debug-label text and pass relationships | Debug-label function names occur, without their arguments | Begin/end function names do not provide label text, command-buffer association, or a verified pass hierarchy. |
| Bindless, indirect, and multipass evidence | Advanced inventories and final screenshots are exported on both releases | Function names and object labels expose inventory only. Descriptor selection, indirect command arguments/bytes, offscreen contents, and pass relationships remain absent. An opaque `indirect_index` is not a reconstructed command or association. |

These are limits of the inspected export path, not universal claims about Nsight
capabilities. Metadata export and a captured screenshot also do not, by themselves,
establish successful execution of a GPU replay loop. The pair's metadata warns
that capture `2026.3.1` is newer than replayer `2026.3`, with the same build ID;
the warning is retained verbatim as evidence.

The documented entry points are the [Graphics Capture CLI metadata
options](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html).
Each raw export remains available in its bundle; the typed summary deliberately
omits `process_environment` and `process_command_line` so it does not automatically
forward process context into ordinary inspection responses.

## Basic and advanced matrix at 0.2.1

The capture harness now selects nine reference/fault pairs, each repeated with
three fresh captures on each release. All **54 captures** pass capture, all five
exports, process cleanup, repeated-reference PNG identity, variant PNG difference,
and persistent pins after restart. Exact commands, inputs, and evidence are in
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). The batch and its export-inventory
summary are explicitly pinned as `bundle-a4bce77c1a8d615412702da25e33fa75`.

| Workload pairs | Events per capture | Objects per capture |
| --- | ---: | ---: |
| Basic shader, binding, pipeline | 22 | 32 |
| Multipass / pass-output error | 32 | 44 |
| Bindless / resource-selection error | 23 | 32 |
| Indirect / indirect-parameter error | 23 | 34 |
| Combined / pass, resource, or indirect error | 34 | 46 |

Counts are the same for both releases and correct/faulty members within each
workload; equal counts do not imply equivalent rendering or state. The inspected
object records contain only the five fields above. Function records contain the
three required fields and optional `sequence_id`, plus `indirect_index` for the
2026.3 indirect/combined records. The observed marker is zero at the exported
`vkCmdDraw` record following `vkCmdDrawIndirect`; 2026.2 omits it. This is retained as an opaque integer,
with no assumed parent/child relationship, expanded draw, or buffer offset.
The basic 2026.2 metadata has no collection `warnings` member, while 2026.3 retains
its version warning. Some semaphore/fence `access_flags` differ (0 versus 524288)
and likewise receive no Vulkan interpretation. Small identified inputs for both
profiles are retained under `tests/fixtures/`, including indirect correct/faulty
pairs; their README files record exact sanitization and pinned source hashes.

All required deeper-state categories in the table remain absent from these
basic and advanced metadata exports on both exact producers. This is a measured
gap in this export path, not a claim about every documented Nsight interface.
No failing extraction attempt is hidden by a successful inventory query.

## Retained inspection core and MCP queries

[Inspection.hpp](../include/ngm/Inspection.hpp) and
[Inspection.cpp](../src/inspection/Inspection.cpp) implement the transport-independent
`InspectionService` over `ArtifactStore`. `capture_metadata`, `capture_events`,
and `capture_objects` are the MCP adapters; their inputs and outputs are described
in [MCP.md](MCP.md). `capture_id` is the managed capture bundle ID returned by
capture submission, not the exported capture UUID. Every returned event/object
ID belongs to that capture only. The optional capture UUID remains a separate
exported value. The service preserves export order and does not infer a relation
between event IDs, object IDs, repeated sequence IDs, names, or access values.

Every query acquires a usage lease before inspection and holds it until parsing
and result construction finish. It accepts only a complete, unquarantined
`nsight_capture` bundle from the documented CLI service. Its schema-1 manifest
and report must agree on capture/job identity, producer observations, and success
and cleanup fields. The report must record a successful capture operation and
successful requested export with exited, cleanup-confirmed processes. Expected
output paths and byte counts must match the published inventory. A caller import
keeps its `caller_provided_import` origin and imported partition; placing copies
of service-looking reports inside an import does not make it inspectable as a
server capture. These consistency checks preserve recorded provenance and do
not provide producer authentication or verify a GPU replay loop.

The schema profiles are deliberately tied to two observed Vulkan producers,
both with metadata version integer `1`:

| Capture and replay CLI version | CLI and metadata build | Exported `nsight_version` |
| --- | --- | --- |
| `2026.3.1.0` | `38722833` | `2026.3.1` |
| `2026.2.0.0` | `37991608` | `2026.2.0` |

Capture, replay, and metadata must match one complete row; a mixture is rejected.
All producer strings retain their exact spelling in query results. Another
release, build, API, missing producer field, or unsupported metadata version
fails explicitly until a sample and profile have been validated. The
metadata export from the **same bundle** is checked before either unversioned
event/object inventory. A valid inventory alone cannot establish its schema or
producer. The current installed tools are not consulted when reading retained
exports, so inspection remains available after restarting without Nsight paths
or desktop variables.

Queries return the relevant `source` path/byte count, `metadata_source`, and
`report_source`, all relative to that capture bundle. Selected metadata fields
use explicit nulls for absence; collection warnings are preserved. Process
environment, command line, arbitrary report fields, and caller-provided
application provenance are excluded. The
`unavailable_from_these_exports` array explicitly lists API arguments,
event/object associations, pipeline state, descriptor bindings, shader bytes and
source, event/shader associations, resource contents, and pass relationships.
It describes this inspected export path, not all Nsight interfaces. Raw exports,
report, and application evidence remain separately retrievable using artifact
tools, with their own recorded origins.

The core reads only inventoried files: reports are limited to 2 MiB and 32 open
JSON containers, with duplicate keys rejected; each evidence export is limited
to the parser's 16 MiB. The artifact core permits an explicit maximum of 16 MiB
while retaining its 1 MiB default. MCP `artifact_read` still caps text reads at
64 KiB. Inventory queries accept at most 100 records per page (default 50) and
stop at 256 KiB of encoded structured JSON. `total` counts all parsed records;
`next_offset` points immediately after the last returned record, or is null at
the end. Offsets refer to array positions, not event/object IDs. Queries never
silently discard over-limit exports or rows. An individual record that cannot
fit receives an error pointing to its raw file. Metadata and page results also
check the serialized structured value plus the escaped text fallback and
reserved protocol overhead against 1 MiB.

Inspection errors have actionable text and stable codes: `not_capture`,
`incomplete_capture`, `invalid_capture_report`, `export_unavailable`,
`unsupported_producer`, `invalid_export`, and `inspection_limit`. Existing
artifact errors distinguish absent, expired, unpublished, or damaged bundles.
A successful metadata query can coexist with an unavailable optional inventory;
the report's per-export outcome remains authoritative.

[InspectionCheck.cpp](../tests/InspectionCheck.cpp) creates synthetic managed
bundles containing the sanitized observed fixtures, without claiming they are
real captures. It checks metadata selection, exact producer spelling, warnings,
scope, pagination and byte budgets, invalid reports and producer mismatches,
failed/missing exports, metadata-before-inventory validation, imported evidence
rejection, and explicit read/result bounds. Its two arguments are the checked-in
2026.3.1 and 2026.2.0 fixture directories, in that order.
[McpCheck.cpp](../tests/McpCheck.cpp) independently exercises
the actual tool handlers and server process boundary using the C++ Nsight
stand-in. These CPU checks do not establish real Nsight compatibility. Build/test
and real retained-capture MCP results are recorded separately in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md).

## Pure parser contract

[NsightEvidence.hpp](../include/ngm/NsightEvidence.hpp) declares three functions
that take a complete `std::string_view` and return typed values:

- `parse_nsight_metadata` returns `NsightMetadata`.
- `parse_nsight_functions` returns `std::vector<NsightEvent>` in export order.
- `parse_nsight_objects` returns `std::vector<NsightObject>` in export order.

The parsers own no processes, files, artifact leases, pagination, or capture
identity. The caller must lease the bundle, read with an explicit byte limit,
associate results with that capture, and retain producer version/build provenance.
Function and object arrays contain no schema-version field of their own; the
caller must validate metadata from the same bundle before exposing inventories.
A parser result alone does not establish the producer release.

`metadata_version` is required and must be integer `1`. Other selected metadata
fields are optional: missing values are `std::nullopt`; a present value of the
wrong type, including `null`, is rejected. Selected text fields retain their
exported spelling, including `captured_frame`, `resolution`, `non_portable`, and
the version/build strings. `has_unsupported_operation` is a boolean;
`graphics_apis` and `graphics_features` are maps of strings to string arrays.
`_metadata_collection_` has independently optional `info` and `warnings` string
arrays. Present empty collections remain distinguishable from missing ones.

Every event requires unsigned 64-bit `event_index` and `thread_index`, plus a
nonempty `function_name`; `sequence_id` and `indirect_index` are optional. Every object requires
unsigned 64-bit `uid` and `access_flags`, nonempty `api` and `type_name`, and an
`object_name` string that may be empty. Numeric strings, fractions, negatives, and
overflow are rejected. Duplicate event indices or object UIDs within an export
are rejected. IDs are capture-scoped, never Vulkan handles or cross-run keys.
Repeated sequence IDs are allowed, as observed in the actual exports.

`sequence_id`, `indirect_index`, and `access_flags` remain opaque observed integers. The parser does
not interpret access flags as Vulkan constants, join events to objects by name or
order, sort records, require contiguous IDs, infer pass nesting, or reconstruct
state. Additional JSON fields are accepted within the same safety limits and
ignored; the raw export remains the source for fields not represented here.

All invalid inputs throw `NsightEvidenceError` with a stable code:
`malformed_json`, `limit_exceeded`, `invalid_schema`, `missing_field`,
`invalid_field`, `duplicate_identifier`, `duplicate_key`, or
`unsupported_version`. Diagnostics identify the known field/index or byte
position without echoing raw values, environment text, or arbitrary object keys.

| Parser limit | Value |
| --- | --- |
| Complete input text | 16 MiB per export |
| Events or objects | 100,000 records per export |
| Simultaneously open JSON containers | 16, including the root |
| Decoded UTF-8 bytes per JSON string or key | 16,384 |
| JSON values, including containers and excluding object keys | 1,000,000 |
| Entries per JSON array or object, including ignored fields | 100,000 |
| Entries per selected metadata map or string list | 256 |
| Total decoded bytes of selected metadata strings and map keys | 64 KiB |

These are project analysis bounds, separate from the backend's larger raw-export
storage bound. Inputs outside them fail explicitly; records are never silently
truncated. A streaming JSON validation pass checks size, nesting, counts, strings,
and duplicate keys before constructing a JSON tree. The second parse sees only
bounded valid text. Unknown fields and excluded process context must also fit
the global limits. The selected metadata total excludes fields not returned.

[NsightEvidenceCheck.cpp](../tests/NsightEvidenceCheck.cpp) exercises the observed
basic and indirect correct/faulty pairs from both releases, optional sequence and
indirect IDs, preserved warnings and labels,
missing/wrong/range-invalid fields, duplicate IDs and escaped duplicate JSON keys,
unknown metadata versions, malformed UTF-8/JSON, string and metadata budgets,
record/value counts, and deeply nested ignored fields. Synthetic mutations are
kept in the check source rather than attributed to real exports. The check is
CPU-only and needs the two checked-in fixture directories in the same order; it
does not require Nsight or a GPU. Executed build/test results belong in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md).

The integrated 20-check CPU aggregate and fresh-context review pass. A real C++
MCP probe exercises the pinned correct/faulty pair described above, all three
typed tools, five-record pagination, and identical metadata after server restart
without desktop variables or a usable tool PATH. Its complete report, transcripts,
and sources are explicitly pinned as `bundle-e8bedf549310ff13da710678546d0a9f`.
That historical 0.2.0 profile explicitly rejected the retained 2026.2 reference.
This establishes the implemented inventory queries only; exact test
versions and broader release limits are in [BUILD_VALIDATION.md](BUILD_VALIDATION.md)
and [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).

At **0.2.1**, a separate C++ MCP probe first passes on both basic correct/faulty
pairs; its report and transcripts are pinned as
`bundle-0a4c5bc3e9902c1e0526f5d2d1ff1843`. After adding the observed optional
indirect field, the focused parser, inspection, and MCP checks pass again.
The final real query matrix passes on all **54 captures** above: all three tools,
every event/object field and ordering checked against raw exports, five-record
pages, bounded protocol output, no process environment/command-line forwarding,
and identical metadata after restart. The server has `PATH=/nonexistent` and no
desktop variables. Exact report, sources, input expectations, and transcripts
are pinned as `bundle-a1af3b8bb46feebabcc66a3bd821ab80`. These are C++ MCP client
runs, separate from the historical Codex capability query.

## Further documented inspection paths

The [Generate C++ Capture
activity](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
is a documented route for generated API source and resource data. The separate
0.2.3 `capture_cpp` path retains this project for artifact access; the inventory
parsers above do not interpret it. See [CPP_CAPTURE.md](CPP_CAPTURE.md) for its
contract and qualification. NVIDIA's
[2026.3 release notes](https://docs.nvidia.com/nsight-graphics/ReleaseNotes/index.html)
deprecate Vulkan C++ Capture support and say it will be removed in a future
release. Probe outcomes and retained evidence belong in
[INVESTIGATIONS.md](INVESTIGATIONS.md); a failure on this configuration must not
be generalized to all documented interfaces.

As of the official-source review on 2026-09-18, NVIDIA's [SIGGRAPH
description](https://www.nvidia.com/en-us/events/siggraph/) describes an
in-development Nsight Graphics MCP preview. No installable server or public API
was found in the reviewed official documentation/download sources. This is a
bounded discovery observation, not a claim that the preview is universally
unavailable. Recheck availability before choosing a future integration path.

The first matching 2026.2 attempt fails during connection; I-013 retains its
frozen input, logs, and pinned failed bundle. Later diagnostic and ordinary
repeats succeed with the same executable and shader inputs. The original failure
cause remains unresolved. I-006 retains the earlier 2026.3 failure, and
I-007/I-009/I-010/I-011 retain the separate `ngfx-replay` GPU replay failures.

### Generated C++ reference evidence on 2026.2

The successful diagnostic capture is pinned as
`bundle-cd9faf11bccbc42e3da46f1275b39073`; a subsequent run without diagnostic
flags is pinned as `bundle-31416cc9a35fef45aa1ef5a828473393`. Both use product
0.2.2's frozen fixture, no SDK calls, seed 42, 192×128, `reference`, and
`--wait-frames=2`. Both exit zero with cleanup confirmed. Their generated
`CommandList00.cpp` labels the captured workload `frame.2` / `scene.raster`.
Independent decoded RGB comparison of each generated `screenshot.bmp` with
the pinned standalone frame-2 baseline `bundle-06953401867d9a2a5f5b38cb73d2aad3`
is exact, without resizing or color transforms. This is correspondence for these
inputs, not a general frame-numbering rule or proof of standalone replay.

In the diagnostic capture's generated project:

| Evidence | Observed source relationship | Remaining limit |
| --- | --- | --- |
| Draw and pipeline | `CommandList00.cpp` event 18 draws three vertices after binding pipeline 38. `Resources00.cpp` records its topology, viewport/scissor, rasterization, blend, and stage settings. | These IDs belong only to this C++ capture; they are not joined to another capture's inventories. |
| Shader association and bytes | Pipeline 38 uses vertex module 35 and fragment module 36. Their creation references resource handles 13/14, with 2196/1300 bytes. | Extraction is qualified for this generated project and helper, not an arbitrary producer or malformed capture. |
| Descriptor selection | Event 17 binds set 32 at set index 0. Its layout declares a fragment uniform buffer at binding 0; setup annotates that binding with buffer 26. | The packed descriptor-write blob has not been decoded through its generated `StructHydrator`; exact descriptor offset/range are not claimed. |
| Buffer contents | Setup references initial 16-byte values; frame code references subsequent 64-byte allocation updates before submission. | Each Vulkan buffer is only 16 bytes. Allocation padding is not shader-visible buffer content; initial and later values have different temporal meaning. |
| Pass and output | Render pass 19 uses framebuffer 21, view 13, and swapchain image 8, with clear/store and presentation transitions. | One raster pass only. Stored setup images represent beginning-of-frame state, not arbitrary after-draw snapshots. |

NVIDIA's generated `ReadOnlyDatabase`/`DataScope` helper was compiled unchanged
with a small native reader; no independent binary-format decoder was introduced.
The corrected reader and seven selected resource blobs are pinned as
`bundle-44c491ce96fef4b2b53817c5ee677c93`. Its inputs are restricted to the exact
capture above and source-referenced handles 0, 1, 5, 6, 11, 13, and 14. Extracted
handles 13/14 match the application's retained vertex/fragment SPIR-V byte for
byte. Their SHA-256 values are respectively
`3598c77b63b8ab122854ac805b2d75eadeb7d595b7c6c5b49a0537847d45add2` and
`425fc681c678f5965bad5dc1cbd42a48958895f5eb5d3c65e0d5c151be881d36`.
The earlier reader result remains pinned as
`bundle-08735abe7d9ca17af019b7f1bac4eda8`; fresh-context review prompted the
explicit capture/handle restrictions and independently verified the byte matches.

R-006 remains in progress. Advanced workloads and deeper resource extraction
require separate qualification before dependent diagnostic features are completed. R-007 still requires an actual source edit, rebuild, recapture, and fix
verification. Preserve generated-source evidence, application evidence, and
remaining gaps separately.

### Experimental correct/faulty C++ evidence on both releases

The 0.2.2 native experiments now qualify the basic `reference` / `shader-error`
pair on matching 2026.2.0.0/build 37991608 and 2026.3.1.0/build 38722833 tools.
All captures, baselines, resource-reader outputs, and the comparison snapshot
below are complete and explicitly pinned in `artifacts/nsight-evidence`.
These are experimental runs, separate from product 0.2.3 MCP acceptance.

| Release | Reference / faulty C++ capture | Reference / faulty resource extraction |
| --- | --- | --- |
| 2026.2 | `bundle-cd9faf11bccbc42e3da46f1275b39073` / `bundle-4625a12836a154589f2f03849a5fa361` | `bundle-44c491ce96fef4b2b53817c5ee677c93` / `bundle-feca43a3a7422eddb52099ec49ff9c0b` |
| 2026.3 | `bundle-38193a3c08f330f06fc7b026d580e633` / `bundle-3bbbc048cc769e4e4d2a46d84ac1fa1f` | `bundle-d1ad05c9083c95efa90fe46cc6227d88` / `bundle-8eb503c687623794de4198f365ad6552` |

Comparison snapshot **`bundle-4d0ca74d88003db698ae2dca0b952e11`** retains
`raw/imported/report.json`, the comparison script, reader binary identities, and
recorded compiler argv. Standalone baselines are
`bundle-06953401867d9a2a5f5b38cb73d2aad3` (2026.2) and
`bundle-d091dc14197d554aa1d943b963cd2f6b` (2026.3).
On each release, independently decoded 192×128 reference RGB exactly matches its
application baseline. Faulty output differs at **7,337 pixels / 14,641 channels**,
maximum channel error 140, mean absolute channel error 8.852064344618055.
The changed bounding box is `[20,16,172,111)`. No resize or color conversion is
used beyond decoding BMP/PPM channel layout.

All **eight shader comparisons** (two modules in four captures) match the frozen
application SPIR-V byte for byte. The faulty fragment module is 1,344 bytes,
SHA-256 `d8f019e218a9fe10bdbd69f870ca950e95df96847f487a13ae473002c95fb7e6`;
reference fragment and common vertex hashes are recorded above. Each native
reader uses the generated unchanged `ReadOnlyDatabase`/`DataScope` helper and
restricts its input to one exact capture/producer and seven selected handles.
The four readers are trusted fixed-capture experiments, not a generic malformed
binary parser. Helper hashes are recorded, not an embedded validation policy.
Fresh-context review independently checked the eight shader matches, 28
helper/database hashes, 28 resource hashes, four reader identities, and eleven
bundle inventories.

The 2026.3 generated sources expose the same basic draw/pipeline/shader,
descriptor, and pass relationships listed above, with IDs interpreted only inside
each capture. Packed descriptor writes and after-draw resource contents remain
unqualified. These image and byte comparisons establish useful real evidence;
they do not establish generated-project compilation, standalone GPU replay, an
advanced workload, or an actual source repair.
