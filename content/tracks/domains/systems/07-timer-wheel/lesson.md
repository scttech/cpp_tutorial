# Timer Wheel

> **You're an engineer on a systems team** writing a network server that holds 500000 connections. Every one needs timeouts: close if idle for 30 seconds, retransmit if no ACK in 200 ms, expire a cached entry in 5 minutes. Most of these timers are **cancelled before they fire** (the ACK arrived, the client sent data). A sorted structure (`std::priority_queue`, `std::map`) pays O(log n) for every start and cancel; a **timer wheel** pays O(1). The Linux kernel, Netty, Kafka, Akka and DPDK all use one.

## The idea: a clock face

Imagine a clock with `S` slots (positions) and a hand that moves one slot per **tick**. A timer due `d` ticks from now goes into slot `(now + d) mod S`. Each tick looks at **one** slot and runs the timers in it whose deadline has come. Timers more than one trip away just sit in their slot until the hand comes round again with the right deadline (this is the "hashed" wheel of Varghese and Lauck). Scheduling and cancelling never search, and a tick costs only what sits in one slot.

The clock here is **logical**: the caller says how many ticks have passed (`advance(n)`), a tick may be 1 ms or one frame; the wheel never looks at a real clock. That also makes it perfectly testable.

## The details that make it hard

- **Callbacks run arbitrary code while you are in the middle of a tick.** They may schedule new timers (even into the slot you are sweeping), cancel other timers (even one due *in this very tick*, later in the order), call `pending()`, `clear()` or even `advance()`. If you hold an iterator into a list while calling out, any of that can invalidate it. The cure is **collect, then fire**: first sweep the slot and `splice` the due nodes into a local list (a splice moves nodes without touching them), *then* run them, skipping any that were cancelled meanwhile.
- **Cancel must be O(1)** but a node in a `std::list` can only be erased by someone who has its iterator *and* its list, and the node may have been spliced elsewhere. The classic answer is **lazy deletion**: `cancel` looks up the timer in an `unordered_map<Id, Timer*>` (list nodes never move, so the pointer stays valid), marks it dead, **drops its callback immediately** (so what the lambda captured is released now, not at some later sweep) and erases it from the index. The next visit of its slot throws the dead node away.
- **Order:** equal deadlines run in scheduling order; with `push_back` and an in-order sweep you get that for free.
- **One tick at a time:** `advance(100)` is 100 single ticks, so inside a callback `now()` is that timer's deadline and a timer scheduled for "+1" fires at the next tick, not in the same `advance` burst's past.
- **Delay 0** cannot mean "now" (the tick is already being processed): it is treated as 1. **Huge delays** must not overflow: `now + delay` saturates at `UINT64_MAX`.
- **`advance` is not re-entrant.** A nested call from a callback is ignored (returns 0).
- While its callback runs, a timer is already off the books: `pending()` excludes it and `cancel(its own id)` is `false`.

## The tiers

- **Works**: firing at exactly the deadline, ordering, laps of the wheel, cancel and its return values, ids, deadlines, every re-entrancy rule above, `clear()` (also from a callback), saturation, release of cancelled callbacks, sweeping of dead nodes.
- **Robust**: 5000 random timers on wheels of 1, 2, 7, 64 and 256 slots each fire once at their deadline; timers that schedule and cancel each other against a straightforward simulation; tiny steps versus one huge step; destruction releases callbacks.
- **Fast**: 200000 timers over 200000 ticks (a per-tick scan of all timers would need 2 * 10^10 steps), 200000 cancels, 200000 timers in one tick (no restarting scans), 5 million idle ticks, two heap nodes per timer and zero allocations while ticking.
- **Clean**: no raw memory or exceptions, no heap/tree/sorting (it is a wheel), no real clocks or threads, and `splice` moves the due timers.

## Tips

`std::list::splice(pos, other, it)` is O(1) and keeps every iterator and pointer to the node valid. In `run_tick`, remember `std::next(it)` *before* you erase or splice `it`. Do not hold a reference into `index_` across a callback.
