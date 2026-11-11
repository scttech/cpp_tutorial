# Debug challenge: Reading concurrency bugs

Concurrency bugs have a reputation: they appear on Tuesdays, never in the debugger, and vanish when you add a `printf`. The reputation is deserved for programs without tools, and much less so for programs with them. A data race that "only happens sometimes" is a data race that **ThreadSanitizer finds every time**. A deadlock is a program that stops, and **a stopped program is evidence**. The skill of this challenge is to read *how the program stopped* and map it to a cause.

## The setup

`jobs.hpp` (the file you edit) holds six small concurrent tools; `main.cpp` (read-only) is a demo that runs six scenarios, one after the other, and **prints a line before each one**. Each tool contains **one** concurrency bug. Press **Run**: the program stops at the first scenario that is broken. Look at how it stopped, fix that bug, run again, meet the next one.

## Symptom, cause, cure

| how the program ended | what it means | where to look |
|---|---|---|
| **`WARNING: ThreadSanitizer: data race`** and the program stops (exit code 77) | two threads touched the same variable, one wrote, nothing ordered them | the report names both accesses and their lines (*Write ... by thread T2*, *Previous write ... by thread T1*). Make the variable atomic, protect both accesses with the same mutex, or stop sharing it |
| **`WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock)`** | two mutexes are locked in opposite orders somewhere. It has not hung yet; it can | the report shows both acquisitions (*Mutex M1 acquired here while holding mutex M0*). Lock in one fixed order, or with `std::scoped_lock` |
| **No output, no error, then "timed out"** (the run is killed by the time limit) | the program is **stuck**: a deadlock, or a wait for a notification that already happened or never comes | the last line printed (that is why the demo prints before each scenario) tells you which scenario hangs. Look for a `wait` without a predicate, a lock taken twice, a thread waiting for itself |
| **`terminate called without an active exception`** (signal ABRT) | a `std::thread` was destroyed while still joinable: nobody called `join()` | the thread object that goes out of scope. Join it, or use `std::jthread` |
| **`terminate called after throwing an instance of 'std::runtime_error'`** (what(): ...) | an exception left a thread's function; there is nobody to catch it | catch inside the thread and carry the exception to the caller with `std::exception_ptr` (or use a future) |
| a **wrong number** with no report | a lost update that you did not run under the sanitizer, or a read before a thread finished | run with `-fsanitize=thread`; check what is read before `join()` |

## Habits

1. **Read how it stopped before you read the code.** The cause is usually in the first line of the report.
2. **Fix the first problem first.** A sanitizer stops the program; a hang stops it for good. The next bug is behind it.
3. **Never "fix" a race with a sleep.** `sleep_for` makes a race rarer, not impossible, and ThreadSanitizer still reports it. A race needs an *ordering* (a mutex, an atomic, a join), not a delay.
4. **Reduce first.** Two threads and a few iterations show the same bug as eight threads and a million.
5. **Print with care.** `std::cout << a << b` from several threads can interleave; `std::osyncstream(std::cout) << ...` (C++20) keeps a line together. The demo flushes with `std::endl` so that you see progress even when the program is killed.
6. **Real debuggers help** with deadlocks: on your own machine, `gdb` can show every thread (`thread apply all bt`) at the moment of the hang. Valgrind's `helgrind` and `drd` are other race and lock-order detectors.

## Your task

Make all six scenarios run to the end, and every test pass. **Run Tests** then checks each tool by itself, in two tiers:

- **Works**: the six tools do what their comment says.
- **Robust**: they hold up under stress and in the awkward order: opposite-direction `forward()`/`backward()` at the same time, a waiter that starts *before* `set()`, a `Worker` that is stopped twice or never started, exceptions that keep their type.

The six bugs, in the order of the file (do not read ahead if you want the full challenge): a shared counter, two locks in opposite orders, a wait that misses its notification, a thread that is never joined, a stop flag that is a plain `bool`, and an exception that escapes a thread.
