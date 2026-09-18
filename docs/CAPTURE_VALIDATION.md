# Capture and MCP hardware validation

`tests/CaptureIntegration.cpp` provides an opt-in C++ integration harness for the
capture/evidence slice. It starts the actual stdio server, submits real Nsight
captures, retrieves retained exports through MCP, closes the server, and inspects
durable pins through a new server process. It shares the generic client in
`tests/McpClient.hpp` with the ordinary CPU MCP regression. The CPU regression
keeps its existing synthetic workloads and short deadlines.

The configured target and corrected local hardware run are verified below. The
earlier development probes and their retained evidence are recorded in
[INVESTIGATIONS.md](INVESTIGATIONS.md); they do not validate future harness
revisions automatically.

## Verified command and result

After the source setup in [README.md](../README.md), select the installation
explicitly and run the opt-in target:

```sh
cmake --preset linux-gcc-debug -DNGM_NSIGHT_ROOT=/absolute/path/to/Nsight-installation
cmake --build --preset linux-gcc-debug --target ngm_capture_integration_run
```

The default evidence/report roots are `artifacts/nsight-evidence` and
`artifacts/capture-validation`; override `NGM_CAPTURE_ARTIFACT_ROOT` and
`NGM_CAPTURE_VALIDATION_ROOT` in the configure cache if needed. This target is
excluded from default builds, CTest, and the ordinary CPU aggregate.

On 2026-09-18, product **0.1.1**, GCC 16.2.1 Debug, Nsight **2026.3.1.0/build
38722833**, RTX 3080 Ti / driver 615.71.09 / KDE Wayland-Xwayland, the corrected
run **passed**. It retains report/transcript bundle
`bundle-9e029958d57c655a53e191cef3bb580c`, explicitly pinned in the managed store.
The local report is
`artifacts/capture-validation/capture-validation-ftIB4T/report/report.json`.

| Case | Outcome | Pinned evidence |
| --- | --- | --- |
| Standalone reference baseline | Pass | `bundle-1a4101435cb327e89285cf6d1cce3fe4` |
| Reference capture, PID 98755 | Pass, all five exports | `bundle-428f16403c26367e2c5bc18c4b3340ba` |
| Reference repeat, PID 98883 | Pass, all five exports | `bundle-ca8b82da72fa166df06d09b051ff264e` |
| Shader-error capture, PID 99012 | Pass, all five exports | `bundle-b2dfd8f3916afcfec7b5c6ffdca5f6de` |
| Fresh targets, repeat PNG identity, faulty PNG difference | Pass | Same capture bundles |
| Baseline/capture pins after server restart; EOF shutdown | Pass | Retained report and MCP transcript |
| Decoded cross-origin pixels, GPU replay, source repair | Skipped | Outside this harness's checks |

The first harness run rejected the legitimate empty logs export. I-008 records
that failed validation, its pinned evidence, and the correction. Independent
review covered the harness and corrections. CPU checks cover timeout/cancellation
classification, empty logs, and absent optional backend export flags. The
successful run validates the selected tools/setup only, not advanced renderer
features. A separate matching-tool run also passes on **2026.2.0.0/build
37991608**, retaining report/transcript
`bundle-2af18aba623ad2e75b3834ba0980b2a8`. The exact second-release inputs,
per-case evidence, acquisition, and remaining gaps are in
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).

## Inputs and isolation

The executable takes five positional arguments, in this order: `SERVER`,
`FIXTURE`, `NSIGHT_ROOT`, `ARTIFACT_ROOT`, and `OUTPUT_ROOT`, followed optionally
by `--workload NAME`. The default is `basic`; unknown selectors and malformed
option placement fail before creating run/store directories. Paths are resolved to
absolute locations. The server and fixture must be executable regular files; the
Nsight root must exist. The artifact and output roots must not overlap, because
the report and standalone baseline are imported from outside the managed store.
Use a store that no other server process owns. Launching the harness requires the
existing windowed desktop and GPU prerequisites described in
[FIXTURE.md](FIXTURE.md) and [NSIGHT_BACKEND.md](NSIGHT_BACKEND.md).

Each invocation allocates an isolated directory under the output root and copies
the server and fixture executables there before hashing and launching them. The
standalone `run_experiment` baseline also retains its own executable snapshot,
shader sources/SPIR-V, compiler settings, GPU/driver, desktop, and application
build provenance. Captures use that baseline's retained shader directory, keeping
the executable and shader identities separate and stable across all launches.
No expected diagnosis or private oracle metadata is submitted to MCP.

| Workload | Baseline and repeated reference | Variant |
| --- | --- | --- |
| `basic` | `reference` | `shader-error` |
| `binding` | `reference` | `binding-error` |
| `pipeline` | `reference` | `pipeline-error` |
| `multipass` | `multipass-reference` | `pass-output-error` |
| `bindless` | `bindless-reference` | `resource-selection-error` |
| `indirect` | `indirect-reference` | `indirect-parameter-error` |
| `combined-pass` | `combined-reference` | `combined-pass-error` |
| `combined-resource` | `combined-reference` | `combined-resource-error` |
| `combined-indirect` | `combined-reference` | `combined-indirect-error` |

At **0.2.1**, all nine selectors pass on both matching Nsight releases, totaling
54 captures. The standalone baseline always uses the selected reference scenario,
and current bundles retain all seven shader source/SPIR-V pairs. The report
records `workload`, `reference_scenario`, and `variant_scenario`. Batch evidence,
producer differences, and the separate typed-query validation are recorded in
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). Fresh-context review found no
actionable defect in selector mappings, validation order, isolation, or evidence
classification.

The server receives an explicit desktop-environment allowlist and a fixed system
`PATH`. `XDG_DATA_DIRS` is intentionally absent to exercise the capture service's
production default. The report retains that server environment; each capture's
raw report separately records its effective isolated worker environment. The
standalone runner's layer policy remains distinct from Nsight injection.

## Checks and evidence limits

The standalone reference and all captures use seed **42** and **192 × 128**.
The standalone baseline reads back application frame **2**. Each of the three
captures requests Nsight capture frame **2**, one presented frame, with the
application configured to finish at frame **20**. Captures run the selected
reference twice and then its variant, each through a fresh application launch.
The default sequence remains `reference`, `reference`, then `shader-error`.
The configured final application frame does not mean that application readback
must finish before Nsight's exit-on-capture cleanup.

For the supplied fixture and observed Nsight 2026.3.1.0 run, a separate decoded
image probe found that capture frame 2 matched application frame 2 exactly;
application frame 1 differed. This is an observed qualification of that workload
and version, not a general equivalence between application and Nsight frame
numbering. The harness retains the selectors separately and does not decode PNG
pixels or assert cross-origin image equality.

The optional SDK mode adds `--sdk-first-boundary-frame N --baseline-frame M` to
the harness invocation. Both selectors are required together: N selects the
application's first explicit SDK boundary, while M independently selects the
standalone application readback. The capture delimiter ordinal remains 2. No
formula equating those selectors is assumed by the harness. Each run still uses
reference/reference/variant in fresh targets. The baseline makes no SDK calls,
including when built with SDK support. The SDK mode additionally checks the
retained pre-call initialization, exact tool/SDK pairing, executable and shader
identities, workload inputs, and separate entered/completed boundary records.
Normal mode verifies the absence of the fixture's SDK-control output. Decoded
pixel correspondence is qualified separately in [SDK_CONTROL.md](SDK_CONTROL.md).

The harness checks:

- Successful matching-tool capture and metadata readability, completed job
  cleanup, released GPU reservation, and publication of a pinned artifact.
- Individual outcomes for metadata, functions, objects, logs, and screenshot
  exports, with no inferred pipeline/descriptor/resource contents. Successful
  logs exports may contain zero bytes when there are no messages; they still
  require a retained file with a matching inventory size and content hash.
  Metadata, functions, objects, and screenshots must contain data.
- Actual MCP initialization, capability query, capture submission, job polling,
  artifact metadata/file inventory, and raw export retrieval. Event/function and
  object exports remain raw evidence; this is not a typed inspection API check.
- The fixture executable hash before/after capture and the retained shader build
  manifest, correlated with the independent standalone baseline.
- Distinct target PID markers observed in each capture's retained Nsight
  `Connection Established` stdout line. Missing markers are `unsupported`;
  retained marker text and file hashes identify the evidence. These observations
  make no subsequent PID-liveness or ownership claim.
- Valid PNG signatures and selected image dimensions, identical raw PNG hashes
  for the repeated reference, and a different raw PNG hash for the selected variant.
  This compares scenarios and does not establish diagnosis or source-fix
  verification.
- Published, unquarantined pins for the standalone baseline and every submitted
  capture after server restart, including failed attempts.

Small UTF-8 files travel as MCP text. For binary logs/images or larger exports,
the harness obtains MCP file references and performs bounded reads of pinned,
published files. It checks scoped artifact IDs, relative paths, exact local
paths, resolved containment, regular-file safety, and inventory sizes; each
read is capped at 16 MiB. Raw evidence stays in its managed bundle.

Capture jobs have a **120-second** deadline. Hardware polling allows **180
seconds** and waits for both worker completion and artifact finalization. MCP
responses and normal EOF shutdown each have a **30-second** bound, distinct from
the CPU client's five-second default and CPU check's ten-second job loop. A
failed assertion stops subsequent submissions and still requests normal EOF
cleanup before restart. Cancellation is requested when the polling bound expires.
Timed-out and cancelled jobs are failures even if discovery never established a
ready Nsight interface. Only an ordinary failed job with an explicitly unavailable
interface is classified as unsupported. The shared rules in
`tests/CaptureValidation.hpp` are exercised by the CPU-only
`tests/CaptureValidationCheck.cpp`, including the successful empty-log case.

## Reports and retention

`report/report.json` is replaced atomically as work progresses. Complete MCP
requests and responses, including tool errors, are appended to
`report/mcp-transcript.ndjson`; session stderr is retained separately. Submitted
job/capture/attempt identities enter the report before polling. Failed and
unsupported cases remain recorded, and unattempted cases are `skipped`.

The report distinguishes `pass`, `fail`, `unsupported`, and `skipped`. A
successful required capture slice exits **0**; unavailable required evidence
exits **3**; a failed check, setup, cleanup, or publication exits **1**. Pixel
decoding, GPU replay execution, and source-edit/rebuild/recapture verification
are explicitly skipped scope checks and cannot be inferred from a passing
capture slice. Only the explicitly executed release/workload combinations are
qualified; a passing run does not establish the complete visual-debugging release.

Every submitted capture requests a persistent pin. The standalone baseline is
imported with a pin even when it fails. After capture and restart sessions end,
the harness freezes the report and transcript and imports that snapshot through
MCP with a persistent pin. It attempts this publication for failed runs too.
Because an immutable report cannot contain its own later artifact ID, the
outer `publication-receipt.json` records the import result, artifact reference,
and publication-session shutdown result. The mutable local report also receives
that receipt information. The imported snapshot states this boundary explicitly.

No validation artifacts are unpinned or pruned automatically. If report import
fails, the local report, transcript, baseline, and any already-pinned capture
bundles remain available. Investigations must record the reported failure and
retained references in [INVESTIGATIONS.md](INVESTIGATIONS.md); the harness does
not edit repository planning or investigation documents.
