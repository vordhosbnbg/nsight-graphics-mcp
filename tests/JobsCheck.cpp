#include "Check.hpp"
#include "ngm/Jobs.hpp"
#include "ngm/Process.hpp"

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using ngm::check::require;
using namespace std::chrono_literals;

class Scratch {
public:
    Scratch() {
        auto pattern = (std::filesystem::temp_directory_path() / "ngm jobs check XXXXXX").string();
        const auto directory = mkdtemp(pattern.data());
        require(directory != nullptr, "create isolated job check directory");
        path = directory;
    }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

template <typename Exception, typename Function>
void require_throws(Function&& function, std::string_view message) {
    try {
        function();
    } catch(const Exception&) {
        return;
    }
    require(false, message);
}

ngm::JobCompletion success(const ngm::JobContext& context) {
    ngm::JobCompletion result;
    result.identity = context.identity;
    result.outcome = ngm::JobOutcome::succeeded;
    result.cleanup_confirmed = true;
    result.artifact_ids = {context.identity.capture_id + "-evidence"};
    return result;
}

ngm::JobRequest request(std::string capture, ngm::JobWorker worker = success, std::string gpu = "default") {
    ngm::JobRequest result;
    result.capture_id = std::move(capture);
    result.gpu_key = std::move(gpu);
    result.deadline = ngm::JobClock::now() + 10s;
    result.worker = std::move(worker);
    return result;
}

ngm::JobSnapshot finished(ngm::JobCoordinator& coordinator, const ngm::JobIdentity& identity) {
    const auto result = coordinator.wait(identity.job_id, 5s);
    require(result && ngm::job_terminal(result->state) && !result->worker_running && !result->finalization_pending,
            "job reached a joined and finalized terminal state");
    require(result->identity == identity, "snapshot retains exact submitted job, capture, and attempt identity");
    return *result;
}

void require_clean(const ngm::JobSnapshot& result, ngm::JobState expected) {
    require(result.state == expected,
            "expected job state, observed " + std::string(ngm::job_state_name(result.state)) + ": " + result.error);
    require(result.cleanup_confirmed && !result.gpu_reserved && !result.worker_running,
            "clean completion releases the reservation after joining the worker");
}

// Both phases have explicit condition-variable handshakes. A failed test cannot
// leave its coordinator destructor waiting forever on this test-only callback.
class ControlledWorker {
public:
    ngm::JobCompletion run(const ngm::JobContext& context, bool honor_stop = true) {
        std::unique_lock lock(mutex_);
        entered_ = true;
        changed_.notify_all();
        bool ready;
        if(honor_stop) {
            ready = changed_.wait_for(lock, context.stop, 4s, [&] { return released_; });
            if(context.stop.stop_requested()) {
                stopped_ = true;
                changed_.notify_all();
                ready = changed_.wait_for(lock, 4s, [&] { return released_; });
            }
        } else {
            ready = changed_.wait_for(lock, 4s, [&] { return released_; });
        }
        auto result = success(context);
        if(!ready) {
            result.outcome = ngm::JobOutcome::failed;
            result.error = "Test worker did not receive its release handshake";
        }
        return result;
    }

    void entered() {
        std::unique_lock lock(mutex_);
        require(changed_.wait_for(lock, 3s, [&] { return entered_; }),
                "worker reached its controlled execution boundary");
    }

    void stopped() {
        std::unique_lock lock(mutex_);
        require(changed_.wait_for(lock, 3s, [&] { return stopped_; }), "worker observed its stop token before cleanup");
    }

    void release() {
        std::lock_guard lock(mutex_);
        released_ = true;
        changed_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable_any changed_;
    bool entered_ = false;
    bool stopped_ = false;
    bool released_ = false;
};

void check_contract() {
    require_throws<std::invalid_argument>([] { ngm::JobCoordinator invalid({{}, 1}); },
                                          "empty GPU configuration rejected");
    require_throws<std::invalid_argument>([] { ngm::JobCoordinator invalid({{"same", "same"}, 1}); },
                                          "duplicate GPU configuration rejected");
    require_throws<std::invalid_argument>([] { ngm::JobCoordinator invalid({{"default"}, 0}); },
                                          "zero job limit rejected");
    ngm::JobCoordinator coordinator({{"default"}, 2});
    require(!coordinator.snapshot("absent") && !coordinator.wait("absent", 0ms), "unknown jobs have no snapshot");
    require(coordinator.cancel("absent") == ngm::JobCancelResult::not_found, "unknown cancellation is explicit");
    for(const auto& capture : {std::string(), std::string("bad\0id", 6), std::string(257, 'x')}) {
        require_throws<std::invalid_argument>([&] { coordinator.submit(request(capture)); },
                                              "invalid capture ID rejected");
    }
    require_throws<std::invalid_argument>([&] { coordinator.submit(request("no-worker", {})); },
                                          "missing worker rejected");
    require_throws<std::invalid_argument>([&] { coordinator.submit(request("gpu", success, "unknown")); },
                                          "unconfigured GPU rejected");
    const auto first = coordinator.submit(request("first"));
    const auto first_result = finished(coordinator, first);
    require_clean(first_result, ngm::JobState::succeeded);
    require(first_result.artifact_ids == std::vector<std::string>{"first-evidence"},
            "valid completion attaches its evidence");
    auto copied = first_result;
    copied.state = ngm::JobState::failed;
    copied.artifact_ids.clear();
    require(coordinator.snapshot(first.job_id)->state == ngm::JobState::succeeded &&
                coordinator.snapshot(first.job_id)->artifact_ids == first_result.artifact_ids,
            "snapshot values cannot mutate coordinator state");
    require(coordinator.cancel(first.job_id) == ngm::JobCancelResult::already_terminal,
            "later cancellation cannot replace accepted success");
    require_throws<std::invalid_argument>([&] { coordinator.submit(request("first")); }, "capture ID cannot be reused");
    const auto second = coordinator.submit(request("second"));
    require(first.job_id != second.job_id && first.capture_id != second.capture_id && second.attempt == 1,
            "jobs and captures have distinct identities and one fresh attempt");
    require_clean(finished(coordinator, second), ngm::JobState::succeeded);
    require_throws<std::runtime_error>([&] { coordinator.submit(request("capacity")); },
                                       "retained job limit is enforced");
    const auto stopped = coordinator.shutdown(2s);
    require(stopped.workers_joined && stopped.cleanup_confirmed && stopped.pending_workers.empty() &&
                stopped.quarantined_jobs.empty(),
            "clean shutdown reports no unresolved ownership");
    require_throws<std::runtime_error>([&] { coordinator.submit(request("after-shutdown")); },
                                       "shutdown closes submission");
}

void check_ownership_destructors() {
    struct Owner {
        ngm::JobCoordinator& coordinator;
        std::atomic<bool>& destroyed;
        ~Owner() {
            (void)coordinator.snapshot("unknown");
            destroyed = true;
        }
    };
    for(const bool valid : {false, true}) {
        ngm::JobCoordinator coordinator;
        std::atomic<bool> destroyed = false;
        auto owner = std::make_shared<Owner>(coordinator, destroyed);
        auto submission = request(valid ? "valid" : "", [owner](const auto& context) {
            (void)owner;
            return success(context);
        });
        owner.reset();
        if(valid) {
            require_clean(finished(coordinator, coordinator.submit(std::move(submission))), ngm::JobState::succeeded);
        } else {
            require_throws<std::invalid_argument>([&] { coordinator.submit(std::move(submission)); },
                                                  "invalid request with retained ownership is rejected");
        }
        require(coordinator.shutdown(2s).workers_joined && destroyed,
                "accepted and rejected callable ownership destructors can inspect coordinator snapshots");
    }
    struct QuarantinedOwner {
        ngm::JobCoordinator& coordinator;
        std::string& observed_job;
        bool& observed;
        ~QuarantinedOwner() {
            const auto snapshot = coordinator.snapshot(observed_job);
            observed = snapshot && snapshot->state == ngm::JobState::succeeded;
        }
    };
    bool observed = false;
    std::string observed_job;
    {
        ngm::JobCoordinator coordinator({{"a", "b"}, 10});
        auto owner = std::make_shared<QuarantinedOwner>(coordinator, observed_job, observed);
        auto quarantined = request(
            "quarantined-owner",
            [owner](const auto& context) {
                (void)owner;
                auto completion = success(context);
                completion.cleanup_confirmed = false;
                return completion;
            },
            "a");
        owner.reset();
        const auto held = finished(coordinator, coordinator.submit(std::move(quarantined)));
        require(held.gpu_reserved && !held.cleanup_confirmed, "quarantine retains callable ownership");
        require_clean(finished(coordinator, coordinator.submit(request("clean-one", success, "b"))),
                      ngm::JobState::succeeded);
        const auto last = coordinator.submit(request("clean-two", success, "b"));
        observed_job = last.job_id;
        require_clean(finished(coordinator, last), ngm::JobState::succeeded);
        require(!observed, "quarantine does not release ownership while the coordinator remains alive");
    }
    require(observed, "quarantine owner destruction can inspect every snapshot before map teardown");
}

void check_finalization() {
    struct Owner {
        ngm::JobCoordinator& coordinator;
        std::promise<void>& entered;
        std::shared_future<void> release;
        std::atomic<bool>& retired;
        ~Owner() {
            (void)coordinator.snapshot("unknown");
            entered.set_value();
            retired = release.wait_for(3s) == std::future_status::ready;
        }
    };
    for(const bool queued : {false, true}) {
        ControlledWorker running;
        std::promise<void> entered;
        auto entered_future = entered.get_future();
        std::promise<void> release;
        std::atomic<bool> retired = false;
        ngm::JobCoordinator coordinator;
        std::optional<ngm::JobIdentity> blocker;
        if(queued) {
            blocker = coordinator.submit(request("blocker", [&](const auto& context) { return running.run(context); }));
            running.entered();
        }
        auto owner = std::make_shared<Owner>(coordinator, entered, release.get_future().share(), retired);
        auto submission = request("finalization", [owner](const auto& context) {
            (void)owner;
            return success(context);
        });
        owner.reset();
        const auto identity = coordinator.submit(std::move(submission));
        if(queued) {
            require(coordinator.cancel(identity.job_id) == ngm::JobCancelResult::requested,
                    "cancel a never-launched job with retained evidence ownership");
        }
        require(entered_future.wait_for(3s) == std::future_status::ready,
                "ownership retirement reached its controlled boundary");
        const auto pending = coordinator.wait(identity.job_id, 0ms);
        require(pending && ngm::job_terminal(pending->state) && !pending->worker_running &&
                    pending->finalization_pending,
                "terminal outcome explicitly retains pending evidence finalization");
        require(!retired, "a timed-out wait cannot imply completed ownership retirement");
        release.set_value();
        require_clean(finished(coordinator, identity), queued ? ngm::JobState::cancelled : ngm::JobState::succeeded);
        require(retired, "a completed wait includes callable-owned evidence retirement");
        if(blocker) {
            running.release();
            require_clean(finished(coordinator, *blocker), ngm::JobState::succeeded);
        }
    }
}

void check_queue_and_cleanup() {
    ControlledWorker running;
    ngm::JobCoordinator coordinator({{"a", "b"}, 20});
    const auto first =
        coordinator.submit(request("first", [&](const auto& context) { return running.run(context); }, "a"));
    running.entered();
    std::vector<std::string> order;
    std::mutex order_mutex;
    const auto record = [&](const ngm::JobContext& context) {
        std::lock_guard lock(order_mutex);
        order.push_back(context.identity.capture_id);
        return success(context);
    };
    const auto cancelled = coordinator.submit(request("cancelled-queued", record, "a"));
    const auto second = coordinator.submit(request("second", record, "a"));
    const auto third = coordinator.submit(request("third", record, "a"));
    require(coordinator.snapshot(second.job_id)->state == ngm::JobState::queued &&
                coordinator.snapshot(third.job_id)->state == ngm::JobState::queued,
            "same-GPU work waits behind its owner");
    const auto independent = coordinator.submit(request("independent", success, "b"));
    require_clean(finished(coordinator, independent), ngm::JobState::succeeded);
    require(coordinator.cancel(cancelled.job_id) == ngm::JobCancelResult::requested, "queued cancellation accepted");
    const auto queued_result = finished(coordinator, cancelled);
    require_clean(queued_result, ngm::JobState::cancelled);
    require(!queued_result.started_at, "queued cancellation launches no callback");
    require(coordinator.cancel(first.job_id) == ngm::JobCancelResult::requested, "running cancellation accepted");
    running.stopped();
    const auto cleaning = *coordinator.snapshot(first.job_id);
    require(cleaning.state == ngm::JobState::running && cleaning.stop_reason == ngm::JobStopReason::cancellation &&
                cleaning.gpu_reserved && !cleaning.cleanup_confirmed,
            "cancellation keeps ownership while cleanup remains in progress");
    require(coordinator.cancel(first.job_id) == ngm::JobCancelResult::already_requested,
            "repeated cancellation cannot choose another outcome");
    require(coordinator.snapshot(second.job_id)->state == ngm::JobState::queued,
            "observing cancellation does not release the GPU");
    running.release();
    require_clean(finished(coordinator, first), ngm::JobState::cancelled);
    require_clean(finished(coordinator, second), ngm::JobState::succeeded);
    require_clean(finished(coordinator, third), ngm::JobState::succeeded);
    require(order == std::vector<std::string>{"second", "third"}, "per-GPU queue is FIFO and skips cancelled entries");
}

void check_deadlines() {
    {
        std::atomic<bool> launched = false;
        ngm::JobCoordinator coordinator;
        auto expired = request("already-expired", [&](const auto& context) {
            launched = true;
            return success(context);
        });
        expired.deadline = ngm::JobClock::now() - 1s;
        const auto result = finished(coordinator, coordinator.submit(std::move(expired)));
        require_clean(result, ngm::JobState::timed_out);
        require(!launched && !result.started_at, "expired request does not launch an application or worker");
    }
    {
        ControlledWorker running;
        ngm::JobCoordinator coordinator;
        const auto owner =
            coordinator.submit(request("owner", [&](const auto& context) { return running.run(context); }));
        running.entered();
        std::atomic<bool> launched = false;
        auto expiring = request("queued-deadline", [&](const auto& context) {
            launched = true;
            return success(context);
        });
        expiring.deadline = ngm::JobClock::now() + 30ms;
        const auto expired = finished(coordinator, coordinator.submit(std::move(expiring)));
        require_clean(expired, ngm::JobState::timed_out);
        require(!launched && !expired.started_at, "queued deadline expires independently of the busy GPU");
        require(coordinator.snapshot(owner.job_id)->gpu_reserved, "queued expiry leaves current GPU ownership intact");
        running.release();
        require_clean(finished(coordinator, owner), ngm::JobState::succeeded);
    }
    {
        ControlledWorker running;
        ngm::JobCoordinator coordinator;
        auto expiring = request("running-deadline", [&](const auto& context) { return running.run(context); });
        expiring.deadline = ngm::JobClock::now() + 250ms;
        const auto owner = coordinator.submit(std::move(expiring));
        running.entered();
        running.stopped();
        const auto pending = *coordinator.snapshot(owner.job_id);
        require(pending.state == ngm::JobState::running && pending.stop_reason == ngm::JobStopReason::deadline &&
                    pending.gpu_reserved && !pending.cleanup_confirmed,
                "deadline selects timeout and retains the reservation until cleanup");
        require(coordinator.cancel(owner.job_id) == ngm::JobCancelResult::already_requested,
                "cancellation cannot replace an already selected timeout");
        running.release();
        require_clean(finished(coordinator, owner), ngm::JobState::timed_out);
    }
    {
        ControlledWorker running;
        ngm::JobCoordinator coordinator;
        auto expiring = request("cancel-before-deadline", [&](const auto& context) { return running.run(context); });
        expiring.deadline = ngm::JobClock::now() + 250ms;
        const auto deadline = expiring.deadline;
        const auto owner = coordinator.submit(std::move(expiring));
        running.entered();
        require(coordinator.cancel(owner.job_id) == ngm::JobCancelResult::requested, "cancellation precedes deadline");
        running.stopped();
        auto timer = request("timer");
        timer.deadline = deadline;
        require_clean(finished(coordinator, coordinator.submit(std::move(timer))), ngm::JobState::timed_out);
        running.release();
        const auto result = finished(coordinator, owner);
        require_clean(result, ngm::JobState::cancelled);
        require(result.stop_reason == ngm::JobStopReason::cancellation,
                "elapsed deadline and successful worker return cannot replace accepted cancellation");
    }
}

void check_completion_validation() {
    for(const std::string corruption :
        {"job", "capture", "attempt", "expected-state", "cleanup", "exception", "unknown"}) {
        std::weak_ptr<int> ownership;
        {
            ngm::JobCoordinator coordinator({{"a", "b"}, 20});
            auto resource = std::make_shared<int>(7);
            ownership = resource;
            const auto invalid = coordinator.submit(request(
                "invalid",
                [resource, corruption](const auto& context) {
                    (void)resource;
                    auto completion = success(context);
                    if(corruption == "job") {
                        completion.identity.job_id += "-stale";
                    } else if(corruption == "capture") {
                        completion.identity.capture_id += "-other";
                    } else if(corruption == "attempt") {
                        ++completion.identity.attempt;
                    } else if(corruption == "expected-state") {
                        completion.expected_state = ngm::JobState::queued;
                    } else if(corruption == "cleanup") {
                        completion.cleanup_confirmed = false;
                    } else if(corruption == "exception") {
                        throw std::runtime_error("deliberate worker exception");
                    } else {
                        throw 17;
                    }
                    return completion;
                },
                "a"));
            resource.reset();
            const auto result = finished(coordinator, invalid);
            require(result.state == ngm::JobState::failed && !result.cleanup_confirmed && result.gpu_reserved &&
                        !result.error.empty(),
                    "invalid completion or exception fails closed and quarantines GPU: " + corruption);
            if(corruption != "cleanup") {
                require(result.artifact_ids.empty(), "untrusted completion cannot attach evidence");
            }
            require(!ownership.expired(), "quarantine retains the worker's artifact lease/ownership token");
            std::atomic<bool> launched = false;
            const auto blocked = coordinator.submit(request(
                "blocked",
                [&](const auto& context) {
                    launched = true;
                    return success(context);
                },
                "a"));
            const auto other = coordinator.submit(request("other-gpu", success, "b"));
            require_clean(finished(coordinator, other), ngm::JobState::succeeded);
            require(coordinator.snapshot(blocked.job_id)->state == ngm::JobState::queued && !launched,
                    "quarantined GPU cannot be reused while other GPUs remain usable");
            const auto stopped = coordinator.shutdown(2s);
            require(stopped.workers_joined && !stopped.cleanup_confirmed && stopped.pending_workers.empty() &&
                        stopped.quarantined_jobs == std::vector<ngm::JobIdentity>{invalid},
                    "joined callbacks do not falsely establish process cleanup");
            require(!ownership.expired(), "shutdown report does not release quarantined ownership");
        }
        require(ownership.expired(), "quarantined callback storage lives until coordinator destruction");
    }
    ngm::JobCoordinator coordinator;
    ngm::JobCompletion old_completion;
    const auto first = coordinator.submit(request("original", [&](const auto& context) {
        old_completion = success(context);
        return old_completion;
    }));
    const auto original = finished(coordinator, first);
    require_clean(original, ngm::JobState::succeeded);
    const auto stale = coordinator.submit(request("new-capture", [&](const auto&) { return old_completion; }));
    const auto rejected = finished(coordinator, stale);
    require(rejected.state == ngm::JobState::failed && rejected.artifact_ids.empty() && rejected.gpu_reserved,
            "duplicate prior completion cannot be applied to a new dispatch");
    const auto unchanged = *coordinator.snapshot(first.job_id);
    require(unchanged.state == original.state && unchanged.identity == original.identity &&
                unchanged.artifact_ids == original.artifact_ids && unchanged.finished_at == original.finished_at,
            "stale payload cannot revive or edit the earlier terminal job");
}

void check_success_cancel_races() {
    for(int iteration = 0; iteration < 24; ++iteration) {
        ControlledWorker running;
        ngm::JobCoordinator coordinator;
        const auto identity =
            coordinator.submit(request("race", [&](const auto& context) { return running.run(context); }));
        running.entered();
        std::barrier ready(3);
        auto cancelling = std::async(std::launch::async, [&] {
            ready.arrive_and_wait();
            return coordinator.cancel(identity.job_id);
        });
        std::jthread completing([&] {
            ready.arrive_and_wait();
            running.release();
        });
        ready.arrive_and_wait();
        const auto cancellation = cancelling.get();
        const auto result = finished(coordinator, identity);
        if(cancellation == ngm::JobCancelResult::requested) {
            require_clean(result, ngm::JobState::cancelled);
        } else {
            require(cancellation == ngm::JobCancelResult::already_terminal, "race has exactly one decision winner");
            require_clean(result, ngm::JobState::succeeded);
        }
        require(coordinator.cancel(identity.job_id) == ngm::JobCancelResult::already_terminal &&
                    coordinator.snapshot(identity.job_id)->state == result.state,
                "terminal race outcome remains stable");
    }
}

void check_shutdown() {
    for(const bool cooperative : {true, false}) {
        ControlledWorker running;
        ngm::JobCoordinator coordinator;
        const auto owner = coordinator.submit(
            request("shutdown-owner", [&](const auto& context) { return running.run(context, cooperative); }));
        running.entered();
        std::atomic<bool> launched = false;
        const auto queued = coordinator.submit(request("shutdown-queued", [&](const auto& context) {
            launched = true;
            return success(context);
        }));
        const auto start = ngm::JobClock::now();
        const auto partial = coordinator.shutdown(10ms);
        require(ngm::JobClock::now() - start < 1s, "shutdown timeout does not wait indefinitely for a worker callback");
        require(!partial.workers_joined && !partial.cleanup_confirmed &&
                    partial.pending_workers == std::vector<ngm::JobIdentity>{owner},
                "shutdown reports a callback still using its reservation");
        if(cooperative) {
            running.stopped();
        }
        require_clean(finished(coordinator, queued), ngm::JobState::cancelled);
        require(!launched && coordinator.snapshot(queued.job_id)->stop_reason == ngm::JobStopReason::shutdown,
                "shutdown cancels queued work without launching it");
        require(coordinator.snapshot(owner.job_id)->gpu_reserved, "shutdown timeout retains live ownership");
        require_throws<std::runtime_error>([&] { coordinator.submit(request("late")); }, "shutdown rejects new work");
        running.release();
        const auto stopped = coordinator.shutdown(3s);
        require(stopped.workers_joined && stopped.cleanup_confirmed && stopped.pending_workers.empty(),
                "subsequent shutdown observes joined workers and confirmed cleanup");
        const auto result = finished(coordinator, owner);
        require_clean(result, ngm::JobState::cancelled);
        require(result.stop_reason == ngm::JobStopReason::shutdown, "shutdown success race keeps the shutdown outcome");
    }
    // Destructor joins its cooperative callback; its reference cannot outlive us.
    std::atomic<bool> exited = false;
    std::promise<void> entered;
    auto ready = entered.get_future();
    {
        ngm::JobCoordinator coordinator;
        coordinator.submit(request("destructor", [&](const auto& context) {
            entered.set_value();
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            changed.wait_for(lock, context.stop, 3s, [] { return false; });
            exited = true;
            return success(context);
        }));
        require(ready.wait_for(2s) == std::future_status::ready, "destructor test actually entered worker callback");
    }
    require(exited, "destruction joins callback instead of detaching it");
}

ngm::ProcessOptions process_options(const std::filesystem::path& executable, const std::filesystem::path& root,
                                    const std::string& name) {
    ngm::ProcessOptions options;
    options.executable = executable;
    options.working_directory = root / name;
    std::filesystem::create_directories(options.working_directory);
    options.stdout_path = options.working_directory / "stdout.txt";
    options.stderr_path = options.working_directory / "stderr.txt";
    options.terminate_grace = 30ms;
    return options;
}

ngm::JobWorker process_worker(ngm::ProcessOptions options, std::filesystem::path export_path = {}) {
    return
        [options = std::move(options), export_path = std::move(export_path)](const ngm::JobContext& context) mutable {
            ngm::JobCompletion completion;
            completion.identity = context.identity;
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(context.deadline - ngm::JobClock::now());
            if(remaining <= 0ms) {
                completion.outcome = ngm::JobOutcome::timed_out;
                completion.cleanup_confirmed = true;
                return completion;
            }
            options.timeout = remaining;
            const auto result = ngm::run_process(options, context.stop);
            completion.cleanup_confirmed = result.cleanup_confirmed;
            completion.error = result.error;
            if(result.timed_out) {
                completion.outcome = ngm::JobOutcome::timed_out;
            } else if(result.cancelled) {
                completion.outcome = ngm::JobOutcome::cancelled;
            } else if(result.exit_code == 0 && result.error.empty()) {
                completion.outcome = ngm::JobOutcome::succeeded;
                if(!export_path.empty() && read_file(export_path) != "{\"ok\":true}\n") {
                    completion.outcome = ngm::JobOutcome::failed;
                    completion.error =
                        std::filesystem::exists(export_path) ? "Malformed test export" : "Missing test export";
                }
            } else if(completion.error.empty()) {
                completion.error = "Target did not exit successfully";
            }
            completion.artifact_ids = {context.identity.capture_id};
            return completion;
        };
}

// Wait for actual target readiness through filesystem notifications. No fixed
// sleep guesses when the separate executable or its descendants have launched.
class FileReady {
public:
    explicit FileReady(const std::filesystem::path& directory) : descriptor_(inotify_init1(IN_CLOEXEC | IN_NONBLOCK)) {
        require(descriptor_ >= 0, "create readiness notification descriptor");
        require(inotify_add_watch(descriptor_, directory.c_str(), IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE) >= 0,
                "watch isolated target output directory");
    }
    ~FileReady() {
        close(descriptor_);
    }
    void wait(const std::filesystem::path& file) {
        const auto deadline = ngm::JobClock::now() + 3s;
        while(read_file(file).find("ready\n") == std::string::npos) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - ngm::JobClock::now());
            require(remaining > 0ms, "target reached readiness before timeout");
            pollfd watched{descriptor_, POLLIN, 0};
            const auto count = poll(&watched, 1, static_cast<int>(remaining.count()));
            if(count < 0 && errno == EINTR) {
                continue;
            }
            require(count > 0, "target readiness notification arrived");
            char buffer[4096];
            while(read(descriptor_, buffer, sizeof(buffer)) > 0) {
            }
        }
    }

private:
    int descriptor_;
};

std::vector<pid_t> require_pids_gone(const std::filesystem::path& path) {
    std::ifstream stream(path);
    std::vector<pid_t> pids;
    pid_t pid;
    while(stream >> pid) {
        require(pid > 1, "stand-in reported a valid owned PID");
        errno = 0;
        require(kill(pid, 0) == -1 && errno == ESRCH, "all owned descendants are reaped before GPU release");
        pids.push_back(pid);
    }
    require(pids.size() == 3, "stand-in launched target, child, and grandchild");
    return pids;
}

class UnrelatedChild {
public:
    UnrelatedChild() : pid_(fork()) {
        require(pid_ >= 0, "launch unrelated sentinel");
        if(pid_ == 0) {
            sleep(20);
            _exit(0);
        }
    }
    ~UnrelatedChild() {
        kill(pid_, SIGKILL);
        while(waitpid(pid_, nullptr, 0) < 0 && errno == EINTR) {
        }
    }
    void require_alive() const {
        siginfo_t information{};
        require(waitid(P_PID, static_cast<id_t>(pid_), &information, WEXITED | WNOHANG | WNOWAIT) == 0 &&
                    information.si_pid == 0,
                "job cleanup leaves unrelated process ownership untouched");
    }

private:
    pid_t pid_;
};

void check_real_processes(const std::filesystem::path& executable) {
    Scratch scratch;
    UnrelatedChild unrelated;
    ngm::JobCoordinator coordinator;
    std::vector<pid_t> leaders;
    for(int iteration = 0; iteration < 2; ++iteration) {
        const auto name = "fresh-launch-" + std::to_string(iteration);
        auto options = process_options(executable, scratch.path, name);
        const auto pids = options.working_directory / "pids.txt";
        options.arguments = {"descendants", pids.string(), "exit", "escape"};
        const auto identity = coordinator.submit(request(name, process_worker(options)));
        require_clean(finished(coordinator, identity), ngm::JobState::succeeded);
        leaders.push_back(require_pids_gone(pids).front());
        unrelated.require_alive();
    }
    require(leaders[0] != leaders[1], "consecutive jobs launch distinct application instances");
    for(const std::string mode : {"success", "failure", "launch-failure", "valid", "missing", "malformed"}) {
        auto options = process_options(executable, scratch.path, mode);
        std::filesystem::path export_path;
        if(mode == "launch-failure") {
            options.executable /= "does-not-exist";
        } else if(mode == "valid" || mode == "missing" || mode == "malformed") {
            export_path = options.working_directory / "export.json";
            options.arguments = {"output", export_path.string(), mode};
        } else {
            options.arguments = {"exit", mode == "success" ? "0" : "23"};
        }
        const auto result =
            finished(coordinator, coordinator.submit(request(mode, process_worker(options, export_path))));
        require_clean(result, mode == "success" || mode == "valid" ? ngm::JobState::succeeded : ngm::JobState::failed);
        if(mode == "success" || mode == "failure") {
            require(read_file(options.stdout_path) == "early stdout\n" &&
                        read_file(options.stderr_path) == "early stderr\n",
                    "job subprocess output streams stay in separate files");
        }
    }
    {
        auto options = process_options(executable, scratch.path, "timeout");
        options.arguments = {"sleep", "10000"};
        auto timed = request("timeout", process_worker(options));
        timed.deadline = ngm::JobClock::now() + 200ms;
        require_clean(finished(coordinator, coordinator.submit(std::move(timed))), ngm::JobState::timed_out);
        require(read_file(options.stdout_path) == "ready\n", "timeout exercised a real launched target");
    }
    {
        auto options = process_options(executable, scratch.path, "cancel-descendants");
        const auto pids = options.working_directory / "pids.txt";
        options.arguments = {"descendants", pids.string(), "wait", "escape"};
        FileReady ready(options.working_directory);
        const auto identity = coordinator.submit(request("cancel-descendants", process_worker(options)));
        ready.wait(options.stdout_path);
        require(coordinator.cancel(identity.job_id) == ngm::JobCancelResult::requested,
                "cancellation follows confirmed target/descendant launch");
        require_clean(finished(coordinator, identity), ngm::JobState::cancelled);
        require_pids_gone(pids);
        unrelated.require_alive();
    }
    {
        auto options = process_options(executable, scratch.path, "shutdown-descendants");
        const auto pids = options.working_directory / "pids.txt";
        options.arguments = {"descendants", pids.string(), "wait", "escape"};
        FileReady ready(options.working_directory);
        const auto identity = coordinator.submit(request("shutdown-descendants", process_worker(options)));
        ready.wait(options.stdout_path);
        const auto stopped = coordinator.shutdown(3s);
        require(stopped.workers_joined && stopped.cleanup_confirmed, "server shutdown cleans real owned process trees");
        require_clean(finished(coordinator, identity), ngm::JobState::cancelled);
        require_pids_gone(pids);
        unrelated.require_alive();
    }
}
} // namespace

int main(int argc, char** argv) {
    return ngm::check::run([&] {
        require(argc == 2, "usage: JobsCheck <process stand-in executable>");
        check_contract();
        check_ownership_destructors();
        check_finalization();
        check_queue_and_cleanup();
        check_deadlines();
        check_completion_validation();
        check_success_cancel_races();
        check_shutdown();
        check_real_processes(std::filesystem::absolute(argv[1]));
    });
}
