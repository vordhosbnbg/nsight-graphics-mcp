#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace ngm {
using JobClock = std::chrono::steady_clock;

struct JobIdentity {
    std::string job_id;
    std::string capture_id;
    std::uint64_t attempt = 1;
    bool operator==(const JobIdentity&) const = default;
};

enum class JobState { queued, running, succeeded, failed, cancelled, timed_out };
enum class JobOutcome { succeeded, failed, cancelled, timed_out };
enum class JobStopReason { cancellation, deadline, shutdown };
enum class JobCancelResult { requested, already_requested, already_terminal, not_found };

bool job_terminal(JobState state) noexcept;
std::string_view job_state_name(JobState state) noexcept;
std::string_view job_stop_reason_name(JobStopReason reason) noexcept;

struct JobContext {
    JobIdentity identity;
    std::string gpu_key;
    JobClock::time_point deadline;
    std::stop_token stop;
};

// Returned only after the worker has finished using its owned processes. A
// successful operation still must explicitly confirm cleanup. The coordinator
// validates the complete identity and expected state before accepting evidence.
struct JobCompletion {
    JobIdentity identity;
    JobState expected_state = JobState::running;
    JobOutcome outcome = JobOutcome::failed;
    bool cleanup_confirmed = false;
    std::string error;
    std::vector<std::string> artifact_ids;
};

using JobWorker = std::function<JobCompletion(const JobContext&)>;

struct JobRequest {
    // Required and unique for the lifetime of this coordinator. Attempts are
    // currently always 1: a retry is a fresh job and capture, never a reused app.
    std::string capture_id;
    std::string gpu_key = "default";
    // Includes time spent queued. An already-expired request never launches.
    JobClock::time_point deadline = JobClock::now() + std::chrono::seconds(30);
    JobWorker worker;
};

struct JobSnapshot {
    JobIdentity identity;
    std::string gpu_key;
    JobState state = JobState::queued;
    JobClock::time_point submitted_at;
    JobClock::time_point deadline;
    std::optional<JobClock::time_point> started_at;
    std::optional<JobClock::time_point> finished_at;
    // A running job stays running during cleanup. The first stop reason wins,
    // and a subsequent worker success cannot override it.
    std::optional<JobStopReason> stop_reason;
    bool worker_running = false;
    // The outcome is fixed, but callable-owned evidence/resources are still
    // being retired outside the state mutex. wait() includes this phase.
    bool finalization_pending = false;
    bool cleanup_confirmed = true; // Queued jobs own no processes.
    bool gpu_reserved = false;
    std::string error;
    std::vector<std::string> artifact_ids;
};

struct JobCoordinatorOptions {
    // One worker per configured GPU, FIFO within each GPU's queue. Unknown keys
    // are rejected, so requests cannot create an unbounded pool of workers.
    std::vector<std::string> gpu_keys{"default"};
    // Includes retained terminal snapshots; reaching the limit rejects submit.
    std::size_t max_jobs = 4096;
};

struct JobShutdownReport {
    bool workers_joined = false;
    bool cleanup_confirmed = false;
    std::vector<JobIdentity> pending_workers;
    std::vector<JobIdentity> quarantined_jobs;
};

// Thread-safe facade; only one internal coordinator thread mutates job states.
// Workers must poll their stop token and return after bounded cleanup. Do not
// install blocking stop callbacks or call submit/cancel/shutdown on this same
// coordinator from a stop callback. No callback is detached. shutdown(timeout)
// can return with pending workers; destruction then waits for those callbacks
// and joins them. Arbitrary C++ callbacks cannot be forcefully stopped safely.
//
// Retain artifact leases/other ownership in the worker's captures. The original
// callable is retained until cleanup is confirmed, or until coordinator
// destruction for quarantined work. Quarantine has no automatic release or
// in-process override: future same-GPU work remains queued until its deadline
// or cancellation. Persisted artifact recovery belongs to the artifact store.
class JobCoordinator {
public:
    explicit JobCoordinator(JobCoordinatorOptions options = {});
    ~JobCoordinator();
    JobCoordinator(const JobCoordinator&) = delete;
    JobCoordinator& operator=(const JobCoordinator&) = delete;

    // Throws invalid_argument for invalid/reused capture IDs, missing workers,
    // or unknown GPUs; runtime_error for shutdown or the retained-job limit.
    JobIdentity submit(JobRequest request);
    JobCancelResult cancel(const std::string& job_id);
    std::optional<JobSnapshot> snapshot(const std::string& job_id) const;
    // Waits for a terminal state with its callback joined and pending ownership
    // finalization finished, or returns the latest value at timeout; nullopt is
    // an unknown ID. A joined terminal callback
    // alone does not imply confirmed cleanup of its owned processes.
    std::optional<JobSnapshot> wait(const std::string& job_id, std::chrono::milliseconds timeout) const;
    JobShutdownReport shutdown(std::chrono::milliseconds timeout);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngm
