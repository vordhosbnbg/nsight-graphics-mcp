# Compute correctness

R-008 extends the visual workflow with three deterministic no-presentation
scenarios: `compute-reference`, `compute-index-error`, and
`compute-arithmetic-error`. R-008 completes at **0.3.1** after independent acceptance and verified publication
of its pinned qualification snapshot.

## Workload and evidence

The fixture dispatches a one-dimensional uint32 affine transform over
`width * height` elements. Local size is 64; excess invocations return before
accessing either storage buffer. Dimensions retain the fixture's 32..4096
individual bounds, with a compute-specific maximum of 16,384 elements. Seed and
zero-based application frame determine the input sequence. The two faults are
in-bounds indexing and arithmetic errors. GLSL, debug SPIR-V, compiler settings,
source/binary hashes, and executable build identity are retained independently.

The application records its actual shader selection, dispatch dimensions,
push-constant count and two buffer bindings in `compute-setup.json`. It publishes
`compute-frame-N.json` after each GPU fence completes. Setup is explicitly
`application_observation`; input/output arrays are `application_readback`.
An execution `pass` does not assert numerical correctness. Expected outputs and
defect checks live only in the separate C++ validation harness.

`ngm-experiment` runs these scenarios through the same isolated configuration,
owned-process, timeout, executable-snapshot and provenance path as graphics.
It validates all intermediate and final readbacks, including exact frame/build/
shader identities, array sizes and uint32 values. Its report says correctness
`not_evaluated`; the numerical integration runner supplies the independent oracle.

## Capture boundary and compatibility

The MCP `capture` tool and `ngm-capture` accept `delimiter: vk_frame_boundary`.
The application must query and enable both `VK_EXT_frame_boundary` and its
feature, then emit boundaries. This does not request NGFX SDK initialization.
The fixture enables it with `--compute-boundary vk_frame_boundary`; standalone
runs default to `none`. Missing support is an explicit unsupported prerequisite.

One empty queue submission with `VK_FRAME_BOUNDARY_FRAME_END_BIT_EXT` follows
each published numerical readback. The preceding dispatch submission has no
boundary annotation. The extension permits boundary information on queue
submissions ([Vulkan reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkFrameBoundaryEXT.html)).
The earlier flags-zero annotation split Nsight's measured interval and is retained
as I-030. For this fixture's one-boundary-per-frame sequence, the qualified
capture ordinal 2 retains application frame 2, including its readback, before
target termination. This measured mapping is not a general join between Nsight
event IDs and application frame IDs.

| Interface/profile | Observation |
| --- | --- |
| Nsight 2026.3.1.0, build 38722833 | Corrected no-presentation graphics capture succeeds; metadata, functions, objects and logs export. |
| Nsight 2026.2.0.0, build 37991608 | CLI advertises the flag, but the injected application lacks the required extension/feature; capture fails with an explicit application unsupported result (I-031). |
| 2026.3 generated C++ capture activity | Private probe could not enable the boundary; no compute project is qualified (I-028). |
| Screenshot of the buffer-only workload | Export exits 1 without an image; recorded separately from successful capture/inventories (I-029). |

These observations use RTX 3080 Ti / driver 615.71.09 in the existing Linux
KDE Wayland/Xwayland session. The compute target creates no window, swapchain or
presentation operation; this does not qualify every display-free host setup.

The observed 2026.3 metadata leaves `primary_api` empty. Typed inventory queries
accept its exact version/build only when explicit Vulkan API/feature inventories,
supported-operation flag, retained service delimiter and invocation agree.
Missing, null or contradictory API evidence remains unsupported. The original
empty metadata field is returned unchanged.

Nsight inventories show a dispatch and named buffers, pipeline and shader module.
They do not expose compute shader bytes, descriptor state or post-dispatch buffer
contents. The fixture's separately labeled source/SPIR-V and readback are
available through normal artifact queries. This source-available diagnosis path
does not establish equivalent detailed inspection for uninstrumented applications.

## Verification

The explicitly invoked C++ numerical runner covers both a partial workgroup
(33x35) and full workgroups (64x32), seeds 42 and UINT32_MAX, multiple frames and
two fresh launches per scenario/input. The independent oracle uses wide integer
math with explicit modulo 2^32 reduction and zero tolerance. Initial qualification
passes 12 launches / 30 frame comparisons with synchronization validation active.

```sh
cmake --build --preset linux-gcc-debug --target ngm_compute_integration
build/linux-gcc-debug/tests/ngm_compute_integration \
  build/linux-gcc-debug/ngm-vulkan-fixture build/compute-validation
```

`ngm_compute_capture_integration` is a separate opt-in C++ MCP client. Supply the
server, fixture, Nsight installation, managed artifact store and report root:

```sh
cmake --build --preset linux-gcc-debug --target ngm_compute_capture_integration
build/linux-gcc-debug/tests/ngm_compute_capture_integration \
  build/linux-gcc-debug/nsight-graphics-mcp \
  build/linux-gcc-debug/ngm-vulkan-fixture \
  /absolute/nsight-installation /absolute/compute-artifacts build/compute-captures
```

Each invocation creates a fresh report directory and checks reference plus both
fault scenarios. It compares capture readbacks with separately validated launches,
queries dispatch/object inventories, retrieves and hashes source/SPIR-V, preserves
protocol logs and pins each capture. Reports/baselines need their own retained
snapshot; capture pinning alone does not protect those directories. The optional
final arguments `SHADER_DIRECTORY original|repaired` select an externally built
shader bundle and the private oracle mode. That mode alone does not prove edits
or rebuilds: preserve the actual source patch and compiler invocation separately.

The initial real MCP matrix passes all three scenarios on 2026.3: reference has
zero mismatches, both faults have 1,155 mismatches, and captured/standalone outputs
match exactly. Actual external edits correct each faulty shader expression, preserving the
scenario selections. Both are recompiled with the recorded source-built glslang,
then recaptured; all three repaired-bundle scenarios have zero mismatches and
exact captured/standalone equality. Original runs retain 0.3.0 development build
identities; repaired runs retain 0.3.1 identities. This does not relabel historical
runs as 0.3.1. Source patch, compiler arguments/hashes and SPIR-V embedded sources
are retained separately from the harness's numerical expectations.

The 0.3.1 GCC Debug aggregate passes all 25 CPU checks (248.16 seconds, no skips).
A fresh-context independent audit recalculates all 30 numerical frames and six
capture/readback results, verifies original/repaired shader bytes and actual
compiler records, and checks capture pins. The 2026.2 MCP integration returns an
explicit unsupported result with its failed capture pinned; it is not a passing
compute profile. The complete supporting snapshot is pinned as
`bundle-1ed0b5e17d49e24b6333d6ae4b10ffc1` in `artifacts/compute-evidence`.
It contains 1,128 files, including a manifest for all 1,127 payload hashes;
publication verification checks every hash and persistent pin state after restart.
It includes numerical baselines, executable snapshots, protocol logs, actual
repair/build records, private failed probes, source snapshot and independent audit.
Ten separately pinned native/MCP attempt bundles retain successful and failed
capture evidence. Snapshot documents preserve their pre-publication wording;
this document and the roadmap record final closure.

Final publication review, receipts and closure documents are independently pinned
in `bundle-b5b3b72601ce244fd492c5231c324d16` in the same store. The review confirms
all snapshot payloads and ten capture pins, with no outstanding acceptance conditions.
