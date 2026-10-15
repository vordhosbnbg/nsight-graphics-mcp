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

### I-026 — Resource MCP qualification included pre-product probe bundles

- **Context:** 0.2.12 working tree, GCC 16.2.1 Debug; retained inputs from Nsight
  2026.3.1.0/build 38722833 and 2026.2.0.0/build 37991608. No new GPU execution.
- **Attempt:** Run `ngm_retained_resource_integration` with the 35 `nsight-evidence`
  cases from the 0.2.11 worker-only qualification matrix. The exact cases and
  transcript are retained at
  `build/resource-query-validation/retained/resource-inspection-NJ2p1r/`.
- **Expected:** Resource-reference listing and byte reads for every selected case.
- **Observed:** Six cases and 72 reads completed, then listing
  `bundle-31416cc9a35fef45aa1ef5a828473393` returned `not_capture`. Its manifest
  origin is `documented_nsight_cpp_capture_probe`, not `nsight_cpp_capture`.
  Four other selected cases have the same historical provenance class.
- **Cause/correction:** The worker-only matrix accepted raw database inputs and
  did not qualify the parent capture/report contract. The product correctly
  rejects these experimental bundles. The retained MCP runner now checks those
  five expected refusals explicitly and keeps all 37 server-produced captures
  across the two source stores as positive byte-query cases. No provenance gate
  was relaxed and no historical manifest was rewritten.
- **Evidence retention:** Source captures and completed read attempts are pinned.
  The failed qualification directory is included under `raw/imported/retained/`
  in pinned resource-query qualification `bundle-6365322f8eb3922893e1d2256983f732`
  in `artifacts/nsight-resource-query-evidence`. All source/read references and
  persistent pins verify after publication.
- **Revisit:** If support for imported or historical probe captures is accepted,
  define and validate their provenance contract explicitly before allowing typed
  capture queries. This failure establishes no limitation in the reader helpers.

### I-027 — Compute probe used output extensions rejected by the adapter

- **Context:** R-008 investigation on 0.3.0, GCC 16.2.1 Debug, matching Nsight
  2026.3.1.0/build 38722833. The private C++ `ProbeRunner.cpp` uses the existing
  Nsight adapter and process supervisor, with a deterministic no-presentation
  Vulkan compute application. No product interface changed.
- **Reproduction:** `probe-runner graphics NSIGHT_ROOT compute-index-error.spv
  NEW_DIRECTORY 120`, using the recorded absolute arguments in
  `build/compute-investigation/graphics-index-error-2026.3/request.json` and the
  following `graphics-index-error-2026.3-v2` attempt.
- **Expected/observed:** The first runner requested `capture.ngfx-bincap`; the
  adapter requires `.ngfx-capture` and returned `invalid_input` before launching
  the application. The corrected second run captured successfully with
  `--delimiter-vk-frame-boundary-ext`, and metadata/functions/objects/logs exported.
  Its screenshot request used `.image`, which the adapter rejected before launch
  because that export requires `.png`. Neither refusal establishes an Nsight
  compute or screenshot limitation.
- **Correction:** Preserve both exact runner revisions/binaries and reports;
  use the adapter's validated output suffixes. The third runner uses `.png`.
  GPU/application cleanup for the actual capture and exports was confirmed.
- **Evidence/retention:** Working evidence and versioned runner sources/binaries
  are retained under `build/compute-investigation`; managed pinning is pending
  publication of this investigation snapshot. These paths alone are not pins.
- **Revisit:** If the adapter's output contract changes, rerun its CPU checks and
  the corresponding integration. Inspect a correctly formed export before making
  a claim about evidence availability for a no-presentation workload.

### I-028 — C++ capture activity does not expose the probe's frame boundary

- **Context:** 0.3.0 R-008 private compute probe, Nsight 2026.3.1.0/build 38722833,
  RTX 3080 Ti / driver 615.71.09, existing KDE Wayland/Xwayland session. The same
  probe successfully creates a graphics capture using `VK_EXT_frame_boundary`.
- **Reproduction:** `probe-runner cpp NSIGHT_ROOT compute-index-error.spv
  NEW_DIRECTORY 120`; exact arguments and process results are in
  `build/compute-investigation/cpp-index-error-2026.3/`. This invokes documented
  `ngfx --activity=Generate C++ Capture --wait-frames=2` through the existing adapter.
- **Expected/observed:** A generated compute project would permit shader and
  serialized-resource inspection. Instead the target reports
  `VK_EXT_frame_boundary extension/feature unavailable`, exits before dispatch,
  and ngfx fails to connect (exit 1). Cleanup is confirmed; no generated project
  is accepted. This is a capability observation for this activity/profile, not
  proof that every C++ capture route or future release lacks compute support.
- **Boundary:** The documented graphics-capture delimiter option cannot simply
  be applied to the separate C++ activity. No unsupported option, hidden present,
  private API or reverse-engineered capture decoder is substituted. For the
  measured no-presentation graphics path, metadata inventories supply dispatch
  records and named objects but no shader bytes or per-event resource contents.
  Source-available application observations/readback must remain separately
  labeled if used for diagnosis.
- **Evidence/retention:** Working sources, binary identities, application failure,
  exact tool help, stdout/stderr and report are under `build/compute-investigation`;
  managed pinning is pending publication of the investigation snapshot.
- **Revisit:** A documented non-presentation C++ activity delimiter or a documented
  shader/resource export for graphics captures, or a new qualified tool release,
  would justify repeating this probe. This attempt alone does not close R-008.

### I-029 — No-presentation compute capture has no exported screenshot

- **Context:** R-008 private reference probe, product 0.3.0, Nsight
  2026.3.1.0/build 38722833, RTX 3080 Ti / driver 615.71.09.
- **Reproduction:** The corrected `probe-runner graphics` invocation in
  `build/compute-investigation/graphics-reference-2026.3/request.json`, followed
  by matching `ngfx-replay --metadata-screenshot OUTPUT.png CAPTURE`.
- **Expected/observed:** Capture and metadata/functions/objects/logs exports
  succeed. The correctly named screenshot export exits 1, produces no PNG and
  emits no stdout/stderr diagnostic. Owned-process cleanup is confirmed. This
  buffer-only workload creates no image or presentation surface; the observation
  establishes only that this run did not supply a screenshot.
- **Evidence/retention:** Exact request, tool observations, process reports and
  logs remain in the working directory above; managed snapshot pinning is pending.
- **Revisit:** A documented screenshot contract for buffer-only captures, a
  different qualified release, or an image-producing compute workload. Numerical
  application readback is a separate evidence origin, not an Nsight screenshot.

### I-030 — Non-END annotations split the measured compute capture interval

- **Context:** R-008 production fixture integration, uncommitted 0.3.0 build,
  Nsight 2026.3.1.0/build 38722833, RTX 3080 Ti / driver 615.71.09.
- **Reproduction:** Exact native capture invocation/report is retained in
  `build/compute-investigation/product-reference-capture-2026.3.json`; delimiter
  `vk_frame_boundary`, capture ordinal 2, compute-reference, seed 42, 33x35,
  application last frame 120. Fixture annotates its compute submission with
  flags 0, then its empty end submission with FRAME_END for the same frame ID.
- **Expected/observed:** Intended interval includes dispatch, completed numerical
  readback, and the ending submission. Capture succeeds, with one dispatch, but
  the target terminates before frame-1 readback; only frame-0 readback is retained.
  The observed delimiter behavior counts the non-END annotation too. Exported
  metadata does not contain an authoritative application-frame join, so the
  retained readback cannot be asserted to represent that captured dispatch.
- **Correction:** Use one annotation per application frame, solely on the empty
  ending submission after publishing readback. Verify actual retained frames and
  dispatch inventories again; do not infer correlation from equal numeric IDs.
- **Evidence/retention:** Complete pinned bundle
  `bundle-3241af7d69c2309b17cccc95631d2a3d` in `artifacts/compute-evidence` retains
  the capture, reports, inventories, setup, shader identities and readback. The
  source/build identity names the exact implementation before this correction.
- **Revisit:** A changed documented delimiter contract or tool version warrants
  retesting multi-submission annotations. This is an interval qualification
  failure, not a failed capture or a general Vulkan extension limitation.

### I-031 — 2026.2 graphics injection does not expose the compute boundary

- **Context:** Corrected one-END-per-frame compute fixture, uncommitted 0.3.0
  build, Nsight 2026.2.0.0/build 37991608, RTX 3080 Ti / driver 615.71.09.
- **Reproduction:** Native `ngm-capture` with the 2026.2 installation override,
  `--delimiter vk_frame_boundary --capture-frame 2`, compute-reference, seed 42,
  33x35, application last frame 120. Exact command and producer observations are
  in the retained attempt report.
- **Expected/observed:** CLI help advertises the delimiter, but the injected
  application finds no Vulkan 1.3 compute device with both the boundary extension
  and feature. It writes an unsupported result; capture exits 255 without an
  output capture. Cleanup is confirmed. The corresponding corrected 2026.3 run
  succeeds and retains application frames 0..2. Help support alone does not
  establish this application's runtime prerequisite on 2026.2.
- **Evidence/retention:** Failed pinned bundle
  `bundle-4037f487c6d761910dd4b26028869468` in `artifacts/compute-evidence`;
  successful corrected 2026.3 bundle
  `bundle-1a3778da489d8ddd8f09e8099946db65` in the same store. Both preserve
  executable/shader identities and tool/process observations.
- **Revisit:** A documented way to expose the extension/feature in 2026.2, a
  separately validated SDK compute boundary, or a changed tool/GPU/driver profile.
  This does not invalidate the separately qualified 2026.2 visual workflows or
  prove every non-presentation compute capture route unavailable.

R-008 retention update (2026-09-18): working probe evidence cited by I-027,
I-028 and I-029 is now included under `raw/imported/investigation` in complete,
pinned `bundle-1ed0b5e17d49e24b6333d6ae4b10ffc1`, store
`artifacts/compute-evidence`. All snapshot payload hashes and its pin after
restart verify. I-030's exact earlier Compute.cpp is reconstructed and verified
against the captured build-input hash in `investigation/I030-Compute.cpp`.
I-031's additional real MCP unsupported result is retained in failed pinned
`bundle-c9f0d80ba38641d2106f390558a4967c`; its report and transcript are in the
snapshot. The native attempt pins above remain independent and protected.

### I-032 — GPU Trace probe's separated platform option reaches Qt

- **Context:** R-009 initial interface probe, product fixture 0.3.1, Nsight
  2026.3.1.0/build 38722833, RTX 3080 Ti / driver 615.71.09.
- **Reproduction:** The private C++ runner
  `build/performance-investigation/TraceProbe-v1.cpp` passes the documented
  platform name as two argv entries: `--platform`, `Linux (x86_64)`, alongside
  GPU Trace Profiler, automatic export and unchanged clocks. The request is in
  `build/performance-investigation/trace-2026.3/request.json`.
- **Expected/observed:** Expected a GPU Trace prerequisite check/trace. Qt instead
  treats the platform value as a Qt plugin name and aborts initialization; the
  crash reporter remains until the owned-process deadline. No profiling export
  is produced. The corrected probe uses `--platform=Linux (x86_64)`, matching the
  already qualified C++ capture adapter. This failure does not establish anything
  about performance-counter availability.
- **Evidence/retention:** Working logs and request are in the directory above;
  managed retention will be recorded with the corrected probe result.
- **Revisit:** A changed documented command-line parser or Nsight/Qt release.
  Keep the joined platform form in backend argument construction.

### I-033 — GPU Trace cannot access performance counters as the current user

- **Context:** Nsight 2026.3.1.0/build 38722833, fixture 0.3.1, RTX 3080 Ti /
  driver 615.71.09, uid 1000 in the existing KDE Wayland/Xwayland session.
- **Reproduction:** Build/run the private `TraceProbe.cpp` C++ runner, using the
  joined platform option, `GPU Trace Profiler`, `--auto-export`, Throughput
  Metrics for Ampere GA10x, start after five presents, limit three frames/1000 ms,
  and `--set-gpu-clocks unaltered`. Exact argv/environment and executable hashes
  are retained in `trace-2026.3-v2/request.json`.
- **Expected/observed:** The fixture launches and presents; Nsight connects, then
  reports `GPU Performance Counters unavailable`. The tool exits 1, cleanup is
  confirmed, and the export directory is empty. `/proc/driver/nvidia/params`
  reports `RmProfilingAdminOnly: 1`; profiling capability minors 4324/4325/4326
  are root-owned mode 0400 without extended ACLs. This establishes a local
  prerequisite failure, not absence of a documented metric export interface.
- **Evidence/retention:** Working request, logs and report are under
  `build/performance-investigation/trace-2026.3-v2`. Managed snapshot reference
  follows below. No GPU/system permission or clock change was made.
- **Revisit:** Explicitly authorized per-user access through the installed R610+
  capability mechanism, or an administrator-provided profiling environment.
  The exact temporary grant/restore proposal is retained with the investigation.
  R-009 cannot claim successful profiling acceptance from this failed probe.

I-032/I-033 retention update (2026-09-18): both probe attempts, runner source and
binaries, exact requests, logs, cleanup reports, CLI help, system observations
and proposed permission changes are retained in complete/pinned
`bundle-8d30dab0cf976915a34adff7b483ad10`, store `artifacts/performance-evidence`.

I-033 follow-up (2026-09-25): the owner approved the temporary per-user access
proposal. The capability nodes were absent in the new session; the documented
`nvidia-modprobe -f` procedure recreated them with root-only defaults. Three
bounded batches granted only the approved user access and restored exact ACL/
modify settings afterward. The corrected basic 2026.3 trace and private compute
variant pairs on both matching releases all exit 0, confirm cleanup and export
metrics. This resolves the permission prerequisite for those runs. It does not
complete R-009 or qualify a product profiling API. [PERFORMANCE.md](PERFORMANCE.md)
records the commands, observations, preliminary workload and retention reference.

### I-034 — Pinned fastmcpp HTTP client compatibility edges

- **Context:** R-004 HTTP adapter at 0.3.2, fastmcpp 3.4.7.1 at the existing pinned
  source revision, its StreamableHttpTransport/Client, and the real local server.
- **Reproduction:** The initial `ngm_http_check` uses `Client::initialize`,
  `list_tools`, and `call_tool("capabilities", {})` against the authenticated
  ephemeral loopback listener. Failed logs are retained in
  `build/http-review/client-{duplicate-header,nullable-schema}-failure.log`.
- **Expected/observed:** Initialization first fails with HTTP 400 because the
  transport explicitly supplies Content-Type and cpp-httplib appends another.
  The adapter now accepts equivalent JSON media declarations but still rejects
  conflicts. After initialization/discovery succeed, high-level result coercion
  throws a JSON type error on valid nullable schema type arrays. The public
  `Client::call` raw-result API works without that optional coercion; tests use it.
  Tool schemas remain unchanged. Actual Codex CLI HTTP calls also succeed.
- **Evidence/retention:** Failed logs, reviewed corrections, passing HTTP check,
  Codex transcript and full aggregate are included in R-004's managed qualification
  snapshot referenced by HTTP.md. No claim is made that fastmcpp's high-level
  typed result conversion works for this tool surface.
- **Revisit:** A future pinned client revision handling nullable schema types and
  duplicate media headers; retest high-level conversion before claiming support.

### I-035 — Offline profile query probe supplied a nonexistent tool root

- **Context:** 0.3.6 retained GPU Trace inspection on 2026-09-25, after successful
  normal-fixture profiles from matching 2026.3.1.0/build 38722833 and
  2026.2.0.0/build 37991608 tools. This failure precedes any Nsight invocation.
- **Reproduction:** The Python MCP probe initially starts the server with
  `--nsight-root build/no-live-backend-required` expanded to an absolute path,
  although that directory does not exist. Initialization receives EOF.
- **Expected/observed:** The probe intended to exercise offline retained queries;
  server argument validation instead correctly rejects a nonexistent explicit
  root with `--nsight-root requires an existing absolute directory`. No profile
  or extraction tool runs. This is a probe setup error, not a backend limitation.
- **Correction/evidence:** Use an existing empty installation directory. Actual
  `profile_metrics` and `artifact_read` calls then succeed on both retained
  profiles, and their persistent pins are verified. Failed initialization logs
  are `build/performance-repair/diagnosis/invalid-root.{stderr,jsonl}`; corrected
  requests/responses and source evidence are alongside them. The performance
  acceptance snapshot in PERFORMANCE.md retains these records.
- **Revisit:** Only if explicit-root startup validation or offline discovery
  policy changes; a missing directory should not be mistaken for unavailable
  profile inspection.

### I-036 — Repeated-profile harness copied a nonexecutable fixture

- **Context:** R-009 source-repair acceptance harness, server/fixture 0.3.6,
  intended Nsight 2026.3.1.0/build 38722833 batch, 2026-09-25.
- **Reproduction:** Python `shutil.copyfile` freezes the fixture bytes without
  executable permissions; the initial C++ batch's `filesystem::copy_file`
  preserves that 0644 mode in its fresh target. Submit `profile` for that target.
- **Expected/observed:** The harness expected a new profiling job. The server
  correctly returns `invalid_argument: Application executable must be an existing
  executable regular file` before allocating a profile job or invoking Nsight.
  No trace is produced. The access wrapper restores the exact baseline ACL and
  capability-modify settings on this failed batch too.
- **Correction/evidence:** The harness explicitly gives its newly copied target
  owner executable permissions. The failed request/response, original harness
  source, target bytes/mode record and restoration journal are under
  `build/performance-repair/failed-attempts`. Subsequent corrected batch results
  are recorded separately in PERFORMANCE.md and its acceptance snapshot.
- **Revisit:** If snapshot/copy ownership changes, assert executable permissions
  before launch. This failure does not indicate a Nsight compatibility problem.

### I-037 — Retained reader validation copied evidence before store initialization

- **Context:** R-018 offline MCP resource qualification, source server/worker
  0.4.2, retained Nsight 2026.3.1.0/build 38722833 capture, 2026-10-15.
- **Reproduction:** Copy the intact source bundle into a new `store/bundles/`
  directory before opening that store through the server; run
  `ngm_retained_resource_integration` against it.
- **Expected/observed:** The probe expected source inspection. The server
  correctly refuses `Refusing to adopt nonempty unmarked artifact root` before
  resource extraction. No Nsight tool or GPU execution occurs.
- **Correction/evidence:** Initialize a separate empty `verified-store` through
  an actual MCP `artifact_list` call, close that process, then copy the intact
  producer-bound source bundle and its existing pin without editing manifests.
  Whole/split byte-oracle reads and restart checks then pass. Both transcripts
  and the failed/passing reports are pinned in
  `bundle-133f94fc5a57cc411bc5eb00a8f1a5b6` at the configured external artifact
  root; see RESOURCE_QUERIES.md for the independent source/read pins.
- **Revisit:** If the retained-validation setup changes, initialize the store
  before copying bundles. This is a probe setup error, not a reader limitation.

### I-038 — Matching-driver fixture baseline passes but current capture times out

- **Context:** Per-job environment implementation 0.4.3 on 2026-10-15;
  Nsight 2026.3.1.0/build 38722833, RTX 3080 Ti, kernel driver 615.71.09,
  KDE Wayland / XCB/Xwayland, GCC 16.2.1 and CMake 4.4.4. Default `nvidia-smi`
  fails with an NVML 615.78 / kernel-driver mismatch; a process-local
  `LD_LIBRARY_PATH` selecting the retained 615.71 files makes that query succeed.
- **Reproduction:** Build `ngm_launch_environment_integration` and invoke it with
  explicit server/fixture/shader/Nsight/store/report paths and the retained overlay
  JSON. It requests the reference fixture, seed 42, 192×128, presented interval 2,
  fresh target ending at application frame 20, timeout 120000 ms. The overlay
  supplies the existing matching driver directory/ICD, QT XCB, SDL Wayland and
  explicit unwanted-layer controls. The final probe removes only
  `VK_LOADER_LAYERS_DISABLE`. No system setting is changed.
- **Expected/observed:** Two independent frame-2 standalone fixture baselines
  complete and retain valid readback on NVIDIA 615.71.09 / XCB. All three fresh
  MCP capture attempts report Nsight connection, and loader diagnostics select
  `libGLX_nvidia.so.615.71.09`, but produce no capture before the deadline.
  Each ends `timed_out` with `cleanup_confirmed=true`, no GPU reservation or
  quarantine, and a pinned failed bundle. Removing the layer filter does not
  resolve the observation. No fresh capture support, root cause, universal
  unavailability or regression relative to an equivalent older-server run is
  established. The older 2026.2 installation is no longer present locally.
- **Evidence:** Snapshot `bundle-2a69ab0ef0bd54fd2df1fdeadc6fc423` is pinned in
  the configured external artifact root. It includes application baselines,
  loader/tool logs, exact arguments, environment/ICD/library hashes, build/shader
  identities and MCP transcripts. Failed source bundles
  `bundle-7a2bfdbfd496de3972e1a8055c1a7761`,
  `bundle-dedb9d7bf9faaeda09c73ca930ab8638` and
  `bundle-05301c2042ae9de1731f6ee408520962` remain independently pinned in the
  external launch-validation store; restart receipts verify their pins.
- **Revisit:** Reproduce through another matching documented Nsight toolchain or
  a newly documented capture interface; compare an independently controlled
  capture invocation and the same selected environment. Any driver/session or
  permission changes require a separate explicit owner decision. Continue
  retained-source and CPU capability work without advertising a fresh capture.

I-038 follow-up, 2026-10-15: the separate documented Generate C++ Capture activity
passes at 0.4.4 on matching 2026.3 tools with the same matching-driver selection
and no layer-disable filter. Its source pin, literal source queries and exact
shader-byte oracle are recorded in BUILD_VALIDATION.md. This narrows the failed
observation to the measured graphics-capture activity; it does not resolve that
failure or prove an underlying cause.

### I-039 — Remaining selected-state values lack a qualified documented path

- **Context:** Feasibility review at product **0.4.7 / abb6387**, installed Nsight
  **2026.3.1.0/build 38722833**, plus the four exact retained hydration inputs from
  matching 2026.3 and **2026.2.0.0/build 37991608**. No new GPU attempt or generic
  hydration experiment is performed.
- **Reproduction:** Run the existing C++ `--verify-descriptors SERVER RUN` harness
  against `draw-state-pC6JVF`, or query `capture_draw_state` for source
  `bundle-160c221f248ae3873459eb60055300b0` and listed execution reference
  `c6bcbda467b377e755d23ef692658fb4ee5f82736e7b5bc5af9215a4fc9124f7`.
  Review its retained source callsites, installed replay help and the documented
  CLI/C++ export paths. Exact query transcript and source pins are retained with
  the 0.4.7 qualification.
- **Expected/observed:** The requested general descriptor contents,
  execution-time uniform/material values and observed GPU event correspondence
  cannot be established by these inputs. The successful query correctly reports
  unavailable value fields and false GPU correspondence. Fresh setup exports use
  serialized hydration calls (four writes/352 output bytes each); that size does
  not establish input blob length. Mapped uploads and private fixture witnesses
  do not establish arbitrary after-event resource lifetime or types. The reviewed
  documented interfaces establish no additional selected-event backend.
- **Evidence/retention:** Complete/pinned audit
  **bundle-7269c28d30326b8f1fa3ef64ebd4ed36** verifies 56 historical file identities,
  both fresh source pins/callsites and all 15 unavailable value/correspondence
  outcomes. Existing fixed hydration qualification
  **bundle-93e2c3dd09c57553642b1afa873e8566** remains pinned separately. See
  [DRAW_STATE_FEASIBILITY.md](DRAW_STATE_FEASIBILITY.md) for exact scope and sources.
- **Revisit:** A new documented machine-readable selected-event interface, or an
  explicitly accepted bounded hydration experiment with independent new-input
  and resource-lifetime/type evidence. The existing five-file production worker
  exception does not authorize adding VulkanStructHydrator. This scoped outcome
  is not a claim that Nsight can never expose these values.

### I-040 — Per-action regime export contains undefined numeric cells

- **Context:** 0.4.8 development controls, based on 4ab02f2; matching Nsight
  **2026.3.1.0/build 38722833**, RTX 3080 Ti / driver **615.71.09**, existing
  KDE Wayland/Xwayland session and selected matching library/ICD overlay.
- **Reproduction:** Explicit C++ `ngm_profile_controls_integration` runs the
  `state-descriptors-reuse` fixture at seed 42, 192×128, final frame 300. Fresh
  profile jobs start after five presents and collect three frames/1000 ms,
  Throughput Metrics for Ampere GA10x, HES explicitly off, clocks unaltered;
  `time_every_action` is false, then true. Exact requests/argv/environment and
  binary/source identities are retained with the attempt.
- **Expected/observed:** Both Nsight processes exit **0** with cleanup confirmed.
  The first managed profile succeeds. The second exports `Time Every Action: Yes`
  but the strict parser rejects its regime table as a missing/nonfinite/malformed
  numeric cell. That table has **39 rows / 438 numeric columns**, including **292
  literal `nan` cells**. Its event table has **13 finite rows / three time columns**;
  frame duration and all 146 frame-metric rows are also finite. The counter cells
  have no usable finite value; the audit does not establish why they are undefined.
- **Evidence/retention:** Complete/pinned investigation
  **bundle-0db93680346958bf72c60f67ff15d435** retains reports, transcript, all ten
  text exports, exact code snapshot and unchanged permission observations.
  Successful source **bundle-d7f5a3fc5da5bc7b2ceb3be2280ab79a** and failed source
  **bundle-bef6722497c3c50eb4186f98a3294a27** are independently pinned under
  `capability-followup/profile-controls-tPCgnK/store`. The failed bundle remains
  failed; it is not promoted to a typed profile. Existing profiler-device/trace
  access suffices; no permission, clock or other machine change is made.
- **Revisit:** Preserve this exact producer/table/token shape as explicit
  unavailable cells, validate null/status coverage and missing-selection refusals,
  then collect fresh profiles with both controls. Keep malformed tokens and
  nonfinite timing/frame values rejected; do not replace missing counters by zero.

I-040 follow-up, 2026-10-15: the producer-qualified regime parser now preserves
only literal `nan` cells as null/unavailable. Two new 0.4.8 profiles succeed with
per-action timing off/on and HES explicitly disabled. The off profile has 146
undefined cells; the on profile has none. Finite neighbors, missing-selection
refusals and identical metadata/pins after restart pass. All 35 serial CPU checks
pass. This does not explain the undefined cells or establish draw correlation.
Final qualification **bundle-193d06c97b109c55fb8a30e35f12b8f9** is complete/pinned;
see PROFILE_CONTROLS.md and BUILD_VALIDATION.md for the exact scope.

### I-041 — Embedded historical profiles are incomplete native-bundle backups

- **Context:** Offline 0.4.8 compatibility probe against the raw R-009 snapshot
  **bundle-d4db174806f33a707b908900599404d5**, containing historical exports from
  matching Nsight **2026.2.0.0/build 37991608** and **2026.3.1.0/build 38722833**.
  No GPU/tool collection occurs.
- **Reproduction:** Initialize an empty managed store through the product, then
  copy the snapshot's unmodified embedded native profile directories
  **bundle-98667e6c4f224577892a438604d1c9cf** and
  **bundle-f4ace6e396d66379f12e74b70d4a84c0** into it with their existing manifests
  and pins. Query the first through `profile_metadata`.
- **Expected/observed:** Inspection correctly refuses `corrupt: Artifact output
  inventory differs`. Each embedded copy is missing four cache/configuration
  files listed by its original manifest: GLCache bin/toc, NsightGraphics.conf
  and .default.ngfx-proj. The available raw exports have no newly observed size
  difference; they do not form a complete native backup. The second profile is
  audited for the same missing-file condition, rather than represented as a
  successful typed query. No manifest is rewritten or missing file fabricated.
- **Evidence/retention:** Complete/pinned audit
  **bundle-404b123f1d2fec9deea906890a71ca1e** retains the source manifests, missing
  inventory, exact restoration script and rejected MCP transcript. The R-009 raw
  snapshot remains pinned. This result does not establish a parser regression or
  change the original retained measurement outcomes.
- **Revisit:** Recover intact original native bundles before claiming real
  historical-profile query compatibility. A separate CPU-only probe creates an
  intact old-format profile through the installed 0.4.1 product and stand-ins;
  current-server read/restart compatibility passes, with its narrower scope
  recorded in BUILD_VALIDATION.md.

I-041 follow-up: a later read-only restart also refuses inspection of the good
profiles while those incomplete negative copies share the validation store;
store-wide inventory checking correctly finds the earlier missing files. With
no server using that store, only the two manually copied negative inputs are
moved intact to `investigation-I041/incomplete-native-copies/`. Their manifests,
pins and all available bytes are preserved with before/after file hashes. No
original historical source is removed. The two fresh 0.4.8 profiles then return
identical metadata and complete/pinned status through a clean server restart;
four additional results pass actual output schemas. The failed restart transcript
and isolation record accompany the complete/pinned final qualification
**bundle-193d06c97b109c55fb8a30e35f12b8f9**.

### I-042 — Private timing harness omitted the selected-frame readback marker

- **Context:** R-022 graphics timing fixture, 0.4.9 development based on
  **7342514**, RTX 3080 Ti / driver 615.71.09, KDE Wayland/Xwayland, 2026-10-15.
- **Reproduction:** Run `ngm_profile_correlation_integration` with its explicit
  server/fixture/shaders/Nsight/output-parent/launch-overlay paths. Its first
  synchronization-validated `timing-direct` baseline presents frames 0–3.
- **Expected/observed:** The application exits 0 with cleanup confirmed. Frames
  0–2 record ten marker begins. Frame 3 also records `application.readback`,
  outside the timed scene, so it has eleven. The new private verifier incorrectly
  requires exactly ten and rejects that valid final-frame witness before any
  Nsight job starts. This is a harness error, not a tool/export failure.
- **Evidence/retention:** Complete/pinned **bundle-82dec3538e6ddf767ff4276b4842aa8f**
  retains exact code/binary identities, failed report, application result,
  raw timestamp/marker witness, layer settings/logs and unchanged permission
  observations. Source directory is `correlation-final-nwprkz0f/profile-correlation-aGlG0u`.
- **Correction/revisit:** Permit the additional marker only with its exact
  readback name, root parent and two-component path. Preserve the distinct
  repeated-marker and two-draw-marker assertions. Revisit if selected-frame
  readback labels change; fresh qualification is recorded separately.

### I-043 — Timing and regime exports omit general action/occurrence keys

- **Context:** R-022 final 0.4.9 graphics matrix, matching Nsight
  **2026.3.1.0/build 38722833**, RTX 3080 Ti / 615.71.09, KDE Wayland/Xwayland.
  All six baselines and fifteen native profiles succeed.
- **Reproduction:** Run the C++ `ngm_profile_correlation_integration` matrix at
  seed 42, 1024×512 with its explicit matching-driver overlay. Inspect exact
  event/regime exports, actual API marker occurrences and timestamp witnesses;
  compare timing off/on, frame/submit intervals and one-frame controls.
- **Expected/observed:** A general draw/marker/counter association needs original
  action or unambiguous occurrence/sample identity. Even with per-action timing
  enabled, the event export contains marker rows without API draw IDs or queue/
  submission keys. Direct/indexed three-frame cases have 10 event rows × three
  identical `time_ms` headers, versus 30 regime rows × 438 metric columns.
  Reuse repeats complete marker paths. Three submit intervals in reuse yield
  ten event rows × two columns, rather than three identified submit samples.
  Raw positions remain valid but do not resolve those identities.
- **Narrow supported finding:** One-frame nonreuse profiles match actual marker
  API inputs and allow unique exact full-path associations except the repeated
  sibling path. Scene timing agrees with independent timestamps within 32/64 ns;
  the reused aggregate agrees within 160 ns. Repeated paths remain ambiguous
  for cross-table occurrence joins. A marker can contain two draw calls.
  `fe__draw_count.sum` reports 10/10/17 versus 6/6/12 application calls in the
  single-frame cases. Cause and API-count equivalence are not established;
  instructions/counters are not assigned isolated-draw or fragment-only scope.
- **Evidence/retention:** PROFILE_CORRELATION.md and BUILD_VALIDATION.md identify
  complete/pinned final qualification **bundle-40d1c8f983a49246a26d5294f20636bb**.
  All fifteen service-produced source profiles
  are independently pinned in `correlation-final-7h9rhq73/profile-correlation-IqjCBy/store`.
  The independent audit retains exact table hashes, paths, duplicate counts,
  raw hardware metrics, private API/timestamp records and schema checks.
- **Revisit:** Implement the authorized bounded unique single-frame marker
  query with explicit ambiguity/scope refusals. Wider action/occurrence or
  physical counter claims need documented export identifiers/definitions and
  independent new-input qualification; a new documented NVIDIA interface may
  provide that evidence. No private trace decoding or GUI backend is selected.

The **0.4.10 follow-up** implements that narrow `profile_marker` query and
qualifies all 180 retained event selections in two fresh stdio processes:
32 available, 48 ambiguous/unavailable and 280 unsupported responses across
restart. Available/ambiguous/unsupported HTTP selections exactly match. The
query preserves original execution IDs, draw membership, physical scaling and
shader-stage isolation as unavailable; it resolves none of the wider I-043
identity/counter limitations. See PROFILE_MARKERS.md and the complete/pinned
`bundle-b8d3515b5cd6079e2ba2c8cb5637067e`. R-022's remaining scope/overhead
assessment stays open.

### I-044 — Repeated timing-control measurements do not isolate causal overhead

- **Context:** R-022, source-built **0.4.11**, matching Nsight
  **2026.3.1.0/build 38722833**, RTX 3080 Ti / 615.71.09, existing KDE
  Wayland/Xwayland-XCB desktop, diagnostic shaders and unaltered clocks.
- **Reproduction:** Run `ngm_profile_correlation_integration` with
  `--repeat-overhead`, the explicit matching-driver overlay and the declared
  PROFILE_OVERHEAD.md plan: four fresh off/on pairs, reversed alternate order,
  30 warmup presents, one collected frame, seed 42 and 1024×512.
- **Expected/observed:** All eight profiles and both baselines succeed; exact
  marker/API inputs and 326 private timestamped submissions verify. Scene
  export/timestamp differences are 32–96 ns within the declared tolerance.
  Paired on/off ratios are 0.999616, 1.009751, 1.002107 and 0.994629. Mixed
  changes and uncontrolled desktop/clocks/power do not isolate a stable causal
  instrumentation cost. The median paired ratio is 1.000861, not a correction
  factor. All 92 telemetry probes succeed but do not resolve interval-local
  clock stability. Profiling access observations match before/after.
- **Evidence/retention:** PROFILE_OVERHEAD.md and BUILD_VALIDATION.md identify
  complete/pinned **bundle-8d667f47f660fc46a582f440ee786c5e**. All eight native sources are
  independently pinned in the external `overhead-final-u6kqwk8_/profile-overhead-Bft1vF/store`.
  The declared plan, every sample/pair, raw exports, private witnesses, telemetry,
  actual MCP transcript/schemas and independent audit are retained.
- **Revisit:** A separately approved controlled experiment could constrain
  clocks/background work and quantify uncertainty at a declared scope. This
  result is inconclusive, not proof of zero cost or universal unmeasurability.
  Product durations retain overhead; no correction or implicit machine-setting
  change is introduced.

### I-045 — Equivalent fixture API calls do not determine exported counter totals

- **Context/reproduction:** The same eight successful 0.4.11 profiles in I-044,
  with identical executable/shaders, scene inputs and private frame-30 records.
  Independently compare all 146 frame-metric cells per run to exact MCP pages;
  inspect the selected scene regime in each timing-on profile.
- **Expected/observed:** Six direct API calls occur in every selected scene.
  Raw `FE_A.TriageSCG.fe__draw_count.sum` frame cells are 10 in six runs and
  71 in two. The two 71 sources are `bundle-8dc3fef15a44ca51541a56ee98dbdda1`
  and `bundle-148ebca11a80506eec7f88999c0008f8`. Timing-on scene cells are
  7, 7, 7 and 3. ZCull/PROP/instruction totals also differ between these
  equivalent API inputs. Cause, physical normalization and exact application
  attribution are unresolved. All 1,168 raw frame cells and 80 selected raw
  facts verify; literal zero remains zero.
- **Evidence/retention:** The complete/pinned I-044 qualification and all eight
  independent native source pins retain exact names/text, raw table hashes,
  exported paths and private API records. No source is excluded as an outlier.
- **Revisit:** A version/architecture-qualified documented definition, attribution
  boundary and independent new-input audit are needed before translating these
  cells into exact application draw counts or isolated-draw/fragment work.
  NVIDIA's GPU Trace guide states that collection includes other GPU activity;
  that alone does not establish the cause of this particular difference. No
  private trace decoder or inferred frame-to-draw division is selected.

### I-046 — Fresh Graphics Capture waits for a low-space warning

- **Context:** R-023, 2026-10-15, pre-fix **0.5.0**, matching Nsight
  **2026.3.1.0/build 38722833**, RTX 3080 Ti / kernel driver **615.71.09**,
  KDE Wayland/Xwayland-XCB and the retained matching-library/ICD overlay.
- **Reproduction:** The C++ `ngm_launch_environment_integration` runs an
  independent seed-42/192×128/frame-2 baseline, then submits the reference
  capture at presented interval 2 with a 120000 ms deadline. Exact invocation,
  original harness source and MCP transcript are retained in the qualification.
- **Expected/observed:** The application baseline passes. The injected target
  connects but sleeps in `do_wait`; its owned child is Zenity with the low-space
  warning. Timestamped PID/parent/start-time observations retain the exact
  command line: approximately **5.4 GB** is available, below Nsight's **20 GB**
  recommendation, and the message specifies `--no-low-disk-space-check`.
  No dialog is clicked. The job times out after **120211 ms**, confirms owned
  cleanup and releases its GPU reservation.
- **Evidence/retention:** Failed native **bundle-1dae31356d8a009e11c1befd3e684be3**
  is independently pinned under
  `/home/vordhosbn/.local/share/nsight-graphics-mcp/roadmap-resolution-aytro126/graphics-current-store`.
  Complete/pinned **bundle-ab49ccec89e6f4584051359b650e7f41** in the configured
  central store retains the process observation, failed report/transcript,
  original harness and successful qualification. All 407 payloads verify.
- **Resolution:** At **0.5.1**, the adapter adds the exact documented flag only
  when selected-tool help advertises it. Three fresh reference/repeat/fault MCP
  captures pass in 6.354–6.655 seconds; every screenshot exactly matches its
  independent frame-2 application readback, distinct targets and restart pins
  verify. The retained 2026.2 help lacks the flag, and the CPU legacy-control
  case preserves that invocation. Managed quotas, publication errors and job
  cleanup remain enforced; no machine setting changes.
- **Revisit:** A different blocking warning or changed CLI control requires its
  own observed diagnosis. This establishes the cause of this run, not of I-038's
  three earlier timeouts without retained child-state observations. See
  BUILD_VALIDATION.md for final-schema correction and exact qualification scope.

### I-047 — Standalone replay probe uses implicit JSON/string comparisons

- **Context:** R-027 private C++ probe, 0.5.1 project libraries, GCC 16.2.1,
  pinned nlohmann/json configuration, 2026-10-15.
- **Reproduction/observation:** The retained first compiler invocation compares
  typed tool-version/build strings directly with JSON values. Compilation fails
  because that string-left overload is unavailable. No probe executable or
  Nsight/GPU operation runs.
- **Resolution:** Extract explicit `std::string` values before comparison.
  This corrects the compilation error; I-048 records the separate subsequent
  link error. Neither is evidence of an unavailable replay interface.
- **Evidence/retention:** Exact source, argv and compiler stdout/stderr are under
  `raw/imported/investigations/failed-replay-build-I047/` in complete/pinned
  **bundle-ab49ccec89e6f4584051359b650e7f41**.
- **Revisit:** Retest explicit types if the JSON/compiler contract changes.

### I-048 — Standalone replay probe omits the launch-validation archive

- **Context:** Same private 0.5.1 replay probe as I-047, after its source type
  correction; GCC 16.2.1 and the actual project static-library layout.
- **Reproduction/observation:** The retained direct compiler/link invocation
  includes artifacts, Nsight and core archives but omits `ngm_capture`, which
  contains `LaunchEnvironment.cpp`. Linking fails with the unresolved
  `validate_launch_environment` symbol. No Nsight/GPU operation runs.
- **Resolution:** Link the existing capture archive before its dependencies,
  following `cmake/Capture.cmake`. The probe then compiles and one actual
  matching 2026.3 replay completes; R-027 still requires product integration
  and independent rendered-output/repeatability qualification.
- **Evidence/retention:** Failed source, argv and link diagnostics are under
  `raw/imported/investigations/failed-replay-build-I048/` in complete/pinned
  **bundle-ab49ccec89e6f4584051359b650e7f41**. The successful private replay is
  independently complete/pinned as **bundle-1ed49923b28aa4c6c8dff50ebf85d95d**
  in the isolated `roadmap-resolution-aytro126/replay-store`.
- **Revisit:** Keep future probe targets linked through the declared CMake
  dependency graph; a link omission establishes no Nsight compatibility limit.

### I-049 — Actual capture metadata violates its advertised producer schema

- **Context:** R-023's independent retained-result audit, 0.5.1 development
  server and actual fresh 2026.3 captures, 2026-10-15. No new GPU operation.
- **Reproduction/observation:** Retrieve `tools/list`, then validate the actual
  `capture_metadata` structured result with JSON Schema 2020-12. Its producer
  contains `api_identification`, which the shared closed output schema omits;
  validation fails on the additional property. The field already exists in
  graphics and compute results; this is a product schema defect, not a capture
  failure or invalid raw export. The saved response and saved schema reproduce it.
- **Resolution:** Add the required bounded two-value discriminator to the
  common metadata/event/object schema. CPU MCP checks now check closed-object
  members and discriminants against actual advertised schemas. Final focused
  MCP/HTTP checks pass. Full JSON Schema validation passes **20** actual results
  across five server processes, including the ordinary graphics discriminator
  and the retained compute frame-boundary discriminator. Capture metadata and
  source/failed pins remain stable across restart.
- **Evidence/retention:** Original schema/transcript, failed audit and source are
  under `raw/imported/investigations/failed-schema-I049/`; final verifiers,
  transcripts and receipts are in complete/pinned
  **bundle-ab49ccec89e6f4584051359b650e7f41**. The compute collection keeps its
  original 0.3.0 identity; this audit does not qualify a new compute GPU run.
- **Revisit:** Any future producer field/value change must preserve agreement
  between actual results and the shared stdio/HTTP output contract.

### I-050 — Capture of the replayer's shell launcher fails to track its target

- **Context:** R-027 private **0.5.1** C++ probe, matching Nsight
  **2026.3.1.0/build 38722833**, RTX 3080 Ti / **615.71.09**, existing
  KDE Wayland/Xwayland-XCB and the matching driver-library/ICD selection.
- **Reproduction:** Run documented Graphics Capture against the selected
  `ngfx-replay` shell launcher, passing `--loop-count=20 --present-app --verbose`
  and an isolated copy of the pinned reference capture. Select capture interval
  2 with a 60-second outer deadline; all arguments/source hashes are retained.
- **Expected/observed:** A new capture of the replayed GPU output was expected.
  The capture launcher instead exits **255**, reporting that launch tracking
  failed; no new capture is produced. Cleanup is confirmed without an outer
  timeout. This does not invalidate the separately successful plain replay.
- **Resolution:** The installed launcher source explicitly invokes its sibling
  `ngfx-replay.bin`. Targeting that same ELF under the sibling capture launcher
  succeeds with matching loader setup. The ordinary replay path continues to use
  the documented launcher; the ELF selection is only the output-verification
  experiment. No private RPC, generated replay compilation or GUI automation is
  used. The first failure's internal launcher cause is not established.
- **Evidence/retention:** Failed native
  **bundle-0b0eeadf85d8b4e7f70cd37ab6d56447** and successful ELF control
  **bundle-859ebe7f55f6d5fffbec9f2f34a8d4c6** are independently pinned in
  `roadmap-resolution-aytro126/replay-store`. Complete/pinned
  **bundle-9592f2cd01c4623855009d7589ed37af** in the central store retains
  requests, launcher sources, native reports/logs and independent image controls;
  all **340** payloads verify.
- **Revisit:** Retest launcher tracking if the documented wrapper changes;
  do not generalize this failed target launch to every script or replay release.

### I-051 — Embedded final-present image is not the replay pixel oracle

- **Context:** Same R-027 **0.5.1** probes and matching 2026.3 runtime as I-050;
  source capture **bundle-644cc39112ddd32696ce24823764ca97**, seed 42,
  192×128, one captured interval at present ordinal 2.
- **Reproduction/observation:** Compare the source's embedded screenshot with
  the screenshot of the fresh capture of its standalone GPU replay. They differ
  on **7372 pixels**, with maximum RGB8 channel differences **4/4/3**. Assuming
  equality between these evidence origins was too strong; this is initially a
  comparison failure, not a demonstrated rendering failure.
- **Resolution within this fixture:** Explicit independent application baselines
  at frames **0, 1 and 2** retain identical executable/shader inputs. The new
  replay image matches frame **1** exactly, while the source embedded image
  matches frame **2**. A fresh repeated reference replay is identical; a shader
  fault replay exactly matches its independent faulty frame-1 readback and
  differs from the reference. The documented no-present-blit control changes
  the newly captured output, discriminating it from a copied embedded image.
  RGB8 tolerance stays **zero** throughout.
- **Boundary:** This measures two distinct image views for the stated workload
  and producer. It does not establish a generic frame-number formula, original
  draw/queue/sample correspondence, or the internal reason for the image offset.
  Unknown applications still need their own equivalent-input pixel oracle.
- **Evidence/retention:** All eight native replay-probe attempts remain
  independently pinned in the isolated replay store. Complete/pinned
  **bundle-9592f2cd01c4623855009d7589ed37af** retains the initial mismatch,
  all explicit baseline requests/results, original and repeated replay outputs,
  the no-blit control, exact source/probe identities and a passing independent
  `verify_pixels.py`. Product integration was unfinished at that 0.5.1 probe
  milestone; 0.5.2 completes it as documented in REPLAY.md.
- **Revisit:** Qualify different intervals, workloads or producer versions before
  reusing this correspondence; never substitute metadata-image equality for
  actual GPU replay verification.

### I-052 — Replay integration build and retention-test corrections

- **Context:** R-027 product **0.5.2** development, GCC 16/C++20 and the CPU
  executable Nsight stand-in; no GPU failure is inferred from these checks.
- **Reproduction/observed:** The first product build compared a `std::string`
  snapshot digest directly with a JSON value and failed with `no match for
  operator!=`. After explicit string extraction, the first replay workflow test
  advanced its fake storage clock by 31 days, unpinned the source, released its
  last usage lease, then tried to pin it again. Publication correctly pruned it;
  the check reported `Maximum retention age reached`.
- **Expected/resolution:** Extract the JSON hash as `std::string` before comparing.
  Test lease protection while queued/running, then restore the persistent pin
  before releasing the last lease. The corrected replay, resource-reader, MCP
  and HTTP focused checks pass. The source-mutation, owned escaped-child cleanup,
  timeout/cancellation/shutdown and shared-GPU checks retain their assertions.
- **Evidence/retention:** The observed diagnostics are transcribed in
  `r027-build-corrections.txt` alongside the final CPU logs in complete/pinned
  **bundle-1c59130aafc0cb1a946cd9e0c4da869c** (all 741 payloads verified). Earlier rerun logs were overwritten; no complete original build
  log or surviving CPU scratch bundle is claimed. The final aggregate and native
  GPU evidence are recorded separately in BUILD_VALIDATION.md.
- **Revisit:** Preserve typed JSON boundaries and restore protection before
  deliberately releasing a retention lease; do not weaken production pruning
  to satisfy a test that asks for an expired bundle.

### I-053 — Unique queue labels do not recover multi-interval timing columns

- **Context:** R-025 private fixture compiled against 0.5.2, matching Nsight
  2026.3.1.0/build 38722833, RTX 3080 Ti / 615.71.09, existing KDE/Xwayland
  and explicit matching-driver overlay. No machine settings change.
- **Reproduction:** Label every queue submission with its explicit application
  frame/occurrence and every direct/indexed call with a unique draw leaf. Retain
  nested/repeated and two-draw human markers, reused recordings and the independent
  private API/scene-timestamp witness. Profile one and three presents through MCP.
- **Observed:** One interval exports two distinct submission roots and 12 unique
  draw leaf paths. Three intervals export a 34-row event tree with only frame-5
  labels and three repeated `time_ms` columns/values. The separate 102-row regime
  table contains frame-5/6/7 paths, with repeated metric columns. The two tables
  have different observed identity coverage; row order cannot repair it.
- **Outcome:** The original multi-column timing association remains unavailable.
  The single-interval path is a candidate for explicit source association, not a
  completed public draw timing API. Raw draw-count cells inside known one-call
  leaves are zero; that does not establish their physical scope or diagnose why.
- **Evidence/retention:** Native complete/pinned profiles
  `bundle-31a0d8e79be6bf67529aa45ea10df3af` and
  `bundle-778b8da3a34df02b7d582bd1b6a67778`, plus C++ capture
  `bundle-41ff2210f8d58d7cc4add01636d14e7e`, in the isolated
  `r025-queue-experiment/store`; exact requests/private patch/build and raw
  tables are retained with the R-025 foundation qualification.
- **Revisit:** A documented timing-column identity or separately qualified
  selected-interval export can change this outcome. Do not infer columns from
  equal values/counts, regime row order or the application witness.

### I-054 — Replay profiling exports a different start trigger

- **Reproduction:** Generate a C++ capture and a GPU Trace from the same pinned
  single-interval `.ngfx-capture`, using the matching public replayer's sibling
  ELF as the unmodified target. Request a one-frame trace after one frame.
- **Expected/observed:** Capture-of-replay succeeds and preserves literal queue/
  draw labels. GPU Trace also exits successfully and exports those same labels,
  but reports `Start After: Replay Pass Start` instead of the requested frame
  trigger. The generic product profile validator correctly rejects the settings
  disagreement and retains a failed/pinned bundle with confirmed cleanup.
- **Boundary:** This is a candidate distinct replay-profile contract. It does
  not justify accepting a changed trigger for arbitrary ordinary profile jobs.
  The documented SDK trigger later produces this header explicitly (I-059).
- **Evidence:** 0.5.2 native source
  `bundle-3a27c156d43faef21d33ddf324e93b92`, C++ result
  `bundle-c2fbb069e009bae7557c8a85aee5502d`, and failed profile
  `bundle-cd0fad690b5c94c51b7b3f5e3f116319`, all independently pinned in
  `r025-replay-experiment/store`; matching runtime as I-053.
- **Revisit:** Add a separate source-validated replay-profile operation with exact
  trigger and input identity checks; preserve generic profile validation.

### I-055 — Start-at-zero replay trace does not finish after exporting tables

- **Reproduction:** Profile the private three-interval saved capture with the
  matching replayer, 20 loops, frame trigger zero and a one-frame limit.
- **Observed:** Tables are exported, but the operation does not finish within
  the 120-second job deadline. The coordinator reports `timed_out`; its stop
  token makes the subprocess report cancellation. Owned cleanup is confirmed.
  The failed artifact is not promoted to a successful typed profile.
- **Evidence:** Native failed/pinned
  `bundle-7891e499a927d804ac9be0179091c217` in
  `r025-multi-interval-experiment/store`, with requests, reproduction information,
  available exports and process logs. Same 0.5.2/2026.3 runtime as I-053.
- **Revisit:** Use a documented trigger that starts after target readiness and
  qualify teardown separately. Exported files alone do not establish completion;
  the internal cause remains unproven. I-058 retains the finite-loop control.

### I-056 — Replay frame request and exported selection disagree

- **Reproduction:** On the same three-interval input, request a trace after
  three frames, with one collected frame and 20 replayer loops.
- **Observed:** Reproduction information reports `2 Frames`, and event labels
  identify original application frame 9. The generic settings validator refuses
  the changed request. A separate explicit after-two-frames request succeeds
  with frame-9 labels. No wraparound rule or internal clamping cause is inferred.
- **Evidence:** Failed/pinned `bundle-8fa15b7151110cf22a1f1ebcb9779b62` in
  `r025-multi-interval-selected/store`; successful independently pinned
  `bundle-d3887e1ba8249f14fa79f4a0a0e2f5e3` in
  `r025-multi-interval-complete/store`. Independent C++ captures at waits 3/4/5
  retain frame-7/8/9 literal submission labels from that exact input.
- **Revisit:** Qualify selectors against retained source labels and exported
  settings for each supported shape; do not manufacture an interval join by
  modulo arithmetic. Other producers remain unqualified.

### I-057 — Interval-count CPU check reuses an existing variable name

- **Context/reproduction:** The first 0.5.3 aggregate build adds a local operation
  result named `unqualified` in NsightCheck.cpp, which already declares a later
  installation variable with that name in the same scope.
- **Observed/resolution:** Compilation fails on the conflicting declarations.
  Rename the new result `unqualified_multiple`; no product behavior changes for
  this correction. The full 37-check aggregate then passes in 372.66 seconds
  before the subsequent plural-range metadata addition. Final validation is
  recorded in BUILD_VALIDATION.md.
- **Evidence/revisit:** The complete first-failure log is retained as
  `r025-multicapture-cpu-first-failure.log` in the foundation qualification.
  Keep distinct typed observation names when extending the shared check scope.

### I-058 — One finite replay loop does not fix start-at-zero profiling

- **Reproduction:** Repeat I-055 directly through the owned C++ process runner,
  changing replay loops to one and the outer deadline to 30 seconds. Retain a
  bounded `/proc` observation of that runner's own process tree.
- **Observed:** The target connects and presents, then the tool waits for a GPU
  Trace report and reports a connection error. No usable table export is produced
  before timeout; owned cleanup is confirmed. The owned observations do not show
  a Zenity process. Neither a crash nor the underlying connection cause is proven.
- **Evidence:** `r025-first-interval-finite` retains exact argv, tool/runner hashes,
  process observations, stdout/stderr and the timeout receipt. Runner 0.5.1 is
  distinct from the 0.5.2 experiment fixture and 2026.3 tools.
- **Revisit:** A delayed documented trigger is a different control; the SDK
  trigger in I-059 succeeds without establishing this failure's internal cause.

### I-059 — Private GPU probes overlap; serial qualification replaces them

- **Observed:** The first 0.5.3 product capture validation and a private
  SDK-triggered replay profile overlap in time. This is experiment orchestration
  outside the shared product job coordinator. Both finish with owned cleanup,
  but the overlap is unsuitable as timing qualification.
- **Correction:** Retain the initial outputs explicitly as overlapping evidence.
  Await each confirmed terminal handle before starting another GPU check. The
  repeated product MCP/CLI captures, replay and restarts pass serially; the
  SDK-triggered profile then passes in a fresh isolated run with exit zero and
  confirmed cleanup. No clock, permission, display or background-process changes.
- **Actual SDK control:** Use help-advertised `--start-with-ngfx-sdk`, replacing
  the frame trigger, with the same three-interval input, 20 replay loops and a
  one-frame limit. Exported `Replay Pass Start` selects literal original frame-7
  labels. Separate after-one/after-two frame controls expose frame-8/frame-9
  labels; each has matching C++ capture evidence. This remains a private candidate
  until a typed source-validated product operation and correlation checks exist.
- **Evidence:** Initial `r025-product-validation` / `r025-replay-sdk-trigger` and
  corrected `r025-product-serial` / `r025-replay-sdk-serial` are retained separately
  with exact requests, binary identities, receipts and native pins. The serial
  product collection validates 118 actual tool results against advertised schemas.
- **Revisit:** Serialize private GPU experiments as well as product jobs; preserve
  trigger semantics and never use the overlapping samples as a clean comparison.

### I-060 — Multi-interval range text is missing from the typed metadata view

- **Observed:** A successful three-interval capture exports
  `captured_frames: "7 - 9"` instead of the single `captured_frame` field. The
  original typed parser ignored the plural field and returned null for the
  singular field, losing available range text.
- **Correction:** Add a separate optional raw string to the parser, retained
  inspection result, advertised closed schema and replay header identity checks.
  Missing fields stay null; neither field is converted into application frame
  IDs, a numeric interval list or profile sample identities.
- **Evidence:** Serial native source `bundle-59a280028a05a9eedfd7d29e876dd0ba`
  has the observed plural range; the default-one source
  `bundle-420c3ddae43aed8e42acc1f98a010f72` retains singular `"7"`. Both remain
  independently pinned in `r025-product-serial/store`. Collection and final
  reader binaries/source are retained separately; final parser/schema/GPU replay
  checks are recorded in BUILD_VALIDATION.md.
- **Revisit:** Qualify any changed producer field types explicitly. Raw range
  text does not establish original GPU event or timing-column correspondence.

### I-061 — Foundation evidence verifier initially fails to parse

- **Context/observed:** The private 0.5.3 qualification verifier has an extra
  closing parenthesis in its CSV reader helper and raises `SyntaxError` before
  checking any evidence. It does not establish a product or Nsight failure.
- **Correction:** Remove the extra parenthesis and run the complete verifier.
  It passes the 118 serial collection schemas, 36 final reader/replay schemas,
  exact image/hash/range checks, compressed binary identities and all 21 native
  pins. The original failing script and diagnostic remain in the snapshot's
  `verifiers/failed-verify-foundation.*` files.
- **Revisit:** Treat verifier execution as required evidence; a written script
  or an unexecuted intended check does not establish acceptance.

**Shared retention for I-053–I-061:** Complete/pinned
**bundle-487017236ba14e318c20a04bbc66dd5a** retains all **2,082** verified payloads,
including all 21 native bundles and their independent pins, plain supervised
probes, exact requests, raw tables, failed/overlapping controls, collection/final
source and compressed executable identities, CPU logs and portable verifier.
Source/imported hashes and lengths match; the imported verifier passes. The
snapshot qualifies the 0.5.3 interval-capture/range foundation and preserves
private feasibility evidence; R-025 remains In Progress. Importer 0.4.1 and each
actual tested build remain distinct.

### I-062 — Existing recorded-state grammar refuses replay-generated timing input

- **Context:** A private syntax probe calls `detail::parse_cpp_replay` on the
  CommandList, Resources and Frame sources from the actual 0.5.4 combined
  collector's first-interval bundle, `bundle-41a1f9abb69675072b38af35b7e951c0`.
  Producer: 2026.3.1.0/build 38722833; RTX 3080 Ti / driver 615.71.09.
- **Expected/observed:** The collector has preserved literal queue/draw labels
  in the C++ and trace exports. The existing recorded-state grammar nevertheless
  refuses with `unrecognized expression helper` before returning any parsed
  occurrence. This does not establish which construct caused the refusal or
  invalidate the raw source labels.
- **Isolation:** A first-party diagnostic copy adds only filename/token detail
  to that error and identifies `VkCommandPoolResetFlags` in Frame0Part00.cpp.
  A separate private type-extension probe then reaches and refuses
  `NV_THREAD_BEGIN`. The actual source also has host query-result vector
  outputs. These diagnostic controls do not change the product grammar or
  establish a callable timing association.
- **Reproduce/evidence:** Build/run `CppTimingParseProbe.cpp` with the retained
  compile/link arrays against that bundle's `project_directory`. The probe
  source, command receipt and stderr are retained with the 0.5.4 collection
  qualification. Its parser is the existing implementation, with no permissive
  fallback or generated-code execution.
- **Revisit:** Isolate the relevant grammar construct and qualify a bounded
  timing-specific interpretation with negative tests. Do not claim source-call
  association from label text alone or widen the old recorded-state contract
  merely to make this input parse.

### I-063 — Single-interval replay profiling exports tables but misses cleanup deadline

- **Context/reproduce:** After the three-interval 0.5.4 core and MCP collections
  pass, run `capture_analyze_timing` for interval zero of the retained native
  single-interval source `bundle-420c3ddae43aed8e42acc1f98a010f72`, using the same
  matching 2026.3 tools, explicit driver overlay, 20 replay loops, SDK trigger
  and 120-second total deadline. No other qualification GPU job overlaps.
- **Observed:** Generate C++ completes and preserves the original literal frame-7
  queue labels. GPU Trace exports its tables and prints `Terminating process...`
  followed by a connection-error message, but does not finish before the job
  deadline. The coordinator reports `timed_out`, confirms owned cleanup and
  releases the GPU reservation. Export files do not turn this into success.
- **Evidence:** Failed/pinned native `bundle-48b3d8b3975111db92545b2b01a48664`,
  the `r025-analysis-single-mcp` transcript/audit and separate subprocess logs
  retain the request, exact source/build identities, partial C++/trace evidence
  and terminal state. The source remains pinned and unchanged. The script's
  success assertion fails as intended for this unexpected outcome.
- **Revisit:** Change one documented replay control at a time in separately
  supervised serial probes. No internal cause, target-lifetime race or generic
  single-interval incompatibility has been established.
- **Control/correction:** The corrected I-064 control changes the replay loop
  count from 20 to 60 while preserving source bytes, SDK trigger and all profile
  settings. It exits zero with confirmed cleanup and exports the original
  frame-7 queue labels. Both controls print the final connection-error text, so
  that text alone does not classify success. The collector now uses a shared
  fixed 60-loop profile constant. The separately qualified 20-loop C++ phase is
  retained, with per-phase counts explicit in settings. Product MCP reruns and
  CPU checks must qualify that final build separately.

### I-064 — Private replay-loop probe omits the required output directory

- **Reproduce/observed:** The initial `r025-single-loop60` control copies the
  I-063 single-interval input and changes the fixed replay loop count to 60,
  but its script fails to create the requested profile output directory.
  `ngfx` exits 1 with `No such output directory` / `Invalid general options`.
  Cleanup is confirmed; no trace exists and no GPU compatibility conclusion
  follows from this attempt.
- **Correction/evidence:** Retain that exact request, source hash, script,
  stdout/stderr and supervised receipt. Create the output directory explicitly
  in a fresh `r025-single-loop60-valid` run before invoking the same documented
  controls. The product collector already creates its profile directory.
- **Revisit:** Require the private experiment's filesystem prerequisites before
  interpreting its tool result. Preserve the failed control separately.

### I-065 — A 60-loop C++ collection fails while entering frame-debugger mode

- **Context/reproduce:** The intermediate 0.5.4 build sets both C++ and profile
  replay loops to 60. Its first three-interval input selection (index zero)
  succeeds, but index one, `--wait-frames=4`, fails during Generate C++ Capture.
- **Observed:** The target connects and presents; the tool reports initial-state
  and function-stream capture progress, then `Preparing to enter frame debugger
  mode... Failed` and exits 1. Owned cleanup is confirmed. No trace phase is
  launched for that failed attempt, and no internal cause is established.
- **Evidence:** Failed/pinned native `bundle-a057196505c5857c688d38bc68d26b59`
  and the separate `r025-analysis-final-mcp` transcript preserve exact input,
  arguments, process result and partial output. The earlier selection's success
  is native `bundle-d818360c2b787329f2ea300bac16618f`.
- **Next control:** Preserve the previously demonstrated 20-loop C++ invocation
  while independently qualifying the 60-loop profile control. Store both phase
  counts explicitly. This choice does not prove the failed C++ capture's cause;
  fresh serial product checks remain required.

### I-066 — Evidence copy omits files declared by native manifests

- **Observed/reproduce:** The first 0.5.4 evidence-copy script excludes every
  directory named `config`, including server-managed native `raw/config`
  payloads. The portable verifier refuses a missing inventoried NVIDIA cache
  file before accepting the snapshot. Native sources/manifests remain intact;
  this is a qualification-copy error, not a capture failure.
- **Correction:** Preserve the original script, verifier and failure log under
  `verifiers/`. Restrict cache omission to private experiment folders, restore
  all **104** missing native payloads (**6,282,950 bytes**) from their pinned
  native sources, and retain each restored path/size/SHA-256 in `repair.json`.
  Do not edit manifests to hide missing files. Rerun complete semantic/hash and
  import verification before pinning the final qualification.
- **Revisit:** Native manifests define the retained payload set; a generic
  experiment-folder exclusion cannot be applied to a managed bundle snapshot.

**Shared retention for I-062–I-066:** Complete/pinned
**bundle-d95a9298e94a1dfc4b1e598bc2445d3f** retains **2,302** verified payloads,
15 native pins, raw source/trace data, exact requests/transcripts, successful and
failed controls, separate build identities, CPU logs and the portable verifier.
The repaired source/imported payload sizes and SHA-256 all match; the imported
verifier passes. It qualifies the 0.5.4 collector only. Draw correlation and
independent per-draw timestamp acceptance remain unfinished.

### I-067 — Timing-reader probe refuses replayer setup grammar

- **Context/reproduce:** The uncommitted 0.5.5 pure correlation reader consumes
  generated C++ plus event/regime tables from the final 0.5.4 first-interval
  analysis `bundle-cb9f1be49f63e6e3ee1781720b550c53`. No new GPU job is run.
  Input producer is 2026.3.1.0/build 38722833 on the recorded 3080 Ti host.
- **First observed refusal:** The new timing parser reaches the source, but the
  interpreter returns zero draws with `recording usage flags are outside the
  qualified timing scope`. The actual generated setup recordings use the
  literal `VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT`; application recording
  flags remain zero. No timing is returned for this refused attempt.
- **Evidence/retention:** Input sources/tables are already protected by the
  0.5.4 pinned qualification `bundle-d95a9298e94a1dfc4b1e598bc2445d3f`.
  `ReplayTimingReadProbe.cpp`, its exact compile/link receipt and
  `r025-correlation-initial.*` retain the working diagnostic; publication of
  these new controls belongs to the unfinished correlation slice.
- **Revisit:** Qualify the literal simultaneous-use form separately from
  one-time submission, and establish any pre-frame callback registrations
  explicitly. Unknown setup/recording forms must continue to refuse; neither
  source naming nor a matching row count establishes an active recording.
- **Second control:** After explicit non-draw initial callback links and the
  symbolic simultaneous-use form are admitted, a fresh raw read still returns
  zero draws with `unclosed recording marker`. The retained
  `r025-correlation-initialization-probe/0.*` shows one setup callback resolved.
  In the actual source, recording 161 opens one label, 155 has three balanced
  inner labels, and 162 closes the first label. Application recording 160 and
  the final blit have balanced labels internally. The per-recording balance
  assumption is too narrow; no malformed capture is established.
- **Documented correction boundary:** Vulkan permits command labels to span
  command buffers and queue submissions, with balance judged in one queue's
  submission stream. Interpret that stream explicitly; do not drop the setup
  records. Keep cross-buffer draw timing and interleaved queue/command shapes
  unavailable until separately qualified. See the
  [Khronos command-label reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdEndDebugUtilsLabelEXT.html).
- **Allocation/ownership controls:** The queue-stream reader next refuses a
  missing qualified command-buffer allocation. A timing-only allocation-block
  reader establishes the required primary-buffer metadata without interpreting
  unrelated pipeline/resource definitions or weakening recorded-state queries.
  All three multi-interval snapshots then return 12 private candidate matches
  each. The single-interval snapshot still refuses a missing current recording:
  the prototype discarded every recording after any pool reset. Using each
  allocation's literal pool ownership, rather than global invalidation, makes
  all four retained snapshots return 12 candidates each with no unsupported
  records. The `stream`, `allocation` and `pool` probe directories retain each
  distinct attempt and source/binary identities. These remain private reader
  controls; public bundle/group validation and independent timestamps are not
  established by this result.

### I-068 — New correlation test fixture collides with an imported name

- **Observed/reproduce:** Building the uncommitted initial-recording tests at
  0.5.5 fails because their `setup` string conflicts with the existing state
  fixture's `setup`, introduced by its using-directive. The compiler reports
  ambiguous references; no new test result or Nsight compatibility follows.
- **Correction/evidence:** Rename the new constant `initial_setup_source` and
  rebuild the focused targets. The original compiler diagnostics remain in
  `r025-correlation-initialization-cpu.log`; this is separate from I-067's
  actual source-reader refusal.
- **Revisit:** Keep shared-fixture names explicit when adding new grammar cases.

### I-069 — Pool-reset negative test names an unrelated fixture pool

- **Observed/reproduce:** The added queue-stream lifetime check fails its
  expected-refusal assertion. Its reset uses pool 40, while the shared fixture's
  command buffer is allocated from pool 41. Keeping that recording is correct;
  the test does not demonstrate an interpreter or Nsight defect.
- **Correction/evidence:** Use pool 41 for the owning-pool negative case and
  retain the pool-99 positive case. The corrected check passes. Preserve
  `r025-correlation-stream-lifetime-cpu.log` and its `fixed` counterpart with
  the correlation slice's qualification.
- **Revisit:** Derive negative lifetime scenarios from the independent fixture
  allocation rather than assuming an object number denotes ownership.

### I-070 — Timing bundle reader passes its group budget as a per-file limit

- **Observed/reproduce:** The uncommitted 0.5.5 retained-bundle reader's first
  probe refuses before parsing the C++ or trace with `Fingerprint limit cannot
  exceed 256 MiB`. It passes the 512 MiB aggregate read budget to the existing
  fingerprint API as a per-file limit. The retained capture itself is smaller;
  no corrupt input or Nsight failure is established.
- **Correction/evidence:** Keep the aggregate group budget at 512 MiB and use
  the existing 256 MiB per-file bound for the saved input. Preserve
  `r025-inspection-first.stderr`, the probe source and compile/link receipt in
  the unfinished correlation slice's evidence. Rerun the retained one/three-
  interval groups before claiming the public reader is qualified.
- **Revisit:** Check both API bounds and aggregate budgets when adding bounded
  readers; a larger total allowance does not enlarge a per-file contract.
- **Next control:** With the bound corrected, the reader reaches the source
  report but assumes an unrecorded `capture_settings.format` field. The native
  graphics report records delimiter, frame and interval count; it has no such
  field. `r025-inspection-file-bound.stderr` and the more detailed
  `r025-inspection-diagnostic.stderr` preserve the refusal. Check the existing
  settings and successful hashed capture operation rather than inventing a
  producer field. This is a reader-schema mistake, not a malformed source.

### I-071 — New timing inspection check treats an optional root as a path

- **Observed/reproduce:** The first 0.5.5 CPU inspection-check build fails at
  its installation path expression: `CaptureServiceOptions.nsight_root` is an
  optional path, and has no path division operator. No check executes or GPU
  qualification follows from this failed build.
- **Correction/evidence:** Dereference the test's explicitly assigned root.
  Preserve `r025-inspection-cpu-first-build.log` with the correlation slice's
  evidence, then rebuild and execute the focused checks.
- **Revisit:** Follow the existing options contract when constructing isolated
  test installations.

### I-072 — Focused test invocation omits updated process dependencies

- **Observed/reproduce:** The first 0.5.5 reader/transport CTest run builds the
  check executables but omits the server and Nsight process stand-in. The MCP
  and HTTP checks reject the old server identity; the reader rejects differing
  synthetic C++/trace GPU names. After building the server alone, the remaining
  stale stand-in still produces the GPU mismatch. No real GPU job is run.
- **Correction/evidence:** Build both process dependencies before invoking
  CTest, or use the registered focused `_run` targets, which declare them. Keep
  `r025-inspection-cpu-first-test.log`, `r025-inspection-cpu-second-test.log`
  and their build logs. Do not weaken version or GPU identity checks to admit
  these stale test inputs.
- **Revisit:** A check executable's build target is distinct from its registered
  run target and does not imply all external process dependencies were rebuilt.

### I-073 — New reader checks assume writable copies and a single HTTP interval

- **Observed/reproduce:** With the current process dependencies built, the new
  CPU mutation check cannot rewrite a copy of a sealed published input. Its
  copied owner permissions remain read-only. The HTTP check separately supplies
  one analysis for its existing three-interval capture; the reader correctly
  refuses an incomplete group. The stdio check passes.
- **Correction/evidence:** Make only the test-owned mutation files writable,
  retain an explicit HTTP incomplete-group rejection, and collect its other two
  synthetic analyses before querying the complete group. Preserve
  `r025-inspection-standin-test.log`. These failures concern the new check setup,
  not the protected source bundles or actual GPU correspondence.
- **Revisit:** Keep artifact sealing and complete-group semantics in the tests
  instead of bypassing either product rule.

**Shared retention for I-067–I-073:** Complete/pinned
**bundle-975763dfe5b2d8a5bbc97f0219e6ccec** retains all **483** new qualification
payloads, source/build identities, successful/failed probes, CPU logs and
transport transcripts. Source/imported payload SHA-256 and lengths agree; all
eight compressed executable identities verify. The imported portable verifier
passes 58 schemas, 14 refusals, 48 API-witness/path/raw-time matches, stable
references and the 704 exact native input files retained in already pinned
ancestor **bundle-d95a9298e94a1dfc4b1e598bc2445d3f**. The new slice qualifies
retained literal draw-range association, with original numeric event IDs and
independent per-draw timestamp acceptance still unavailable. R-025 stays In
Progress; no new GPU work or machine-policy change occurs in this reader slice.

### I-074 — New timestamp validation finds its old runtime overlay removed

- **Context/reproduce:** After the committed 0.5.5 reader qualification, the
  uncommitted 0.5.6 per-draw fixture passes its focused CPU contract check. Its
  GPU preflight reuses the earlier process-local 615.71.09 runtime paths from
  `replay-default.json`; both the directory and ICD JSON are now absent.
- **Observed:** Preflight refuses before launching a fixture or Nsight job.
  A separate default `nvidia-smi` query exits 18 with a driver/library version
  mismatch: NVML is 615.78, while `/proc/driver/nvidia/version` still reports
  kernel driver 615.71.09. This does not invalidate the retained offline reader
  qualification or establish a new capture/profiling failure. Existing device/
  trace capability nodes remain readable; no access grant or machine setting
  is changed.
- **Evidence/retention:** `r025-draw-witness-first-preflight.json`,
  `r025-draw-witness-default-driver.json` and
  `r025-draw-witness-capability-access.json` retain exact observations in the
  unfinished timestamp slice's external working evidence. Its qualification
  publication remains pending.
- **Recovery/revisit:** Recreate the already selected matching runtime in an
  ignored validation directory using the
  [official 615.71.09 archive and checksum](https://download.nvidia.com/XFree86/Linux-x86_64/615.71.09/)
  and documented extraction-only operation. Select those files explicitly for
  test processes, with no installer, global driver, display or permission
  change. Verify the checksum, chosen library/ICD identities and an actual
  standalone baseline before attempting serial Nsight validation. A matching
  version string alone does not qualify this new runtime selection.

### I-075 — Standalone direct-draw baseline rejects a desktop-resized window

- **Context/reproduce:** The first 0.5.6 timestamp baseline plan uses
  `timing-direct`, seed 73, 160×96, final frame 5 and `--timing-scope draw`.
  The verified private 615.71.09 runtime is selected explicitly.
- **Observed:** After device creation, the fixture reports that the desktop
  resized the requested window, exits 3/unsupported and confirms owned cleanup.
  No timestamp witness is created. The preceding 192×128 scene/default and
  draw/reuse controls pass; all 96 per-call records validate independently and
  their images are byte-identical. No Nsight profile has been attempted.
- **Evidence/retention:** `r025-draw-witness-baselines` retains the original
  plan, requests, receipts, partial audit, successful witnesses and unsupported
  case logs/result. These belong to the unfinished timestamp qualification.
- **Revisit/control:** Use a new explicitly recorded 224×160 direct-draw input
  in a fresh run. Keep the original unsupported case distinct; do not silently
  resize a workload or relax its pixel/timestamp acceptance. No display setting
  or machine policy change is needed.

**Final retention for I-074/I-075:** Complete/pinned
**bundle-168c040e6fa84e7efd645b16a5413735** retains all original failed controls
alongside the completed 0.5.6 timestamp/repair qualification. Its lossless archive
and per-member inventory preserve the exact logs, plans, receipts and runtime
identities. The imported portable verifier passes. See DRAW_TIMING_VALIDATION.md.

### I-076 — Descriptor hydration rejects an unaligned database block

- **Context/reproduce:** After the owner's explicit two-file extension approval,
  the first uncommitted 0.5.7 confined worker reads the ten retained 0.5.6 C++
  inputs. Exact fingerprints and optional real-helper synthetic checks pass.
  The first analysis input succeeds; input
  `bundle-10b54f1670fcc352f300cdd39e26eec8`, handle 10, write count one,
  output allocation 88, exits 2 during hydration with the combined input-bounds/
  alignment diagnostic. Cleanup is confirmed and stdout is empty.
- **Observed/correction:** The database reader does not guarantee Vulkan
  structure alignment. Copy opaque resource bytes into aligned storage before
  invoking the helper, retaining its relative-offset contract and all range/
  alignment checks. The final measured pointer remainder for this input is four
  bytes. Seven of ten sources have that remainder. All ten corrected reads pass,
  and each full input SHA-256/length matches the separately supervised existing
  byte worker. This establishes the alignment correction for these cases; it
  does not qualify arbitrary descriptor lifetime/type evidence.
- **Evidence:** `r024-worker-probe` preserves its frozen initial worker SHA-256
  `22c0da5e6c1dfadcc03d86e67176db44095f67c452f1f6234deacf618c33f047`,
  original helper/patch/bounds snapshots, harness, failed/successful receipts and
  logs. `r024-worker-aligned` retains the corrected ten-input matrix, exact
  source registrations, opaque-byte comparisons and four expected refusals.
  These external working records await the 0.5.7 qualification publication.
- **Revisit:** Keep relocation and pointer/copy checks together; do not weaken
  alignment checks or equate a database block pointer with an ABI-aligned
  structure. New descriptor shapes and source/tool profiles need their own
  independently checked inputs. No GPU or machine setting change occurs.

**Final I-076 retention:** Complete/pinned
**bundle-8e3eb665326adc38968783d5809c7973** retains all 336 new qualification
payloads and the complete initial/corrected worker attempts. Its imported
portable verifier passes the ten source/opaque-byte comparisons and seven
alignment corrections, plus all source/input hashes against the pinned 0.5.6
ancestor. The 41-check CPU suite passes; R-024 remains In Progress.

### I-077 — Initial descriptor parent build compares strings to JSON values

- **Context/reproduce:** The uncommitted 0.5.8 source parser passes its focused
  CPU check. Building the new `ngm_descriptor_read_check_run` with GCC 16.2.1,
  C++20 and pinned nlohmann/json 3.11.3 fails before the parent process tests run.
- **Observed/correction:** Four fingerprint comparisons place `std::string` on
  the left and a JSON value on the right, for which this configuration has no
  matching equality operator. Extract the already validated string explicitly,
  as the existing byte-reader parent does. Keep the pinned JSON configuration
  and dependency unchanged. No worker, Nsight or GPU compatibility inference
  follows from this compile failure.
- **Evidence:** External working files `r024-parent-first-cpu.log` and
  `r024-parent-initial.DescriptorRead.cpp` retain the original errors/source.
  `r024-parent-typed-cpu.log` records the corrected build/check attempt. These
  belong to the unfinished 0.5.8 source-query slice and await publication.
- **Revisit:** Preserve explicit typed fingerprint comparisons and test the
  parent boundary with stand-ins before actual MCP extraction qualification.

### I-078 — Descriptor MCP selector omits the validator's string bound

- **Context/reproduce:** The uncommitted 0.5.8 parent/source checks pass, including
  quota-failure evidence retention. The new MCP check lists descriptor sources,
  then requests the explicit `unsupported` section.
- **Observed/correction:** The input enum schema omits `maxLength`, which the
  project's bounded argument validator requires for every string. The call
  returns a structured `operation_failed` JSON exception instead of accepting
  the supported selector. Add the same 32-byte bound used by the other source
  query section enums. No parser, helper, GPU or producer limitation is inferred.
- **Evidence:** `r024-service-first-cpu.log` and
  `r024-service-initial.Workflow.cpp` preserve the original check/source. The
  source and parent checks pass in 0.01/30.07 seconds before this MCP failure;
  later transport checks and qualification publication remain pending.
- **Revisit:** Exercise explicit selectors as well as defaults through the
  actual protocol validator before new retained-data MCP qualification.

### I-079 — Descriptor MCP harness restores snapshots before initializing its store

- **Context/reproduce:** After the 0.5.8 stdio/HTTP checks pass, the first
  retained-data MCP harness copies two complete/pinned 0.5.6 native C++ bundles
  into a fresh directory before any product artifact operation opens that root.
- **Observed:** The first source-listing call refuses to adopt a nonempty
  unmarked artifact root. No descriptor helper is invoked, no source changes
  occur and the server closes normally. This is the intended store ownership
  protection, not a capture/parser/reader failure.
- **Correction/evidence:** Preserve `r024-descriptor-mcp`, its frozen binaries,
  harness and transcript. Use a new run directory, initialize its empty store
  through `artifact_usage`, close the server, then restore the trusted snapshots
  and reopen it. Do not forge a marker or weaken the product guard. The new
  `R024DescriptorMcpInitialized.py` records that initialization as an actual call.
- **Revisit:** Keep private snapshot restoration subordinate to the same
  managed-root ownership rules as product use. Qualification remains pending.

### I-080 — Fresh descriptor harness uses the Graphics Capture frame argument

- **Context/reproduce:** The first fresh 0.5.8 C++ capture harness supplies
  `capture_frame: 7`, copied from the Graphics Capture request shape.
- **Observed:** The actual advertised `capture_cpp` schema uses `wait_frames`.
  The independent client validator rejects the extra property before sending a
  tool call; no job or GPU process starts. This is a private harness error.
- **Correction/evidence:** Preserve `r024-fresh-cpp` with the original plan,
  harness, advertised schemas and failed audit. A new `r024-fresh-cpp-wait` run
  uses the demonstrated `wait_frames: 7` control. That control is not promoted
  to an original captured-frame identifier; source labels remain separate.
- **Revisit:** Select request fields from each actual advertised tool schema,
  keeping C++ capture triggers distinct from Graphics Capture frame selection.

### I-081 — Fresh descriptor harness selects an unknown combined scenario

- **Context/reproduce:** The 0.5.8 fresh C++ descriptor qualification uses
  matching Nsight 2026.3.1.0/build 38722833, the separately recorded process-local
  615.71.09 runtime and `wait_frames: 7`. The basic reference capture and its
  uniform write decode succeed. The next request uses `--scenario combined`.
- **Observed:** Nsight exits 1 after the target exits before connection; owned
  process cleanup is confirmed. No C++ project is produced. Running the frozen
  fixture with the exact scenario arguments separately exits 2 with
  `unknown scenario combined`. Its actual help lists `combined-reference`.
  This establishes an invalid private harness input, not a combined Vulkan
  feature or Nsight compatibility failure.
- **Correction/evidence:** Preserve `r024-fresh-cpp-wait`, its failed pinned
  native bundle `bundle-8af345d9df358bd9131a62929335e0f7`, complete CLI logs,
  help and direct argument-refusal receipt. The corrected new
  `r024-fresh-cpp-names` run uses `combined-reference` with the same dimensions,
  seed and capture settings. Do not overwrite the original attempt or replace
  its advanced workload with a simpler one. Qualification publication is pending.
- **Revisit:** Validate scenario spelling against the frozen fixture's actual
  help before launching a new hardware matrix. No machine setting changes.

### I-082 — Source-identity harness supplies an absent toolchain directory

- **Context/reproduce:** After the corrected three-case 0.5.8 descriptor matrix
  passes, a separate private source/fixture oracle harness selects an empty
  toolchain to verify retained queries need no running Nsight process.
- **Observed:** It passes a nonexistent directory. CLI validation refuses the
  path before MCP initialization, with no tool call or worker execution. This
  is a harness setup error, not evidence that source queries require Nsight.
- **Correction/evidence:** Preserve `r024-source-identity` with its stderr,
  transcript and audit. The new `r024-source-identity-directory` run creates an
  actual empty directory before launch; the source bundles and fixture API
  oracle stay unchanged. Qualification publication remains pending.
- **Revisit:** Keep explicit path validation and create selected empty roots
  before server startup; do not relax the CLI's directory guard.

### I-083 — Descriptor evidence collector names a nonexistent prerequisite receipt

- **Context/reproduce:** The final 0.5.8 43-check CPU aggregate and real source
  query matrices pass. The first private lossless evidence collector selects
  the existing process-local runtime/access qualification records.
- **Observed/correction:** Its prerequisite list spells the before/after access
  receipt `capability-access-unchanged.json`; the retained file is
  `capability-unchanged.json`. Collection stops before an archive or product
  import. Preserve the first script, empty output directory and complete error
  log, then select the exact existing filename in a new output directory.
- **Evidence/revisit:** `r024-query-evidence-initial` and
  `r024-query-collection.log` retain the failed collector. This is a private
  evidence-path error; it changes no test, source payload, access permission or
  runtime identity. Verify every selected prerequisite exists before publication.

**Final I-077–I-083 retention:** Complete/pinned
**bundle-52a3d6a428d29f5fad1e043e61fb9983** retains all 343 verified payloads,
including every original failed source/schema/harness/collector attempt and
all 41 native bundle instances. Its imported portable verifier reconstructs all
1,764 file paths and passes 313 total successful schema results, including 231
from final qualified runs. All 43 CPU checks pass in 433.91 seconds. The
0.5.8 query qualifies source descriptor writes; R-024 remains In Progress for
its selected-draw lifetime/type/value and repair acceptance.
