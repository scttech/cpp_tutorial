# Fast path: what's different in C++

- **Specialization** (class templates): `template <> struct X<int> {...}` is a *full* specialization (one exact type); `template <class T> struct X<T*> {...}` is a *partial* one (a pattern). The primary template is the fallback; the most specialized match wins.
- **Function templates cannot be partially specialized.** Overload instead: `to_text(const std::pair<A, B>&)` beats `to_text(const T&)` by partial ordering.
- **`if constexpr (cond)`** chooses a branch at compile time; the other branches are *discarded* (not compiled for this `T`), unlike a plain `if`. Conditions: `<type_traits>` (`std::is_integral_v<T>`) or `requires { ... }` ("would this compile?").
- Order the branches from most to least special: `bool` and `char` are integral, `std::string` is a range, `const char*` is a pointer.
- **Two-phase lookup**: a dependent call inside a template sees what is declared *above* it plus ADL (namespaces of the argument types). Declare overloads *before* the generic function that calls them.
- Build strings with `out += piece`, not `out = out + piece`: the second copies everything each time (quadratic).
- `std::to_string(1.5)` is "1.500000"; `os << 1.5` is "1.5".
