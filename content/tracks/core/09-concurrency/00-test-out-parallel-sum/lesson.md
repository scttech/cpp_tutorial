# Test-out: Parallel sum

> **Already comfortable with threads?** Pass this challenge (the **Works** and **Robust** tiers) and Module 9 is marked complete. You can still open any of its lessons later.

## The task

Add up a big vector with several threads. `parallel_sum.hpp` has the signature and a stub:

```cpp
long long parallel_sum(const std::vector<int>& values, unsigned thread_count);
```

Split the vector into contiguous parts, give each part to a thread of its own, wait for all the threads, and add up what they found. The details are in the comment at the top of the file; the important ones:

- `thread_count == 0` is a mistake (`std::invalid_argument`).
- Never more threads than values: an empty vector needs no thread, and three values never need a thousand.
- The result is a `long long`, because `int` values can add up to more than `INT_MAX`.
- Every thread has finished when the function returns.

`main.cpp` (read-only) is a demo: press **Run**.

## ThreadSanitizer is watching

This lesson builds with `-fsanitize=thread`. If two threads touch the same variable at the same time and at least one of them writes, ThreadSanitizer stops the program and says where. It finds races that only *could* happen, even if the answer came out right this time, so there is no "it works on my machine". The tests also have time limits: a thread that waits for ever is reported as a timeout.

The sandbox gives your program **one** CPU, so the threads take turns and the sum is not faster than a plain loop. That is fine: the point here is to get the structure right.

## The tiers

- **Works**: correct sums with different numbers of threads, a result that does not overflow, and the work really done by several threads.
- **Robust**: empty input, zero threads, far more threads than values, lengths that do not divide evenly, negative numbers.
- **Clean**: no `detach`, no `static` state, no raw `new`/`delete`.
