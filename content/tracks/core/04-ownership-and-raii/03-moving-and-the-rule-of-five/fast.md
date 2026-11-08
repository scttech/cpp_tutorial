# Fast path: what's different in C++

- **Move semantics**: `T(T&&) noexcept` and `T& operator=(T&&) noexcept` steal an expiring object's resources (copy the pointer, null the source) instead of copying them. `std::move(x)` is only a cast to rvalue; the move happens in the constructor/assignment it selects.
- **Leave the source valid** (destructible and assignable; here: empty with a null pointer). If it keeps the pointer: double free.
- **Make moves `noexcept`**: `std::vector` relocates with `move_if_noexcept`, so a throwing move means growth **copies** everything.
- **Rule of five**: destructor, copy ctor, copy assignment, move ctor, move assignment. Declaring a copy operation or the destructor suppresses the implicit moves, so `std::move(a)` silently falls back to a **copy** (compiles, works, slow).
- Guard self-move in move assignment (`if (this != &other)`); `std::exchange(x, v)` returns the old value and stores `v`.
- Give owners a cheap `noexcept` **`swap`** (+ a `friend swap` for ADL). Copy-and-swap is a way to write assignment with the strong guarantee.
- Returning a local by value is already optimal (copy elision / implicit move): do not write `return std::move(local);`.
