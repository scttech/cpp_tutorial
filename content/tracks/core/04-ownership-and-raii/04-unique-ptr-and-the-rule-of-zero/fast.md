# Fast path: what's different in C++

- **`std::unique_ptr<T>`**: sole owner of a heap object, deletes it in its destructor, **move-only**. Create with **`std::make_unique<T>(args)`**; never write `new`/`delete` in application code. `.get()` = non-owning raw pointer (a view), `.reset()` = delete now, `.release()` = give up ownership without deleting (rare; you must delete the result).
- Raw pointers and references mean "I only look"; the owner is whoever holds the `unique_ptr`. Ownership is part of the type.
- **Rule of zero**: build classes from owning members (`vector`, `string`, `unique_ptr`) and write none of the five special members. Writing *any* of them suppresses the implicit moves.
- **Cached state breaks the rule of zero**: generated moves copy plain members like `size_t size_` without resetting the source. Derive instead of caching, or write the moves.
- To edit a linked structure, hold a **`unique_ptr<Node>*`** pointing at the owning pointer; `*link = std::move(child)` replaces a node and frees the old one.
- Copying a move-only type is deleted; provide an explicit `clone()` if you need a deep copy.
