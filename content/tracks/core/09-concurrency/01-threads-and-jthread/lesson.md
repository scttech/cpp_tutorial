# Threads and `std::jthread`

A **thread** is an independent line of execution inside your program. A program starts with one (the one that runs `main`); you can start more, and they all run **at the same time** and share the program's memory. That is what makes concurrency useful (several things at once, or waiting for one thing without blocking another) and what makes it dangerous (they share memory, and in the next lesson you will see what that costs).

## Starting and finishing a thread

```cpp
#include <thread>

void work(int id) { /* ... */ }

int main() {
    std::thread t(work, 7);     // starts running at once, on a new thread
    // ... the main thread keeps going while `work(7)` runs
    t.join();                   // wait until the thread has finished
}
```

- **`std::thread t(f, args...)`** starts `f(args...)` on a new thread. The new thread begins running immediately, possibly before the constructor returns.
- **`t.join()`** waits for it to finish. A thread that is *joinable* (started, not yet joined or detached) when its `std::thread` object is **destroyed** makes the program call `std::terminate`. Forgetting to join is a crash, not a leak.
- **`t.detach()`** lets the thread run on its own, and you can never wait for it. Almost always a bad idea: the detached thread outlives the variables it was given references to.
- **`std::jthread`** (C++20) is the better default: it **joins automatically** in its destructor, and it supports **cooperative cancellation** (below).
- `std::this_thread::get_id()`, `sleep_for(duration)`, `yield()` act on the current thread; `std::thread::hardware_concurrency()` says how many threads the machine can run at once (a hint; it may be 0).
- A thread object is **movable but not copyable**, like a `unique_ptr`: `std::vector<std::jthread>` works, `emplace_back` starts a thread.

## The arguments are COPIED

This surprises everyone once:

```cpp
void fill(std::vector<int>& out, int n);

std::vector<int> v;
std::thread t(fill, v, 5);              // does NOT compile: error: static_assert failed
                                        // "std::thread arguments must be invocable after conversion to rvalues"
std::thread t(fill, std::ref(v), 5);    // correct: pass a reference wrapper
std::thread t([&v] { fill(v, 5); });    // also correct: a lambda that captures by reference
```

`std::thread` copies (or moves) each argument into storage inside the new thread, then calls your function with *rvalues*. A function that takes a non-const reference cannot be called with an rvalue, hence the error. `std::ref(v)` is a small object that holds a reference and converts back to `std::vector<int>&`. (If the function took its parameter *by value*, the copy would be the intended behavior, and a thread that fills a copy changes nothing for the caller.)

### Lifetimes: the thread must not outlive what it uses

A lambda with `[&]` or a `std::ref` gives the thread *the address of your variable*. If the variable is destroyed while the thread still runs (the function returned, the scope ended), the thread reads a dead object: undefined behavior. `join()` before the end of the scope is the usual fix, and it is why `jthread` is good: the join cannot be forgotten.

## Exceptions do not cross threads

An exception that leaves a thread's function cannot be caught by the thread that started it: the program calls `std::terminate`. A thread function must catch what can be thrown. To hand an exception to another thread, capture it with `std::current_exception()` (a `std::exception_ptr`, which can be stored and copied) and rethrow it elsewhere with `std::rethrow_exception(e)`. You will do exactly that in `run_all`.

## Stopping a thread politely: `stop_token`

You cannot kill a thread (and you would not want to: its locks and objects would never be released). C++20 gives a cooperative protocol: a `std::jthread` owns a **stop source**; its function can take a `std::stop_token` as the first parameter and check `token.stop_requested()`. `jthread::request_stop()` sets the flag, and the **destructor calls `request_stop()` and then `join()`**.

A loop that sleeps cannot notice a stop request until it wakes up: `std::this_thread::sleep_for(10s)` is a ten-second delay for the destructor. The fix is a wait that the stop request can interrupt: `std::condition_variable_any::wait_for(lock, stop_token, duration, predicate)` returns early when a stop is requested. (Condition variables are the subject of lesson 3; here you only use this one call.)

One more rule that bites: members of a class are destroyed in **reverse order of declaration**. A `std::jthread` member that uses the class's mutex and condition variable must be declared **after** them, so that it is stopped and joined *before* they are destroyed.

## Your task: `workers.hpp`

1. **`fill_in_thread(out, n)`**: fill the caller's vector from a new thread. Try passing `v` instead of `std::ref(v)` and read the error: the Problems tab explains it.
2. **`run_all(jobs)`**: run jobs on threads of their own, all at the same time (the tests contain jobs that wait for each other), join them all, and rethrow the exception of the lowest-numbered job that failed.
3. **`Ticker`**: a thread that calls a function now and then every period, and stops promptly when the `Ticker` is destroyed.

`main.cpp` (read-only) is a demo: press **Run**. ThreadSanitizer is on, and each test has a time limit: a thread that waits for ever shows up as a timeout, and a data race stops the program with a report.

- **Works**: all three tools do their basic job, including jobs that need to run concurrently.
- **Robust**: exceptions, the order of errors, repeated use, a ten-second period that must not delay the destructor, no tick after destruction, no ticking faster than the period.
- **Clean**: no `detach`, the `Ticker` built on `std::jthread` and a stop token without `sleep_for`, no `static` state, no raw `new`/`delete`.
