# Asynchronous job coordinator

`ngm::JobCoordinator` is the process-independent R-011 coordinator core. Its
typed API is in `include/ngm/Jobs.hpp`; the implementation is in
`src/jobs/Jobs.cpp`. The core does not claim Nsight capture compatibility.
Full R-011 acceptance also requires the capture adapter, artifact leases,
MCP status/cancellation, and a real Nsight capture through this lifecycle.

## Dispatch and ownership

Submit a `JobRequest` with a unique capture ID, configured GPU key, absolute
`steady_clock` deadline, and worker callable. The deadline includes queue time;
the request default is 30 seconds from construction. The default GPU key is
`default`; other keys must be configured when constructing the coordinator.
Exactly one worker can own each GPU. Queues preserve submission order within a
GPU, while another configured GPU can run independently.

Submission returns a `JobIdentity` containing a generated job ID, the supplied
capture ID, and attempt 1. Capture IDs cannot be reused within the coordinator.
There are no reusable application sessions or in-place retries. Each worker
must launch its fresh target through `run_process`, using argument arrays and
separate stdout/stderr log files. The default limit is 4,096 retained jobs,
including terminal snapshots; reaching it rejects new work explicitly.

The worker receives the complete identity, GPU key, deadline, and stop token.
For each process invocation, compute its remaining timeout from that deadline
and pass the context's stop token to `run_process`. Return `JobCompletion` only
after process supervision and evidence work finish. A successful process exit
does not prove expected exports exist or are valid; that validation belongs
to the capture adapter. `artifact_ids` are adapter-owned references; the core
does not inspect bundle manifests or interpret artifact contents.

Retain a shared artifact usage lease and any cleanup ownership in the callable's
captures. The coordinator retains the original callable until cleanup is
confirmed, including after its worker thread returns. If cleanup cannot be
confirmed, it retains the callable and GPU reservation for its entire remaining
lifetime. Do not put the only lease in a callback-local variable or explicitly
release it before returning an unconfirmed cleanup result. Artifact-store
restart reconciliation remains necessary when the process exits.

## State and completion rules

One coordinator thread applies all job-state transitions. Workers return
records through a private mailbox; they never mutate a job's state. Snapshots
are independent value copies, safe to inspect from other threads.

| State | Meaning |
| --- | --- |
| `queued` | No worker or target launched; no GPU reservation. |
| `running` | Worker owns its GPU, including time spent cancelling or cleaning up. |
| `succeeded` | Accepted successful completion with confirmed process cleanup. |
| `failed` | Failed operation, invalid completion, worker exception, or unconfirmed cleanup. |
| `cancelled` | Cancellation or shutdown won before completion. |
| `timed_out` | Deadline won before completion, or the worker reported a timeout. |

For running work, cancellation, deadline expiry, or shutdown sets the first
`stop_reason` and requests the worker's stop token. The state remains `running`
while cleanup executes. Later success cannot override the selected stop reason.
Queued cancellations and expired queued deadlines become terminal without
launching a worker. Commands and completions arbitrate at their serialized
mailbox observation time; deadline expiry wins an exact timestamp tie. A worker
return that has not reached the mailbox before its deadline is late.

The dispatch wrapper binds its own immutable identity to the returned record.
The coordinator checks the job, capture, attempt, and expected `running` state
against that dispatch. A stale envelope cannot change terminal state or cleanup
status. A returned payload with a mismatched identity/state fails its own
dispatch conservatively, without attaching that payload's artifact references.
The wrapper publishes once; duplicate prior payloads returned by a later worker
also fail identity validation. Worker exceptions similarly leave cleanup
unconfirmed, because a thrown exception does not prove there are no descendants.

`cleanup_confirmed`, `gpu_reserved`, and `worker_running` are explicit snapshot
fields. A terminal outcome alone is insufficient to reuse a GPU. The coordinator
joins the callback and releases the reservation only after a valid completion
confirms cleanup. Unconfirmed cleanup quarantines the reservation; same-GPU
requests remain queued until their own deadline, cancellation, or shutdown.
There is no automatic retry or in-process quarantine override. Other configured
GPUs remain available. If cancellation or timeout already won, that terminal
outcome remains stable even when the cleanup result is unconfirmed; the flags
and error retain the cleanup failure.

`finalization_pending` reports callable-owned evidence/resource retirement after
the outcome is fixed. These ownership destructors run outside the state mutex;
a never-launched capture can publish its failed evidence during this phase.
`wait(job_id, timeout)` waits for terminal state, a joined callback, and completed
pending finalization, returning the latest snapshot if its wait expires. Retained
quarantine ownership is not pending retirement and remains protected.
`snapshot` performs no wait. Unknown
IDs return no snapshot; cancellation distinguishes unknown IDs, a first accepted
request, a previously requested stop, and an already-terminal job.

## Shutdown and limits

`shutdown(timeout)` closes submission, cancels queued jobs, requests stop for
running jobs, and waits up to the supplied duration. Its report distinguishes
pending worker callbacks from joined jobs whose process cleanup remains
quarantined. Repeating shutdown can observe later cleanup. All callbacks joined
does **not** imply all processes cleaned up.

No callback is detached, and no late callback accesses a destroyed coordinator.
Destruction requests shutdown and joins the coordinator and all worker threads.
Consequently, destruction waits for a noncooperative C++ callback; arbitrary
callbacks cannot be safely force-stopped within a guaranteed wall-clock bound.
Production adapters must use bounded process supervision and cooperative
evidence operations. Stop callbacks must be short and nonblocking, and must
not synchronously invoke this coordinator's `submit`, `cancel`, or `shutdown`.
No user callback or retained ownership destructor executes under the state
mutex, so snapshot inspection from a worker or stop callback is supported.

The existing Linux process layer performs bounded TERM/KILL/reaping and reports
its actual `cleanup_confirmed` result. Its documented limitations, including
uninterruptible kernel waits and processes created through external services,
still apply. The coordinator never turns those limitations into a claim of
successful cleanup.

## CPU verification

`tests/JobsCheck.cpp` covers immutable snapshots, unique identities and limits,
FIFO GPU scheduling, independent GPUs, queued cancellation and expiry,
cancellation/success/deadline races, malformed and duplicate completion
payloads, worker exceptions, quarantine and retained ownership, shutdown
timeouts, and joined destruction. Condition-variable/barrier handshakes control
state races; filesystem notifications wait for executable readiness.

The existing C++ process stand-in also runs through the coordinator and real
`run_process` boundary. Cases cover distinct application launches and logs,
successful/early-failed/failed-exec outcomes, missing and malformed exports,
timeouts, cancellation, and shutdown. Target/child/grandchild PIDs are checked
after cleanup, including escaped process groups, while an unrelated sentinel
process remains alive. These checks establish coordinator/process behavior;
they do not establish Nsight or export-schema compatibility.

On 2026-09-18, an isolated GCC C++20 build with warnings as errors passed the
complete `JobsCheck` executable against a separately built `ProcessStandin`.
The same complete check passed with AddressSanitizer and UndefinedBehaviorSanitizer.
`cmake/Jobs.cmake` supplies `ngm_add_jobs()` and `ngm_add_jobs_checks()` for the
repository build integration. The integrated 20-check GCC Debug aggregate and
real capture matrices on two matching Nsight releases now pass, completing
R-011 in the 0.2.0 capture/evidence group after independent acceptance review.
The real matrices demonstrate distinct target launches, confirmed cleanup,
released reservations, separate evidence, and normal MCP shutdown. Exact
results and limits are in [BUILD_VALIDATION.md](BUILD_VALIDATION.md) and
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md).
