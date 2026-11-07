# Fast path: what's different in C++

- A reference (`T&`) is another name for an existing object: no copy, never null, bound once at initialization and never re-seated. Assigning to it assigns to the original.
- Parameters: `T` = copy, `T&` = modify the caller's object, `const T&` = read without copying (also binds temporaries).
- Two reference parameters may alias the same object (`swap(x, x)`); code that breaks on aliasing (XOR-swap) is buggy.
- A function may return a reference only to something that outlives the call (an argument, a member, a container element). A reference to a local, or to a temporary argument, **dangles**.
- `for (auto x : v)` copies each element; `for (auto& x : v)` aliases it; `for (const auto& x : v)` reads without copying. Forgetting `&` when you mean to modify is a silent no-op.
