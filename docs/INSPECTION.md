# Capture inspection evidence

This early R-006 slice parses basic metadata, event inventories, and object
inventories from actual Nsight Graphics exports and exposes bounded queries over
retained server capture bundles. The observations below are from
**2026.3.1.0, build 38722833**, on Linux with RTX 3080 Ti / driver 615.71.09 and
the repository's windowed basic Vulkan fixture. They do not establish support for
another Nsight release or the advanced R-010 scenarios.

**R-006 and R-007 remain incomplete.** Inventory parsing does not provide the
detailed state required to diagnose a defect, edit its source, rebuild, recapture,
and verify the fix. This document records the parser contract and observed
exports; [MCP.md](MCP.md) and [BUILD_VALIDATION.md](BUILD_VALIDATION.md) own the
integrated tool surface and executed validation results.

## Observed evidence and limits

The correct/faulty pair is retained in explicitly pinned bundles
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
| Function/event inventory | `--metadata-functions`, JSON array with 22 entries per capture | `event_index`, `function_name`, `thread_index`, and optional `sequence_id`. No API arguments, object references, or complete event state. |
| Object inventory and names | `--metadata-objects`, JSON array with 32 entries per capture | `uid`, `api`, `object_name`, `type_name`, and `access_flags`. Includes application labels for pipelines, shaders, and buffers; no object definitions or event associations. |
| Capture screenshot | `--metadata-screenshot`, 192×128 PNG in each pair bundle | A capture screenshot is available as `raw/exports/screenshot.png`. This does not expose an arbitrary selected resource or framebuffer at an event. |
| Embedded capture logs | `--metadata-logs`, empty file in each pair bundle | These exports contain no log entries. The separate metadata collection contains a version warning and must not be replaced by the empty log result. |
| Pipeline state and draw-to-pipeline association | Absent from the inspected metadata exports | A `vkCmdBindPipeline` name and a pipeline object label do not identify the pipeline bound at `vkCmdDraw`, nor its raster/depth/blend state. |
| Descriptor bindings and resource selection | Absent from the inspected metadata exports | `vkCmdBindDescriptorSets`, descriptor-set names, and opaque access values do not establish binding contents or selected buffer values. |
| Shader bytes, source, and draw-to-shader association | Absent from the inspected metadata exports | Shader-module names are visible. Retained GLSL/SPIR-V under `raw/application/shaders/` is application-provided evidence, not shader extraction by Nsight. |
| Buffer/texture contents at an event | Absent from the inspected metadata exports | An object inventory and final screenshot do not reveal arbitrary resource bytes, subresources, or event-specific contents. |
| Debug-label text and pass relationships | Debug-label function names occur, without their arguments | Begin/end function names do not provide label text, command-buffer association, or a verified pass hierarchy. |
| Bindless, indirect, and multipass evidence | Not established by this basic fixture pair | R-010 workloads and a sufficient documented extraction path remain required. |

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

The current schema profile is deliberately tied to the observed Vulkan producer:
capture and replay CLI version `2026.3.1.0`, build `38722833`; metadata version
integer `1`; exported `nsight_version` string `2026.3.1` and build string
`38722833`. All producer strings retain their exact spelling in query results.
Another release, build, API, missing producer field, or unsupported metadata
version fails explicitly until a sample and profile have been validated. The
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
rejection, and explicit read/result bounds. Its sole argument is the checked-in
fixture directory. [McpCheck.cpp](../tests/McpCheck.cpp) independently exercises
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
nonempty `function_name`; only `sequence_id` is optional. Every object requires
unsigned 64-bit `uid` and `access_flags`, nonempty `api` and `type_name`, and an
`object_name` string that may be empty. Numeric strings, fractions, negatives, and
overflow are rejected. Duplicate event indices or object UIDs within an export
are rejected. IDs are capture-scoped, never Vulkan handles or cross-run keys.
Repeated sequence IDs are allowed, as observed in the actual exports.

`sequence_id` and `access_flags` remain opaque observed integers. The parser does
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
correct/faulty pair, optional sequence IDs, preserved warnings and labels,
missing/wrong/range-invalid fields, duplicate IDs and escaped duplicate JSON keys,
unknown metadata versions, malformed UTF-8/JSON, string and metadata budgets,
record/value counts, and deeply nested ignored fields. Synthetic mutations are
kept in the check source rather than attributed to real exports. The check is
CPU-only and needs the checked-in fixture directory as its sole argument; it
does not require Nsight or a GPU. Executed build/test results belong in
[BUILD_VALIDATION.md](BUILD_VALIDATION.md).

The integrated 20-check CPU aggregate and fresh-context review pass. A real C++
MCP probe exercises the pinned correct/faulty pair described above, all three
typed tools, five-record pagination, and identical metadata after server restart
without desktop variables or a usable tool PATH. Its complete report, transcripts,
and sources are explicitly pinned as `bundle-e8bedf549310ff13da710678546d0a9f`.
The retained 2026.2 reference is explicitly rejected by the current producer
profile. This establishes the implemented inventory queries only; exact test
versions and broader release limits are in [BUILD_VALIDATION.md](BUILD_VALIDATION.md)
and [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).

## Further documented inspection paths

The [Generate C++ Capture
activity](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
is a documented route under investigation for deeper state, API arguments, shader
and resource evidence. It is not implemented by these parsers, and its existence
does not establish a working extraction path for this workload. NVIDIA's
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

Complete R-006 still requires evidence categories tied to reproducible documented
extraction or demonstrated gaps for representative basic and advanced captures,
and a selected next action for unresolved needs. R-007 still requires sufficient
evidence for the actual source-edit/rebuild/recapture/fix-verification workflow.
