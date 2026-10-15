# Retained-capture GPU replay

At **0.5.2**, `replay` submits a finite standalone GPU replay of a complete
service Graphics Capture. It shares the capture/profile job coordinator,
GPU reservation, deadlines, cancellation, shutdown and artifact publication.
The original application is not launched. The matching qualified producer is
**Nsight 2026.3.1.0/build 38722833** on the measured Linux desktop path.
Nsight 2026.2 GPU replay remains unqualified after the historical timeouts.

## MCP contract

Both stdio and authenticated loopback HTTP expose the same request:

```json
{
  "capture_id": "bundle-<32 hexadecimal characters>",
  "loop_count": 1,
  "timeout_ms": 120000,
  "pin": true
}
```

`capture_id` names a complete native Graphics Capture in this server's artifact
store. Imports, profiles, generated C++ captures, failed attempts and replay
results are refused. `loop_count` is **1..600**, default 1; `timeout_ms` is
**1..600000**, default 120000; `pin` defaults to false. The optional closed
`launch_environment` overlay follows [LAUNCH_ENVIRONMENT.md](LAUNCH_ENVIRONMENT.md).
Choose the installed tools with the existing server `--nsight-root` option.

The result contains `identity` and `artifact_id`, matching capture submissions.
Poll `job_status` until the worker has retired and cleanup is confirmed.
Submission alone does not establish replay success. `job_cancel` and service
shutdown use the existing owned-process cleanup path; another capture/profile
cannot acquire the GPU until cleanup is confirmed.

The backend uses the public `ngfx-replay` launcher with exactly
`--loop-count=N --present-app --verbose INPUT`. It requires those options in
matching help, exit zero, confirmed cleanup and the qualified GPU initialization,
execution-engine and successful-completion messages. Empty or metadata-only
output cannot establish success. No original application arguments are executed.

## Source identity and retained evidence

A source lease starts before submission validation and lasts through queued and
running work. Unpinning the source cannot make it eligible for pruning while the
job still uses it. Source protection is temporary; pin sources needed after the
job completes separately from pinning the result.

Each job snapshots fixed inventoried source paths with no symlink traversal and
cancellable streaming copies. Capture payloads are bounded to **1 GiB**; report
and metadata inputs are bounded to **16 MiB** each. The result retains:

- `raw/input/capture.ngfx-capture`, `metadata.raw` and `report.json`;
- fresh matching metadata in `raw/exports/metadata.raw`;
- isolated tool-discovery logs and `raw/logs/replay/replay.stdout.txt` / stderr;
- `raw/report.json` with source/job identities, tool observations, selected
  environment, snapshot hashes, process outcome and cleanup.

The fresh metadata must agree with retained capture identity fields, including
any UUID, producer version/build, API, GPU, driver, process, request time,
interval and resolution. Newly collected Graphics Captures record a payload
SHA-256; replay checks its snapshot against that hash. Legacy captures without
that field remain eligible, but the result explicitly reports that original-byte
authenticity is unverified. Their retained header/report hashes, fresh metadata
and observed snapshot hash are checked. Recorded provenance is not producer
authentication. A successful replay also verifies that its input snapshot did
not change during execution.

Failures retain available snapshots and logs in failed bundles. Unconfirmed
cleanup uses quarantine and keeps GPU ownership. Results are managed by the same
quota, pin and restart policies as other artifacts. The 256 KiB accepted stdout
observation limit does not impose a live subprocess disk-write quota.

## Measured qualification and image scope

The development host uses RTX 3080 Ti, kernel driver **615.71.09**, KDE Wayland
and XCB/Xwayland presentation. The process-local overlay selects the retained
matching 615.71 driver libraries/ICD instead of the mismatched default NVML.
No driver, permission, display, clock or other machine setting is changed.
The older initialization failures do not establish the cause of this host's
historical stalls; they remain in [INVESTIGATIONS.md](INVESTIGATIONS.md).

Three actual **0.5.2** MCP replay jobs succeed against separately retained
reference, repeated-reference and faulty **0.5.1** captures, with three loops
each, in **3.851–3.976 seconds**. All inputs match their retained source hashes;
source payloads and pins remain unchanged. Two restarts preserve result reports
and pins. **73** actual successful tool results validate against advertised
schemas. Another actual MCP run observes the execution-engine phase before
cancellation, deadline expiry and EOF shutdown: all three retain failed/pinned
bundles with confirmed cleanup. Its **442** successful tool results validate
against the same schemas.

Independent earlier **0.5.1** probes provide the rendered-output oracle on the
same matching Nsight tools and input captures. Three plain replays and four
capture-of-replay controls succeed. For the fixed seed-42, 192×128 fixture,
healthy/repeated/faulty replay images exactly match independent application
**frame 1** readbacks. The source's embedded screenshot instead matches
**frame 2**. A no-present-blit control changes the fresh replay output. I-050 and
I-051 retain the failed shell-launch verification and initial comparison mismatch,
including their bounded corrections. All **340** probe payloads verify in
complete/pinned **bundle-9592f2cd01c4623855009d7589ed37af**, and all eight native
probe attempts are independently pinned.

Tool-reported execution completion does not itself prove pixels, general
application-frame arithmetic, executed descriptor state, draw correspondence or
replay timing accuracy. The public replay result makes none of those inferences.
Unknown applications need their own equivalent-input output oracle.

All **37** serial CPU checks pass in **381.19 seconds**, including the new
`ngm_replay_workflow_check`. This exercises source mutation, evidence-origin and
producer refusals, invalid bounds, queued retention/cancellation/deadlines,
shared capture/profile serialization, escaped-child cleanup, shutdown and actual
stdio calls. The shared HTTP check also submits and inspects a replay.
Final MCP and CLI ELF dependencies remain the documented system C/C++/math
runtimes; no vendored shared-library dependency is introduced.

Complete/pinned **bundle-1c59130aafc0cb1a946cd9e0c4da869c** retains **741** verified
payloads, including the full nine-bundle native store snapshot, independent image
probes, frozen tested server/source, transcripts, verifiers and CPU/ELF logs.
The final/collected server SHA-256 is
`d1f7c3d7577047c00e5e9d2493d0d41a375b336142813cf20909beef19b9744e`.
Both source and imported payload lengths/hashes pass; the portable imported
qualification verifier passes. The installed **0.4.1** importer is separate from
the tested **0.5.2** product. Native sources/results remain independently pinned.
