# Hero Project: Memory Allocator

> **`malloc`, from the inside.** Every `new`, every `std::vector`, every `std::string` ends up asking an allocator for bytes. You have used containers and smart pointers for ten modules; here you write the thing underneath: a *general-purpose allocator* that carves variable-sized blocks out of one region of memory, takes them back, merges neighbours so the memory does not shatter into unusable crumbs, and notices when the program passes it a pointer it never gave out. This is the lowest-level project of the course, and the most unforgiving: a single wrong offset corrupts everything silently.

## The problem

You are given a `Heap`: a region of raw bytes (any address, any size). You implement `class Allocator` on it:

```cpp
void*       allocate(std::size_t bytes);              // ≥ bytes usable, 16-byte aligned, inside the heap; nullptr if nothing fits
void        deallocate(void* p);                      // nullptr: nothing. A live block: free + merge. Anything else: rejected and counted
void*       reallocate(void* p, std::size_t bytes);   // like realloc (see below)
std::size_t usable_size(const void* p) const;         // the real capacity of a live block, 0 for anything else
Stats       stats() const;                            // counters (types.hpp)
bool        check() const;                            // walk the whole heap, verify your own invariants
```

- `allocate(0)` gives a valid minimal block; `allocate(SIZE_MAX)` fails cleanly (watch the rounding arithmetic: `SIZE_MAX + 15` overflows).
- `reallocate(nullptr, n)` is `allocate(n)`; `reallocate(p, 0)` frees `p` and returns `nullptr`; otherwise a block of at least `n` bytes holding the old contents (up to the smaller size). **If it cannot be done, return `nullptr` and leave the old block valid.** Shrinking never moves; growing first tries to absorb a free neighbour.
- `deallocate` / `reallocate` with a pointer that is not the start of a live block (outside the heap, in the middle of a block, already freed, the start of the heap…) must be **rejected**: counted in `stats().rejected_frees`, nothing changes. Not a crash, not corruption. `nullptr` is simply ignored and not counted.

## The memory model: everything is an offset

The `Heap` class (read-only, `types.hpp`) gives you `base()`, `size()` and **`at<T>(offset)`**, a reference to a `T` stored `offset` bytes from `base()`. You keep *all* bookkeeping **inside the blocks** and refer to blocks by byte offsets, not pointers. `at<T>` checks that the access is inside the heap and aligned (it aborts with a message otherwise) and **counts** every access, which is how the Fast tests measure your design. Your allocator object itself may hold only a few numbers (the head of the free list, counters); no `std::vector`, no `new`.

### A design that works (you may choose another)

```
 offset 0                                                                                       heap.size()
 ┌──────────┬───────────────────────┬──────────┬──────────────┬──────────┬─────────────────────────────┐
 │ Header   │ payload (in use)      │ Header   │ links…(free) │ Header   │ …                           │
 └──────────┴───────────────────────┴──────────┴──────────────┴──────────┴─────────────────────────────┘
   size|flag, prev_size                                          one block = header + payload; sizes are multiples of 16
```

- **Header** (16 bytes): the block's total size with bit 0 = "in use", and the size of the **physically previous** block (a *boundary tag*). The payload therefore starts at a multiple of 16: alignment for free.
- **Free blocks** keep a `next`/`prev` link (offsets) in their payload: a doubly linked **free list**, pushed at the front (O(1)), unlinked in O(1).
- **Splitting**: a free block bigger than needed is cut in two; the remainder must be at least a minimal block (header + 16 bytes), otherwise the extra bytes stay with the allocation.
- **Coalescing**: when a block is freed, look at the next block (header at `offset + size`) and the previous one (header at `offset − prev_size`); merge with those that are free, **removing them from the free list first**. After every free no two free blocks are neighbours. Remember to update the `prev_size` of the block that comes after the merged one.
- **Validating a pointer**: it must be inside the heap, a multiple of 16, at least one header from the start, and the header before it must say "in use" with a size that is sane, and *agree with its neighbours* (the next block's `prev_size` equals it). A double free is caught because the flag is clear; a pointer into a payload is caught because the "header" there is just data.

## The tiers

- **Works**: alignment, no overlap, data survives, splitting, coalescing in every combination, reuse of holes, `reallocate` in all its cases, the counters.
- **Robust**: pointers that are not blocks (outside, interior, double free, heap start), huge sizes, odd heap addresses and sizes down to nothing, failed `reallocate`, fragmentation, **canary bytes** around the heap that must never change, and a 30000-operation random stress test that checks patterns in every live block, overlaps, `check()`, and that freeing everything gives back *one* block.
- **Fast**: a constant number of heap accesses per `allocate` and `deallocate` (measured: ≤ 12 and ≤ 22 on average, and the same for 5000 and 20000 blocks); a steady-state churn; no use of `new`/`malloc` by the allocator itself; overhead of at most 32 bytes per block; no slivers; 400000 operations in time. *Hint for the last two:* freeing needs the previous block in O(1) (boundary tags!), and the free list must not be kept sorted by address (that is an O(n) insertion).
- **Clean**: no `new`/`delete`/`malloc`/`free`, no `reinterpret_cast` (all access goes through `Heap::at`), no containers or smart pointers.

## How to approach it

Write `blocks.hpp` first (the header, the links, a few helpers), then the constructor and `allocate` with splitting, then `deallocate` without merging (check the Works tests), then add the merging, then `validate`, then `reallocate`. Write `check()` early and call it in your own experiments: it is the best debugging tool you have. The Run button shows a small session.
