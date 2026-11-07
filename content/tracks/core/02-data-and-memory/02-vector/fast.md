# Fast path: what's different in C++

- `std::vector<T>` owns a heap block. **size** = elements held, **capacity** = room before the next reallocation. Growing past capacity allocates a bigger block, moves everything, and frees the old one.
- `reserve(n)` changes capacity (size stays 0); `resize(n)` changes size (creates elements). Know the final size? `reserve` it: one allocation instead of ~log2(n).
- **Invalidation:** reallocation invalidates all iterators/pointers/references; `erase` invalidates those at and after the erased position. Never erase inside a plain iterator loop. Use erase-remove, or `std::erase_if(v, pred)` in C++20.
- `v[i]` is unchecked (UB out of range); `v.at(i)` throws. `vector<bool>` is a bit-packed special case with proxy references: avoid it.
- Widen before you multiply: `static_cast<long long>(i) * i`, not `i * i` (signed overflow is UB).
