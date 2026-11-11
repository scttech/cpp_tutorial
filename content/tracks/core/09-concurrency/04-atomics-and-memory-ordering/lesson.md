# Atomics and memory ordering (an introduction)

Mutexes are the tool you reach for first. **Atomics** are the tool underneath: variables that threads can read and write at the same time without a data race, because every operation on them is indivisible. They are faster than a mutex for the simplest jobs (a counter, a flag), they are how mutexes and condition variables are built, and they are easy to get subtly wrong. This lesson is an introduction: enough to read, use and distrust them.

## `std::atomic<T>`

```cpp
#include <atomic>

std::atomic<int> counter{0};

counter.fetch_add(1);            // one indivisible step: read, add, write. Returns the old value
counter++;                       // the same thing
counter.store(7);                // write
int x = counter.load();          // read
int old = counter.exchange(9);   // write 9, return what was there
```

An operation on an atomic is never torn and never races with another operation on the same atomic. That is all it promises about *that variable*. Two things it does **not** do:

1. **A sequence of atomic operations is not atomic.** `if (a.load() > 0) a.store(a.load() - 1);` has a gap between the test and the change (*check-then-act*), exactly like the unlocked check in the bank lesson. Each step is safe; the combination is not.
2. **It does not protect other variables** (unless you ask for that, which is what memory orders are for).

### `compare_exchange`: the update loop

The tool for "read the value, compute a new one, store it, but only if nobody changed it meanwhile" is **compare-and-swap**:

```cpp
int old = a.load();
while (!a.compare_exchange_weak(old, old * 2)) {
    // someone else changed `a`: `old` was refreshed with the value that is really there; try again
}
```

`compare_exchange_weak(expected, desired)` stores `desired` **only if** the variable still equals `expected`; if it does not, it copies the current value into `expected` and returns `false`. The loop retries with fresh data until it wins. (`weak` may fail spuriously, which is fine in a loop and cheaper on some processors; `strong` does not fail spuriously.) This is how every lock-free update is built, and the reason the exercise below uses a test atomic that *forces* the unlucky interleaving.

## Memory orders: what else becomes visible

A processor and a compiler may reorder memory operations. Every atomic operation takes a `std::memory_order` that says how much ordering you need *around* it. Three levels are enough for now:

| order | meaning | typical use |
|---|---|---|
| `memory_order_relaxed` | atomicity only; no ordering with other variables | a statistics counter that nothing else depends on |
| `memory_order_release` (a store) + `memory_order_acquire` (a load) | **if the acquire load sees the value that the release store wrote, then everything the writer did BEFORE the store is visible to the reader AFTER the load** | publishing data: write the data, then set a flag |
| `memory_order_seq_cst` (the default) | one global order of all such operations; the simplest to reason about | when in doubt |

The pair that matters most is **release/acquire** (a *happens-before* edge):

```cpp
int data;                           // ordinary variable
std::atomic<bool> ready{false};

// thread A                             // thread B
data = 42;                              while (!ready.load(std::memory_order_acquire)) {}
ready.store(true, std::memory_order_release);     use(data);   // sees 42: guaranteed
```

With `relaxed` instead, thread B may see `ready == true` and still read the old `data`: **and it is a data race on `data`**, undefined behavior, which ThreadSanitizer reports. A mutex does the same: `unlock()` is a release and the next `lock()` is an acquire, which is why the data inside a critical section is safe. This lesson makes you write that logic yourself, in a spin lock.

**Rules of thumb**: start with the default (`seq_cst`). Use `relaxed` only for counters and flags that guard nothing else. Use acquire/release for publication and for locks. Use anything cleverer only after measuring, with an expert's review. And prefer a mutex until a profiler says otherwise.

## Waiting on an atomic (C++20)

A loop that tests an atomic over and over wastes the processor (busy waiting). C++20 gave atomics a sleeping wait: `flag.wait(false)` blocks while the value is `false`, and `flag.notify_one()` / `notify_all()` wake the waiters (the same idea as a condition variable, without the mutex).

## Lock-free, and its traps

An algorithm is *lock-free* if some thread always makes progress, whatever the others do (no thread can block the rest by holding a lock). The `LockFreeStack` below is the classic one. A lock-free **pop** is a famous trap:

- **The ABA problem**: a thread reads the head `A`, is preempted; others pop `A`, pop `B`, push `A` back (same address, new contents); the first thread's compare-and-swap sees `A` and succeeds, but the `next` pointer it prepared is stale: the stack is corrupted.
- **Memory reclamation**: when is it safe to `delete` a node that another thread may still be reading? Solutions (hazard pointers, epoch-based reclamation, RCU, reference counting) are research-grade; that is why this stack has `push`, `peek` and `drain` but no `pop`.

Most programs should use a mutex-protected structure or a library queue. Know what atomics do, use them for counters, flags and publication, and be suspicious of anything fancier you wrote yourself.

## Your task: `atomics.hpp`

Five tools. The starter is wrong on purpose (plain variables, or the weakest memory order everywhere), and **ThreadSanitizer understands memory orders**, so every wrong order is reported as the data race it is. Press **Run** and read the reports.

1. **`Counter`**: `add()`/`value()` with `fetch_add`. Relaxed is correct here.
2. **`SpinLock`**: `lock()` loops on `test_and_set` (acquire), `unlock()` clears (release); `try_lock()`. It must work with `std::lock_guard<SpinLock>`.
3. **`OneShot<T>`**: set once, read by waiting threads: release in `set`, acquire in `get`, and `get` must **sleep** (`wait`/`notify_all`).
4. **`fetch_double(a)`**: atomically double and return the old value, with a `compare_exchange` loop. The tests use a wrapper atomic that makes four threads read the same old value before any of them writes.
5. **`LockFreeStack<T>`**: `push` with a compare-exchange loop (release on success), `peek` with an acquire load, `drain`.

- **Works**: each tool does its job, also with several threads.
- **Robust**: real mutual exclusion, several readers, the forced-interleaving `fetch_double`, a stack that is pushed from four threads while another reads it.
- **Fast**: `OneShot::get()` waiting for a quarter of a second must cost almost no processor time.
- **Clean**: acquire and release chosen explicitly, a real `wait`/`notify`, no mutex and no `volatile` in a file about atomics.
