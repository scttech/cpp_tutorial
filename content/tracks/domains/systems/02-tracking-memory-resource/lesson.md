# Memory Tracker with Leak Reports

> **You're an engineer on a systems team.** A long-running service slowly eats memory and nobody knows where. Valgrind is too slow for production and AddressSanitizer is a test-time tool. What you want is a tiny, always-available *accounting layer*: "how many allocations are live, how much memory, what was the peak, and, when the component shuts down, what was never freed". C++17 gave you the right hook for it: **`std::pmr::memory_resource`**.

## The idea

`std::pmr` (polymorphic memory resources) separates *what* a container stores from *where its memory comes from*. A `std::pmr::vector<int>` asks a `memory_resource*` for bytes, and any resource you derive will do. Yours **wraps another resource** (the "upstream"), forwards every request, and keeps books on the side. Because containers propagate their allocator to the elements they contain (`pmr::vector<pmr::string>`), one tracker sees the whole tree.

You implement three virtual functions:

```cpp
void* do_allocate(std::size_t bytes, std::size_t alignment) override;
void  do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override;
bool  do_is_equal(const std::pmr::memory_resource& other) const noexcept override;   // identity!
```

## The books

| Counter | Meaning |
|---|---|
| `live_count()` / `live_bytes()` | blocks and bytes allocated and not yet freed |
| `peak_bytes()` | the **maximum** `live_bytes()` ever seen |
| `total_allocations()` / `total_deallocations()` | lifetime totals |
| `bad_frees()` | deallocations of a pointer that is **not live** (never allocated here, or a double free) |
| `mismatched_frees()` | deallocations whose size or alignment differ from the allocation |
| `leaks()` | the live blocks as `{ptr, bytes, alignment, id}` in allocation order |

Every successful allocation gets the next **id** (1, 2, 3 ...). Ids are never reused, so "leak #7" always means the same allocation.

## Rules that make it trustworthy

- **Forward exactly what was asked**: no header in front of the block, no padding. A tracker that changes the sizes is not transparent, and it breaks resources that care (a `monotonic_buffer_resource` upstream, an arena that asserts on size).
- **Record only after the upstream succeeded.** If it throws `bad_alloc`, nothing may be counted and the next id is not consumed.
- **A bad free is counted, never forwarded.** Passing a foreign pointer or a double free to the real allocator is what corrupts the heap; the tracker's job is to catch it first.
- **A mismatched free is counted, and the block is freed as it was allocated** (with the recorded size and alignment), so the upstream stays consistent.
- **O(1) bookkeeping**: finding the record of a pointer must not scan. A `std::unordered_map<void*, Allocation>`; sort only when a report is requested.
- The books are *not* allocated from the upstream: the tracker's own nodes use the ordinary heap.

## The report

```
no leaks (<total_allocations> allocations, peak <peak> bytes)
```
or
```
LEAK: <n> allocation(s), <bytes> byte(s) not freed
  #<id>: <bytes> bytes, alignment <a>          (one line per leak, in allocation order)
```
followed, if they happened, by
```
BAD FREE: <k> deallocation(s) of unknown pointers
MISMATCH: <k> deallocation(s) with the wrong size or alignment
```
Each line ends with a newline. No addresses (they differ from run to run).

## The tiers

- **Works**: counters, peak, leaks list, ids, both report formats, containers that die cleanly versus one that leaks, bad and mismatched frees, equality.
- **Robust**: the upstream receives exactly the sizes and alignments requested; an upstream that throws; stacked trackers; a `monotonic_buffer_resource` on top; nested pmr containers where every byte goes through the tracker; 20000 random operations against a model.
- **Fast**: 200000 live blocks allocated and freed in shuffled order inside the timeout (no scanning), at most about one extra heap allocation per tracked block, a 30000-leak report, no padding.
- **Clean**: no raw memory or exceptions, and no printing inside the resource.
