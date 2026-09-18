#include "ngm/Jobs.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <future>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

#include <unistd.h>

namespace ngm {
namespace {
std::atomic<std::uint64_t> coordinator_sequence{0};

JobState state_for(JobOutcome outcome) {
    switch(outcome) {
        case JobOutcome::succeeded:
            return JobState::succeeded;
        case JobOutcome::failed:
            return JobState::failed;
        case JobOutcome::cancelled:
            return JobState::cancelled;
        case JobOutcome::timed_out:
            return JobState::timed_out;
    }
    return JobState::failed;
}

bool valid_identifier(const std::string& value) {
    return !value.empty() && value.size() <= 256 && value.find('\0') == std::string::npos;
}
} // namespace

bool job_terminal(JobState state) noexcept {
    return state == JobState::succeeded || state == JobState::failed || state == JobState::cancelled ||
           state == JobState::timed_out;
}

std::string_view job_state_name(JobState state) noexcept {
    switch(state) {
        case JobState::queued:
            return "queued";
        case JobState::running:
            return "running";
        case JobState::succeeded:
            return "succeeded";
        case JobState::failed:
            return "failed";
        case JobState::cancelled:
            return "cancelled";
        case JobState::timed_out:
            return "timed_out";
    }
    return "unknown";
}

std::string_view job_stop_reason_name(JobStopReason reason) noexcept {
    switch(reason) {
        case JobStopReason::cancellation:
            return "cancellation";
        case JobStopReason::deadline:
            return "deadline";
        case JobStopReason::shutdown:
            return "shutdown";
    }
    return "unknown";
}

struct JobCoordinator::Impl {
    struct Submit {
        JobRequest request;
        std::shared_ptr<JobWorker> worker;
        std::promise<JobIdentity> reply;
    };
    struct Cancel {
        std::string job_id;
        std::promise<JobCancelResult> reply;
    };
    struct Shutdown {};
    struct Returned {
        // Bound by the dispatch wrapper, independently of the worker's payload.
        JobIdentity dispatched_identity;
        JobCompletion completion;
        bool exception = false;
    };
    struct Event {
        JobClock::time_point observed_at;
        std::variant<Submit, Cancel, Shutdown, Returned> value;
    };
    struct Entry {
        JobSnapshot snapshot;
        std::shared_ptr<JobWorker> worker;
        std::stop_source stop;
        std::thread thread;
    };
    struct Finished {
        std::string job_id;
        std::thread thread;
    };
    struct Actions {
        std::vector<std::stop_source> stops;
        std::vector<Finished> finished;
        std::vector<std::shared_ptr<JobWorker>> release;
        std::vector<std::string> finalizing;
    };

    explicit Impl(JobCoordinatorOptions configured) : options(std::move(configured)) {
        if(options.gpu_keys.empty() || options.max_jobs == 0) {
            throw std::invalid_argument("Job coordinator needs configured GPUs and a positive job limit");
        }
        for(const auto& key : options.gpu_keys) {
            if(!valid_identifier(key) || !reservations.emplace(key, std::nullopt).second) {
                throw std::invalid_argument(
                    "Configured GPU keys must be distinct nonempty identifiers (up to 256 bytes)");
            }
        }
        prefix = "job-" + std::to_string(getpid()) + '-' + std::to_string(JobClock::now().time_since_epoch().count()) +
                 '-' + std::to_string(coordinator_sequence.fetch_add(1)) + '-';
        coordinator = std::thread([this] { run(); });
    }

    ~Impl() {
        dispose();
    }

    void dispose() {
        if(coordinator.joinable()) {
            shutdown(std::chrono::milliseconds::zero());
            coordinator.join();
        }
        // Quarantined callbacks still own their resources. Release them while
        // every snapshot/map node is alive: an ownership destructor may inspect
        // a snapshot, just as it can during ordinary callback retirement.
        for(auto& [id, entry] : jobs) {
            (void)id;
            entry.worker.reset();
        }
    }

    JobIdentity submit(JobRequest request) {
        Submit command{std::move(request), {}, {}};
        if(command.request.worker) {
            command.worker = std::make_shared<JobWorker>(std::move(command.request.worker));
        }
        auto reply = command.reply.get_future();
        {
            std::lock_guard lock(mutex);
            if(!accepting) {
                throw std::runtime_error("Job coordinator is shutting down");
            }
            events.push_back({JobClock::now(), std::move(command)});
        }
        changed.notify_all();
        return reply.get();
    }

    JobCancelResult cancel(const std::string& job_id) {
        Cancel command{job_id, {}};
        auto reply = command.reply.get_future();
        {
            std::lock_guard lock(mutex);
            if(!accepting) {
                const auto found = jobs.find(job_id);
                if(found == jobs.end()) {
                    return JobCancelResult::not_found;
                }
                return job_terminal(found->second.snapshot.state) ? JobCancelResult::already_terminal
                                                                  : JobCancelResult::already_requested;
            }
            events.push_back({JobClock::now(), std::move(command)});
        }
        changed.notify_all();
        return reply.get();
    }

    JobShutdownReport shutdown(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex);
        if(accepting) {
            events.push_back({JobClock::now(), Shutdown{}});
            accepting = false;
            changed.notify_all();
        }
        changed.wait_for(lock, timeout, [&] { return coordinator_finished; });
        JobShutdownReport report;
        report.workers_joined = coordinator_finished;
        report.cleanup_confirmed = coordinator_finished;
        for(const auto& [id, entry] : jobs) {
            (void)id;
            if(entry.snapshot.worker_running) {
                report.pending_workers.push_back(entry.snapshot.identity);
            } else if(!entry.snapshot.cleanup_confirmed) {
                report.quarantined_jobs.push_back(entry.snapshot.identity);
            }
            if(!entry.snapshot.cleanup_confirmed) {
                report.cleanup_confirmed = false;
            }
        }
        return report;
    }

    // State transition helpers are called only by run(), with mutex held.
    void retire(Entry& entry, Actions& actions) {
        if(entry.worker) {
            entry.snapshot.finalization_pending = true;
            actions.finalizing.push_back(entry.snapshot.identity.job_id);
            actions.release.push_back(std::move(entry.worker));
        }
    }

    void finish_queued(Entry& entry, JobState state, JobStopReason reason, JobClock::time_point when,
                       Actions& actions) {
        entry.snapshot.state = state;
        entry.snapshot.stop_reason = reason;
        entry.snapshot.finished_at = when;
        retire(entry, actions);
    }

    void request_stop(Entry& entry, JobStopReason reason, Actions& actions) {
        if(!entry.snapshot.stop_reason) {
            entry.snapshot.stop_reason = reason;
            actions.stops.push_back(entry.stop);
        }
    }

    void expire(JobClock::time_point when, Actions& actions) {
        for(auto& [id, entry] : jobs) {
            (void)id;
            if(entry.snapshot.deadline > when) {
                continue;
            }
            if(entry.snapshot.state == JobState::queued) {
                finish_queued(entry, JobState::timed_out, JobStopReason::deadline, entry.snapshot.deadline, actions);
            } else if(entry.snapshot.state == JobState::running) {
                request_stop(entry, JobStopReason::deadline, actions);
            }
        }
    }

    void apply(Submit command, JobClock::time_point when, Actions& actions) {
        // Rejected requests can also carry ownership with user destructors.
        // Keep a copy until outside the state mutex, just as for finished jobs.
        actions.release.push_back(command.worker);
        try {
            if(!valid_identifier(command.request.capture_id)) {
                throw std::invalid_argument("A capture ID must contain 1-256 bytes and no NUL");
            }
            if(!command.worker) {
                throw std::invalid_argument("A job requires a worker");
            }
            if(!reservations.contains(command.request.gpu_key)) {
                throw std::invalid_argument("The requested GPU key is not configured");
            }
            for(const auto& [id, entry] : jobs) {
                (void)id;
                if(entry.snapshot.identity.capture_id == command.request.capture_id) {
                    throw std::invalid_argument("The capture ID already belongs to a job");
                }
            }
            if(jobs.size() >= options.max_jobs) {
                throw std::runtime_error("The retained job limit has been reached");
            }
            Entry entry;
            entry.snapshot.identity = {prefix + std::to_string(next_id++), std::move(command.request.capture_id), 1};
            entry.snapshot.gpu_key = std::move(command.request.gpu_key);
            entry.snapshot.submitted_at = when;
            entry.snapshot.deadline = command.request.deadline;
            entry.worker = std::move(command.worker);
            const auto identity = entry.snapshot.identity;
            queue.push_back(identity.job_id);
            try {
                jobs.emplace(identity.job_id, std::move(entry));
            } catch(...) {
                queue.pop_back();
                throw;
            }
            command.reply.set_value(identity);
        } catch(...) {
            command.reply.set_exception(std::current_exception());
        }
    }

    void apply(Cancel command, JobClock::time_point when, Actions& actions) {
        const auto found = jobs.find(command.job_id);
        auto result = JobCancelResult::not_found;
        if(found != jobs.end()) {
            auto& entry = found->second;
            if(job_terminal(entry.snapshot.state)) {
                result = JobCancelResult::already_terminal;
            } else if(entry.snapshot.stop_reason) {
                result = JobCancelResult::already_requested;
            } else {
                result = JobCancelResult::requested;
                if(entry.snapshot.state == JobState::queued) {
                    finish_queued(entry, JobState::cancelled, JobStopReason::cancellation, when, actions);
                } else {
                    request_stop(entry, JobStopReason::cancellation, actions);
                }
            }
        }
        command.reply.set_value(result);
    }

    void apply(Shutdown, JobClock::time_point when, Actions& actions) {
        stopping = true;
        for(auto& [id, entry] : jobs) {
            (void)id;
            if(entry.snapshot.state == JobState::queued) {
                finish_queued(entry, JobState::cancelled, JobStopReason::shutdown, when, actions);
            } else if(entry.snapshot.state == JobState::running) {
                request_stop(entry, JobStopReason::shutdown, actions);
            }
        }
    }

    void apply(Returned returned, JobClock::time_point when, Actions& actions) {
        const auto found = jobs.find(returned.dispatched_identity.job_id);
        if(found == jobs.end()) {
            return;
        }
        auto& entry = found->second;
        auto& snapshot = entry.snapshot;
        // An envelope for an old/terminal dispatch can never mutate it, including
        // its cleanup state. The internal wrapper publishes exactly once.
        if(snapshot.identity != returned.dispatched_identity || snapshot.state != JobState::running ||
           !snapshot.worker_running || !entry.thread.joinable()) {
            return;
        }
        const bool valid =
            returned.completion.identity == snapshot.identity && returned.completion.expected_state == snapshot.state;
        if(!valid || returned.exception) {
            snapshot.state = JobState::failed;
            snapshot.cleanup_confirmed = false;
            snapshot.error = valid ? std::move(returned.completion.error)
                                   : "Worker completion identity or expected state did not match its dispatch";
        } else {
            snapshot.state = state_for(returned.completion.outcome);
            snapshot.cleanup_confirmed = returned.completion.cleanup_confirmed;
            snapshot.error = std::move(returned.completion.error);
            snapshot.artifact_ids = std::move(returned.completion.artifact_ids);
            if(!snapshot.cleanup_confirmed) {
                snapshot.state = JobState::failed;
                if(!snapshot.error.empty()) {
                    snapshot.error += "; ";
                }
                snapshot.error += "Owned-process cleanup was not confirmed; GPU reservation is quarantined";
            }
        }
        if(snapshot.stop_reason) {
            snapshot.state =
                *snapshot.stop_reason == JobStopReason::deadline ? JobState::timed_out : JobState::cancelled;
        }
        snapshot.finished_at = when;
        actions.finished.push_back({snapshot.identity.job_id, std::move(entry.thread)});
    }

    void schedule(Actions& actions) {
        for(auto queued = queue.begin(); queued != queue.end();) {
            auto& entry = jobs.at(*queued);
            auto& snapshot = entry.snapshot;
            if(snapshot.state != JobState::queued) {
                queued = queue.erase(queued);
                continue;
            }
            const auto now = JobClock::now();
            if(snapshot.deadline <= now) {
                finish_queued(entry, JobState::timed_out, JobStopReason::deadline, snapshot.deadline, actions);
                queued = queue.erase(queued);
                continue;
            }
            auto& reservation = reservations.at(snapshot.gpu_key);
            if(reservation) {
                ++queued;
                continue;
            }
            snapshot.state = JobState::running;
            snapshot.started_at = JobClock::now();
            snapshot.cleanup_confirmed = false;
            snapshot.gpu_reserved = true;
            snapshot.worker_running = true;
            reservation = snapshot.identity.job_id;
            const JobContext context{snapshot.identity, snapshot.gpu_key, snapshot.deadline, entry.stop.get_token()};
            try {
                entry.thread = std::thread([this, context, worker = entry.worker] {
                    Returned returned;
                    returned.dispatched_identity = context.identity;
                    try {
                        returned.completion = (*worker)(context);
                    } catch(const std::exception& error) {
                        returned.exception = true;
                        returned.completion.identity = context.identity;
                        returned.completion.error = std::string("Job worker threw: ") + error.what();
                    } catch(...) {
                        returned.exception = true;
                        returned.completion.identity = context.identity;
                        returned.completion.error = "Job worker threw an unknown exception";
                    }
                    {
                        std::lock_guard lock(mutex);
                        events.push_back({JobClock::now(), std::move(returned)});
                    }
                    changed.notify_all();
                });
            } catch(const std::exception& error) {
                snapshot.state = JobState::failed;
                snapshot.finished_at = JobClock::now();
                snapshot.error = std::string("Could not start job worker: ") + error.what();
                snapshot.worker_running = false;
                snapshot.cleanup_confirmed = true;
                snapshot.gpu_reserved = false;
                reservation.reset();
                retire(entry, actions);
            }
            queued = queue.erase(queued);
        }
    }

    JobClock::time_point next_deadline() const {
        auto result = JobClock::time_point::max();
        for(const auto& [id, entry] : jobs) {
            (void)id;
            if(entry.snapshot.state == JobState::queued ||
               (entry.snapshot.state == JobState::running && !entry.snapshot.stop_reason)) {
                result = std::min(result, entry.snapshot.deadline);
            }
        }
        return result;
    }

    bool any_workers() const {
        return std::any_of(jobs.begin(), jobs.end(),
                           [](const auto& job) { return job.second.snapshot.worker_running; });
    }

    void run() {
        for(;;) {
            Actions actions;
            {
                std::unique_lock lock(mutex);
                while(!events.empty()) {
                    auto event = std::move(events.front());
                    events.pop_front();
                    // Commands and completions arbitrate at their serialized
                    // mailbox observation time. Deadline wins an exact tie.
                    expire(event.observed_at, actions);
                    std::visit([&](auto command) { apply(std::move(command), event.observed_at, actions); },
                               std::move(event.value));
                }
                expire(JobClock::now(), actions);
                if(!stopping) {
                    schedule(actions);
                }
                changed.notify_all();
                if(actions.stops.empty() && actions.finished.empty() && actions.release.empty()) {
                    if(stopping && !any_workers()) {
                        coordinator_finished = true;
                        changed.notify_all();
                        return;
                    }
                    changed.wait_until(lock, next_deadline());
                    continue;
                }
            }
            // User stop callbacks, thread destruction and ownership destructors
            // run without the state mutex. A worker may safely read snapshots.
            for(auto& source : actions.stops) {
                source.request_stop();
            }
            for(auto& finished : actions.finished) {
                finished.thread.join();
                std::lock_guard lock(mutex);
                auto& entry = jobs.at(finished.job_id);
                entry.snapshot.worker_running = false;
                if(entry.snapshot.cleanup_confirmed) {
                    reservations.at(entry.snapshot.gpu_key).reset();
                    entry.snapshot.gpu_reserved = false;
                    retire(entry, actions);
                }
                changed.notify_all();
            }
            // A queued job may publish its failed-attempt evidence when its
            // never-invoked callable is released. Keep waiters in finalization
            // until these destructors finish, without holding the state mutex.
            actions.release.clear();
            {
                std::lock_guard lock(mutex);
                for(const auto& id : actions.finalizing) {
                    jobs.at(id).snapshot.finalization_pending = false;
                }
                changed.notify_all();
            }
        }
    }

    JobCoordinatorOptions options;
    std::string prefix;
    std::uint64_t next_id = 1;
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    std::deque<Event> events;
    std::map<std::string, Entry> jobs;
    std::deque<std::string> queue;
    std::map<std::string, std::optional<std::string>> reservations;
    bool accepting = true;
    bool stopping = false;
    bool coordinator_finished = false;
    std::thread coordinator;
};

JobCoordinator::JobCoordinator(JobCoordinatorOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
JobCoordinator::~JobCoordinator() {
    // Ownership destructors may call snapshot(). Keep the facade's unique_ptr
    // alive while disposing; some standard libraries clear it before invoking
    // the pointee destructor.
    impl_->dispose();
}

JobIdentity JobCoordinator::submit(JobRequest request) {
    return impl_->submit(std::move(request));
}

JobCancelResult JobCoordinator::cancel(const std::string& job_id) {
    return impl_->cancel(job_id);
}

std::optional<JobSnapshot> JobCoordinator::snapshot(const std::string& job_id) const {
    std::lock_guard lock(impl_->mutex);
    const auto found = impl_->jobs.find(job_id);
    return found == impl_->jobs.end() ? std::nullopt : std::optional(found->second.snapshot);
}

std::optional<JobSnapshot> JobCoordinator::wait(const std::string& job_id, std::chrono::milliseconds timeout) const {
    std::unique_lock lock(impl_->mutex);
    const auto found = impl_->jobs.find(job_id);
    if(found == impl_->jobs.end()) {
        return std::nullopt;
    }
    impl_->changed.wait_for(lock, timeout, [&] {
        return job_terminal(found->second.snapshot.state) && !found->second.snapshot.worker_running &&
               !found->second.snapshot.finalization_pending;
    });
    return found->second.snapshot;
}

JobShutdownReport JobCoordinator::shutdown(std::chrono::milliseconds timeout) {
    return impl_->shutdown(timeout);
}
} // namespace ngm
