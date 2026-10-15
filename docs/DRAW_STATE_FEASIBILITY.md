# Selected-state feasibility outcome

Date: **2026-10-15**. Product implementation: **0.4.7** (`abb6387`).
This report completes R-021's conditional assessment in
[CAPABILITY_FOLLOWUP_PLAN.md](CAPABILITY_FOLLOWUP_PLAN.md). It does not claim
complete effective GPU state. The implemented [recorded-state contract](DRAW_STATE.md)
retains `captured_gpu_execution_correspondence: false` and
`full_effective_state_coverage: false`.

## Evidence-supported outcome

| Requested information | Outcome and evidence |
| --- | --- |
| Identified draw occurrence | Capture-local recording/event/submission references, with reuse, lifecycle and refusal checks. This is the generated submission domain; graphics inventory IDs are a separate domain. |
| Pipeline/shader identities and raster state | Implemented at 0.4.5: four fresh 2026.3 captures with private API comparisons and eight exact shader reads. Pipeline libraries, specialization, unsupported extensions and ambiguous creation refuse coverage. |
| Push inputs | Implemented at 0.4.6: ordered stage/range writes, partial updates, compatible layouts and explicit opaque bytes. Nine recorded occurrences, independent API byte banks and restart/pin checks pass. Shader types and material meaning are not inferred. |
| Descriptor binding identities and relative dynamic offsets | Implemented at 0.4.7: literal definitions/allocations, prefix/push-range compatibility, disturbance and array-element mapping. Fifteen recorded occurrences pass independent API comparisons. This does not establish descriptor contents or effective buffer addresses. |
| Descriptor contents/write visibility | **Unavailable.** Retained 2026.2/2026.3 examples restore serialized descriptor structures through a hydration helper. The fixed four-input experiment does not qualify the new capture inputs or a generic decoder. |
| Execution-time uniform/material values | **Unavailable.** A proven descriptor resource/range, write lifetime and verified type/layout schema are missing. Initial or uploaded bytes alone do not prove post-GPU values or shader array selection. |
| General captured GPU execution correspondence | **Unavailable.** The reviewed documented exports do not establish a join between a generated submission occurrence and an observed selected GPU event. Fixture API witnesses remain a separate, private validation origin. |
| New 2026.2 state capability | **Unqualified and explicitly refused.** Historical two-release source/resource capabilities retain their own scope. No new two-release state claim is made. |

The implementation qualification snapshots remain pinned:
`bundle-556efc8c7f65c8ebdb96e360197fc015` (0.4.5),
`bundle-754e06a3afa984d58e2d073bac3b590f` (0.4.6), and
`bundle-b59b7343be911eec0e49b85ffe5767ef` (0.4.7).
[BUILD_VALIDATION.md](BUILD_VALIDATION.md) records the native source/read pins,
versions, full CPU checks, schema checks and exact hardware evidence.

## Remaining-field assessment

The read-only audit verifies **56** source/helper/database identities across the
four pinned [fixed hydration cases](DESCRIPTOR_HYDRATION.md), from matching
2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608. The retained experiment's
final report remains complete/pinned as
`bundle-93e2c3dd09c57553642b1afa873e8566`, product 0.2.7. It is not rerun or extended.

The two fresh 0.4.7 descriptor captures each contain four
`VulkanHelper_UpdateDescriptorSetsFromDatabase` calls, each declaring four writes
and a **352-byte output allocation**. That allocation is not a general input blob
length. Neither setup contains literal `VkWriteDescriptorSet` aggregates. The
captured frame also has two `VulkanHelper_UpdateMappedMemory2` calls before
submission. These observed inputs do not establish a generic descriptor decoder,
arbitrary resource history or a material type schema. A private partial-array
update witness does not fill those gaps in product evidence.

The approved production resource worker still compiles only the five-file
ReadOnlyDatabase/DataScope closure. The fixed experiment additionally used
`VulkanStructHydrator`, whose pointer hydration lacks general payload bounds.
The plan explicitly requires a separate owner decision before extending that
closure. No generated replay, additional helper or new opaque-structure decoder
was compiled or executed during this assessment.

Installed matching `ngfx-replay --help` advertises metadata, function, object,
log and embedded-screenshot exports. The reviewed
[CLI documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
does not establish a selected-event state/value export. NVIDIA's
[C++ capture documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
describes recorded source and ReadOnlyDatabase resource access. The reviewed
[MCP preview description](https://www.nvidia.com/en-us/events/siggraph/) and scoped
official searches establish no additional callable documented state backend.
This is a bounded interface review, not proof that no future interface can exist.

The assessment is pinned as **bundle-7269c28d30326b8f1fa3ef64ebd4ed36** in the
central managed store. Its 13 files retain installed version/help output, the
source/hash/callsite audit, the historical hydration report, review URLs and a
read-only audit verifier. The importer reports 0.4.1; the payload explicitly
identifies the assessed 0.4.7 implementation and independent source pins.
No new capture, replay, profiling, hydration experiment or machine-setting change
is performed. Documentation/diff review and the retained-input audit pass;
no new product build or CPU suite is required for this documentation-only change.

## Conditions for revisiting

- A documented machine-readable selected-event interface with actual schemas,
  producer identity and an unambiguous captured execution association.
- An explicit owner decision for a separate bounded hydration experiment,
  followed by confinement, ABI/helper fingerprints, new-input bounds and
  independent descriptor write/copy tests. The old fixed inputs do not authorize
  or qualify a generic worker.
- Proven resource/range/write lifetime and a verified type/layout schema before
  advertising execution-time uniform or material values. GPU-written values and
  bindless access require their own evidence.

The accepted conditional outcome is therefore implemented recorded state plus
retained, explicit unavailability for the remaining fields. R-021 is Done on
that basis; full effective-state coverage remains unavailable. R-022 owns the
separate timing/counter assessment.
