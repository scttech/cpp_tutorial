# Fast path: what's different in C++

- **Integers do not forgive overflow.** Signed overflow is *undefined behaviour* in C++: the compiler may assume it never happens, and a `if (a + b < a)` check can be optimised away. Use a wider type (`__int128`), `__builtin_add_overflow`, or C++26's `std::add_sat`/`checked` helpers. Java's `Math.addExact` and Rust's `checked_add` exist for the same reason.
- **`INT64_MIN` is the odd one out.** `-INT64_MIN`, `abs(INT64_MIN)` and `INT64_MIN / -1` all overflow (and the division traps on x86!). Every robust integer library has a special case for it.
- **Division rounds toward zero** in C++ (since C++11; before, it was implementation-defined). `-7 / 2 == -3`, `-7 % 2 == -1`. Python's `//` floors, so code ported from Python is often wrong for negatives.
- **`std::optional` instead of exceptions or sentinels.** A function that can fail for ordinary reasons returns `std::optional<T>` (or `std::expected<T, E>` in C++23). It is a value, costs a flag byte, never allocates, and the compiler warns if you ignore it (`[[nodiscard]]`).
- **`constexpr` money.** The whole type can be computed at compile time because it is just integers; `constexpr std::optional<Money>` works in C++17 and later. Prices known at compile time (fees, tax rates) cost nothing at run time.
- **Why not `decimal` types?** Java has `BigDecimal`, Python `decimal.Decimal`, C# `decimal`. They are arbitrary-precision or 128-bit decimal floats and slow. Trading and payment systems usually store integer minor units (cents, or 1/10000 of a unit) exactly as you do here.
