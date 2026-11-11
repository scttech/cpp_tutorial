# Capstone: Job Scheduler

You have met every piece. Now put them together: a **job scheduler**, the engine inside every server, build system and game: a pool of worker threads that run jobs by **priority**, let you **cancel** a job that has not started, tell you when everything is **idle**, and shut down **gracefully**. Thread pools are what you built in lesson 5; this one has the features that real ones need.

## The contract

```cpp
Scheduler scheduler(4);                                         // four worker threads
auto report = scheduler.submit(Priority::High, [] { return build_report(); });   // returns a JobHandle
auto email  = scheduler.submit(Priority::Low,  [] { send_digest(); });

report.get();                    // waits for the result (or rethrows the job's exception)
email.cancel();                  // true if it had not started yet: it never runs, and email.get() throws JobCancelled
scheduler.wait_idle();           // nothing queued, nothing running
scheduler.shutdown(ShutdownMode::Discard);   // or Drain (the default): the destructor drains
```

The comment at the top of `scheduler.hpp` is the full specification: ordering rules (highest priority first, first come first served inside a priority), what each call returns in each situation, what `Drain` and `Discard` mean, what the statistics count. Read it twice.

## What is given, and what is yours

Given (complete, marked in the file): `Priority`, `ShutdownMode`, `JobCancelled`, a `detail::Task<R>` that runs a job and stores its result or its exception in a promise, and `JobHandle<R>` (a future plus a "canceller" function). **Yours: the `Scheduler` class.** The starter runs each job immediately on the calling thread, which compiles and is not a scheduler.

### Hints about the design (the pieces that the tests check)

- **The queue** must give back the right job first: an ordered container whose key is *(minus the priority, a sequence number)* has the next job at `begin()`. A `std::map` also lets `cancel()` find and **erase** a queued job by its key.
- **One mutex** guards the queue, the number of running jobs, the stop flag and the statistics. **Two condition variables**: *work available* (workers wait on it) and *idle* (`wait_idle` waits on it). Every `wait` has a predicate.
- **A worker**: wait for a job or for the stop flag; take the first job; **release the lock**; run it; take the lock; update the counters; tell the idle waiters if nothing is left. Never run a job while holding the lock (other workers, `submit`, `cancel`, a job that submits more jobs would all block).
- **`cancel()`** is a race between "a worker takes the job" and "the caller removes it": the mutex decides. Whoever erases the job from the queue wins; if the key is already gone, the job started (or ended) and `cancel()` returns `false`. The winner of a cancel must make the future throw `JobCancelled` (there is a function for that) **outside** the lock.
- **`shutdown(Drain)`**: set the stop flag, wake the workers, join them; they leave only when the queue is empty, so every queued job runs. **`shutdown(Discard)`**: also take the queued jobs out of the queue first, and cancel their futures. Both are idempotent; `submit` after the flag is set throws `std::runtime_error`.
- **Exceptions** are already handled by `Task::run` (it returns `false` and stores the exception), so a failing job cannot kill a worker; you only count it.
- The workers are `std::jthread`s declared **last**.

## The tiers

- **Works**: results through handles, **priority order** and **FIFO within a priority** (the tests block the only worker, queue the jobs, then release it), `pending()` and `stats()`, `wait_idle()`.
- **Robust**: exceptions and statistics, `cancel()` in all its situations, drain, discard, submit after shutdown, real concurrency (jobs that wait for each other), submitting from four threads at once, submitting and cancelling from inside a job, `get()` that blocks, many threads in `wait_idle()`.
- **Fast**: threads are reused (the process never has more than the workers' threads extra) and **waiting is free**: idle workers and a thread in `wait_idle()` use almost no processor time while a job runs.
- **Clean**: no `detach`, no `std::async`; every wait has a predicate; no sleeping, yielding or spinning; no raw `new`/`delete`, no `volatile`, no hand-written `lock()`/`unlock()`.

ThreadSanitizer is on, as everywhere in this module, and every test has a time limit: a deadlock or a lost wake-up is reported as a timeout.
