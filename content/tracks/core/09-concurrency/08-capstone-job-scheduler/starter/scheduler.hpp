#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// A JOB SCHEDULER: a pool of worker threads that run jobs in order of PRIORITY, with cancellation, a way to wait until everything is done, and two kinds
// of shutdown. It combines the whole module: threads, mutexes, condition variables, futures, graceful shutdown.
//
//   Scheduler s(workers)                  `workers` threads (std::invalid_argument for 0), started at once
//   auto h = s.submit(priority, job)      queue `job` (a callable without arguments, copyable); returns a JobHandle for its result.
//                                         std::runtime_error after shutdown().
//   ORDER: a worker that is free takes the queued job with the HIGHEST priority; among jobs of the same priority, the one that was submitted FIRST.
//   h.get()                               waits for the job, returns its result; rethrows the exception the job threw; throws JobCancelled if the job was cancelled
//   h.cancel()                            removes the job from the queue if it has NOT STARTED: returns true and the job never runs (h.get() then throws
//                                         JobCancelled). False if it is running, finished, or already cancelled.
//   s.wait_idle()                         returns when no job is queued and no job is running
//   s.shutdown(mode)                      accepts no more jobs, then joins the workers. Drain (the default, also what the destructor does): every queued job
//                                         still runs. Discard: the queued jobs are cancelled (their handles throw JobCancelled); jobs that are running
//                                         finish. Calling it again is harmless.
//   s.stats()                             {completed, failed, cancelled}: jobs that returned, jobs that threw, jobs that were cancelled (by cancel() or Discard)
//   s.pending()                           the number of jobs waiting in the queue (not started, not cancelled)
//   s.worker_count()
//
// Everything may be called from any thread, including from inside a job (a job may submit or cancel other jobs; it must not wait for them).
// Parts marked "given" are complete. The Scheduler class is yours. THE STARTER RUNS EACH JOB AT ONCE ON THE CALLING THREAD: it compiles, and it is not a scheduler.

enum class Priority { Low = 0, Normal = 1, High = 2 };
enum class ShutdownMode { Drain, Discard };

// ---- given -----------------------------------------------------------------------------------------------------------------------------------------------
struct JobCancelled : std::runtime_error {
    JobCancelled() : std::runtime_error("job cancelled") {}
};

namespace detail {

// What the Scheduler stores in its queue: a job together with the promise for its result.
struct TaskBase {
    virtual ~TaskBase() = default;
    virtual bool run() = 0;                    // runs the job and puts its result (or its exception) into the future; true if the job returned, false if it threw. Never throws.
    virtual void cancel_with_exception() = 0;  // makes the future throw JobCancelled
};

template <class R>
class Task final : public TaskBase {
public:
    explicit Task(std::function<R()> job) : job_(std::move(job)) {}
    std::future<R> get_future() { return promise_.get_future(); }

    bool run() override {
        try {
            if constexpr (std::is_void_v<R>) {
                job_();
                promise_.set_value();
            } else {
                promise_.set_value(job_());
            }
            return true;
        } catch (...) {
            promise_.set_exception(std::current_exception());
            return false;
        }
    }
    void cancel_with_exception() override { promise_.set_exception(std::make_exception_ptr(JobCancelled{})); }

private:
    std::function<R()> job_;
    std::promise<R> promise_;
};

}  // namespace detail

template <class R>
class JobHandle {
public:
    JobHandle() = default;
    JobHandle(std::future<R> future, std::function<bool()> canceller) : future_(std::move(future)), canceller_(std::move(canceller)) {}

    R get() { return future_.get(); }
    bool cancel() { return canceller_ && canceller_(); }
    bool valid() const { return future_.valid(); }

private:
    std::future<R> future_;
    std::function<bool()> canceller_;  // asks the Scheduler to take the job out of its queue
};

// ---- yours -----------------------------------------------------------------------------------------------------------------------------------------------
class Scheduler {
public:
    struct Stats {
        std::size_t completed = 0;
        std::size_t failed = 0;
        std::size_t cancelled = 0;
    };

    explicit Scheduler(std::size_t worker_count) : worker_count_(worker_count) {
        if (worker_count == 0) throw std::invalid_argument("a scheduler needs at least one worker");
    }
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler() { shutdown(ShutdownMode::Drain); }

    template <class F>
    auto submit(Priority priority, F job) -> JobHandle<std::invoke_result_t<F&>> {
        using R = std::invoke_result_t<F&>;
        (void)priority;
        auto task = std::make_shared<detail::Task<R>>(std::move(job));
        std::future<R> future = task->get_future();
        if (task->run()) ++stats_.completed; else ++stats_.failed;  // TODO: queue the task for a worker instead of running it here
        return JobHandle<R>(std::move(future), [] { return false; });
    }

    void wait_idle() {}
    void shutdown(ShutdownMode mode = ShutdownMode::Drain) { (void)mode; }
    Stats stats() const { return stats_; }
    std::size_t pending() const { return 0; }
    std::size_t worker_count() const { return worker_count_; }

private:
    std::size_t worker_count_;
    Stats stats_;
    // TODO: a mutex; a queue ordered by (priority, submission order), e.g. std::map<std::pair<int, std::uint64_t>, std::shared_ptr<detail::TaskBase>>
    //       with the key (-priority, sequence number) so that begin() is the next job; the number of running jobs; a stop flag; two condition variables
    //       (work available / everything idle); the worker threads (declared last).
};
