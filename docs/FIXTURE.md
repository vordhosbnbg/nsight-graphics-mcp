# Deterministic Vulkan experiments

The fixture presents a real window using Vulkan 1.3 and `VK_KHR_xcb_surface`.
On the development machine this is Xwayland within the existing KDE Wayland
session. It uses no NGFX SDK and produces **application readback**, not Nsight
capture evidence. Nsight capture/source inspection remains separate work.

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
are 0..600; the seed is an unsigned 32-bit integer. Scenarios are `reference`,
`shader-error`, `binding-error`, and `pipeline-error`. The runner defaults to a
30-second process deadline; `--timeout-ms` accepts 1..600000. An optional
`--shader-dir` selects a complete, hash-verified diagnostic shader bundle.

The standalone fixture accepts the same workload options, with `--output` naming
a new directory instead of the runner's `--output-root`. It can run independently
of both the MCP server and experiment runner. Missing display/device prerequisites
fail explicitly. Requested API features and readback-capable UNORM swapchain
formats are checked; the fixture does not silently render a different workload.

## What a run retains

Each runner invocation creates a unique `run-*` directory containing:

- `config/run.json`, isolated HOME/XDG/temp directories, and a private executable
  snapshot that is both hashed and executed. A concurrent rebuild of the original
  executable cannot change the binary selected by an allocated run.
- `logs/stdout.log` and `logs/stderr.log`, separated from the runner's output.
- `output/image.ppm`, the selected pre-presentation RGB8 readback, and `result.json`.
- `output/shaders/`, with retained GLSL source, SPIR-V, and shader-bundle provenance.
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
The recorded Nsight and SDK states are `not_used`. A future capture adapter must
configure its documented injection environment separately.

These directories are local experiment records. They are outside the future
R-012 managed artifact store and are not automatically pruned or represented as
pinned capture bundles. Keep important local results until they can be imported
and explicitly pinned by that store.

## Explicit hardware checks

```sh
cmake --build --preset linux-gcc-debug --target ngm_fixture_integration_run
```

This target runs each scenario twice at 192x128/seed 42/frame 2 and
257x193/seed 2271560481/frame 5. Repeats must be byte-identical. A separate CPU
mathematical oracle compares triangle interpolation, palette selection, and
write-mask behavior with a tolerance of one RGB8 channel step. Pixels within
the declared narrow triangle-edge band are excluded from the analytic check;
the complete images still participate in exact repeat comparisons. Every faulty
scenario must visibly differ from its corresponding correct reference.

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

The desktop dependencies are documented in [DEPENDENCIES.md](DEPENDENCIES.md).
The [Khronos XCB surface reference](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_xcb_surface.html)
defines the selected window-system interface. Per-run implicit-layer isolation
uses the [documented loader filters](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md).
