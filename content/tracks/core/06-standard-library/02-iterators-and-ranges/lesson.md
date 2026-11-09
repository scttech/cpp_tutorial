# Iterators and ranges

Algorithms do not know about containers. `std::sort`, `std::find` and `std::copy` work on a pair of **iterators**, so one algorithm serves a `vector`, a `deque`, an array or a file. C++20 adds **ranges**: algorithms that take the container itself, and **views**, a pipeline syntax for transforming sequences lazily.

## Iterators and their categories

An iterator is anything that behaves like a pointer into a sequence. What it can do depends on the container, and the algorithms use that:

| Category | Adds | Examples |
|---|---|---|
| input | `*it`, `++it`, one pass | `std::istream_iterator` |
| output | `*it = x`, `++it` | `std::back_inserter`, `std::ostream_iterator` |
| forward | multi-pass | `std::forward_list` |
| bidirectional | `--it` | `std::list`, `std::map`, `std::set` |
| random access | `it + n`, `it[n]`, `it2 - it1` in O(1) | `std::deque` |
| contiguous | elements adjacent in memory | `std::vector`, `std::array`, `std::string`, pointers |

Every category includes the ones above it: a `vector` iterator is also random access, bidirectional, forward and input. In C++20 each is a **concept** (`std::random_access_iterator<It>`...), so a function can pick its strategy:

```cpp
template <class It> void advance_by(It& it, std::ptrdiff_t n) {
    if constexpr (std::random_access_iterator<It>) it += n;      // one jump, O(1)
    else for (; n > 0; --n) ++it;                                // n steps, O(n)
}
```

This is exactly what `std::advance` does, and why `std::sort` needs random access (a `std::list` has its own `sort()`).

## Ranges and views

```cpp
std::vector<int> v{1, 2, 3, 4, 5, 6};
auto pipeline = v | std::views::filter([](int x) { return x % 2 == 0; })
                  | std::views::transform([](int x) { return x * x; });
for (int x : pipeline) ...        // 4 16 36
```

A **view** is a cheap, non-owning description of a sequence. Nothing is computed until you iterate, and no intermediate containers exist. That makes **infinite** sequences usable: `std::views::iota(0) | std::views::filter(p) | std::views::take(5)` stops after the fifth match.

The common views: `filter`, `transform`, `take`, `drop`, `take_while`, `drop_while`, `reverse`, `split`, `join`, `keys`, `values`, `iota`. In C++20 collect a pipeline with a loop or `std::ranges::copy`; `std::ranges::to<std::vector>` arrives in C++23.

Gotchas:

- `take_while` **stops at the first failure**; `filter` goes on. `{-3, -1, 5, 7, 120, 9}` with `take_while(< 100)` after the negatives are dropped is `{5, 7}`, not `{5, 7, 9}`.
- `std::views::split` **keeps empty pieces** (`"a,,b"` is three pieces) and gives subranges, not strings.
- **A view does not own what it views.** `auto bad = make_vector() | std::views::filter(p);` views a temporary that is gone by the next line.
- Views are **lazy**: a predicate with side effects runs when you iterate, as many times as you iterate.

## Iterator invalidation

An iterator, pointer or reference into a container is only valid until the container is changed in a way that moves elements. For a `vector`, `push_back` and `insert` may reallocate (everything is invalidated) and `erase` invalidates from the erased element on. For a `list` or a `map` only iterators to erased elements are. So **never mutate a container structurally inside a loop over it** without using the iterator the function returns, or rebuilding in one pass: `duplicate_each` here does the second.

## Your task

Fill in `iter_tools.hpp`: iterator categories (`category_name`), a category-driven `advance_by`, four range pipelines (`squares_of_evens`, `middle_section`, `split_pieces`, `first_n_matching`) and `duplicate_each`. **Works**, **Robust** (every category, `const` and reverse iterators, empty pieces), **Fast** (a random access `advance_by` must be one jump, `first_n_matching` must be lazy, `duplicate_each` must handle 200000 elements).
