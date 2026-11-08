# Fast path: what's different in C++

- **RAII**: constructor acquires, **destructor `~T()` releases**; the compiler calls it when the object goes out of scope on **every** path (return, break, continue, exception). There is no `finally`; this is the mechanism.
- Locals are destroyed in **reverse order of construction**; members in reverse order of declaration.
- **Destructors must not throw** (they are implicitly `noexcept`; a throw during stack unwinding calls `std::terminate`). Catch inside if the cleanup can fail.
- A class that owns a raw resource must not be copied blindly (two owners, double free): `T(const T&) = delete; T& operator=(const T&) = delete;` until you write correct copying (next lessons).
- Standard containers, `std::string`, streams and smart pointers are RAII classes. Prefer them to `new`/`delete`.
- Under the sanitizers a leak, a double free or a mismatched `delete[]` is a precise report with the line that allocated the memory.
