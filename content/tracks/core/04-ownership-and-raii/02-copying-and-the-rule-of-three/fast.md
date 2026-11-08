# Fast path: what's different in C++

- **Rule of three**: a class that needs a destructor, a copy constructor or a copy assignment almost always needs all three. If you own a raw resource, the compiler's member-wise copy is a **shallow copy** (two owners of one buffer: double free).
- **Copy constructor** `T(const T&)`: build a new object with its **own** copy of the resource. **Copy assignment** `T& operator=(const T&)`: the object already owns something.
- Assignment recipe: handle `this == &other` (or make it harmless), **build the copy first**, then release the old resource, then take the new one (strong exception guarantee), `return *this;` by reference.
- **Aliasing**: `a = a`, `a.append(a.c_str())`: an argument may point into the object being modified. Read from it before you release the old storage.
- `new[]` pairs with `delete[]`; `new` with `delete`.
- Sanitizers turn all of these into precise reports: double free, heap-use-after-free, leak, with the line that allocated the memory.
- (Better still: don't own raw memory at all, use `std::vector`/`std::string`, and the rule of zero applies. Two lessons ahead.)
