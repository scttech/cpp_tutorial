# Condition variables and blocking queues

A mutex keeps threads out of each other's way. It does not let one thread **wait for something that another thread will do**: "wait until there is work", "wait until the buffer has room", "wait until the other thread is ready". The wrong way to wait is a loop that keeps checking:

```cpp
while (queue.empty()) { }          // BUSY WAITING: burns a whole processor to learn nothing, and it is a data race
while (queue.empty()) std::this_thread::sleep_for(10ms);   // polling: wastes time, adds latency, still needs a lock
```

The right way is a **condition variable**: a thread goes to sleep, uses no processor, and another thread wakes it when something has changed.

## `std::condition_variable`

```cpp
std::mutex m;
std::condition_variable cv;
std::deque<int> items;              // the shared state: guarded by m

// consumer
std::unique_lock lock(m);
cv.wait(lock, [&] { return !items.empty(); });    // sleeps until items is not empty
int x = items.front(); items.pop_front();

// producer
{
    std::lock_guard lock(m);
    items.push_back(42);
}
cv.notify_one();                    // wake one waiting thread
```

How `wait(lock, predicate)` works: it **atomically unlocks the mutex and goes to sleep**; when woken it **locks the mutex again** and checks the predicate; if the predicate is false it sleeps again. That is exactly:

```cpp
while (!predicate()) wait_without_predicate(lock);
```

### Why the predicate is not optional

1. **Spurious wake-ups.** A waiting thread can wake up for no reason at all. Without a re-check it would continue with the condition false.
2. **Lost wake-ups.** A notification is **not remembered**: if `notify_one()` happens when nobody is waiting, it is gone. A consumer that calls `wait(lock)` *after* the producer notified sleeps for ever. With a predicate, the condition is tested *first*, so a change that happened earlier is seen at once.
3. **Somebody else got there first.** With several consumers, you are woken because an item arrived, but another consumer may take it before you lock the mutex. The predicate checks again.

So: **always wait with a predicate** (or a `while` loop around the plain wait), **always change the shared state under the mutex**, then notify.

### `notify_one` or `notify_all`?

`notify_one` wakes one waiting thread, `notify_all` wakes all of them (each re-checks its predicate). Use `notify_one` when any one waiter can use the change (one item arrived: one consumer). Use `notify_all` when **every** waiter needs to know (the queue was closed, a flag changed, or several different conditions share one condition variable). When in doubt, `notify_all` is correct and `notify_one` is an optimization.

You may notify with or without holding the mutex. Releasing it first avoids waking a thread that immediately blocks on the mutex you still hold.

## The blocking queue

A **bounded blocking queue** is the standard way to connect threads: *producers* push work, *consumers* pop it. Two conditions, so two condition variables:

- consumers wait for **not empty**; producers wait for **not full** (the capacity limit is *backpressure*: a fast producer is slowed down instead of eating all the memory);
- `push` notifies "not empty", `pop` notifies "not full";
- and a way to **finish**: `close()` says "no more values"; it must wake **every** waiting thread (both kinds), after which `push` fails and `pop` returns whatever is left and then "nothing". Without a close, consumers wait for ever for the value that will never come.

## Other tools of the family (C++20)

- **`std::latch`**: a countdown that threads wait on, once (`count_down()`, `wait()`, `arrive_and_wait()`). The tests of this module use it to start threads together.
- **`std::barrier`**: like a latch that can be reused, for threads that work in rounds and must meet at the end of each.
- **`std::counting_semaphore` / `binary_semaphore`**: a counter of permits; `acquire()` waits for one, `release()` gives one. A condition variable with the counter built in.
- **`std::condition_variable_any`** works with any lock type and with a `std::stop_token` (lesson 1).

## Your task: `BoundedQueue<T>`

`queue.hpp` documents the whole interface in its comment. The starter is an unbounded, non-blocking, non-thread-safe queue. Make it a real one: a mutex, two condition variables, predicate waits, `close()` that wakes everybody.

- **Works**: FIFO order; `pop` waits for a value; `push` waits for room; many producers and a consumer lose nothing.
- **Robust**: `close()` wakes all waiting consumers and producers, queued values survive a close, `pop` on a closed empty queue does not wait (also when the close came *before* the wait began), a stress test with 4 producers and 4 consumers on a queue with room for 4, move-only values.
- **Fast**: waiting is free. The test measures the **processor time** that two waiting threads use in a quarter of a second: a thread that spins uses all it can get, a sleeping one uses none.
- **Clean**: every `wait` has a predicate; no sleeping, yielding or spinning; no raw `new`/`delete`.
