# Fast path: what's different in C++

- **`std::thread`** (`<thread>`, link `-pthread`): starts running when constructed; **must be joined or detached before destruction or `std::terminate` is called**. **`std::jthread`** (C++20) joins in its destructor and has a stop token. Both are move-only.
- **Arguments are copied/moved into the thread**; the callable is invoked with rvalues. Reference parameters need `std::ref(x)` (or a lambda capture); passing an lvalue to a `T&` parameter is a `static_assert` failure. Prefer lambdas that capture exactly what they use (by value for things that may die first).
- **Lifetime is your job.** A thread that holds a reference to a local must be joined before the local dies. `detach()` makes that nearly impossible to guarantee: avoid it.
- **Exceptions do not propagate across threads**: an uncaught one calls `std::terminate`. Capture with `std::current_exception()`, store the `std::exception_ptr`, rethrow with `std::rethrow_exception` in the joining thread (or use `std::promise`/`std::future`, lesson 5, which does this for you).
- **Cancellation is cooperative**: `std::jthread::request_stop()`, `std::stop_token::stop_requested()`, `std::stop_callback`. Blocking waits must be interruptible to be cancellable: `std::condition_variable_any::wait(lock, stop_token, pred)` / `wait_for(...)`. `sleep_for` is not interruptible.
- **Destruction order**: members are destroyed in reverse declaration order; declare the thread member **last**.
- **`std::thread::hardware_concurrency()`** is a hint (can be 0). More threads than cores is fine for I/O-bound work, wasteful for CPU-bound work. Thread creation costs about tens of microseconds: do not start a thread per tiny task (lesson 5: pools).
- **Sanitizer**: build with `-fsanitize=thread`; races are found even if the output happens to be right.
