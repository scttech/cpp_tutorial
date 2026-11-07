# Fast path: what's different in C++

- **Signed integer overflow is undefined behavior** (not wraparound). Check *before* the operation; the compiler may delete after-the-fact checks.
- **Unsigned overflow wraps** (defined). Mixed signed/unsigned arithmetic converts to unsigned: `-1 < 1u` is false.
- `/` truncates toward zero and `%` takes the dividend's sign: `-7 % 5 == -2`. Integer `/ 0` or `% 0` is UB.
- Shifts: shifting by >= the bit width, or left-shifting negative values, is UB/implementation-defined. Do bit twiddling on unsigned types.
- Use `<cstdint>` for exact widths and `<limits>` (`std::numeric_limits<int>::max()`) for bounds.
- `x & (x - 1)` clears the lowest set bit; `x != 0 && (x & (x - 1)) == 0` tests for a power of two.
