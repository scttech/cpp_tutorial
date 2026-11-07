# Test-out: Rotate and dedupe

> **Already comfortable with `std::vector`?** Pass this challenge (both the **Works** and **Robust** tiers) and Module 2
> is marked complete. You can still open any of its lessons later.

## The task

Implement

```cpp
std::vector<int> rotate_and_dedupe(const std::vector<int>& values, int k);
```

It does two things, **in this order**:

1. **Rotate left by `k`.** Rotating `{1, 2, 3, 4, 5}` left by 2 gives `{3, 4, 5, 1, 2}`: the first two elements move to
   the back.
2. **Remove duplicates, keeping the first occurrence** of each value and the order of what is left.

```
rotate_and_dedupe({1, 2, 1, 3}, 1)
  rotate left by 1:  {2, 1, 3, 1}
  drop later repeats: {2, 1, 3}
```

### The details

- `k` can be **larger than the size** (rotating by the size is a full turn: nothing changes), **zero**, or **negative**
  (a negative `k` rotates **right**: `{1, 2, 3, 4, 5}` by `-1` gives `{5, 1, 2, 3, 4}`).
- An **empty** vector gives an empty vector.
- The input must not be modified (it is `const&`).
- It should stay fast for a few hundred thousand elements.

### Things to watch

- In C++, `-1 % 5` is `-1`, not `4`. You met this in Module 1.
- `k % size` is a division: what if the vector is empty?
- `k` can be as extreme as `INT_MIN`. Think about what you do to it before using it as an index.
- Searching the output for every element works for ten values. For 200 000 distinct values it does not.
