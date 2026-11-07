# Fast path: what's different in C++

- Declare before use. A prototype (`int f(int);`) is a declaration; the body is the definition.
- **Arguments are copied by default**, including big containers. Pass read-only large objects as `const T&`; use `T&` for out-parameters; pass small scalars by value.
- Overloading is resolved at compile time on parameter types (not the return type). Integral/float *promotions* beat conversions: `f('a')` picks `f(int)` over `f(double)`.
- Default arguments belong on the declaration and are substituted **at the call site** from the declaration visible there; only trailing parameters can have them.
- Functions that promise a return value but fall off the end are undefined behavior (except `main`).
