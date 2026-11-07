# Fast path: what's different in C++

- `struct` and `class` are the same, except default access: `struct` members are public, `class` members private. Use `struct` for plain data, `class` (private data, public interface) to protect an invariant.
- Mark member functions that do not modify the object `const`. `this` is a pointer to the current object; `return *this;` plus a `T&` return type enables chaining. Returning `T` by value returns a copy.
- Initialize members in the constructor's **initializer list**. They are initialized in **declaration order**, not list order: declare a member after anything its initializer reads (`-Wreorder` / `-Wuninitialized` warn about this).
- Padding: members are aligned, so `sizeof` can exceed the sum of the members. Declaring larger members first usually minimizes padding.
- Keep derived/cached members in sync everywhere the inputs change.
