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

    explicit Scheduler(std::size_t worker_count) {
        if (worker_count == 0) throw std::invalid_argument("a scheduler needs at least one worker");
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) workers_.emplace_back([this] { work(); });
    }
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler() { shutdown(ShutdownMode::Drain); }

    template <class F>
    auto submit(Priority priority, F job) -> JobHandle<std::invoke_result_t<F&>> {
        using R = std::invoke_result_t<F&>;
        auto task = std::make_shared<detail::Task<R>>(std::move(job));
        std::future<R> future = task->get_future();
        Key key;
        {
            std::lock_guard lock(mutex_);
            if (stopping_) throw std::runtime_error("the scheduler has been shut down");
            key = {-static_cast<int>(priority), next_sequence_++};  // highest priority first, then first come first served
            queue_.emplace(key, task);
        }
        work_available_.notify_one();
        return JobHandle<R>(std::move(future), [this, key] { return cancel_queued(key); });
    }

    void wait_idle() {
        std::unique_lock lock(mutex_);
        idle_.wait(lock, [this] { return queue_.empty() && running_ == 0; });
    }

    void shutdown(ShutdownMode mode = ShutdownMode::Drain) {
        std::vector<std::shared_ptr<detail::TaskBase>> discarded;
        {
            std::lock_guard lock(mutex_);
            if (!stopping_) {
                stopping_ = true;
                if (mode == ShutdownMode::Discard) {
                    for (auto& entry : queue_) discarded.push_back(std::move(entry.second));
                    stats_.cancelled += discarded.size();
                    queue_.clear();
                }
            }
        }
        work_available_.notify_all();  // sleeping workers must see the stop flag
        for (auto& task : discarded) task->cancel_with_exception();  // outside the lock: this wakes threads that wait on the futures
        for (std::jthread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        idle_.notify_all();
    }

    Stats stats() const {
        std::lock_guard lock(mutex_);
        return stats_;
    }
    std::size_t pending() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }
    std::size_t worker_count() const { return workers_.size(); }

private:
    using Key = std::pair<int, std::uint64_t>;

    bool cancel_queued(Key key) {
        std::shared_ptr<detail::TaskBase> task;
        {
            std::lock_guard lock(mutex_);
            auto it = queue_.find(key);
            if (it == queue_.end()) return false;  // already started (or finished, or cancelled)
            task = std::move(it->second);
            queue_.erase(it);
            ++stats_.cancelled;
            if (queue_.empty() && running_ == 0) idle_.notify_all();
        }
        task->cancel_with_exception();
        return true;
    }

    void work() {
        for (;;) {
            std::shared_ptr<detail::TaskBase> task;
            {
                std::unique_lock lock(mutex_);
                work_available_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (queue_.empty()) return;  // stopping, and nothing left to run
                auto first = queue_.begin();
                task = std::move(first->second);
                queue_.erase(first);
                ++running_;
            }
            const bool returned = task->run();  // NOT under the lock: other workers and other threads go on meanwhile
            {
                std::lock_guard lock(mutex_);
                --running_;
                if (returned) ++stats_.completed;
                else ++stats_.failed;
                if (queue_.empty() && running_ == 0) idle_.notify_all();
            }
        }
    }

    mutable std::mutex mutex_;  // guards everything below except workers_
    std::condition_variable work_available_;
    std::condition_variable idle_;
    std::map<Key, std::shared_ptr<detail::TaskBase>> queue_;  // ordered: begin() is the next job to run
    std::uint64_t next_sequence_ = 0;
    std::size_t running_ = 0;
    bool stopping_ = false;
    Stats stats_;
    std::vector<std::jthread> workers_;  // declared last: destroyed first
};
