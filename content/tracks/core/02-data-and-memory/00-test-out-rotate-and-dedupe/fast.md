# Fast path

Implement `std::vector<int> rotate_and_dedupe(const std::vector<int>&, int k)`: rotate **left** by `k` (any `int`: larger than the size, negative = right, vector may be empty), *then* drop repeats keeping each value's first occurrence. Must be fast for ~200 000 elements. The two steps do not commute: `{1,2,1,3}` with `k=1` is `{2,1,3}`.
