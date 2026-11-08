# Fast path: what's different in C++

- **`std::shared_ptr<T>`**: shared ownership by reference counting (atomic, so not free); the object is deleted when the last owner goes. Create with **`std::make_shared<T>(args)`**. `.use_count()` shows the count. Prefer `unique_ptr`; use `shared_ptr` only when sharing is the design.
- **Cycles leak**: if A owns B and B owns A through `shared_ptr`s, neither count reaches zero. Decide who owns whom; make the back-reference a **`std::weak_ptr<T>`** (observes, does not own).
- `weak_ptr::lock()` returns a `shared_ptr` (empty if the object is gone): use it right away, never store the result as "the parent". `expired()` exists but `lock()` is the race-free way.
- Parent owns children (`shared_ptr`/`unique_ptr`), children observe the parent (`weak_ptr`/raw pointer): the standard tree.
- **Aliasing**: an argument may be a reference into the container you are about to modify (`f(a, b->children()[0])`). Copy the `shared_ptr`s into locals before you erase from the container.
- A function that must keep an object alive while it works takes/copies a `shared_ptr` (static `detach(const shared_ptr<Node>&)`, not a member through `this`).
- No raw `new`/`delete` here either: `make_shared` does it.
