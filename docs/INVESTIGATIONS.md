# Nsight Integration Investigations

Record each failed Nsight integration or evidence-extraction attempt for future
investigation, as requested in planning round 5. Include inconclusive probes;
distinguish an observed failure from a proven limitation. The current integration
scope uses documented Nsight interfaces only.

Related planning: [ROADMAP.md](ROADMAP.md), especially R-006 and its dependent
diagnostic workflows. Guidance and the dated local baseline: [AGENTS.md](../AGENTS.md).

## Recording rules

- Give each failed attempt a stable ID and its own entry. Link related attempts
  and successful follow-ups; preserve the original result when resolving a gap.
- Record enough context to reproduce the attempt: documented entry point and
  source, exact arguments or a minimal SDK example, workload/build identity,
  configuration, and tool/SDK/GPU/driver versions as relevant.
- Describe expected and observed results separately, including exit status and
  the relevant error or missing data. Retain raw evidence as local artifacts;
  commit only small, sanitized excerpts or fixtures, not large captures/logs.
- Automatic artifact retention was selected in round 8. Explicitly pin the evidence
  bundles relied on by each entry and record their IDs/retention state; a link does
  not itself protect data from pruning. If evidence is intentionally removed later,
  update the entry to mark it unavailable while preserving the reproduction steps
  and sanitized findings. R-012 tracks the storage and pinning implementation.
- State the narrow conclusion supported by the evidence: a configuration error,
  prerequisite, version-specific failure, missing documented path, or inconclusive
  result. Do not label a capability universally unsupported from one failed probe.
- Name the affected roadmap item/capability, current limitation, and a useful
  revisit condition, such as a new documented interface, tool version, or corrected
  prerequisite. Investigation records do not authorize excluded integration paths
  or count as implementation of a missing capability.

## Attempts

### I-001 — Window-system extension unavailable under initial capture injection

Date: **2026-09-18**. State: **Resolved by I-004's explicit data-search-path correction**. Related items: R-001, R-011, R-012;
the missing capture also prevents R-006 inspection probes.

The first real capture used the documented
[Graphics Capture CLI](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html),
with `ngfx-capture` and matching replay tools **2026.3.1.0, build 38722833**.
SDK 0.9.2 is installed but was not used. Environment: Arch Linux x86-64,
RTX 3080 Ti, driver 615.71.09, KDE Wayland / Xwayland (`DISPLAY=:1`),
GCC 16.2.1 Debug, product 0.1.1, and source-built glslang 16.4.0.

Workload: the uninstrumented basic fixture, `reference`, seed 42, 192×128,
rendering frames 0 through 20. Nsight was asked to capture one frame at its
one-based capture frame 2 using presentation delimiters. The exact executable
SHA-256 was `1eda588561c7bf699ba57bd7ef85223d675b34130f9053956f0cbcbde526dc5f`.
Shader source/SPIR-V identities and compilation settings are retained with the
attempt. The same binary completed an isolated standalone preflight beforehand.

Minimal reproduction from a built checkout and the same desktop:

```sh
build/linux-gcc-debug/ngm-capture \
  --artifact-root "$PWD/artifacts/nsight-evidence" \
  --nsight-root /opt/nsight-graphics/NVIDIA-Nsight-Graphics-2026.3 \
  --executable "$PWD/build/linux-gcc-debug/ngm-vulkan-fixture" \
  --working-directory "$PWD" \
  --argument --scenario --argument reference --argument --seed --argument 42 \
  --argument --width --argument 192 --argument --height --argument 128 \
  --argument --frame --argument 20 --capture-frame 2 --timeout-ms 120000 \
  --application-output-option --output --pin
```

The original invocation additionally supplied a JSON copy of the standalone
preflight's application/build/shader/GPU metadata through `--provenance-json`.
That supplemental metadata is labelled caller-provided. Exact backend argv,
isolated environment, tool observations, and outcomes are in `raw/report.json`.

Expected: a nonempty saved capture and metadata readable by matching replay tools.
Observed: Nsight established its target connection, then `ngfx-capture` exited
255 after the target exited 3. The fixture's stderr reported:

```text
unsupported prerequisite: the Vulkan loader lacks required desktop extension VK_KHR_surface
```

No capture was produced and no replay/export was attempted. The job completed as
failed after approximately 4.1 seconds, with owned-process cleanup confirmed and
its GPU reservation released. The failed bundle was published normally, not
quarantined. Its pin survived reopening the store through MCP; `artifact_info`
and `artifact_read(raw/report.json)` retrieved the retained evidence.
The Nsight stdout log contains one NUL byte, so the strict MCP text-read operation
rejected that log as binary; the original bytes remain available on disk.

Evidence and retention, under `artifacts/nsight-evidence`:

- **Pinned failed capture:** `bundle-2681d459696671f55ed0ab2a15a7d71f`.
  See `raw/report.json`, `raw/logs/capture/capture.stdout.txt`,
  `raw/logs/capture/capture.stderr.txt`, discovery logs, and `raw/application/`.
- **Pinned successful application-readback preflight:**
  `bundle-1d1bd01e1378fc5f759d0e108b5e1aec`, with its report, executable snapshot,
  image, application metadata, and shader bundle under `raw/imported/`.

Conclusion: this exact injected fixture launch fails before Vulkan instance
creation. This does not establish that Nsight cannot capture Vulkan/XCB on this
machine or explain why the extension list changed. The CLI argument delivery,
injection, loader configuration, and extension observations still need isolation.
Next action/revisit condition: run fresh, pinned controls using the documented
`--passthrough` and `--no-process-injection` launch modes, then inspect explicit
loader diagnostics before changing any integration choice. No machine settings
were changed, and this attempt does not qualify Nsight release support.

### I-002 — Passthrough control retains the instance-extension failure

Date: **2026-09-18**. State: **Open**, related to I-001 / R-001.
Environment, tool version/build, executable hash, shader bundle, and workload
inputs are identical to I-001. The documented `ngfx-capture --passthrough`
control tracks launches without enabling capture. The isolated C++ control
runner uses the same owned-process and managed-artifact primitives as production.

Reproduction: compile the retained `raw/LaunchControl.cpp` with C++20, project
headers and the source-built `ngm_capture`, `ngm_artifacts`, `ngm_nsight`, and
`ngm_core` static archives; invoke it with absolute store, installation, fixture,
`passthrough`, and baseline bundle ID arguments. The retained runner executable
and report identify the exact probe. Its underlying command is:

```text
ngfx-capture --passthrough --exe FIXTURE --working-dir CHECKOUT
  --args="--scenario reference --seed 42 --width 192 --height 128 --frame 20 --output FRESH_OUTPUT"
```

Expected: the target renders normally without capture interception, preserving
the standalone output. Observed: connection established, target exit 3 with the
same missing `VK_KHR_surface` message, wrapper exit 255, and confirmed cleanup.
No capture/replay was requested. **Pinned failed control bundle:**
`bundle-b76bf96dd0c9bd3c8f9cc461e520fa17`, under `artifacts/nsight-evidence`;
see `raw/report.json`, `raw/logs/{stdout,stderr}.txt`, and the retained probe.

A direct-launch control with the capture service's environment (including
implicit layers enabled) passed and exactly matched the pinned preflight image:
**pinned bundle `bundle-afa82ab081555b7ed6478bed6e4ad8da`**. Thus the fixture-only
runner's implicit-layer filter does not explain the difference by itself.
Conclusion: disabling capture through passthrough did not remove the failure.
This does not isolate every part of injection or prove an Nsight incompatibility.
Revisit with loader diagnostics and the no-process-injection control below.

### I-003 — No-process-injection target succeeds; launcher status remains 255

Date: **2026-09-18**. State: **Open** for launcher-control exit semantics;
related to I-001/I-002 and R-001/R-011. Environment, tools, exact executable, and
workload match those records. Use the retained I-002 runner with mode
`no-process-injection`, which selects documented `--no-process-injection`
instead of `--passthrough` and supplies the same executable/working-dir/args.

Expected: a normal target launch with no process injection and a clean wrapper
exit. Observed: the target completed with exit 0, reported application status
`pass`, and produced the exact standalone reference image (SHA-256
`325f2859c6ef3c55c21bbed8a9d68960cbed17bc4c51bff3e7589d66397077e6`). Nevertheless,
the wrapper exited 255 and stated that it expected terminate-after-capture code
119, even though this control did not supply `--terminate-after-capture`.
The runner conservatively retained a failed control result for the nonzero
wrapper status; subsequent inspection of the retained application result and
image established the target's successful rendering. Cleanup was confirmed.

**Pinned bundle:** `bundle-6896f4f6d29fb0d64afcdf1853dac8ec` under
`artifacts/nsight-evidence`; exact argv/environment, tool versions, runner
source/executable, logs, application result/image/shaders are retained in `raw/`.
No capture/replay was requested. Conclusion: the same launcher argument delivery
can run this workload correctly when process injection is disabled. This narrows
I-001 toward injection/loader interaction; it does not yet identify the cause.
The control-specific nonzero result is not a new success exception in the capture
adapter. Revisit with an injected run carrying documented Vulkan loader logging,
and recheck control-mode exit semantics on another Nsight version.

### I-004 — Injected loader searches no installed driver manifest directory

Date: **2026-09-18**. State: **Resolved for the tested configuration**, related to I-001/I-002 and R-001.
The tool versions, desktop, GPU/driver, reference workload, and retained fixture
executable hash match I-001. A fresh capture used the same `CaptureService`
with the documented Vulkan loader diagnostic variable `VK_LOADER_DEBUG=all`;
its timeout was 60 seconds. No machine configuration changed.

Reproduction: the C++ diagnostic source is retained in
`raw/report.json` under `caller_provided_application_provenance.probe_source`.
Build it with the project
headers and `ngm_capture`, `ngm_artifacts`, `ngm_nsight`, and `ngm_core` archives;
invoke it with absolute store, Nsight installation, and executable arguments.
The executable was copied from the pinned I-001 preflight into a separate
development directory and made executable; the pinned original was unchanged.
Exact arguments, environment, probe hash, and application hash are in the report.

Expected: loader diagnostics identify the driver/layer discovery preceding the
missing `VK_KHR_surface` error. Observed: target exit 3, launcher exit 255, no
capture or replay, and the same missing-extension error after approximately
4.1 seconds. Owned-process cleanup was confirmed. The loader finds Nsight's
Pylon implicit-layer manifest, but all driver searches report `Found no files`.
The searched locations include isolated XDG home directories, `/etc/xdg`,
`/etc`, and Nsight's Pylon directory; `/usr/share` and `/usr/local/share` are
absent. Read-only inspection confirms this machine's NVIDIA driver manifest is
`/usr/share/vulkan/icd.d/nvidia_icd.json`.

**Pinned failed bundle:** `bundle-5a8c49eaa826debb04c91bb85894e40c` in
`artifacts/nsight-evidence`. See `raw/report.json` and
`raw/logs/capture/capture.stderr.txt` (10,309 bytes). The bundle is a normal
published failure, not a cleanup quarantine.

Conclusion: this launch cannot discover the installed driver through its logged
search paths. The capture environment currently omits `XDG_DATA_DIRS`; the
injected search includes only Nsight's data directory beyond the other explicit
locations. This suggests loss of the normal XDG data-directory defaults during
injection, but does not yet prove a successful capture after correction.
Revisit with the documented XDG defaults explicitly supplied to a fresh isolated
capture, then preserve caller-specified data search paths if the probe confirms it.

Successful follow-up: supplying only
`XDG_DATA_DIRS=/usr/local/share:/usr/share` in the diagnostic's explicit
environment produced **pinned complete bundle
`bundle-0863b91c8d1a178482d1cd3abebff32c`**, with the same application hash,
scenario/inputs, tools, and loader logging. The job succeeded in 5.2 seconds;
capture and all five export processes exited 0, cleanup was confirmed, and the
GPU reservation was released. The metadata's target environment confirms the
standard paths followed by Nsight's injected Pylon path. This resolves the
missing-driver/missing-extension failure for this configuration. The production
environment correction and repeat captures through MCP subsequently passed;
I-005 retains their evidence and the observed fixture-frame correspondence.

The saved capture is 98.6 KiB; exports contain JSON metadata, 22 function-stream
entries, 32 object records, an empty captured log, and a valid 192×128 PNG of
the embedded final present. The standalone preflight selected application frame
20 whereas this capture selected Nsight frame 2; their images are not equivalent
frame references. Nsight terminates the target after capture, before its frame-20
readback/result is written; retained application shader files remain distinct
from Nsight evidence. No shader/pipeline/resource-state extraction is established.
Metadata emits a version warning (`2026.3.1` capture versus `2026.3` replay)
despite both tools reporting 2026.3.1.0/build 38722833. The successful commands
read metadata only; actual GPU replay remains untested at this point.

Sources: [Vulkan loader diagnostics](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDebugging.md),
[XDG data-directory defaults](https://specifications.freedesktop.org/basedir/latest/),
and [Nsight Vulkan troubleshooting](https://docs.nvidia.com/nsight-graphics/UserGuide/troubleshooting.html).

### I-005 — Capture frame numbering needs an observed application correspondence

Date: **2026-09-18**. State: **Resolved for this fixture and Nsight release**.
Related items: R-001, R-006, R-007; follows I-004. Nsight 2026.3.1.0/build
38722833, retained fixture hash `1eda588561c7bf699ba57bd7ef85223d675b34130f9053956f0cbcbde526dc5f`,
and the same GPU/driver/desktop/shader settings as I-001.

The corrected production environment completed three captures through the real
stdio MCP server: two reference runs and one shader-error run. All selected
Nsight capture frame 2, one frame, presentation delimiter; the application was
configured with seed 42, 192×128, final readback frame 20 and was terminated by
Nsight after capture. All five export operations exited 0 for every capture.

Expected comparison probe: because CLI help describes one-based capture frame
numbering, test whether capture frame 2 corresponds to the fixture's zero-based
application frame 1. Observed: decoded RGB differs from a fresh standalone
frame-1 readback by up to 4, 4, and 3 channel steps. That correspondence is not
supported; this is a comparison failure, not a failed capture or invalid PNG.
The fixture changes its palette by frame, so arbitrary frame references cannot
be substituted or accepted with an expanded tolerance.

Follow-up: `ngm-experiment` with the same executable/seed/resolution/scenario and
explicit `--frame 2` produced a standalone image matching the Nsight screenshot
exactly in decoded RGB8 (zero difference on every channel and pixel). The two
fresh reference captures also match exactly, while the shader-error image
differs. This establishes the frame mapping only for this workload/backend
configuration. Later boundaries, SDK controls, or versions must establish their
own correspondence.

Evidence, all explicitly pinned under `artifacts/nsight-evidence`:

- MCP validation transcript and retained C++ probe sources:
  `bundle-5ff26740ed6e0aa24ca466c348da8799`.
- First reference capture, target PID 90251:
  `bundle-019e2411c941dafdbfba953018451416`.
- Repeated reference capture, target PID 90471:
  `bundle-55689147d2b220f4b7b72aff355e0800`.
- Shader-error capture, target PID 90601:
  `bundle-3f5db7a4c55c1a3e1b6a53636b091066`.
- Standalone frame-1 baseline: `bundle-3710db7eae97bf05e761f31579481868`.
- Standalone frame-2 baseline: `bundle-dc92f1dd95b72165dda5ba1089cf451f`.
- Decoded comparison report and exact Python/Pillow script:
  `bundle-01f092932bf93bbeb91abd8eb9ee45fe`. The frame-1 comparison differs on
  7,372 pixels; frame 2 and reference repeat differ on zero; the shader-error
  capture differs on 7,337 pixels. The report identifies decoder version and
  raw file hashes. These are measurements, not an inferred diagnosis.

Reproduction: build the retained C++ probe using the project test client and
`ngm_experiments`/`ngm_core`; its explicit inputs and tool calls are in
`raw/imported/workflow.json`. Decode `raw/exports/screenshot.png` from each
capture and `raw/imported/output/image.ppm` from the baselines into RGB8 without
rescaling or color transforms. The separate standalone frame-2 command was
`ngm-experiment --fixture RETAINED_FIXTURE --output-root FRESH_ROOT --scenario
reference --seed 42 --width 192 --height 128 --frame 2`. Reference screenshots
are embedded capture images, not replay-rendered output. Source repair has not
been attempted by these variant comparisons.

### I-006 — Generate C++ Capture times out while connecting to the fixture

Date: **2026-09-18**. State: **Open**. Related item: R-006. The documented
[Generate C++ Capture activity](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
offers a separate source export that may expose more state than the Graphics
Capture metadata inventory. This probe does not parse private capture formats.
NVIDIA's 2026.3 release notes deprecate Vulkan C++ Capture; any usable result
would need explicit version qualification.

Environment: Nsight **2026.3.1.0/build 38722833**, the same fixture executable
hash, GLSL/SPIR-V, GPU/driver and KDE/Xwayland configuration as I-004. The target
used `reference`, seed 42, 192×128, final application frame 120. HOME and writable
XDG/temp directories were isolated; `XDG_DATA_DIRS=/usr/local/share:/usr/share`
was explicitly retained. No SDK or system configuration change was involved.

Reproduction: compile retained `raw/CppCaptureProbe.cpp` with C++20 and the
project `ngm_capture`, `ngm_artifacts`, `ngm_nsight`, and `ngm_core` archives;
invoke with absolute store, installation, and fixture paths. Its exact argv is
in `raw/report.json`; the documented operation was:

```text
ngfx --activity="Generate C++ Capture" --platform="Linux (x86_64)"
  --exe=FIXTURE --dir=CHECKOUT --output-dir=FRESH_CPP_DIRECTORY --wait-frames=2
  --args="--scenario reference --seed 42 --width 192 --height 128 --frame 120 --output FRESH_APPLICATION_OUTPUT"
```

Expected: generated C++ resource/state/frame source with associated data for
inspection. Observed: launcher detected target PID 93088 with status `Vulkan
instance created`, then repeatedly searched for that attachable process before
its internal operation timeout. `ngfx` exited **1** after approximately 37 seconds;
the outer 120-second deadline did not expire. Owned-process cleanup was confirmed.
No C++ files, application result, or final application readback were produced.
Shader source/SPIR-V and launcher/discovery logs were retained. Qt emitted a
locale warning and selected C.UTF-8 itself; this warning does not establish the
connection failure's cause.

**Pinned failed bundle:** `bundle-cb453ee75e7a466f98fcaf75621bf3b8` under
`artifacts/nsight-evidence`. See `raw/report.json`, `raw/CppCaptureProbe.cpp`,
`raw/logs/stdout.txt`, `raw/logs/stderr.txt`, and discovery help/version logs.
This is a normal failed bundle, not a cleanup quarantine. The discovery-only
exit-one exception was not applied to this operation.

Conclusion: this documented C++ export path has not produced inspectable source
for the fixture on this release. The connection timeout is narrower evidence
than universal lack of Vulkan C++ capture. Revisit with documented verbose and
loader diagnostics to distinguish target initialization from activity connection,
or with the second compatible release required by R-015. The working Graphics
Capture/export path remains independently qualified by I-004/I-005.

### I-007 — Actual GPU replay stalls during resource initialization

Date: **2026-09-18**. State: **Open**. Related items: R-006/R-015 and later
replay-dependent performance work. This is distinct from the successful metadata
exports in I-004/I-005: metadata options exit without rendering the capture.

The matching **2026.3.1.0/build 38722833** replayer ran against pinned reference
capture `bundle-019e2411c941dafdbfba953018451416`, on the same RTX 3080 Ti /
615.71.09 / KDE Wayland-Xwayland environment. The documented command was
`ngfx-replay --loop-count=1 --present-app CAPTURE`. The C++ probe retained a usage
lease on the input capture, used a new pinned output bundle and isolated
HOME/XDG/temp paths, and supplied the corrected data-directory search path.
No machine settings changed.

Expected: one replay loop and normal process completion. Observed: file loading
completed, capture/replay GPU and tool identities were printed, and resource
creation reached **4%**. No further progress appeared before the outer
120-second process deadline. The process was terminated with signal 15;
`timed_out: true`, `cleanup_confirmed: true`. Total probe time including tool
discovery and cleanup was approximately 124 seconds. No replay image or usable
timing result was obtained. The same capture/replayer version warning as I-004
appeared; the evidence does not prove that warning caused the stall.

**Pinned failed replay bundle:** `bundle-4d49fea86748c5b780bad23734da2d82` under
`artifacts/nsight-evidence`. See `raw/report.json`, `raw/logs/stdout.txt`,
`raw/logs/stderr.txt`, and retained `raw/CppCaptureProbe.cpp`. To reproduce,
compile that source with the project libraries as in I-006 and invoke with
`STORE INSTALLATION RETAINED_FIXTURE replay CAPTURE_BUNDLE_ID`.

Conclusion: actual GPU replay is not qualified by the current successful capture
and export runs. The captured file is readable through documented metadata
commands, but this replay configuration times out during initialization.
Revisit with documented replay diagnostics or single-threaded initialization to
isolate the stage, and the matching tools from R-015's second release. Keep
export success and GPU replay outcomes separate in capability claims.

Source: [Capture/replay CLI](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
and the retained matching `ngfx-replay --help` output.

### I-008 — Capture validation rejects a successful empty log export

Date: **2026-09-18**. State: **Resolved for this harness configuration**.
Related items: R-001/R-011/R-012.
This is a defect in the project's new C++ hardware harness, separate from Nsight
capture or extraction failure. The run used product 0.1.1, GCC 16.2.1 Debug,
Nsight 2026.3.1.0/build 38722833, and the RTX 3080 Ti / 615.71.09 / KDE
Wayland-Xwayland configuration from I-005. The retained fixture executable hash
is `8750c8744acd4defc665a4f7bf6af874f751b946af015699a6bd762234adc1d0`.

Reproduction: configure `linux-gcc-debug` with `NGM_NSIGHT_ROOT` selecting that
installation, then run `cmake --build --preset linux-gcc-debug --target
ngm_capture_integration_run`. The harness runs a standalone reference at frame 2,
then requests reference/reference/shader-error captures through MCP, with seed
42, 192×128, capture frame 2 and application final frame 20.

Expected: a successful empty `--metadata-logs` export is retained and accepted,
and validation continues to the repeat and faulty capture. Observed: the first
capture job succeeded with all five export outcomes `success`, complete artifact
publication, and confirmed cleanup. The logs file contained zero bytes, as in
I-005. `CaptureIntegration.cpp` required every successful export to be nonempty,
raised `successful export contains retained evidence`, and marked the harness
run failed. The remaining two captures were skipped. Exit status was 1; this
does not contradict the successful underlying capture.

Evidence, all explicitly pinned under `artifacts/nsight-evidence`:

- Harness report and MCP transcript: `bundle-8174c285d2e843592ef40e8f61b3043a`.
- Standalone reference baseline: `bundle-c4e331a2b00f3a4659eb28b700721dde`.
- Successful first capture: `bundle-2b6f7ba2fc62cecdbf6cec65e8b85d4c`.

The report import is a complete copy of a failed validation report; its bundle
status does not claim hardware validation passed. The local report is
`artifacts/capture-validation/capture-validation-wvLLnQ/report/report.json`.
Baseline and capture pins were rechecked after server restart.

Correction: the harness accepts an empty successful logs file while retaining
its inventory and hash checks. All other export types still require contents.
Timeout/cancellation now precede discovery-unavailable classification, and the
adapter returns `unavailable` for absent optional export flags. The new CPU
`ngm_capture_validation_check` and expanded `ngm_nsight_check` pass; a separate
reviewer found no remaining issues in the corrections.

The complete corrected hardware harness passes on the same recorded setup.
Its pinned report/transcript is `bundle-9e029958d57c655a53e191cef3bb580c`;
standalone baseline `bundle-1a4101435cb327e89285cf6d1cce3fe4`; reference captures
`bundle-428f16403c26367e2c5bc18c4b3340ba` and
`bundle-ca8b82da72fa166df06d09b051ff264e`; faulty capture
`bundle-b2dfd8f3916afcfec7b5c6ffdca5f6de`. All five exports pass for each capture,
the three observed target PIDs differ, reference PNG hashes match, the faulty PNG
differs, and baseline/capture pins survive restart. The local report is
`artifacts/capture-validation/capture-validation-ftIB4T/report/report.json`.
Decoded cross-origin comparison, actual GPU replay, and source repair are
explicitly skipped by this harness. See docs/CAPTURE_VALIDATION.md for its scope.

### I-009 — Serial resource initialization does not resolve GPU replay stall

Date: **2026-09-18**. State: **Open**. Related items: R-006/R-015; follow-up
to I-007. The same pinned input capture
`bundle-019e2411c941dafdbfba953018451416`, Nsight 2026.3.1.0/build 38722833,
RTX 3080 Ti / 615.71.09 / KDE Wayland-Xwayland, and isolated environment were
used. The changed documented replay options were
`--no-multithreaded-init --verbose`, alongside `--loop-count=1 --present-app`.
Installed help and the [capture/replay CLI documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html)
describe disabling multithreaded resource initialization.

Expected: determine whether resource-initialization threading explains I-007.
Observed: loading completed, the same version warning appeared, and resource
creation/initialization again stopped at **4%**. The outer 120-second deadline
expired; signal 15 terminated the replay, and owned-process cleanup was
confirmed. No replay output or timing measurement was obtained.

**Pinned failed bundle:** `bundle-a52b587381aa9298f78096ef411d5eab`, under
`artifacts/nsight-evidence`. Its `raw/report.json`, `raw/CppCaptureProbe.cpp`,
and `raw/logs/` preserve the exact command, isolated environment, tool identities,
source, and output. Reproduce by compiling that retained C++ probe as in I-006
and invoking `STORE INSTALLATION RETAINED_FIXTURE replay CAPTURE_BUNDLE_ID
serial-init`. The input capture was held under a usage lease during the run.

Conclusion: serial resource initialization alone does not resolve the stall on
this configuration. This does not establish its cause or a universal replay
limitation. Next, probe the documented `--no-block-on-incompatibility` option:
the repeated version warning makes an incompatibility prompt a concrete
possibility, not a confirmed explanation. Retain default behavior separately.

### I-010 — Nonblocking incompatibility handling does not resolve GPU replay stall

Date: **2026-09-18**. State: **Open**. Related items: R-006/R-015; follows
I-007/I-009. This probe used the same pinned reference capture
`bundle-019e2411c941dafdbfba953018451416` and Nsight/GPU/driver/desktop identities
as I-009. It restored default resource initialization and added the documented
`--no-block-on-incompatibility --verbose` flags to
`ngfx-replay --loop-count=1 --present-app CAPTURE`.

Expected: distinguish a blocking incompatibility prompt from the previously
observed initialization stall. Observed: the same version warning and **4%**
resource creation/initialization progress, followed by the outer 120-second
deadline. Signal 15 stopped the process; cleanup was confirmed. No replay frame
or usable timing result was produced. The flag did not resolve the stall and
does not establish whether another initialization problem is responsible.

**Pinned failed bundle:** `bundle-dac60e54541ab776bc49fd593dbeb0b0` under
`artifacts/nsight-evidence`. The retained `raw/CppCaptureProbe.cpp`, report,
version/help logs, and process stdout/stderr preserve the exact experiment.
Compile that source as in I-006 and invoke
`STORE INSTALLATION RETAINED_FIXTURE replay CAPTURE_BUNDLE_ID no-block`.
The input capture remained protected by a usage lease during the run.

The option is listed in installed help and the
[documented replay troubleshooting options](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html).
Revisit with loader diagnostics that identify a failing initialization call or
the matching tools from R-015's second release; none of these failed probes
qualifies replay support or changes the successful capture/metadata results.

### I-011 — Matching 2026.2 tools also time out during GPU replay initialization

Date: **2026-09-18**. State: **Open**. Related items: R-006/R-015; follows
I-007/I-009/I-010. This attempt uses Nsight **2026.2.0.0/build 37991608**
against its own successful reference capture,
`bundle-9c1f4095228eaf248119d63c9604ee86`. It is not cross-version replay.
SDK 0.9.0 is present but unused. The RTX 3080 Ti, driver 615.71.09, KDE
Wayland/Xwayland desktop, and isolated HOME/XDG/temp environment are the same
as the earlier probes; no machine settings changed.

The documented command is `ngfx-replay --loop-count=1 --present-app CAPTURE`.
Expected: determine whether the initialization problem is specific to the
2026.3 tools or their capture/replayer version warning. Observed: file loading
completed, matching capture/replay version and build identities were printed,
and output stopped at `Initializing Replay:`. Unlike the 2026.3 logs, this
output contains neither the version warning nor a resource-creation percentage.
The outer 120-second deadline expired; signal 15 terminated the process and
owned-process cleanup was confirmed. No replay frame or timing result was
obtained. The complete probe, including discovery and cleanup, took about
124 seconds.

**Pinned failed bundle:** `bundle-19445f072a14e312debcda6a04be6ba9` under
`artifacts/nsight-evidence`. Its report, exact retained `raw/CppCaptureProbe.cpp`,
version/help output, and process logs preserve the attempt. Compile that source
as in I-006 and invoke `STORE INSTALLATION RETAINED_FIXTURE replay
CAPTURE_BUNDLE_ID`, using the 2026.2 installation and input capture above. The
input remained protected by a usage lease. The successful basic capture/export
matrix is separately pinned as `bundle-2af18aba623ad2e75b3834ba0980b2a8`.

Conclusion: a matching second release without the observed version warning
does not resolve GPU replay initialization on this setup. This does not identify
the cause or invalidate either release's successful metadata exports. Revisit
with documented Vulkan-loader diagnostics or an independently supervised replay
control that can distinguish environment/process-lifecycle effects from replay
initialization. Preserve matching tools, input capture, and cleanup evidence.

Source: the retained 2026.2 `ngfx-replay --help` and the
[capture/replay CLI documentation](https://docs.nvidia.com/nsight-graphics/UserGuide/graphics-capture-cli.html).

### I-012 — SDK capture harness compares different shader-provenance representations

Date: **2026-09-18**. State: **Resolved for this harness configuration**.
Related items: R-002/R-015. Product **0.2.2**, GCC **16.2.1 Debug**, matching
Nsight **2026.3.1.0/build 38722833** and SDK **0.9.2**, on the RTX 3080 Ti /
615.71.09 / KDE Wayland-Xwayland configuration recorded above.

Reproduction: the new C++ `ngm_capture_integration` harness selects the basic
workload with `--sdk-first-boundary-frame 6 --baseline-frame 7`. It requests
Graphics Capture API delimiter ordinal 2, seed 42, 192×128, and application
final frame 20. The exact argument array, executed binaries and harness source
are retained in `bundle-cf13ba74e9bb509e0125eb9507e74c48`.

Expected: validate the pre-call SDK report against the retained executable,
shader build and workload, then continue with fresh repeat and faulty captures.
Observed: the first capture job succeeds, all five exports succeed, and process
cleanup and artifact publication complete. The harness rejects the shader-bundle
comparison: the raw `shaders/provenance.json` has no `kind` field, while the
application's typed SDK context adds `kind: "shader_bundle"`. The harness exits
1 and skips the remaining captures. A fresh-context reviewer independently found
the same representation mismatch. This is a harness failure, not an SDK capture
or evidence-extraction failure.

Evidence, all persistently pinned under `artifacts/nsight-evidence`:

- Harness report and MCP transcript: `bundle-e3d274fd8982fb847b7580c1ba683b78`.
- Standalone frame-7 baseline: `bundle-a75d80b0eaa96dafbfc4db2ed6a6f034`.
- Successful SDK capture: `bundle-b1f6b4cc9835b14c8d45eaeab5e4e11c`.
- Exact failed harness source, inputs and command: `bundle-cf13ba74e9bb509e0125eb9507e74c48`.

Baseline and capture pins pass inspection after server restart; both server
sessions and report publication exit normally. The imported report bundle is a
complete copy of a failed validation report and does not imply validation passed.
The local report is
`build/sdk-validation/sdk-0.9.2-mcp/sdk-frame6/capture-validation-mNNrhm/report/report.json`.

Correction: add the expected typed `kind` tag before comparing the complete
shader manifest, matching the existing experiment runner's validation. The
fresh-context reviewer reports no remaining code findings after the correction.
The rebuilt harness passes the same SDK selection through MCP, including three
fresh captures, all five exports each, matching provenance and boundary reports,
distinct target PIDs, repeat/variant PNG checks, cleanup, and pins after restart.
Corrected report `bundle-5ae595db97eecc3d2d117e1a5123c31f` and baseline
`bundle-4f743867c23e9429d81e3e2ef2ec3bb5` are pinned, as are captures
`bundle-7a4f272db685aafccb5845031c5a40ca`,
`bundle-5964f875d33d9890fd2cb386eb2af1ea`, and
`bundle-56721d17ee8014faad7685808ea6ee50`.
Revisit if the application context or raw shader-manifest representation changes;
the harness still must compare their complete content after the explicit tag.

### I-013 — Matching 2026.2 C++ capture also fails during connection

Date: **2026-09-18**. State: **Open**. Related item: R-006; follow-up to I-006.
The documented [Generate C++ Capture
activity](https://docs.nvidia.com/nsight-graphics/UserGuide/generate-cpp-activity.html)
was tested with matching **2026.2.0.0/build 37991608** tools. The retained help
advertises the activity and every requested option. This probes a documented
source export, without parsing private capture formats.

The frozen **0.2.2** fixture has SHA-256
`cc4330e4e804b2a43c584635bd33eeff945dcaa0ed623707fab097b67c144563`, matching the
successful default-mode capture `bundle-f5e4dab66b22c440fdf230bd73eaf539` and
application baseline `bundle-06953401867d9a2a5f5b38cb73d2aad3`. Both references
remain pinned. It was built with SDK **0.9.0**, but this invocation requests no
SDK control. The references identify RTX 3080 Ti / driver 615.71.09 and KDE
Wayland through Xwayland/XCB. The probe copies and hashes its executable and
shader inputs, uses isolated HOME/XDG/temp directories, and preserves the system
data search path. No driver, desktop, or profiling configuration changed.

Reproduction: compile retained `raw/CppCaptureInspection.cpp` against the
project's C++20 headers and `ngm_capture`, `ngm_artifacts`, `ngm_nsight`, and
`ngm_core` archives. Its arguments are absolute store, matching installation,
frozen fixture, reference-capture ID, and baseline ID. `raw/report.json` retains
the complete launch arguments and input identities. The activity uses
`--wait-frames=2`, scenario `reference`, seed 42, 192×128, final application frame
120, and an explicit retained shader directory and fresh application output.

Expected: generated C++ resource/state/frame sources and associated data.
Observed: Nsight detects PID **144293** with status `Vulkan instance created`,
then repeatedly searches for the process before reporting its internal operation
timeout. It exits **1**; the outer 120-second deadline does **not** expire, and
there is no cancellation or process-runner error. Cleanup is confirmed. No C++
files, application result, or final readback are produced. The copied executable
hash is unchanged; application shader provenance is retained. These logs do not
identify the reason connection stops after the initial observation.

**Pinned failed bundle:** `bundle-f2a49d27a1ea24b00c0ecb3b95bfb574` under
`artifacts/nsight-evidence`, with `raw/report.json`, the exact probe source,
fixture/shaders, launcher/discovery logs, manifest, and persistent pin. It is a
normal failed bundle, not a cleanup quarantine. Fresh-context probe review
corrected the success guard before this run so timeout, cancellation, and process
errors cannot pass merely because source files exist.

Follow-up: a fresh run with documented CLI `--verbose` and target
`VK_LOADER_DEBUG=all` succeeds, exits zero, and confirms cleanup. It is pinned as
`bundle-cd9faf11bccbc42e3da46f1275b39073`. Its generated metadata records the
requested loader variable; captured launcher stderr does not contain loader
diagnostics. A subsequent run with neither diagnostic option also succeeds,
pinned as `bundle-31416cc9a35fef45aa1ef5a828473393`. Both preserve the same
executable/shader inputs, contain generated C++/data, and have screenshots that
match the retained frame-2 baseline exactly in decoded RGB. The successful
follow-ups establish a working reference extraction path; they do not identify
the first attempt's failure cause or prove that logging fixed it.

[INSPECTION.md](INSPECTION.md#generated-c-reference-evidence-on-20262) records
source associations, selected resource extraction through the generated helper,
and remaining qualification. The current host reports `vm.nr_hugepages=128`;
an existing `/etc/profile.d/hugepages.sh` writes this value during login-shell
startup. Subsequent orchestration uses non-login shells. No before/after kernel
setting evidence establishes any relationship to the capture results.

Conclusion: keep this failed attempt and its cause unresolved, alongside the
successful follow-ups. Revisit connection diagnostics if the failure recurs
under a controlled run; qualify correct/faulty pairs and other workloads/releases
before generalizing the demonstrated source path. Ordinary Graphics Capture
remains independently qualified. [Khronos loader logging
documentation](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDebugging.md)
describes the diagnostic variable used here.

### I-014 — Faulty shader C++ capture reaches the internal connection timeout

Date: **2026-09-18**. State: **Open**. Related item: R-006; follows I-013's
successful reference repeats. A fresh **2026.2.0.0/build 37991608** Generate C++
Capture attempt selects `shader-error` with the same frozen executable/shaders,
seed 42, 192×128, final application frame 120, and `--wait-frames=2`. The native
probe adds the reviewed `--verbose` / `--env=VK_LOADER_DEBUG=all` diagnostic
options; its exact source and arguments are retained. The executable hash remains
`cc4330e4e804b2a43c584635bd33eeff945dcaa0ed623707fab097b67c144563` before/after.
The qualified reference capture and baseline are the same pinned inputs as I-013.

Expected: a faulty member of the generated-source pair, allowing shader and
output comparison. Observed: the launcher reports `Vulkan instance created`,
then its internal operation timeout. Exit is **1**, with no outer timeout,
cancellation, or process-runner error; cleanup is confirmed. No C++ files are
produced. This attempt does not complete the correct/faulty extraction pair.

**Pinned failed bundle:** `bundle-02687bc350297f6dd2e51d39d2c2340e`. Its
`raw/CppCaptureScenarioProbe.cpp`, `raw/report.json`, frozen inputs, and
launcher/discovery logs reproduce the attempt. The native experiment accepts
only the basic `reference` / `shader-error` selectors and otherwise retains the
previous probe's lifecycle and success checks.

The successful ordinary reference attempt took roughly 31 seconds including
discovery/export, while this failed attempt took roughly 38 seconds. These
durations do not identify the connection failure's cause. The next probe tests
the launcher's documented suggestion, `--no-timeout`, while retaining the native
runner's independent **120-second** deadline and owned-process cleanup. That
separates Nsight's shorter internal connection deadline from the experiment's
bounded lifetime. The option is documented in the [CLI launch
reference](https://docs.nvidia.com/nsight-graphics/UserGuide/launch-application-overview.html#cli-arguments-details)
and the retained matching help. No machine setting change is needed.

### I-015 — Bounded C++ capture waits for a migration notice

Date: **2026-09-18**. State: **Open**. Related item: R-006; follows I-014.
The same 2026.2 `shader-error` attempt adds documented `--no-timeout`, retaining
the native runner's 120-second deadline. The frozen executable/shaders and all
workload inputs remain the same. Expected: distinguish a shorter internal
connection deadline from a target that cannot progress. Observed: the native
deadline expires, sends signal **15**, and confirms owned-process cleanup.
`exit_code` is null, `timed_out` is true, and no C++ source is produced.

During this owned invocation, read-only Linux process inspection finds fixture
PID **186237** sleeping in `do_wait` and its child **186270**, `zenity`. Reading
that child's command line after verifying its parent records the informational
title `Next-generation Graphics Capture Tools Available`. Its message explains
that Graphics Capture replaces the legacy Frame Debugger for D3D12/Vulkan and
explicitly documents **`NSIGHT_SUGGEST_GRAPHICS_CAPTURE=0`** to suppress this
notice. No window was clicked or automated. The separate observations at
06:45:01 and 06:46:18 UTC support the blocking-notice diagnosis; they are not a
simultaneous `waitpid` trace and do not retroactively establish the
cause of I-006/I-013/I-014, whose child state was not retained.

**Pinned failed capture probe:** `bundle-2e10c11bd0bd7f5378f32c300376b5e0`, with
`raw/CppCaptureNoTimeout.cpp`, full arguments, input identities, process outcome,
and launcher/discovery logs. **Pinned process-observation snapshot:**
`bundle-57f831ab54a3e2dde0c588f9ab8413b8`, with
`raw/imported/process-snapshot.json` and
`raw/imported/owned-dialog-command.json`, including timestamps, exact observed
PIDs/parent, and the message text. The native importer reports product 0.2.2.

The fresh follow-up passes the message-documented suppression variable through
`ngfx --env` and succeeds in roughly 9.5 seconds including discovery/export,
with exit zero and cleanup confirmed. The generated faulty capture is pinned as
`bundle-4625a12836a154589f2f03849a5fa361`. This supplies the faulty source export;
image and shader comparisons are separate qualification steps. The native
deadline and cleanup remain unchanged. This is a per-launch Nsight setting, not a driver/desktop
configuration change or GUI interaction. No matching public web documentation
was found for the variable in the bounded NVIDIA-source search; its authority
here is the exact tool-generated runtime instruction retained above. Qualify
the resulting behavior on each tested release before using it in a backend.

Subsequent 0.2.2 experiments succeed on the basic correct/faulty pair on both
matching releases. Independent image and shader comparisons are pinned as
`bundle-4d0ca74d88003db698ae2dca0b952e11`; [INSPECTION.md](INSPECTION.md) identifies
all source and resource bundles. These results qualify the per-launch suppression
for those experiments without assigning causes to earlier unobserved failures.
The production 0.2.3 mode uses the same suppression without diagnostic environment
variables; its separate acceptance results are in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).

### I-016 — Product C++ capture omitted the required output directory

Date: **2026-09-18**. State: **Resolved**. Related items: R-006, R-015.
The first 0.2.3 `capture_cpp` hardware run used matching Nsight
**2026.3.1.0/build 38722833**, GCC 16.2.1 Debug, and the same RTX 3080 Ti /
615.71.09 / KDE Wayland-Xwayland configuration as I-015. No SDK was used.
The documented Generate C++ Capture activity received `--wait-frames=2`,
`--no-timeout`, and the runtime-documented migration-notice suppression.
The fresh reference target used seed 42, 192×128, and final frame 120.

Expected: a generated source project. Observed: launcher exit **1** after about
5.2 seconds, with `No such output directory` and `Invalid general options`.
Cleanup was confirmed and the GPU reservation released. The backend rejected
an existing output path but failed to create the fresh directory before launch;
the earlier successful experiment explicitly created it. This is an adapter
error, not an unavailable Nsight capability.

**Pinned failed attempt:** `bundle-18972888569f4911619fb90616c78711` retains
`raw/report.json` with exact argv/environment/tool identities and launcher logs.
**Pinned harness/input/transcript snapshot:**
`bundle-b579b128bedb15d655cb8a1f7ff6b7fc`, containing the frozen server, fixture,
shaders, runner source, report, and MCP transcript. The fixture SHA-256 is
`0cc871be97972deee89e335e3c86063b1e6a79227516fbc0c17f9c2b326ebc11`.
Reproduction uses `ngm_cpp_capture_integration` with server, fixture, shader,
Nsight installation, artifact, and isolated run paths followed by
`reference shader-error`; the exact local invocation is retained in
`build/cpp-mcp-validation/nsight-2026.3/invocation.json`.

Correction: exclusively create the output directory after rejecting pre-existing
paths, and make the CPU stand-in require it. Revisit immediately with the corrected
backend through the same MCP harness, then qualify the second matching release.
The corrected 2026.3 run passes all three fresh captures, generated-source reads,
screenshot repeat/difference, and pins/indexes after restart. Its report, frozen
inputs, runner, and transcripts are pinned as
`bundle-d2579d201899eba43073dfdcecf2387f`; the exact successful captures are listed
in [NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md). This resolves the directory error.

### I-017 — Opaque descriptor bytes invalidated a cross-capture comparison assumption

Date: **2026-09-18**. State: **Resolved comparison error; fixed setup descriptor semantics qualified at 0.2.7**.
Related items: R-006, R-007. The product 0.2.3 combined reference/fault C++ captures
succeed on matching 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608 tools,
on the same RTX 3080 Ti / 615.71.09 / KDE Wayland-Xwayland / GCC 16.2.1 host.
Each uses seed 42, 192×128, wait_frames 2, and no SDK.
The documented generated `ReadOnlyDatabase`/`DataScope` helper successfully
extracts thirteen explicitly source-referenced blocks per capture. Four native
readers enforce exact capture/producer/helper/database identities before reading.

The initial independent comparison incorrectly expected only handle19 (postpass
push constant) to differ. It failed its `changed==[19]` assertion: **handles16,
17, and19 differ on both releases**. Handles16/17 are opaque serialized descriptor
writes (176/88 bytes). Byte differences across captures do not establish different
logical descriptor bindings, and no private decoder was attempted. The failure
is an overstrong comparison assumption, not a failed capture or resource read.

Pinned comparison snapshot **`bundle-79ab7d070ca805b5f0284066f7c0e7c5`** retains
`raw/imported/compare-initial-failed.py`, `initial-failure.txt`, corrected
`compare.py` / `report.json`, all four reader source/build argv/log/receipt sets,
and a fresh `nvidia-smi` GPU/driver observation. Capture reports/inputs are pinned
as `bundle-90cc2a26b7c8d4f0dda20721baf32ce8` (2026.3) and
`bundle-e7196d4bdbb9519a4c1872bf44840b8a` (2026.2). Resource outputs are pinned as
`bundle-aa4a38db5c92874565fb1f012e5bd5ce`,
`bundle-45d9b15ae9b8b7c34e1890878457c179`,
`bundle-c5a443e432cee3eb2f4c59cec48b71b4`, and
`bundle-56e9ef3c57706b145d64d0013a2244af` (reference/fault in release order).

Reproduce the initial assertion with the retained initial script and its original
local input layout, or compare the resource paths/hashes named in the retained
reports. The corrected comparison passes, separates opaque descriptor differences
from public typed payloads, verifies exact source-declared lengths, and matches
all sixteen extracted shaders to frozen application SPIR-V. Of the eleven
non-descriptor blocks, only postpass push value `channel_order` differs (0 versus
1). Matched GLSL shows a red/blue swap when nonzero. This supports that diagnosis;
it does not rule out additional descriptor-state differences or establish a fix.
Revisit descriptor interpretation with a demonstrated generated-helper calling
contract; raw-byte similarity/difference is insufficient. Preserve the unknown
array-element mapping until that path is actually qualified. Final snapshot
`bundle-cb069965dca0cc90e2b672194195beb5` includes the same verified comparison
with “postpass push constant” wording, clarifying that the update precedes its draw;
the original comparison/failure snapshot remains pinned.

Follow-up at **0.2.7**: the fixed-capture generated-helper runner now resolves
all three setup descriptor writes in each of the four exact captures. Palette
array slots 0/1 map to buffers32/34 at offset0/range16; the postpass write uses
view27/sampler29 with shader-read-only layout. Those typed fields agree across
reference/fault despite different serialized bytes. This resolves the setup-field
ambiguity without explaining the opaque differences or claiming executed slot
selection. See [DESCRIPTOR_HYDRATION.md](DESCRIPTOR_HYDRATION.md) and its pinned
qualification record in BUILD_VALIDATION.md. Generic product extraction remains
unfinished.

### I-018 — Generated-source qualification rejected the resource progress message

Date: 2026-09-18. State: **Resolved**. Related: R-007/R-015.
The first 0.2.6 retained MCP association probe expected a source relationship for
basic reference `bundle-10af9199c5194f650980d5d896ed07bf`, produced by matching
Nsight 2026.3.1.0/build 38722833 on RTX 3080 Ti / driver 615.71.09, KDE
Wayland/Xwayland. This attempt read existing documented Generate C++ Capture
source; it invoked no Nsight process, SDK or GPU work.

Reproduction used `ngm_retained_cpp_integration SERVER artifacts/nsight-repair-evidence
build/cpp-query-validation/nsight-repair-evidence-cases.json
build/cpp-query-validation/repair`, with the absolute GCC Debug server/harness
identified by SHA-256 in the run report. Run
`build/cpp-query-validation/repair/cpp-inspection-QWancs` exited 1. The MCP tool
returned draw 18 and its explicit bind to pipeline 38, but no pipeline association,
and six unsupported-object records. A diagnostic pure-parser read identified
`resource setup parent contains unsupported direct statements`. The source parent
contains the generated `NV_MESSAGE_VERBOSE("literal")` progress statement, which
the initial narrow parent grammar omitted.

The correction accepts exactly that literal logging form, retaining rejection of
unknown executable statements and helpers. A synthetic regression and successful
final retained MCP runs establish resolution; this did not require a different
capture backend or relaxed creation checks. Revisit if a new producer introduces
a different progress expression. The failed report/transcript, diagnostic outputs,
and final qualification are explicitly pinned in **bundle-ce6ae6385721977dfd7091469cb57888** in
`artifacts/nsight-repair-evidence`. This is a product grammar limitation, not a
finding that Nsight lacks the source relationship.

### I-019 — Nested platform setup scopes failed resource-statement qualification

Date: 2026-09-18. State: **Resolved**. Related: I-018, R-007/R-015.
After adding stricter sibling-block checks and the literal progress-message form,
the same retained MCP command/profile/input exited 1 in
`build/cpp-query-validation/repair/cpp-inspection-slupXt`. It still returned the
literal draw/bind but refused the object association. The diagnostic parser
reported `unterminated resource setup statement`: unrelated window-system setup
contains nested anonymous scopes under platform preprocessor branches, which do
not end in a statement semicolon.

The correction recursively qualifies those scopes, preserving the same helper,
statement, and handle-reference restrictions at every level. Relevant conditional
creation definitions remain unsupported. A nested-platform regression and the
final 36-capture query matrix pass. No GPU or capture was rerun. Failed MCP and
diagnostic records are explicitly pinned with the final results in
**bundle-ce6ae6385721977dfd7091469cb57888** in `artifacts/nsight-repair-evidence`.
Revisit when a generated nested scope contains a new supported setup construct;
do not treat arbitrary nested code as safe simply because unrelated setup exists.

### I-020 — Native XCB handle expressions exceeded the generated helper grammar

Date: 2026-09-18. State: **Resolved**. Related: I-019, R-007/R-015.
A pure-parser retry after nested-scope support, using
`build/capture-acceptance/cpp-parser-probe-review7` on the same basic reference
bundle, returned draw/bind evidence but unavailable pipeline association.
`build/cpp-query-validation/repair/sibling-probe-reason3.json` records
`unrecognized helper in resource sibling: ,`. The generated XCB surface initializer
uses grouped pointer dereferences of `reinterpret_cast<xcb_connection_t**>` and
`reinterpret_cast<xcb_window_t*>` over the generated window-system accessors.
The scanner treated the preceding comma as an unknown function-call prefix.

The correction accepts the two exact observed native-handle expressions, with a
regression rejecting a substituted unknown accessor. It does not allow arbitrary
casts, calls, or assignments. The corrected pure parser resolves 48 draws in all
36 retained projects from the two existing producer profiles; final MCP queries,
source hashes, pagination, and restart checks also pass. This was a retained
source-analysis retry, not a new capture or GPU run. Diagnostic output, input
references, test sources, and successful follow-up records are explicitly pinned
in **bundle-ce6ae6385721977dfd7091469cb57888** in `artifacts/nsight-repair-evidence`.
Revisit for another documented generated window-system form after recording its
actual source; this result does not qualify other window systems or platforms.

## Entry template

Use the next unused I-### ID. An unsuccessful retry gets a new entry linked to the
earlier attempt so changes in method or environment remain visible.

```text
ID: I-001
Date:
State: Open | Resolved | Superseded
Related roadmap items and attempts:
Capability sought:
Documented interface and source:
Environment: Nsight/SDK versions, GPU/driver, and relevant platform details.
Workload: Scenario, source/build identity, settings, and input artifacts.
Reproduction: Exact command arguments or minimal SDK example and prerequisites.
Expected result:
Observed result: Exit status, error, or missing data.
Evidence: Artifact locations and a small sanitized excerpt where useful.
Retention: Explicitly pinned evidence-bundle IDs, or unavailable evidence and why.
Conclusion: Supported finding and remaining uncertainty.
Current limitation:
Revisit condition:
Resolution: Leave open until supported by a linked successful follow-up.
```

### I-021 — Descriptor probe attempted an unsupported BlobProxy conversion

Date: **2026-09-18**. State: **Resolved probe compilation error**. Related: R-007.
The first `ngm_descriptor_hydration_integration` run selected the pinned combined
reference capture `bundle-938942909c8c6fffa39c4f414a09d757` from matching Nsight
2026.3.1.0/build38722833. The runner copied its exact generated helper/database
inputs and invoked GCC with ASan/UBSan to compile the native worker. Expected:
compile and inspect two source-referenced descriptor-write blocks. Observed:
compiler exit1 at `require(bool(resource), ...)`; NVIDIA's `BlobProxy` has no
conversion to bool. No helper execution, resource read, or GPU operation occurred.

The isolated failed run is
`build/descriptor-hydration-validation/descriptor-hydration-3u6vjs`.
Its worker source, exact compiler arguments, stderr, process cleanup result, and
fingerprinted inputs are retained in explicitly pinned qualification bundle
`bundle-93e2c3dd09c57553642b1afa873e8566` in `artifacts/nsight-repair-evidence`,
under `raw/imported/failed-I021/`; see [BUILD_VALIDATION.md](BUILD_VALIDATION.md). Reproduce by compiling that retained
worker against its identified helper closure. This was a first-party API-use
error, not an unavailable Nsight capability. The corrected worker checks
`resource.Get() != nullptr`; the subsequent four-capture run succeeds. Revisit
only if a different generated helper changes its resource-access contract.

### I-022 — Diagnosis capture omitted the fixture output option

- **Context:** 2026-09-18, product 0.2.8, matching Nsight 2026.3.1.0/build
  38722833, original fixture SHA-256
  `e6ced467def8f7df41d0c9a1b2ff2a60a52d717a6b8462d402bd05041597d302`.
- **Reproduction:** `build/state-repair-validation/diagnosis/reference.argv.json`
  records the exact `ngm-capture --format cpp --wait-frames 2` invocation,
  selecting reference/seed42/192x128/frame120 and the frozen shader directory.
  It omitted `--application-output-option --output`; this fixture requires
  an explicit output directory.
- **Expected/observed:** Expected generated capture; target exited before attachment,
  and Nsight reported “Failed to connect. The target process may have exited.”
  CLI exit 1, no outer timeout, cleanup confirmed and GPU reservation released.
  No generated project was published as complete.
- **Evidence/retention:** Failed bundle `bundle-d98ea3f9dc96d1fa334898c880883439`
  is explicitly pinned in `artifacts/nsight-state-repair-evidence`, including
  raw report, argv and tool stdout/stderr. The omitted required option is
  established by the invocation and fixture contract; Nsight's generic connection
  message alone does not establish the target's exit reason.
- **Next action/revisit:** Repeat with the fixture output option supplied. Revisit
  Nsight connection behavior only if the corrected invocation also fails. The
  corrected reference/binding/pipeline attempts all passed; their pinned IDs are
  in [STATE_REPAIR.md](STATE_REPAIR.md). This failed invocation is not evidence
  that the scenario is unsupported.

### I-023 — Source-repair regression check required the wrong scene label

- **Context:** 2026-09-18, development product0.2.9, matching
  Nsight2026.3.1.0/build38722833, existing combined postpass repair inputs.
- **Reproduction:** After the four binding/pipeline repair runs passed,
  `build/state-repair-validation/matrix/2026.3-combined-pass-error/invocation.json`
  records a regression invocation of `ngm_source_repair_integration`.
- **Expected/observed:** Expected three passing captures. The reference capture
  itself succeeded and cleanup completed, but the newly added source-label
  assertion required `scene.raster`. The generated source correctly labels this
  multipass scene `scene.offscreen`, followed by `post.present`. The harness
  exited1 before submitting the faulty and repaired captures.
- **Evidence/retention:** Capture `bundle-f65dcf53cb735b2cddfbe64f00010b2e` and
  failed qualification report `bundle-cd74be68c7973c40e882599e88812682` are pinned
  in `artifacts/nsight-state-repair-evidence`. The report contains the frozen
  pre-correction harness source, transcript, inputs, and independent baselines.
  Three baseline IDs are in that report and are pinned.
- **Correction/revisit:** Select `scene.offscreen` for multipass profiles and
  `scene.raster` for basic profiles. Rerun the final binding/pipeline matrix and
  both postpass regressions. Revisit if the corrected label check fails on these
  same qualified scenarios; this was a harness failure, not failed Nsight capture.
  The final four binding/pipeline runs and both postpass regressions all passed
  with the corrected harness (18 fresh captures); [STATE_REPAIR.md](STATE_REPAIR.md)
  records the final report bundles.

### I-024 — Fixed resource experiment used implicit JSON string comparisons

- **Context:** 2026-09-18, development product0.2.10, GCC16.2.1, pinned JSON
  dependency; diagnosis capture `bundle-9854d553e16d9c48997d1f2f7224577b` from
  Nsight2026.3.1.0/build38722833, retained and pinned in
  `artifacts/nsight-advanced-repair-evidence`.
- **Reproduction:** `build/advanced-repair-validation/diagnosis/`
  `extraction-bindless-reference/build.argv.json` records compilation of the
  fixed-input worker and unchanged generated ReadOnlyDatabase/DataScope helpers.
- **Expected/observed:** Expected a compiled worker to read source-referenced
  resources. Compilation exited1: the pinned JSON configuration has no matching
  `std::string == json` overload for fingerprint and source-line comparisons.
  The worker was never executed; no resource extraction or GPU work occurred.
- **Evidence/retention:** The failed directory preserves source, exact helper/data
  inventory and snapshots, compiler arguments/result, and full diagnostic log.
  It is included in pinned advanced repair qualification snapshot
  `bundle-1e6ebc236bc4232d22303d0be4ca3583` in the same store.
- **Correction/revisit:** Use explicit `get<std::string>()` at both comparisons
  and compile in a separate output directory. Revisit compiler compatibility if
  the explicit typed comparisons still fail; this error establishes no Nsight
  data limitation.
  The corrected seven workers and independent reruns passed with empty
  sanitizer diagnostics, extracting 33 source-referenced resources.

### I-025 — Truncated copied databases crash the generated reader inside confinement

- **Context:** Development product 0.2.11, GCC 16.2.1, exact unchanged reader
  closures from Nsight 2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608.
  This is a deliberate corruption test of retained copies, not a failed capture
  of an application or evidence that either release cannot read valid data.
- **Reproduction:** `build/resource-worker-validation/validate.py` copies a
  qualified generated database and record file, replaces only `data.bin` with
  one byte, and requests a source-declared shader resource through the compiled
  worker. Each `negative-*-truncated-data/attempt.argv.json` records exact inputs;
  the C++ `RunWorker.cpp` driver uses `run_process`, empty environment, regular
  output logs and a ten-second wall deadline.
- **Expected/observed:** The corrupt data must not escape the worker or produce
  accepted evidence. Both generated readers terminate with SIGSEGV (11), with
  confirmed cleanup, no timeout, and bounded output. The wrapper cannot convert
  every malformed database into a normal library error. CPU/file/address-space
  limits, Landlock and seccomp were installed before reader construction/Init.
- **Evidence/retention:** The qualification records preserve the copied malformed
  inputs, worker identities, argv, stdout/stderr and process results. Final pinned
  snapshot `bundle-78b10c057ee352241bb5eb276b22ff11` is pinned in
  `artifacts/nsight-resource-worker-evidence`; BUILD_VALIDATION.md records the
  complete qualification scope.
  The same matrix records expected refusals for wrong sizes/offsets/handles,
  empty/short/corrupt record files, missing/symlink data and excessive record size.
- **Correction/revisit:** Keep all abnormal terminations as explicit extraction
  failures. The pending service must validate the bounded response only after
  successful exit and confirmed cleanup, and must retain failure diagnostics.
  Do not modify or reverse-engineer the vendor format to mask this result. Revisit
  on a changed helper profile or if a malformed-input run defeats the process
  boundary or returns evidence that the parent incorrectly accepts.
