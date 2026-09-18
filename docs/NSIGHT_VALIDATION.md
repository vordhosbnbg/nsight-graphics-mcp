# Local Nsight release qualification

Date: **2026-09-18**. R-015 remains **In Progress**. Basic uninstrumented
capture/export passes on two releases; this is not qualification of the complete
visual-debugging release. The SDK, advanced capture matrix, typed inspection on
both producers, and source-edit/rebuild/recapture workflow remain separate checks.

## Tools and common environment

| Component | First release | Second release |
| --- | --- | --- |
| Capture/replay CLI version | 2026.3.1.0 | 2026.2.0.0 |
| Tool build | 38722833 | 37991608 |
| Metadata version spelling | 2026.3.1 | 2026.2.0 |
| SDK present | 0.9.2, not used | 0.9.0, not used |
| Explicit installation | `/opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3` | Repository-local `build/nsight-2026.2/installation` |

Both matrices use product **0.1.1**, GCC **16.2.1 Debug**, Arch Linux x86-64,
NVIDIA GeForce **RTX 3080 Ti**, driver **615.71.09**, Vulkan device API
**1.4.351**, and the existing KDE Wayland desktop through **Xwayland/XCB 1.17.0**.
The source-built glslang **16.4.0** compiles GLSL for Vulkan 1.3 / SPIR-V 1.6
with `-g -Od`. The basic fixture executable SHA-256 for these runs is
`8750c8744acd4defc665a4f7bf6af874f751b946af015699a6bd762234adc1d0`.
The reports and standalone baselines retain exact executable, shader, host,
desktop, and command identities. Later advanced-fixture builds are different
inputs and do not inherit these results.

Use each release's matching capture and replay tools. No cross-version replay
compatibility is claimed. Installation paths are explicit run inputs, not
runtime constants in the server.

## Basic capture/export matrix

The C++ harness and its limits are described in
[CAPTURE_VALIDATION.md](CAPTURE_VALIDATION.md). Each release ran a standalone
application-readback baseline and three fresh MCP capture jobs: reference,
reference repeat, and shader-error, with seed 42, 192x128, and capture frame 2.

| Check | 2026.3.1.0 | 2026.2.0.0 |
| --- | --- | --- |
| Three uninstrumented captures and matching metadata reads | Pass | Pass |
| Metadata, function inventory, object inventory, logs, screenshot exports | Pass | Pass |
| Distinct observed target PIDs and confirmed process cleanup | Pass | Pass |
| Equal reference PNG files and different faulty PNG file | Pass | Pass |
| MCP raw evidence retrieval and pins after restart | Pass | Pass |
| Normal MCP EOF shutdown | Pass | Pass |
| Typed metadata/event/object queries on retained basic pair | Pass; separate pinned query probe | Unsupported by current producer profile |
| GPU replay execution, tested separately | Fail: initialization timeout, I-007/I-009/I-010 | Fail: initialization timeout, I-011 |
| Decoded cross-origin pixel comparison in this harness | Skipped | Skipped |
| Optional SDK control, advanced captures, source repair | Skipped | Skipped |

Empty logs exports are valid when the retained file exists and its inventory
matches. A saved screenshot is captured evidence; exporting it does not execute
a GPU replay loop. Reference/fault differences demonstrate scenario behavior,
not source repair. The separate frame-correspondence probe in I-005 applies only
to its recorded 2026.3 workload.

All evidence below is explicitly pinned under `artifacts/nsight-evidence`.

| Evidence | 2026.3.1.0 bundle | 2026.2.0.0 bundle |
| --- | --- | --- |
| Report and MCP transcript | `bundle-9e029958d57c655a53e191cef3bb580c` | `bundle-2af18aba623ad2e75b3834ba0980b2a8` |
| Standalone baseline | `bundle-1a4101435cb327e89285cf6d1cce3fe4` | `bundle-a250e0cfe700672bb718df85fcf11674` |
| Reference capture | `bundle-428f16403c26367e2c5bc18c4b3340ba` | `bundle-9c1f4095228eaf248119d63c9604ee86` |
| Reference repeat | `bundle-ca8b82da72fa166df06d09b051ff264e` | `bundle-83dc3b972ffe48f17a45903c6dc3eca2` |
| Shader-error capture | `bundle-b2dfd8f3916afcfec7b5c6ffdca5f6de` | `bundle-49d90463ab1842ecb147e4c511fd4a91` |

Local reports are `artifacts/capture-validation/capture-validation-ftIB4T/report/report.json`
and `artifacts/capture-validation/capture-validation-9WYYGE/report/report.json`.
Each report identifies its immutable imported snapshot and publication receipt.

The second run invoked the existing harness directly with positional inputs:

```sh
build/linux-gcc-debug/tests/ngm_capture_integration \
  "$PWD/build/linux-gcc-debug/nsight-graphics-mcp" \
  "$PWD/build/linux-gcc-debug/ngm-vulkan-fixture" \
  "$PWD/build/nsight-2026.2/installation" \
  "$PWD/artifacts/nsight-evidence" \
  "$PWD/artifacts/capture-validation"
```

## Second-release acquisition

The official [NVIDIA download catalog](https://developer.nvidia.com/tools-downloads)
lists the Linux package
[`NVIDIA_Nsight_Graphics_2026.2.0.26134-linux_x64.deb`](https://developer.nvidia.com/downloads/assets/tools/secure/nsight-graphics/2026_2_0/linux_x64/NVIDIA_Nsight_Graphics_2026.2.0.26134-linux_x64.deb).
The downloaded archive contains **465,479,016 bytes**; its locally measured
SHA-256 is
`1ca531f34f04fd66d0f6de031ef78e1fc90974bd9893a50908c2a2d96b24ee0c`.
This records the received file, not verification against a publisher signature.
Only the vendor payload was extracted into the ignored repository build tree;
Debian maintainer scripts were not run, and no installed driver, desktop, or
system profiling settings were changed. The available SDK was observed but not
loaded by these tests.

The archived [2026.2 release notes](https://archive.docs.nvidia.com/nsight-graphics/2026.2/ReleaseNotes/index.html)
specify Linux driver 580.126.18 or newer. The tested driver exceeds that version
minimum; this alone establishes no additional workload compatibility. The local
acquisition receipt is `build/nsight-2026.2/acquisition.json`; a small immutable
copy is explicitly pinned as `bundle-0effb14aea52838c6dbe2c7bfd5b4cd3`, alongside
the source of the C++ importer. Proprietary tool binaries and the package remain
outside version control.

## Remaining qualification

The current typed inspection profile accepts only observed 2026.3.1.0/build
38722833 exports. Both releases expose schema-1 metadata and basic inventories,
but a raw export success does not qualify a new typed producer profile. See
[INSPECTION.md](INSPECTION.md) for the known fields and absent event state.

Complete R-015 still requires available typed evidence queries, advanced
workloads, optional SDK capture, and an actual source edit/rebuild/recapture
verification on each selected release. Failed or skipped required cases do not
count as full support. Replay failures and other documented-interface probes are
retained in [INVESTIGATIONS.md](INVESTIGATIONS.md).
