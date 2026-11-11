# Test-out: Make it fast

> **Already know how to make code fast?** Pass this challenge (all three tiers) and Module 10 is marked complete. You can still open any of its lessons later.

## The task

`count_pairs.hpp` holds a function that is **correct but slow**:

```cpp
template <class T>
long long count_pairs(const std::vector<T>& values, const T& target);
```

It counts the pairs of positions `i < j` with `values[i] + values[j] == target`. It does that by trying every pair, so doubling the input quadruples the work. Rewrite it so that the work grows like `n log n`, and keep every answer the same.

Two equal values at different positions *are* a pair (`{3, 3}` with target 6 has one pair), a value is never paired with itself, and the vector you were given must not be changed.

## How "fast" is measured here

Nobody times a program on this site: a stopwatch gives a different number on every computer, and in a browser or a busy container it is close to noise. Instead the tests give your function a number type, `Num` (in `probe.hpp`), that **counts every `==`, `<`, `+` and copy** done to it. The counts are the same on every run, on every machine. Press **Run** and `main.cpp` prints them for 500, 1000 and 2000 numbers: look at how the count grows when the input doubles.

## The tiers

- **Works**: correct counts for small and random inputs (including many equal values).
- **Robust**: empty input, one value, runs of equal values, negative numbers, the caller's vector unchanged.
- **Fast**: a budget for 2000 numbers, a *scaling* test (twice the numbers cost about twice the work), and a limit on copies.
