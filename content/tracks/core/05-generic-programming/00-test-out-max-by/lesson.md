# Test-out: Generic max-by

> **Already comfortable with templates?** Pass this challenge (the **Works** and **Robust** tiers) and Module 5 is marked
> complete. You can still open any of its lessons later.

## The task

Write `max_by` in `max_by.hpp`: a function **template** that finds the element of *any* range with the largest *key*, where the
key of an element is computed by a **projection** you pass in.

```cpp
std::vector<Person> people = {{"Ada", 36}, {"Grace", 45}, {"Alan", 41}};
auto oldest = max_by(people, &Person::age);          // an ITERATOR to Grace (a member pointer is a projection)
auto longest = max_by(words, [](const std::string& w) { return w.size(); });   // a lambda is a projection
auto smallest = max_by(v, [](int x) { return x; }, std::greater<>{});          // the optional third argument compares keys
```

`main.cpp` (read-only) is a small demo: press **Run**.

### The rules

| | |
|---|---|
| Result | An **iterator** to the element with the largest key; the **first** of several with equal keys; `end()` for an empty range. |
| The range | Anything with `begin()`/`end()` (or a C array): `std::vector`, `std::array`, `std::list`, a `const` container (you get a `const_iterator`), an array of move-only elements. Take it as a **forwarding reference** (`Range&&`), and use `std::begin`/`std::end`. |
| The projection | Anything callable on an element: a lambda, a function, **or a member pointer** such as `&Person::age`. Use `std::invoke(proj, element)`, which calls all of them. |
| The comparison | An optional third parameter with a default (`class Compare = std::less<>`). It compares two **keys**. `max_by(v, id, std::greater<>{})` therefore finds the minimum. |
| Requirements on the keys | Only what the comparison needs: with the default, just `operator<` (never `>` or `==`). |
| Elements | Never copied or moved: `max_by` only looks at them. |

## Tiers

- **Works**: the basic cases: vector, member pointer, ties, empty range, array/list, `const`, the comparison.
- **Robust**: different key types, a projection returning a reference, move-only elements, `Tracked` elements (no copies),
  keys with only `operator<`, one element, a long range.
- **Fast**: the projection is called **exactly once per element**, and the comparison once per element after the first (remember
  the best key instead of computing it again).

The tests call your template with many types. If it is too narrow (for example it only accepts `std::vector<int>&`), the failure
message says which call did not compile.
