# Fast path: what's different in C++

- **Floating point error is a design input.** Python's `statistics.stdev` and NumPy use Welford-like or two-pass algorithms for the same reason; a hand-rolled `sum_sq - n*mean²` is the most common numerical bug in production analytics code.
- **`std::nth_element`** is quickselect: it rearranges the range so the k-th element is where it would be if sorted, with everything before it not greater. O(n) on average versus O(n log n) for `std::sort`; `std::partial_sort` gives the k smallest in order. Pandas' `quantile` does the same.
- **Integer arithmetic for indices.** `(n * tail + 99) / 100` is `ceil(n * tail / 100)` for non-negative integers with no floating point: always compute positions and counts with integers.
- **`std::optional` for domain errors.** A risk number that cannot be computed is *not* `0.0` and *not* `NaN` (which silently propagates); `nullopt` forces the caller to decide. C++23 `std::expected` adds the reason.
- **One pass** matters when data does not fit in memory or comes from a stream: Welford, running max (drawdown), running min/max are all *online* algorithms. The `Series` here counts reads so you can prove it.
- **Take by value to copy once.** `historical_var(std::vector<double> returns, ...)` takes its argument by value because it needs to reorder it: the caller's vector is untouched and the copy is made at the call, where the compiler can elide it if the caller passes a temporary.
