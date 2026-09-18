# Fixed-capture descriptor hydration

Product **0.2.7** adds an opt-in C++ qualification runner for four retained
combined reference/postpass-fault captures from matching Nsight
2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608. This is a prerequisite
experiment for R-007 resource access, not a new MCP tool. The product surface
remains 20 tools. Arbitrary descriptor/resource extraction remains unfinished.

NVIDIA documents the generated resource helper in
[Generate C++ Capture](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html).
This experiment compiles the retained, unchanged `ReadOnlyDatabase`, `DataScope`,
and `VulkanStructHydrator` helpers. It does not implement a private database or
packed-descriptor decoder. Generated replay code supplies the calling contract;
these observations qualify only the exact retained inputs and ABI below.

## Calling contract

The retained `VulkanReplay.cpp` implements
`VulkanHelper_UpdateDescriptorSetsFromDatabase` by reading the top-level
`VkWriteDescriptorSet` array from the resource database, allocating the
source-declared output byte count, and calling:

```cpp
hydrator.Rehydrate(nullptr, descriptorWriteCount, pDescriptorWritesIn);
```

The helper interprets nested offsets relative to the source array's base.
The generated output-allocation argument is not a general declaration of input
blob length. The runner records the independently observed database length and
output bytes consumed; they happen to agree for these fixed inputs.

The callback maps `(VkObjectType, serialized object ID)` through source
`VulkanHelper_RegisterObject` calls. The worker checks the ID and object type
against exact source registrations and returns distinct artificial tokens.
Those tokens never reach Vulkan. Returned descriptor fields are translated back
to source symbols, preventing an identity callback from being mistaken for actual
runtime handles. An explicit `DataScope` keeps resource data alive throughout
hydration and reporting.

## Scope and inputs

The checked-in [case inventory](../tests/fixtures/descriptor-hydration/cases.json)
contains four exact capture IDs, producer tuples, helper/database/source hashes,
file sizes, and registration/update callsites. All four captures were previously
pinned; the runner verifies that status and holds a usage lease while copying
and inspecting each capture. It writes isolated inputs, build/run arguments and
results, source snapshots, compiler version, linked core/header identities, and
reports under a unique run directory.

Only the fingerprinted helper closure is compiled, using direct compiler argument
arrays and the repository's pinned Vulkan/JSON headers. The worker also checks
its compiled-in case fingerprint and every input identity before database
initialization. An altered-database negative check must fail in that identity
phase. It does not feed malformed data to the generated reader. Worker and helper
translation units use AddressSanitizer and UndefinedBehaviorSanitizer; supporting
`ngm_core` code is from the ordinary Debug build. Compiler and worker processes
have deadlines and confirmed descendant cleanup.

The generated helpers are **not safe general parsers for untrusted captures**:
database initialization and pointer hydration lack necessary bounds, and helper
fingerprints alone do not qualify new payloads. The opt-in runner is a trusted
experiment, not a sandbox or a product extraction API. It does not compile the
full generated replay, launch a Vulkan application, or execute GPU work.

Build and run explicitly:

```sh
cmake --build --preset linux-gcc-debug --target ngm_descriptor_hydration_integration
build/linux-gcc-debug/tests/ngm_descriptor_hydration_integration \
  artifacts/nsight-evidence build/descriptor-hydration-validation
```

This requires the four named retained captures; a clean checkout does not contain
them. The target is excluded from default builds, CTest, and `ngm_check`. It uses
the configured compiler and requires its ASan/UBSan runtimes. No currently
installed Nsight or desktop session is needed. Reconfigure/rebuild the harness
when changing the fixed inventory. Changing it authorizes a new trusted experiment
and requires fresh review; it does not extend a supported product profile.

## Observed setup descriptor fields

Each capture independently resolves these symbols through its own registrations:

| Destination | Descriptor | Referenced resource | Offset/range or layout |
| --- | --- | --- | --- |
| Set39, binding0, array element0 | Storage buffer, count1 | Buffer32 (primary palette) | 0 / 16 bytes |
| Set39, binding0, array element1 | Storage buffer, count1 | Buffer34 (secondary palette) | 0 / 16 bytes |
| Set40, binding0, array element0 | Combined image sampler, count1 | View27 / sampler29 (scene output) | `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` (5) |

The fixture writes two adjacent palette descriptors in one API update; these
Nsight exports restore them as **two separate count-one writes**. The probe does
not establish the single-write/multiple-elements helper path. The observed
x86-64 Vulkan ABI sizes are 64 bytes for `VkWriteDescriptorSet` and 24 bytes for
either buffer/image descriptor info. The two database blocks and output buffers
are 176 and 88 bytes respectively.

Reference and postpass-fault captures have different serialized block hashes but
identical values for all these inspected descriptor fields, on both releases.
This resolves I-017's ambiguity about these setup bindings. It does not explain
why the opaque bytes differ, prove all other state equal, or establish which array
slot the shader actually accessed. Earlier push-constant/SPIR-V evidence remains
separate. Initial setup writes are not an arbitrary event-state or resource-history
query. Fresh bindless/binding repair captures still require their own qualification.

Final run and pinned evidence are recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).

The final four-case run passes all twelve setup-write checks and four altered-input
rejections. Complete qualification, including I-021's failed attempt, is explicitly
pinned as **`bundle-93e2c3dd09c57553642b1afa873e8566`** in
`artifacts/nsight-repair-evidence`. The original four capture bundles remain pinned
separately. The 0.2.7 CPU aggregate passes **22/22**; independent implementation
and evidence reviews find no blocker within this experiment's stated scope.
