# Fast path: what's different in C++

- A **concept** (C++20) is a named compile-time requirement on a type: `template <class T> concept Number = ...;`. Build it from standard ones (`std::integral`, `std::floating_point`, `std::same_as`, `std::convertible_to`, `std::invocable`) with `&&`, `||`, `!`.
- **Four ways to constrain**: `template <Number T>`, `template <class T> requires Number<T>`, a trailing `requires`, and `Number auto x` (each `auto` parameter is a template parameter).
- A **requires-expression** is code that is *checked, not run*: `requires(const T& t) { os << t; { t.size() } -> std::convertible_to<std::size_t>; typename T::value_type; }`.
- Constraints check the **declaration**, not the body: constrain exactly what the body needs, or the error shows up inside the template.
- **Subsumption**: of several viable overloads, the *more constrained* wins. The compiler compares atomic constraints structurally, so define `Number` **in terms of** `Integer` (`Integer<T> || floating_point<T>`); copying Integer's definition makes the call ambiguous.
- `bool` and `char` are integral types: decide whether your concept wants them (`std::integral<T> && !std::same_as<T, bool>`).
- A rejection by a constraint is a **compile error**, so test it with `requires { f(x); }`, not by calling.
