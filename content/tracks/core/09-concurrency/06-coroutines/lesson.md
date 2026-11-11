# Coroutines (a first look)

C++20 added **coroutines**. They are a different kind of "doing several things": not parallel (no second thread), but **cooperative**. A *coroutine* is a function that can **suspend** itself in the middle, hand control back to its caller, and later be **resumed** exactly where it stopped, with all its local variables intact. This lesson is deliberately an *awareness* lesson: coroutines are powerful, but the C++ design is low-level and verbose, and most programmers use them through a library. You will read the machinery once, finish a small piece of it, and write four coroutines.

## What a coroutine looks like

A function is a coroutine if its body contains one of three keywords:

| keyword | meaning |
|---|---|
| `co_yield value;` | suspend and **produce** `value` to whoever resumed us (a *generator*) |
| `co_await x;` | suspend until `x` is ready (an asynchronous result, a timer, a lock...) |
| `co_return v;` | finish, optionally with a result |

```cpp
Generator<int> range(int begin, int end) {
    for (int i = begin; i < end; ++i)
        co_yield i;                 // suspends here; the next call to next() continues from this line, with i still alive
}

for (int x : range(0, 5)) { ... }   // 0 1 2 3 4, each computed when the loop asks for it
```

The values are made **on demand**. `range(0, 1000000000)` costs nothing until you ask for values: a *lazy sequence*. A generator can even be infinite (`fibonacci()`), because the caller decides when to stop.

## The machinery (what the compiler does)

The compiler splits a coroutine in two parts. All the locals that must survive a suspension go into a **coroutine frame**, a heap block (the optimizer can often remove the allocation). And the **return type** controls the behavior through a nested type called `promise_type`: the compiler calls its members at every important moment.

| `promise_type` member | called when | what you decide |
|---|---|---|
| `get_return_object()` | the coroutine is called | build the object that the caller receives (here: the `Generator`, holding a *handle* to the frame) |
| `initial_suspend()` | before the body starts | `suspend_always`: start **lazily**; `suspend_never`: run at once |
| `yield_value(v)` | `co_yield v` | store `v`; returning `suspend_always` suspends |
| `return_void()` / `return_value(v)` | the body ends / `co_return` | |
| `final_suspend()` | after the last statement | `suspend_always` keeps the frame alive so the owner can inspect `done()` and then destroy it |
| `unhandled_exception()` | an exception leaves the body | store or terminate |

`std::suspend_always` and `std::suspend_never` are the two ready-made *awaiters*: what to do at a suspension point ("do suspend" / "do not suspend"). A **`std::coroutine_handle<promise_type>`** is a non-owning pointer to the frame: `resume()` continues the coroutine, `done()` says whether it has finished, `promise()` gives the promise object, and `destroy()` frees the frame. **Somebody must call `destroy()` exactly once**: the `Generator` class owns the handle the way a `unique_ptr` owns memory (a destructor, no copying, a move that leaves the old owner empty).

## Pitfalls that are particular to coroutines

- **Parameters are copied into the frame; references are not.** A coroutine that takes `const std::string&` keeps a reference to the caller's string, which may be a temporary that is gone by the time the coroutine resumes. Take coroutine parameters **by value**.
- **A coroutine that finishes must stay at its final suspend point** (`final_suspend` returns `suspend_always`), or its frame disappears while the owner still holds the handle. Resuming a finished coroutine is undefined behavior.
- **Exceptions**, **lifetimes of temporaries across a `co_await`**, and **what thread resumes the coroutine** are all your responsibility.
- They are **hard to debug**: the stack trace of a suspended coroutine is not a stack.

## What coroutines are for

1. **Generators** (this lesson): lazy sequences, tree traversals, parsers that produce tokens one at a time. C++23 adds `std::generator<T>` to the standard library so that you do not have to write the class yourself.
2. **Asynchronous code without callbacks**: `auto data = co_await socket.read();` reads like blocking code, but the thread is free while it waits. A *task* type (in Boost.Asio, `folly::coro`, `cppcoro`, libunifex) turns a network service into straight-line code. This is where coroutines meet this module: they are *concurrency without threads*, a program that handles ten thousand connections on a few threads by suspending whatever is waiting.
3. **Cooperative multitasking**: simulations and games with thousands of "scripts" that each run a little and yield.

A coroutine by itself does not run in parallel. It runs on whichever thread resumes it, and it can be resumed from another thread, which is where races come back (everything from lesson 2 applies).

## Your task: finish a `Generator<T>`

`generator.hpp` is a skeleton: a `Generator` that never suspends and never produces anything. Follow the TODO comments in order:

1. `initial_suspend` → **lazy** (`std::suspend_always`).
2. `final_suspend` → stay suspended at the end.
3. `yield_value` → store the value and **suspend**.
4. The `Generator` owns the frame: **destructor**, **no copy**, **move** that leaves the source empty.
5. `next()` resumes and says whether a value is available (check `done()` first).
6. **Range-for support**: `begin()`/`end()` and an iterator.
7. Write the four **coroutines**: `range`, `fibonacci`, `take` (lazy: it must not ask its source for a value it will not use), `words`.

`main.cpp` (read-only) is a demo: press **Run**; it prints nothing useful until the skeleton is finished.

- **Works**: ranges, `next`/`value` by hand, lazy start, Fibonacci, `words`.
- **Robust**: laziness of `take`, an abandoned generator frees its frame (the test counts allocations: a leak, or a frame destroyed twice, is reported), moves, a generator returned from a function, a coroutine that keeps its own copy of its argument.
- **Clean**: no raw `new`/`delete`, the functions really are coroutines, parameters by value.
