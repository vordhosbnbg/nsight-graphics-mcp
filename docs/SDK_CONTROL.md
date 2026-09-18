# Optional application SDK control

At product **0.2.2**, optional SDK control is implemented in the fixture and the
shared service, native command, and MCP capture request. Real MCP qualification
passes **12 SDK-controlled captures and 6 captures without SDK calls** on the
two matching tool/SDK pairs below. This qualifies the selected basic windowed
workload; it does not add detailed state inspection, source repair, advanced SDK
workload qualification, or compute capture without presentation.

## Selected interface

The [documented SDK](https://docs.nvidia.com/nsight-graphics/UserGuide/sdk.html)
requires activity initialization before creating a Vulkan instance and supports
explicit Vulkan frame boundaries. The fixture uses only
`NGFX_GraphicsCapture_InitializeActivity_Vulkan` and `NGFX_FrameBoundary_Vulkan`,
with versioned parameter structures and checked return values. The existing
`ngfx-capture` process performs injection. The fixture neither self-injects nor
creates a reusable application-control session.

An explicit `--sdk-first-boundary-frame N` enables SDK control for that launch.
It initializes before `Renderer::initialize`, then emits boundaries on the
graphics queue before rendering each frame starting at zero-based application
frame N. It waits for the prior submit and, when a maintenance presentation fence
is available, the prior presentation fence before each call.
The selected final application frame must leave at least two frames after N.
Without this option, even an SDK-enabled build makes no SDK calls.

Capture requests accept `delimiter: "present"` (the default) or
`"graphics_capture_api"`; the native command uses the same names with
`--delimiter`. The backend verifies the chosen delimiter flag in the installed
CLI's help. `capture_frame` counts the selected Nsight delimiters and must be at
least 2. It is not an application frame index. Actual image/frame correspondence
must be measured before claiming which rendered frame a capture contains.

## Build boundary and provenance

The normal build needs no SDK. `NGM_NSIGHT_SDK_ROOT` explicitly selects the
optional headers from a separately installed Nsight toolchain. This is a
documented GPU-toolchain exception alongside the existing Nsight runtime
prerequisite; the six vendored dependencies still use exact-commit submodules.
SDK headers are compiled into the fixture, with no downloaded or prebuilt
project library substituted. The server itself does not depend on SDK headers.

The current optional build accepts two exact source bundles. A bundle digest is
SHA-256 over sorted relative filenames, each followed by `:`, its file SHA-256,
and a newline. The inputs are `LICENSE.txt` and all headers below `include/`:

| Observed SDK | Files | Source-bundle SHA-256 |
| --- | ---: | --- |
| 0.9.2, distributed with tested Nsight 2026.3.1.0 | 38 | `2acf4cc605a651ae7f0db91a5417b569aedf76763f9ebbac06c144ac455505b6` |
| 0.9.0, distributed with tested Nsight 2026.2.0.0 | 38 | `73ac6585967dc22cd66d6d0e8b4d8ed8a3791a7b399f7824d28566ad79e556c5` |

An unknown or changed bundle is rejected at configuration pending qualification.
The build records every input hash, the bundle digest, and its identified SDK
version in the executable's build identity and SDK report. Directory names do
not establish the version. Header edits trigger reconfiguration. The source
fingerprint establishes build input identity, not GPU/runtime compatibility.
Each hardware validation must use a fixture built with that release's matching
SDK and its matching capture/replay tools.

## Application evidence

`sdk-control.json` is atomically replaced in the fixture's new output directory
before and after each SDK operation. It records the requested application frame,
SDK build identity, initialization result, boundaries entered/completed, and
the most recent call/frame/result. Before initialization it also records the
executable/build identity, selected workload/inputs, shader-bundle provenance,
and desktop context, so capture termination cannot lose those identities.
An entered boundary clears the preceding call's result until this call returns.
The result is also included in a completed
application-readback report. Missing SDK compilation or injection fails before
Vulkan instance creation and preserves an explicit unavailable result.

Nsight may terminate the target inside a boundary call after saving a capture.
An entered call therefore remains distinct from a successfully returned call.
The SDK report is labelled `application_sdk_control`; it cannot prove capture
completion. The service's successful process cleanup, saved capture, and matching
metadata export remain separate requirements. Its generic capture report labels
the SDK mode `application_control_requested`, without claiming it observed SDK
initialization. The default present delimiter records
`application_control_not_requested`; it does not claim that an arbitrary target
never initializes the SDK independently. Application evidence remains distinct
from Nsight exports.

This slice does not provide pipeline state, descriptor/resource contents, shader
extraction, or source repair. Compute workloads without presentation remain R-008.

## Verified build and invocation

Configure with the exact matching installed SDK version directory, then build
the regular server and optional hardware harness. For the recorded 2026.3 setup:

```sh
cmake --preset linux-gcc-debug \
  -DNGM_NSIGHT_SDK_ROOT=/opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3/SDKs/NsightGraphicsSDK/0.9.2
cmake --build --preset linux-gcc-debug --target ngm_capture_integration ngm_fixture_contract_check_run
```

The corresponding tested 2026.2 SDK directory is
`build/nsight-2026.2/installation/SDKs/NsightGraphicsSDK/0.9.0` under the checkout;
pass its absolute path. Tool paths are explicit inputs, not runtime defaults.
The contract check needs no GPU or Nsight injection: it verifies the normal mode
and explicit SDK failure before Vulkan initialization when injection is absent.

The C++ hardware harness accepts its existing five positional arguments
`SERVER FIXTURE NSIGHT_ROOT ARTIFACT_ROOT OUTPUT_ROOT`, followed by
`--sdk-first-boundary-frame 6 --baseline-frame 7` or
`--sdk-first-boundary-frame 10 --baseline-frame 11`. The exact executed arrays
are retained in the pinned batch below. Omitting both flags tests the default
present delimiter and fixture mode without SDK calls. Each invocation runs a
standalone baseline and three fresh MCP captures, then checks pins after restart.
Reset `-DNGM_NSIGHT_SDK_ROOT=` when returning to the normal build; CMake caches
the explicit optional choice until changed.

## Real qualification on 2026-09-18

All runs use product **0.2.2**, GCC **16.2.1 Debug**, the basic
reference/reference/shader-error sequence, seed **42**, **192×128**, application
final frame **20**, and capture delimiter ordinal **2**. GPU/driver and desktop
are RTX 3080 Ti / **615.71.09**, KDE Wayland with Xwayland/XCB. The standalone
baseline and raw Nsight reports retain their exact environment and build
identities. Every SDK fixture was separately built against the matching headers.

| Nsight CLI version/build | SDK | First boundary / standalone application frame | Fresh captures | Result |
| --- | --- | --- | ---: | --- |
| 2026.3.1.0 / 38722833 | 0.9.2 | 6 / 7 | 3 | Pass |
| 2026.3.1.0 / 38722833 | 0.9.2 | 10 / 11 | 3 | Pass |
| 2026.3.1.0 / 38722833 | Compiled, no SDK calls | Present delimiter / 2 | 3 | Pass |
| 2026.2.0.0 / 37991608 | 0.9.0 | 6 / 7 | 3 | Pass |
| 2026.2.0.0 / 37991608 | 0.9.0 | 10 / 11 | 3 | Pass |
| 2026.2.0.0 / 37991608 | Compiled, no SDK calls | Present delimiter / 2 | 3 | Pass |

All 18 target PID observations differ. Every capture has successful matching
metadata/functions/objects/logs/screenshot exports, confirmed cleanup, released
GPU reservation, and a complete pinned bundle. References repeat exactly as PNG
files; the selected faulty scenario differs. Baselines, captures and reports
remain pinned after normal server EOF and restart. SDK reports show successful
initialization before Vulkan, three entered and two completed boundaries; the
last entered call remains pending with a null result when Nsight terminates the
target inside it. The application context retains executable, shader and input
identities before that termination.

An independent retained Pillow script decodes the screenshots and standalone
PPM readbacks as RGB8, without scaling or color transformation. All 12 reference
screenshots match their selected standalone frames exactly. Each faulty image
differs from the reference; this compares scenarios, not a source repair. Moving
the first SDK boundary from 6 to 10 changes 7,372 reference pixels on each
release, matching the independently selected application frames 7 and 11.
PNG alpha is uniformly 255 and is reported separately. These observations are
specific to the recorded workload, inputs and exact producers, not a general
formula for application/SDK/capture frame numbering.

The complete matrix, exact executed harness/server/fixture binaries and hashes,
harness source, six reports, comparison script/results, and verified fixture
source inputs are pinned as **`bundle-7b6c2aa712deb108b66af94d86b50e75`**
(119,017,546 bytes). Its `raw/imported/report.json` and per-SDK `batch.json`
files map all six baseline, eighteen capture and six report bundles. Every
referenced bundle has an explicit persistent pin. Local evidence is
`build/sdk-validation/sdk-matrix-evidence/`; the publication receipt is
`build/sdk-validation/sdk-matrix-evidence-receipt.json`.
The outer snapshot was imported by the retained **0.1.1** `EvidenceImport`
utility, so its manifest records that importer version. The payload's harness,
server, fixtures and actual capture bundles record **0.2.2** independently;
the snapshot's import version does not relabel their build identities.

The earlier native SDK 0.9.2 capture is separately pinned as
`bundle-e02e879da06c7a44c8bf099e3a950600`; its frozen inputs, standalone frame-7
baseline and exact RGB comparison are pinned as
`bundle-ee6f6727f11306dee5f0c1e39f1dbfb2`. A failed first MCP harness check,
its successful underlying capture, and the reviewed correction are retained in
[investigation I-012](INVESTIGATIONS.md#i-012--sdk-capture-harness-compares-different-shader-provenance-representations).
