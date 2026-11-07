# Fast path: what's different in C++

- A pointer holds an address; `&x` takes it, `*p` follows it, `nullptr` means "none" (dereferencing null is UB). `p + 1` moves by one *element*. A range is `[first, last)`: `last` is one past the end and must not be accessed.
- **Stack** = locals, freed automatically on return, small. **Heap** = `new`/`delete`, you manage the lifetime. Every `new` needs exactly one matching `delete` (`new[]` with `delete[]`).
- The classic bugs: **leak** (never freed), **use after free** (`delete p; *p`), **double free**, **wrong deleter** (`delete` vs `delete[]`), null dereference. In this course freed memory is poisoned with `0xDD` so use-after-free is loud.
- Linked list rules: read `node->next` *before* `delete node`; unlink before deleting; operations that can change the head take and return it; use loops, not recursion, for long lists (stack depth).
- In real code prefer `std::vector`, `std::string` and smart pointers (Module 4): they do this bookkeeping for you.
