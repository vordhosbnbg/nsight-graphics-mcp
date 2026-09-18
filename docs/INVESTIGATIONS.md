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
