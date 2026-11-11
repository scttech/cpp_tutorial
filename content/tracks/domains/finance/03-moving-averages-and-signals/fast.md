# Fast path: what's different in C++

- **Floating point is not associative.** `(a + b) + c` and `a + (b + c)` can differ, and a running sum is a long chain of such operations. This is why numerical libraries ship `Kahan`/`Neumaier` summation (and why `-ffast-math` is dangerous: it lets the compiler *remove* your compensation term because algebraically it is zero).
- **`std::isfinite`** (in `<cmath>`) is the right test for "a usable number"; `x != x` works for NaN but not for infinity and gets optimised away under `-ffast-math`.
- **Ring buffers** are the idiom for sliding windows: fixed storage, an index that wraps with `%` (or a power-of-two mask), no shifting. The standard library has none until C++26 (`std::inplace_vector` aside); `std::vector` + an index is the usual implementation, and `std::deque` would allocate chunks as it grows and shrinks.
- **`std::optional<double>`** for "no value yet" avoids the sentinel-NaN habit and costs 16 bytes, no heap.
- **Real systems** keep the indicators for thousands of instruments in structure-of-arrays form so that one tick updates a few cache lines; the O(1) update is the prerequisite for that.
- **Numerically**, the EMA is also an *infinite* impulse response filter: old prices never fully leave, but their weight decays by `(1-α)` per step, so it needs no buffer at all.
