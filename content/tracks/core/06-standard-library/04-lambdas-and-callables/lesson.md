# Lambdas and callables

A **callable** is anything you can call with `f(args)`: a function, a function pointer, a function object (a class with `operator()`), or a **lambda**. Standard algorithms, callbacks, event systems and threads all take callables. This lesson is how to write them, store them, and pass them without paying for more than you use.

## Lambdas are closures

```cpp
int factor = 3;
auto times = [factor](int x) { return x * factor; };       // captures factor BY VALUE: its own copy
auto bump  = [&factor]()     { ++factor; };                // captures it BY REFERENCE: the original
auto count = [n = 0]() mutable { return n++; };            // an INIT-CAPTURE: a new member, initialized here
auto both  = [=]() { ... };  auto refs = [&]() { ... };    // default captures: convenient, and they hide what is captured
```

A lambda is syntactic sugar for a class: the captures are its **members**, the body is `operator() const`. That explains three rules:

- **`mutable`**: by default the call operator is `const`, so a by-value capture cannot be modified. `mutable` makes it non-const, and the state lives *in the closure*: `make_counter` works because each returned closure has its own `next`.
- **Copying a closure copies its state.** A copy of a counter continues independently from where the copy was made.
- **A by-reference capture does not keep anything alive.** A closure that outlives the function that made it (stored, returned, sent to another thread) must capture by **value**. `for (int f : factors) fs.push_back([&f](int x) { return x * f; });` captures a loop variable that is gone on the next iteration: a dangling reference, the same bug as a returned reference to a local.

A lambda with `auto` parameters is a **generic lambda**: its `operator()` is a template, so `[](auto&&... args)` accepts anything. `compose(f, g)` returns one, so that it accepts whatever `f` accepts.

## Storing callables: `std::function`

`std::function<R(Args...)>` can hold *any* callable with that signature (a lambda, a function pointer, a functor): that is **type erasure**. It is what you need to store callables of different types in one container or member, as in the `EventBus`.

It has a cost: it may allocate, copying a `std::function` **copies the callable inside it**, and every call is an indirect call the compiler usually cannot inline. So:

| Situation | Use |
|---|---|
| Store callables of different types (a registry, a callback member) | `std::function` |
| A function that just *calls* its argument (`apply_n`, an algorithm) | a **template parameter** `F&&`: no copy, no allocation, inlinable |
| A parameter that is only used during the call and need not be a template | `std::function_ref` (C++26) or a template |

Pass and store callables by **forwarding reference** or by **move**, not by copy: `subscribe(topic, Handler handler)` takes the handler by value and `std::move`s it into the registry; `publish` iterates the entries by `const&` so that no `std::function` is copied.

## Your task

Fill in `callbacks.hpp`: an `EventBus`, `make_counter`, `make_multipliers`, `compose`, `apply_n` and `make_once`. The tiers: **Works**, **Robust** (ids, order, unsubscribing, copies of closures, captured values that must outlive their scope, several arguments), **Fast** (**no copy** of a handler or of the callable passed to `apply_n`, one million calls without indirection).
