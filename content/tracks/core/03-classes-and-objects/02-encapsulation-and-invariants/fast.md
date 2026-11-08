# Fast path: what's different in C++

- **Encapsulation = `private` data + member functions that keep the invariant.** `struct` defaults to public, `class` to private; use `class` for anything with an invariant.
- Getters must not leak write access: return a **copy** or a **`const&`**, never a `T&` to a private member. Getters are **`const` member functions** (otherwise they cannot be called on a `const&`).
- **Validate, then commit** (strong guarantee): check everything that can fail before changing anything, so a failed operation changes nothing. Do the step that can throw (e.g. `push_back`) first.
- When an invariant spans several members, change them in **one private function** so they cannot drift apart.
- **Aliasing:** `a.f(a)` must still work. Check `&other == this` where it matters.
- Throw from constructors when the object cannot be valid (`std::invalid_argument`); signed overflow is UB, so test before adding.
