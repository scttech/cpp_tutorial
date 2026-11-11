# Allocation costs and arenas

> **`new` is not free.** An allocation asks the heap manager (`malloc`) to find a free block of the right size, mark it used, and later to take it back, merge it with its neighbours and keep its own lists in order. That is hundreds of instructions, often a lock, and the block lands wherever there was room, which hurts the cache. A program that makes a million small allocations in a loop spends much of its time in the allocator.

## Fewer allocations first

Before you reach for a custom allocator, remove the allocations you do not need:

- `reserve()` a `std::vector` whose size you know (one allocation instead of about 20 growths);
- build a `std::string` with `append`/`reserve` instead of `s = s + x` in a loop;
- keep objects on the stack, or in a `std::array`, when the size is fixed;
- pass `std::string_view`/`std::span` instead of copying; use `emplace_back` and move;
- and when the lifetimes allow it, use an **arena**.

## The arena (bump allocator)

Many programs allocate a lot of small things that all die together: the temporary nodes of one request, one frame of a game, one parse of a file. An arena takes ONE big block and hands it out from front to back:

```
block:  [ used ................ | marker -> free ........................ ]
allocate(n):  p = marker;  marker += n;  return p;
```

An allocation is a few instructions, there is no per-allocation bookkeeping and the objects are packed together in the order they were made (good for the cache). The price: **you cannot free one object**. Everything goes at once, with `reset()` or when the arena is destroyed. Two rules follow from that:

1. **Lifetime**: nothing may use the arena's memory after `reset()` or after the arena is destroyed. A container that outlives its arena holds dangling pointers.
2. **No destructors**: an arena never calls them. An object that owns something (a `std::string` with heap memory, a `std::vector`, a file) must not live in a plain arena unless something else cleans it up. That is why `create<T>` here only accepts trivially destructible types. (Containers that use the arena for *all* their memory, as below, are fine, because they have nothing that needs releasing: you only have to make sure they do not outlive the arena.)

## Alignment

A `double` must sit at an address that is a multiple of 8, a `std::max_align_t` at a multiple of 16, a cache-aligned struct at a multiple of 64. So `allocate(size, align)` first moves the marker up to the next multiple of `align`:

```cpp
aligned = (address + align - 1) & ~(align - 1);        // align must be a power of two
```

Align the **address**, not the offset: the block itself does not have to start on a 64-byte boundary. The skipped bytes are padding, and they count as used.

## `std::pmr`: allocators as objects

Standard containers normally take their allocator as a template argument, so `std::vector<int, A1>` and `std::vector<int, A2>` are different types. C++17 added **polymorphic memory resources** in `<memory_resource>`: an abstract class `std::pmr::memory_resource` with `allocate(size, align)` and `deallocate(p, size, align)`, and aliases `std::pmr::vector`, `std::pmr::string`, `std::pmr::map`, ... that take a resource at run time:

```cpp
Arena arena(1 << 20);
std::pmr::vector<std::pmr::string> names(&arena);    // the vector and every string inside it allocate from the arena
```

The resource is passed on to the elements automatically (**uses-allocator construction**): `emplace_back` builds each `pmr::string` with the vector's resource. The standard library also ships ready-made resources: `std::pmr::monotonic_buffer_resource` (an arena, like yours), `std::pmr::unsynchronized_pool_resource` and `std::pmr::synchronized_pool_resource` (pools of equal-size blocks), and `std::pmr::new_delete_resource()` (the normal heap). A **pool** keeps a free list per size class, so it can reuse single blocks; an arena cannot.

## The exercise

`arena.hpp` has an `Arena` that derives from `std::pmr::memory_resource`. The stub forwards every request to its *upstream* resource (the heap), which is correct and has exactly the cost the arena should remove. Fill in the comments' rules: take one block from upstream, hand it out with correctly aligned addresses, send what does not fit to upstream (and send it back when it is deallocated), `reset()`, and `create<T>`.

`CountingResource` (`counting.hpp`) plays the heap and counts the trips to it. Press **Run** to see the cost of 1000 strings with and without an arena.

## The tiers

- **Works**: separate writable regions, `used()`, `create<T>`, and `pmr::vector`/`pmr::string` that really use the arena.
- **Robust**: alignments up to 64, the fallback path and who frees what, an exact fit, absurd sizes, `reset()`, zero-byte requests, the destructor, `is_equal`.
- **Fast**: one trip to the heap for thousands of allocations, no per-allocation overhead, and a reusable block.
