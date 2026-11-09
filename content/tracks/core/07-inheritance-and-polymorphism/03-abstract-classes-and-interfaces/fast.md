# Fast path: what's different in C++

- **There is no `interface` keyword.** An abstract class (at least one `= 0` function) cannot be instantiated; a class with only pure virtual functions and a virtual destructor is an interface by convention. Derived classes implement every pure virtual function or stay abstract themselves.
- **Multiple inheritance is real** (`class A : public B, public C`). Inheriting from several *interfaces* is routine; inheriting data from several bases leads to the diamond (lesson 4).
- **A pure virtual function can have a definition** (outside the class body): `inline std::string Sink::format(...) const { ... }`. Derived classes can call it. A pure virtual *destructor* always needs one.
- **Interfaces are non-copyable** by convention: delete the copy operations (and give a `protected` default constructor). Clone with a virtual function instead.
- **The Interface Segregation Principle**: many small interfaces beat one fat one. Compare Java's `Closeable`, `Flushable`, `Comparable`.
- **Decorator** = wrap an object that has the same interface. Holding a reference or pointer means *you must not outlive it*; holding a `unique_ptr` means you own it.
- **`std::string_view` parameters** are borrowed: copy into a `std::string` if you keep the text.
- **Cross-cast**: `dynamic_cast<Other*>(base_ptr)` can go sideways between unrelated polymorphic bases of the same object. Returns null when the object has no such base. Needs RTTI.
- **A constrained factory**: `template <std::derived_from<Sink> S, class... Args> S& emplace(Args&&...)` returns the real type; the concept rejects wrong types at the call site.
- **`final`** on a concrete leaf class (`class JsonSink final : ...`) says "do not derive from me" and lets the compiler devirtualize calls on it.
