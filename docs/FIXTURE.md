# Deterministic Vulkan experiments

The graphics fixture presents a real window using Vulkan 1.3 and `VK_KHR_xcb_surface`.
On the development machine this is Xwayland within the existing KDE Wayland
session. Its normal mode makes no NGFX SDK calls and produces **application
readback**, distinct from Nsight capture evidence. R-002's optional SDK build and
per-launch boundary controls pass the basic two-release matrix at 0.2.2; see
[SDK_CONTROL.md](SDK_CONTROL.md) for the exact qualified workload and evidence.
Source inspection remains separate work.

R-008 adds three no-presentation compute scenarios and numerical readback; see
[COMPUTE.md](COMPUTE.md). The shared shader bundle now includes seven graphics
and three compute shaders. Rebuild earlier seven-shader override bundles before
using them with the updated fixture. The graphics behavior below is unchanged.

Build the normal GCC Debug preset, then launch an isolated experiment:

```sh
cmake --build --preset linux-gcc-debug
build/linux-gcc-debug/ngm-experiment \
  --fixture build/linux-gcc-debug/ngm-vulkan-fixture \
  --output-root artifacts/fixture-validation \
  --scenario reference --seed 42 --width 192 --height 128 --frame 2
```

All workload inputs are explicit. Frame selection is zero-based and the fixture
presents frames zero through the selected frame. Dimensions are 32..4096; frames
are 0..600; the seed is an unsigned 32-bit integer. Basic scenarios are `reference`,
`shader-error`, `binding-error`, and `pipeline-error`. Advanced selections are listed
below. The runner defaults to a 30-second process deadline; `--timeout-ms` accepts 1..600000. An optional
`--shader-dir` selects a complete, hash-verified diagnostic shader bundle.

The standalone fixture accepts the same workload options, with `--output` naming
a new directory instead of the runner's `--output-root`. It can run independently
of both the MCP server and experiment runner. Missing display/device prerequisites
fail explicitly. Requested API features and readback-capable UNORM swapchain
formats are checked; the fixture does not silently render a different workload.

## Advanced workloads

| Workload | Correct selection | Controlled variant selections |
| --- | --- | --- |
| Two passes, offscreen color attachment, and post-processing | `multipass-reference` | `pass-output-error` |
| Bindless storage-buffer resources | `bindless-reference` | `resource-selection-error` |
| Indirect instanced draws | `indirect-reference` | `indirect-parameter-error` |
| All three paths together | `combined-reference` | `combined-pass-error`, `combined-resource-error`, `combined-indirect-error` |

The multipass path renders the scene into an `R8G8B8A8_UNORM` offscreen image,
transitions it from color attachment to sampled-image access, and uses a second
fullscreen pass to fetch each texel and apply a color transform. The image is
transitioned back before the next frame. The combined path draws the bindless
scene indirectly into that same offscreen pass before post-processing.

The bindless shader declares an unsized runtime array of storage-buffer
descriptors. Two descriptors are fully populated before submission; the fragment
shader chooses between them using a nonuniform, bounded index. Device selection
queries `VkPhysicalDeviceVulkan12Features`, requires and explicitly enables
`runtimeDescriptorArray` and `shaderStorageBufferArrayNonUniformIndexing`, and
checks the descriptor/push-constant limits. This path does not require partially
bound, update-after-bind, or variable-descriptor-count bindings.

The indirect path reads one `VkDrawIndirectCommand` from a buffer with
`VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`. It keeps `drawCount = 1` and
`firstInstance = 0`, so `multiDrawIndirect` and `drawIndirectFirstInstance` are
unnecessary. The vertex shader places each instance separately. Valid instance
counts select the controlled variation; there is no out-of-bounds vertex or
resource indexing. Offscreen image format features, extent, usage combination,
and sample count are queried before creating the image. No advanced request
falls back to the basic workload when prerequisites are unavailable.

These choices follow the Khronos references for
[descriptor-indexing features](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceVulkan12Features.html),
[descriptor-array layout counts](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorSetLayoutBinding.html),
and [indirect draw requirements](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDrawIndirect.html).

Each result records `workload` with the selected paths and required API features.
`device_support` retains queried features, relevant limits, and explicit reasons
for rejected candidate devices; successful runs also record the features enabled
at device creation. A missing display is rejected before any device claims are
made. Basic scenarios require no optional descriptor-indexing features.

R-010 is complete at **0.2.1** after local compilation, fresh-context review,
application GPU validation, and basic/advanced Nsight capture compatibility with
recorded inspection gaps. The validation records below distinguish the initial
basic matrices from the later complete fixture and capture matrices.

## What a run retains

Each runner invocation creates a unique `run-*` directory containing:

- `config/run.json`, isolated HOME/XDG/temp directories, and a private executable
  snapshot that is both hashed and executed. A concurrent rebuild of the original
  executable cannot change the binary selected by an allocated run.
- `logs/stdout.log` and `logs/stderr.log`, separated from the runner's output.
- `output/image.ppm`, the selected pre-presentation RGB8 readback, and `result.json`.
- `output/shaders/`, with retained GLSL source, SPIR-V, and shader-bundle provenance.
  The complete inventory is `scene.vert`, `scene.frag`, `shader-error.frag`,
  `indirect.vert`, `bindless.frag`, `post.vert`, and `post.frag`, each with its SPIR-V.
  Overrides must include and hash all seven sources/binaries, even for a basic run;
  an older three-shader bundle is explicitly rejected.
- `report.json`, with explicit outcome, inputs, environment, executable/shader
  identities, GPU/driver/UUID, desktop backend, timing, and owned-process cleanup.

Outcomes distinguish `pass`, `fail`, `unsupported`, `timeout`, and `cancelled`.
Here `pass` means the requested launch completed and its output contract validated;
correct image content is established separately by the integration harness.
Malformed/missing outputs remain failed runs with available logs. JSON and PPM
inputs are bounded; image files must be regular files and cannot block on FIFOs.

The runner forwards only the selected desktop session/authentication locations,
sets fresh HOME/XDG directories, and disables implicit Vulkan layers for its
fixture-only launch. It does not copy authentication contents into reports.
The standalone runner records Nsight and SDK states as `not_used`; its normal
fixture invocation makes no SDK calls. The capture adapter configures its
documented injection environment separately, and optional SDK observations are
retained in `sdk-control.json` as described in [SDK_CONTROL.md](SDK_CONTROL.md).

These directories are local experiment records outside the managed artifact
store. They are not automatically pruned or pinned. Import and explicitly pin
important runs through the implemented artifact store; the capture validation
harness does this for its standalone baselines.

## Explicit hardware checks

```sh
cmake --build --preset linux-gcc-debug --target ngm_fixture_integration_run
```

This existing target keeps its basic-suite behavior: four scenarios twice at
192x128/seed 42/frame 2 and 257x193/seed 2271560481/frame 5, plus the shader-bundle
override check. The same hardware harness accepts `--suite basic|advanced|all`;
`basic` is the default. `advanced` selects ten advanced scenarios at the same two
input tuples with two fresh launches each (40 launches), and `all` covers all
fourteen plus the override (57 launches). `--validation` remains independent of
the suite selection. The full `--validation --suite all` invocation is now
validated separately from the historical basic matrix below.

Repeats must be byte-identical across the complete image. A separate CPU
mathematical oracle compares triangle interpolation, resource selection,
instance placement, write-mask behavior, intermediate UNORM quantization, and the
post-process transform. It allows one RGB8 channel step for a single pass and two
steps for multipass output, accounting for both UNORM conversions. Pixels within
a barycentric distance of `2 / min(width, height)` of a rendered triangle edge are
excluded from the analytic check; all pixels still participate in exact repeat
comparisons. Each faulty scenario must differ from its own equivalent correct
reference on more than one tenth of all pixels using a one-step difference
threshold. Unsupported advanced requirements are reported as `unsupported` and
cannot make the matrix pass.

The private expectations are documented in [tests/SCENARIOS.md](../tests/SCENARIOS.md)
and implemented only in `tests/FixtureIntegration.cpp`. They are never returned
as MCP evidence. Source repair verification through Nsight is R-007; reproducing
these known variants alone does not complete that workflow.

The optional `--validation true` runner mode selects the separately installed
Khronos validation layer and writes per-run synchronization-validation settings.
It changes no machine configuration. The `ngm_fixture_validation_run` hardware
target additionally requires logs demonstrating active synchronization validation
and no validation errors. Missing validation prerequisites fail that check.

Ordinary CTest and `ngm_check` do not build or execute the hardware harness.
CPU executable stand-ins test runner failure handling without implying GPU or
Nsight compatibility.

## Validation record

Initial 2026-09-18 matrix: all eight scenario/configuration pairs passed, across
16 fresh application launches, on NVIDIA GeForce RTX 3080 Ti, driver 615.71.09,
Vulkan device API 1.4.351, KDE Wayland/Xwayland, XCB 1.17.0, GCC 16.2.1 Debug,
and source-built glslang 16.4.0 targeting Vulkan 1.3/SPIR-V 1.6 with `-g -Od`.
Summary: `artifacts/fixture-validation/summaries/matrix-1789680447222860605.json`.
This initial result predates review corrections.

Correction validation at product version **0.1.0**, on the same recorded GPU,
driver, desktop, and compiler configuration, passed all eight pairs again and
one shader-bundle override case: **17 fresh launches**, with synchronization
validation active and no validation errors. Summary:
`artifacts/fixture-validation/summaries/matrix-1789682490851820326.json`.
Each pair was byte-identical across repeats and matched the independent analytic
oracle within one RGB8 channel step outside the documented edge band. The override
kept executable identity unchanged while retaining distinct shader-bundle metadata.

The local driver exposes `VK_KHR_swapchain_maintenance1`; the fixture uses its
presentation fences for semaphore reuse and teardown. When neither the KHR nor
EXT maintenance1 path is supported, the fixture reports the conventional
`vkDeviceWaitIdle` presentation-teardown limitation in stderr and its rendering
metadata. That fallback is not qualified by this local matrix.

Fresh-context review and correction covered executable snapshot/hash races,
independent shader versus executable build identity, bounded regular-file reads,
strict standalone provenance types, and complete runner evidence. The runner now
requires the retained shader manifest to agree with inline metadata after the
`kind` annotation, and rejects missing/malformed/contradictory manifests or FIFOs.
The final `ngm_check` run passes all 13 CPU checks, including the additional
integer-schema regression added after this GPU matrix. Those checks validate
failure handling without claiming Nsight compatibility.

Advanced validation at product **0.1.1**, on the same recorded configuration,
passes all 28 scenario/input pairs twice plus the shader override: **57 fresh
launches**, with synchronization validation active and clean. Repeated images
are identical; the independent oracle passes within one RGB8 step for a single
pass and two for multipass; controlled faults exceed the declared 10% threshold.
The exact invocation was:

```sh
build/linux-gcc-debug/tests/ngm_fixture_integration \
  "$PWD/build/linux-gcc-debug/ngm-vulkan-fixture" \
  "$PWD/artifacts/fixture-validation" --validation --suite all
```

Summary: `artifacts/fixture-validation/summaries/matrix-1789691021831101042.json`.
The complete matrix, all 57 run directories, and harness log are explicitly
pinned in managed bundle `bundle-a5e2bf5a70cfdff49bb8705afff9aba5`.
Its `raw/imported/relocation.json` maps original run paths to retained copies;
`raw/imported/matrix.json` preserves the original report. The snapshot is
**application-readback** evidence. It does not establish Nsight compatibility.
Fresh-context source review found no actionable defect in feature enablement,
resource/descriptor/indirect bounds, offscreen synchronization, or the analytic
oracles. The new fixture-contract and experiment checks and full 20-check CPU
aggregate pass.

At **0.2.1**, the capture harness passes all nine workload pairs on matching
Nsight 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 tools: **54 fresh
captures**, all five exports, repeated-reference PNG identity, variant difference,
cleanup, and persistent pins. Typed metadata/event/object queries also pass on
all 54 captures, including server restart. The capture batch and typed report
are respectively pinned as `bundle-a4bce77c1a8d615412702da25e33fa75` and
`bundle-a1af3b8bb46feebabcc66a3bd821ab80`. Exact inputs and version differences
are in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md); [INSPECTION.md](INSPECTION.md)
records the missing pipeline, binding, shader, resource, and pass state. This
completes R-010's renderer/capture-gap requirements. At 0.2.2, optional SDK control
also passes the separately recorded basic two-release matrix. GPU replay and
actual source repair remain unfinished.

The desktop dependencies are documented in [DEPENDENCIES.md](DEPENDENCIES.md).
The [Khronos XCB surface reference](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_xcb_surface.html)
defines the selected window-system interface. Per-run implicit-layer isolation
uses the [documented loader filters](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md).
