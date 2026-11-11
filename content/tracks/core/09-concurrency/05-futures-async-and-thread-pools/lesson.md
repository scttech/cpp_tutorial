# Futures, `async` and thread pools

Starting a `std::thread` for a job has two awkward sides: you cannot get a **result** back (the thread function's return value is thrown away), and an **exception** in the thread kills the program. Futures solve both. Thread pools solve a third problem: threads are expensive, so a program with thousands of small jobs should reuse a few of them.

## `std::future` and `std::promise`

A **future** is a ticket for a value that will exist later. A **promise** is the other end: the place where some thread puts that value.

```cpp
std::promise<int> promise;
std::future<int> future = promise.get_future();

std::jthread worker([p = std::move(promise)]() mutable {
    try {
        p.set_value(compute());               // or p.set_exception(std::current_exception());
    } catch (...) {
        p.set_exception(std::current_exception());
    }
});

int answer = future.get();                    // waits until the value is there; rethrows the exception if there is one
```

`get()` blocks until the result is ready, returns it **once** (a future is single-use), and **rethrows** the exception that the task stored. So exceptions *do* cross threads, if you use a future. `wait_for(100ms)` / `wait_until(...)` wait with a timeout and return `std::future_status::ready` or `timeout`. `std::shared_future<T>` (from `future.share()`) can be read by many threads.

## `std::packaged_task`

`std::packaged_task<R(Args...)>` wraps a function so that **calling it runs the function and fulfils a future**: the promise logic, packaged. It is the building block of a thread pool: the pool needs a callable to put in its queue, and the caller needs a future.

```cpp
std::packaged_task<int()> task([] { return 42; });
std::future<int> f = task.get_future();
std::thread t(std::move(task));     // a packaged_task can be MOVED, not copied
t.join();
f.get();                            // 42
```

## `std::async`

`std::async(policy, f, args...)` starts `f` and returns a future:

```cpp
auto f = std::async(std::launch::async, compute, 7);     // runs on a new thread, now
int r = f.get();
```

Three traps:

- **The default policy is not what you think.** `std::async(f)` without a policy may run `f` *lazily*, on the thread that calls `get()` (`std::launch::deferred`), or on a new thread: the implementation chooses. Always write the policy.
- **The future of an `async` blocks in its destructor** until the task is done. `std::async(std::launch::async, f);` as a statement (result ignored) therefore runs `f` and waits for it right there: not asynchronous at all.
- It starts a thread (or not) per call. For many small tasks, use a pool.

## A thread pool

A pool has a **fixed number of worker threads** and a **queue of tasks**. `submit()` puts a task in the queue and returns a future; each worker loops: wait for a task, take it, run it.

```cpp
void work() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) return;            // stopping, and nothing left
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();                                      // outside the lock!
    }
}
```

The details that make it right:

- **A queue of `std::function<void()>`** needs copyable callables, but a `packaged_task` is move-only. The classic answer: put it in a `std::shared_ptr` and queue a small lambda that calls it. (C++23 has `std::move_only_function`.)
- **Perfect forwarding**: `submit(F&& f, Args&&... args)` stores the call (`std::bind`, or a lambda with captures) so that tasks can take arguments; the result type is `std::invoke_result_t<F, Args...>`.
- **Exceptions**: the `packaged_task` catches them and stores them in the future, so a failing task never kills a worker.
- **Shutdown**: set a stop flag under the mutex, `notify_all()`, and let the workers **finish the tasks that are already queued** before they leave. Then join them. Tasks submitted after the shutdown are an error.
- **Do the work outside the lock**: never run a task while holding the queue's mutex.
- **Never wait for another task from inside a task** in a bounded pool: with 2 workers and 2 tasks that each wait for a third task, nobody is left to run the third (*thread-pool starvation*, a deadlock). A task may *submit* more work; it must not *wait* for it.
- **Task size matters.** Submitting a million tiny tasks costs more in locks and wake-ups than it saves. Give each task enough work (a chunk of a thousand elements, not one).

Real libraries add work stealing, priorities, and (in C++26) `std::execution` *senders and receivers* as the standard vocabulary for all of this. The pool you write here is the core of every one of them.

## Your task: `pool.hpp`

The starter `ThreadPool` compiles and returns correct results, because it runs every task on the calling thread, inside `submit`. Make it a real pool: worker threads, a queue, a mutex, a condition variable, a stop flag, graceful shutdown. `parallel_map(pool, input, fn)` is given: it submits one task per element and collects the futures in order.

- **Works**: results through futures, arguments (including `std::ref`), tasks run on the workers, `size()`, `parallel_map`, and the destructor finishes the queue.
- **Robust**: exceptions travel through the future and the pool survives, N workers work at the same time, many threads submit at once, a task submits a task, move-only tasks and results, `shutdown()`, a failing `parallel_map`.
- **Fast**: threads are **reused** (the test samples the process's thread count while 200 slow tasks run on a pool of 3) and idle workers **sleep** (the processor time of an idle pool is measured).
- **Clean**: built from `std::packaged_task`; no `detach`, no `std::async` inside the pool; waits with predicates; no sleeping or spinning; no raw `new`/`delete`.
