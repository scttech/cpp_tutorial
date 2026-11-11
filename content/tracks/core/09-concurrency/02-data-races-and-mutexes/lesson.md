# Data races and mutexes

Threads share memory. That is the whole power of them, and the whole problem. This lesson is about the most important rule of concurrent C++:

> **A data race is undefined behavior.**
> A *data race* happens when two threads access the same memory location, at least one of them writes, and nothing orders the two accesses (no mutex, no atomic, no join).

Undefined behavior does not mean "a wrong number now and then". The compiler is allowed to assume that races never happen, so it may keep a variable in a register for ever, reorder your code, or remove a check. A program with a race can print the right answer a million times and fail on the million and first, or only on another machine, or only with the optimizer on.

## The lost update

```cpp
int counter = 0;
void work() { for (int i = 0; i < 100000; ++i) ++counter; }     // two threads call this
```

`++counter` is three steps: *read* the value, *add one*, *write* it back. If both threads read 5, both write 6, one increment is lost. The final value is less than 200000, and different every run. The same thing happens to a bank balance, a vector's size, a pointer.

**ThreadSanitizer** (this module builds with `-fsanitize=thread`) finds these without needing the unlucky timing. It watches every memory access and every synchronization, and reports two accesses that nothing orders, **even when the answer happened to be right**. Read its report like this: it names the two accesses (*Write of size 8 ... Previous write of size 8 ...*), the lines of code, and the threads. The Problems tab and the report card explain the common cases.

## Mutexes: one thread at a time

A **mutex** (mutual exclusion) is an object that at most one thread can *own* at a time. The code between taking it and giving it back is a **critical section**. Whoever arrives second waits.

```cpp
std::mutex m;
int counter = 0;

void work() {
    for (int i = 0; i < 100000; ++i) {
        std::lock_guard lock(m);      // takes the mutex here...
        ++counter;
    }                                 // ...and gives it back at the end of the block, whatever happens
}
```

Never call `m.lock()` and `m.unlock()` by hand: an early `return` or an exception between them leaves the mutex locked for ever. Use an **RAII lock** (the same idea as `unique_ptr` for memory):

| | |
|---|---|
| `std::lock_guard lock(m);` | locks now, unlocks at the end of the scope. The simple default |
| `std::unique_lock lock(m);` | like `lock_guard`, but can be unlocked and locked again, moved, and used with condition variables (next lesson) |
| `std::scoped_lock lock(m1, m2, ...);` | locks **several** mutexes at once, with a deadlock-avoiding algorithm (C++17) |
| `std::shared_mutex` with `std::shared_lock` / `std::unique_lock` | many readers **or** one writer |

Rules that keep critical sections healthy:

- **Protect the data, not the code.** Decide which mutex guards which variables, and write it down (`// guarded by mutex_`). Every access, reads included, takes that mutex. A *read* without the lock races with a write.
- **A mutex that protects a class's data belongs in the class**, and is `mutable` if `const` member functions (`balance()`, `total()`) must lock it.
- **Keep critical sections short.** Do the slow work (I/O, calls into code you do not control) outside the lock. Never call a callback while holding a lock: it may try to take the same lock, or another one.
- **Check and act under the same lock.** `if (balance >= amount) balance -= amount;` with the lock released between the test and the change is a race (*check-then-act*), even if each step is locked on its own.

## Deadlock

A **deadlock** is two (or more) threads each waiting for something that another one holds. The classic one:

```
thread 1:  lock(A);  lock(B);        thread 2:  lock(B);  lock(A);
```

If thread 1 gets A and thread 2 gets B, both wait for ever. The program does not crash, it just stops. The cure: **everyone takes several mutexes in the same order**, or takes them together with `std::scoped_lock lock(a, b);`, which locks both without risking a deadlock whatever order you list them in. (Also: never lock a non-recursive `std::mutex` that the same thread already holds, and never hold a lock while waiting for another thread to do something.)

ThreadSanitizer reports a **lock-order inversion** as soon as it has seen the two orders, even if the unlucky timing never occurred (*"a possible deadlock"*). The tests of this lesson contain a deadlock check as well: a test that does not finish is reported as a timeout.

### A consistent view

Sometimes you must read several things *at one moment*: the total of all accounts, a copy of a whole table. Reading them one by one, each under its own lock, can see a transfer half done: the money has left one account and not yet arrived in the other. Lock everything you need (always in the same order), read, and release. This is the price of fine-grained locking.

## Other tools in the same family

- **`std::call_once(flag, f)`**: run an initialization exactly once, thread-safely. A function-local `static` is initialized thread-safely too since C++11 (the "magic static").
- **`thread_local`**: each thread gets its own copy of the variable. No sharing, no race, no lock.
- **Avoid sharing at all** when you can (the previous lesson's per-thread slots): the best lock is the one you do not need.

## Your task: `Bank`

`bank.hpp` holds a bank with a fixed number of accounts. Its comment says what each function does. The starter is correct with one thread and **wrong with several**: no locks. Make it thread-safe:

- every function may be called from several threads at once;
- `transfer` between the same two accounts in opposite directions must not deadlock;
- `total()` must be a consistent snapshot: during transfers it always equals the money the bank started with.

One mutex for the whole bank is the simplest correct answer. One mutex per account (a `std::vector<std::mutex>`) lets unrelated transfers run at the same time, and then you need `std::scoped_lock` for the transfer and an ordered lock-all for `total()`. Either passes the tests; think about which you would choose and why.

- **Works**: the operations, no lost updates from four threads, money conserved across random transfers.
- **Robust**: opposite transfers (deadlock and lock-order checks), self-transfer, the audit that `total()` is never off, a reader racing with writers, argument errors.
- **Clean**: RAII locks only (no `lock()`/`unlock()` calls), no sleeping or `volatile` "fixes".
