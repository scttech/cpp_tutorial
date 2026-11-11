# Low-Latency Event Queue (SPSC)

> **You're an engineer at a trading firm.** Market data arrives on one thread (the feed handler) and the strategy runs on another. The queue between them was a `std::queue` behind a `std::mutex`; at peak load the mutex costs microseconds, sometimes a context switch, and the 99.9th percentile latency is the number the desk looks at. The fix is the workhorse of low-latency systems: a **lock-free single-producer / single-consumer ring buffer**.

## Why it can be lock-free

If exactly **one** thread writes and **one** thread reads, they never write the same variable:

- the **producer** is the only writer of `tail` (the next slot to fill) and of the slots it fills,
- the **consumer** is the only writer of `head` (the next slot to read).

So there is nothing to lock. What is needed is *ordering*: the consumer must not read a slot before the producer's write to it is complete, and the producer must not overwrite a slot before the consumer is done with it. `std::atomic` with **release/acquire** provides exactly that:

```
producer:  buffer[tail] = item;                 consumer:  if (head == tail.load(acquire)) return empty;
           tail.store(next, release);                      out = buffer[head];
                                                           head.store(next, release);
```

The `release` store of `tail` makes the earlier write of the slot *visible* to a thread whose `acquire` load of `tail` sees the new value. Without it (with `relaxed`, or with plain variables) the consumer can read garbage: a **data race**, which ThreadSanitizer reports.

## The design

- A ring buffer of **N + 1** slots: one is always left empty, so `head == tail` means *empty* and `next(tail) == head` means *full* (otherwise the two states look alike).
- `try_push` / `try_pop` never block: they return `false` when full / empty and the caller decides (spin, yield, drop the event). That is what makes them *wait-free*.
- A thread reads **its own** index with `memory_order_relaxed` (nobody else changes it) and the **other** thread's index with `acquire`.
- **False sharing**: if `head` and `tail` share a 64-byte cache line, every write by one thread invalidates the line in the other thread's cache, and both slow down by 10 or more. Put each on its own line with `alignas(64)`.
- Items may be **move-only** (`std::unique_ptr`): move into the slot, move out of it.

## The tiers

- **Works** (single thread): FIFO order, capacity exactly N, full and empty, wrap-around, capacity 1, move-only items, `size_approx`.
- **Robust** (two threads, under ThreadSanitizer): 200000 events in order and untorn, capacities 1/2/3/7/100/1024, a slow producer and a slow consumer, move-only items across threads, a watcher thread calling `size_approx`.
- **Fast**: head and tail on separate cache lines (the object is at least 128 bytes, aligned to 64), lock-free atomics, no allocation in a million operations, and the source uses `release`, `acquire` and `relaxed`.
- **Clean**: no mutex or any lock, no `volatile`, no `seq_cst` spelled out, storage in a `std::array`.

## Tips

`advance(i)` is `i + 1 == N + 1 ? 0 : i + 1` (a modulo with a branch; for power-of-two sizes a mask is faster). `size_approx` = `tail - head` modulo `N + 1`. Do not use `volatile` for threading: it does not order anything.
