# Bounded descriptor worker proposal for R-024

Status: **approved explicitly by the owner on 2026-10-15**, including confined
execution and recorded bounds instrumentation. This proposal does not
qualify descriptor contents or execution-time values. R-025's qualified literal
occurrence paths are available; the descriptor/value evidence chain still needs
implementation and independent acceptance.

## Exact extension

Extend the optional 2026.3 helper closure from the existing five files to seven,
adding only these generated sources from matching **2026.3.1.0/build 38722833**:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `VulkanStructHydrator.cpp` | 5,696 | `1e68e3163910a896250b1290c9aaf7e6cd27079bb0a649724de55e667edf534a` |
| `VulkanStructHydrator.h` | 3,496 | `34a9c79394a6dae41e785202aeaceb927e614966389002648a70b7e62d064c3d` |

The inspected copy is in ordinary C++ capture
`bundle-99c85e0891c5229f12fa821cfe2c4668`, retained completely in pinned
`bundle-168c040e6fa84e7efd645b16a5413735`. Only the existing database/scope
helpers, these two files, standard-library headers, pinned Vulkan headers and
first-party wrapper/confinement code are needed. The preprocessor-disabled
platform includes in the database helper retain their existing qualification.
No generated replay/application code or build scripts would be compiled.
Generated proprietary sources remain outside version control.

The wrapper would use the call contract already recorded in
[DESCRIPTOR_HYDRATION.md](DESCRIPTOR_HYDRATION.md). That earlier four-input
experiment is useful evidence, but does not qualify a generic reader. The vendor
helper defines the serialized structure interpretation; no private format
decoder is proposed. Its output remains source-scoped descriptor writes rather
than live Vulkan handles.

## Execution and bounds

Use a separate optional worker with the existing fail-closed Landlock/seccomp,
empty environment, immutable leased inputs, closed inherited descriptors, output/
memory/CPU limits and supervised wall deadline. The installed five-file worker
stays usable independently. The parent checks producer/helper/worker/input hashes
and the entire bounded output before accepting any field.

The reviewed hydrator lacks general input bounds; its default assertion macro
does not enforce output bounds. The proposed experiment therefore includes
explicit assertion enforcement and narrowly recorded bounds instrumentation in
an isolated copy of these exact helper sources. Preserve the original hashes,
an auditable patch and derived hashes. Check input-relative pointer arithmetic,
copy multiplication, input/output extents, alignment, recursion/count limits and
every translated object type/ID. Malformed data must refuse or terminate only
the confined child. No fallback to unconfined helper execution is permitted.

Start with buffer descriptors and combined image samplers. Refuse unknown
extensions, unsupported descriptor types, copy/update lifetimes and unrecognized
source grammar until individually qualified. Source registrations supply symbol
identity; artificial worker tokens never reach Vulkan. A hydrated descriptor
alone does not establish resource contents or a shader's dynamic selection.

## Evidence required before product exposure

1. CPU stand-ins and adversarial payloads exercise confinement, malformed input,
   pointer/count/output limits, worker failures and parent-response validation.
2. New independent captures cover single/multiple descriptor writes, partial
   array updates, copies and invalidated lifetimes. Compare fields/ranges to a
   private fixture API oracle and preserve literal occurrence paths from R-025.
3. Follow recorded resource creation/binding and mapped/upload writes, refusing
   uncertain order, GPU mutation, aliasing and unknown ranges. Typed values need
   layout/type evidence from the actual captured SPIR-V and exact bytes in the
   proven range. Initial uploads alone cannot establish execution-time values.
4. A real source/shader edit, rebuild and fresh capture must diagnose and verify
   a descriptor/value defect. Bounded MCP pages, source/reader/tool identities,
   negative controls, pins and restart stability must pass before claiming R-024
   complete. New 2026.2 support would require separate matching qualification.

The approval authorizes this two-file extension and its bounded experiment/
implementation, including the recorded safety instrumentation above. A wider
helper closure, generated replay compilation, a system change or publication of
unverified execution-time values is outside that authorization.
