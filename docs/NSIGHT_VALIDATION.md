# Local Nsight release qualification

Date: **2026-09-18**. R-015 remains **In Progress**. Basic/advanced uninstrumented
capture/export and typed inventory queries pass on two exact producers across
**54 captures** at product **0.2.1**. At **0.2.2**, optional SDK control passes
12 basic captures plus 6 default-mode regressions on matching tool/SDK pairs.
The source-edit/rebuild/recapture workflow remains unqualified; this is not the
complete visual-debugging release.

## Tools and common environment

| Component | First release | Second release |
| --- | --- | --- |
| Capture/replay CLI version | 2026.3.1.0 | 2026.2.0.0 |
| Tool build | 38722833 | 37991608 |
| Metadata version spelling | 2026.3.1 | 2026.2.0 |
| SDK present | 0.9.2, not used | 0.9.0, not used |
| Explicit installation | `/opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3` | Repository-local `build/nsight-2026.2/installation` |

The historical basic matrices use product **0.1.1**, GCC **16.2.1 Debug**, Arch Linux x86-64,
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

## Historical basic capture/export matrix

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
| Typed metadata/event/object queries on retained basic pair | Pass; separate pinned query probe | Unsupported by the then-current 0.2.0 producer profile |
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

## Basic and advanced qualification at 0.2.1

On the same GPU/driver/desktop/compiler configuration, the workload-selectable
harness passes **18 runs**: nine workload pairs on each release, with a standalone
application baseline and three fresh MCP captures per run. Every capture passes
matching-tool metadata/functions/objects/logs/screenshot exports, confirmed
cleanup and GPU-reservation release, retained raw evidence retrieval, and pins
after server restart. References repeat with identical PNG hashes and every
variant differs. All runs shut down their MCP sessions normally.

| Harness workload | Reference / variant | 2026.3.1.0 | 2026.2.0.0 |
| --- | --- | --- | --- |
| `basic` | `reference` / `shader-error` | Pass | Pass |
| `binding` | `reference` / `binding-error` | Pass | Pass |
| `pipeline` | `reference` / `pipeline-error` | Pass | Pass |
| `multipass` | `multipass-reference` / `pass-output-error` | Pass | Pass |
| `bindless` | `bindless-reference` / `resource-selection-error` | Pass | Pass |
| `indirect` | `indirect-reference` / `indirect-parameter-error` | Pass | Pass |
| `combined-pass` | `combined-reference` / `combined-pass-error` | Pass | Pass |
| `combined-resource` | `combined-reference` / `combined-resource-error` | Pass | Pass |
| `combined-indirect` | `combined-reference` / `combined-indirect-error` | Pass | Pass |

Each run selects seed 42, 192×128, and capture frame 2. The harness uses the
selected reference scenario for its standalone baseline and retains all seven
shader source/SPIR-V pairs. Requested advanced features are checked explicitly;
the test does not substitute a basic workload. The batch freezes the harness,
server, and fixture executables before its first run. Their hashes, each exact
command, baseline/build/shader identities, per-release report IDs, and all 54
capture IDs are retained in the explicitly pinned batch:

- `bundle-a4bce77c1a8d615412702da25e33fa75`, a 57,622,648-byte imported snapshot.
- `raw/imported/batch.json` contains the 18 run results and commands.
- `raw/imported/export-inventory-summary.json` maps each run to its pinned
  baseline, report, and captures, with observed inventory counts and key sets.
- The snapshot includes the executed binaries and exact harness/client sources.

The local batch is `artifacts/capture-validation/workload-batch-rl80zzcx`.
Baseline, individual report/transcript, and capture bundles are independently
pinned too. Use the five positional harness arguments above with an optional
`--workload NAME`; [CAPTURE_VALIDATION.md](CAPTURE_VALIDATION.md) defines the
selectors and comparison scope. The default remains `basic`.

After the two typed producer profiles and observed optional `indirect_index`
field were implemented, a separate **0.2.1** C++ MCP client queried all 54 retained
captures. All metadata/events/objects queries pass, every paginated row matches
the raw export fields and ordering, and all metadata remains identical after
server restart without desktop variables or a usable tool PATH. Its exact
report, input expectations, sources, and transcripts are explicitly pinned as
`bundle-a1af3b8bb46feebabcc66a3bd821ab80`. This query uses a later server binary
than the frozen capture batch; both reports retain their actual executable hashes.
The earlier two-release basic query probe is separately pinned as
`bundle-0a4c5bc3e9902c1e0526f5d2d1ff1843`.

The inspected metadata exports still omit detailed pipeline/descriptor/shader/
resource state, pass relationships, and indirect command contents; version
differences and exact inventory contracts are in [INSPECTION.md](INSPECTION.md).
Decoded cross-origin pixel comparison, GPU replay execution, SDK control, and
source repair are **skipped** in this capture harness. Separately attempted GPU
replay remains **failed** on both releases as recorded above. Passing variant
comparison is not a source repair or replay-rendered result.

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
system profiling settings were changed. The SDK was only observed during that
acquisition; its later runtime qualification is recorded below.

The archived [2026.2 release notes](https://archive.docs.nvidia.com/nsight-graphics/2026.2/ReleaseNotes/index.html)
specify Linux driver 580.126.18 or newer. The tested driver exceeds that version
minimum; this alone establishes no additional workload compatibility. The local
acquisition receipt is `build/nsight-2026.2/acquisition.json`; a small immutable
copy is explicitly pinned as `bundle-0effb14aea52838c6dbe2c7bfd5b4cd3`, alongside
the source of the C++ importer. Proprietary tool binaries and the package remain
outside version control.

## Optional SDK qualification at 0.2.2

The C++ MCP harness passes **18 fresh captures**: on each release, three basic
captures with SDK boundaries starting at application frame 6, three starting at
frame 10, and three with no SDK calls using the default present delimiter.
Each group captures reference/reference/shader-error with matching tools,
completed cleanup, separate target PID observations, all five exports, and
persistent pins verified after server restart. Fixtures are explicitly built
against matching **SDK 0.9.2** for 2026.3.1.0/build 38722833 and **SDK 0.9.0**
for 2026.2.0.0/build 37991608.

Independent RGB decoding confirms SDK reference screenshots match separately
rendered application frames 7 and 11 respectively, and the default references
match application frame 2. The frame-7 and frame-11 outputs differ, establishing
the effect of the selected boundary for these inputs. This is not a general
frame-numbering rule. Advanced SDK workloads, no-presentation compute, actual
GPU replay and source repair are not established by this matrix.

The combined matrix, comparison script, frozen inputs and exact bundle mapping
are persistently pinned as `bundle-7b6c2aa712deb108b66af94d86b50e75`.
[SDK_CONTROL.md](SDK_CONTROL.md) records invocation, identities, all measured
limits, and the distinction between the snapshot importer's version and its
0.2.2 payload. I-012 retains the first failed harness assertion and successful
correction separately from the underlying capture results.

## Remaining qualification

The typed inspection profiles accept the two exact version/build tuples above,
with matching same-bundle schema-1 Vulkan metadata. Other producers are rejected
until separately qualified; raw export success alone does not qualify them. See
[INSPECTION.md](INSPECTION.md) for the known fields and absent event state.

Complete R-015 still requires an actual source edit/rebuild/recapture
verification on each selected release, including sufficient
retrievable diagnostic evidence. Failed or skipped required cases do not
count as full support. Replay failures and other documented-interface probes are
retained in [INVESTIGATIONS.md](INVESTIGATIONS.md).

## Product generated C++ capture, 0.2.3

The separate `capture_cpp` mode passes its basic MCP matrix on **both matching
releases** on 2026-09-18. The C++ harness snapshots the server, fixture, and shader
inputs, makes three fresh captures per release, retrieves the derived index and
real command source through MCP, checks exact screenshot file repeat/difference,
and verifies persistent pins and identical indexes after restart without desktop
variables or an Nsight override. All jobs succeed with confirmed cleanup and
released GPU reservations; all generated metadata reports
`has_unsupported_operation: false`. All six captures and both reports below are
complete and explicitly pinned in `artifacts/nsight-evidence`.

| Exact release | Reference | Repeated reference | Shader error | Report, inputs, sources, transcripts |
| --- | --- | --- | --- | --- |
| 2026.3.1.0 / 38722833 | `bundle-acdca6fd76d4b9a6e153d3078fe11ef7` | `bundle-8c356036e93abe7d7f45dbf2ffb5c89e` | `bundle-cb123656b3f61d803d6d7e5035793cb4` | `bundle-d2579d201899eba43073dfdcecf2387f` |
| 2026.2.0.0 / 37991608 | `bundle-b1b222b28ffecf6932f72d20a5bf3511` | `bundle-f88354f27982a13dca9a3013670fdcb2` | `bundle-fc62ec56688368e10d7c3db34e41caba` | `bundle-8287081bf6c6f039ced6341fd9aa59e4` |

Configuration: Arch Linux x86-64, GCC **16.2.1 20260810** Debug, source-built
glslang **16.4.0**, RTX **3080 Ti**, driver **615.71.09** (rechecked with
`nvidia-smi`), KDE Wayland/Xwayland with XCB fixture presentation. No SDK calls.
Each target uses seed 42, 192×128, final frame 120, and the documented C++ activity
`wait_frames=2`. Retrieved source contains the complete `"frame.2"` label.
The final frame keeps the fresh target running long enough for capture; it is
not the requested capture index. Each job took approximately 9.5–10.1 seconds.
Production uses the runtime-documented notice suppression without verbose or
Vulkan-loader diagnostic environment settings. I-016 retains the initial failed
output-directory attempt and the reviewed correction before these passing runs.

Common retained executable SHA-256 identities:

- Server: `7334dfb79aaed6a53ad6050e5aa0d599bd79537b0f06adb09482d024e70ac6bd`.
- Fixture: `0cc871be97972deee89e335e3c86063b1e6a79227516fbc0c17f9c2b326ebc11`.
- Harness: `03d89fc035e4497b3c13b38f151959c6829a80db3b8cbc259cffbd8848375e4e`.

Reference screenshot file SHA-256 is
`c32658800ea2b20c881da011561496de69e1b731c226673c66a7fd5ff2b495b2`; faulty is
`3d89d5e30ef184380420c1ca5cb72303f724bd42ddf6b8dea1f08d41d49cb813` on each release.
This harness compares **files**, not decoded pixels. The independent decoded
image/SPIR-V comparisons in [INSPECTION.md](INSPECTION.md) belong to earlier
0.2.2 experiments. Neither result establishes source repair, generic resource
extraction, advanced generated-C++ workloads, or standalone GPU replay.

Reproduce using the built `ngm_cpp_capture_integration` argument contract in
[CPP_CAPTURE.md](CPP_CAPTURE.md), ending with `reference shader-error`.
Exact local argv/stdout/stderr/exit records are under
`build/cpp-mcp-validation/nsight-2026.3-corrected/` and
`build/cpp-mcp-validation/nsight-2026.2/`; the durable pinned reports retain the
MCP arguments, source, frozen inputs, hashes, and successful restart exchanges.
The hardware target is opt-in, separate from the passing CPU aggregate and
focused correction checks recorded in [BUILD_VALIDATION.md](BUILD_VALIDATION.md).
R-006 and R-015 remain in progress; R-007 still requires actual source repair.
