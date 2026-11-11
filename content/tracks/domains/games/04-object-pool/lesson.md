# Object Pool with Checked Handles

> **You're an engineer at a game studio.** The bullet hell level allocates and frees thousands of small objects per frame. `new` and `delete` are slow, they fragment the heap, and every so often a hitch of 30 ms ruins a boss fight. The lead asks for an **object pool**: a fixed block of memory for N objects, constant-time create and destroy, no heap, no fragmentation. And since other systems keep references to bullets, a destroyed bullet must never be mistaken for the new bullet that took its place.

## What to build

`Pool<T, N>` in `pool.hpp`. Everything lives **inside** the pool object: N slots of raw storage, plus a few small arrays. Read the comment at the top of the starter for the exact behaviour; the key ideas:

- **Raw storage, explicit lifetime.** The slots are `unsigned char` buffers (`alignas(T)`!). You construct a `T` in a slot with `std::construct_at(ptr, args...)` and end its life with `std::destroy_at(ptr)`. A slot that holds no object holds no `T`: no default constructor runs for the unused ones.
- **An intrusive free list.** Free slots are linked through an array `next_free_`; `free_head_` is the first one. `create` pops the head, `destroy` pushes the slot back: both O(1). The slot freed last is reused first.
- **Generational handles.** A `Handle` is `{index, generation}`. `destroy` increments the slot's generation, so every old handle to that slot stops matching: `get` returns `nullptr`, `destroy` returns `false`, and the new occupant is safe.
- **Perfect forwarding.** `create(Args&&... args)` hands `std::forward<Args>(args)...` to the constructor: move-only arguments and in-place construction both work, with no temporary `T`.
- **Exception safety.** Construct **before** you change the bookkeeping. If the constructor throws, the free list is untouched and the slot is not lost.
- **The destructor** destroys the objects that are still alive. The pool is not copyable.

## The tiers

- **Works**: create/get/destroy with forwarding, in-place construction (no copies/moves), a full pool, slot reuse with generations, stale handles, destructors, `for_each`, stable addresses.
- **Robust**: invalid/out-of-range/double destroy, a pool of one, over-aligned types (`alignas(32)`), move-only arguments, const access, not copyable, a throwing constructor, 20000 random operations against a model.
- **Fast**: 2 million cycles on a 200000-slot pool (constant time), zero heap allocations, at most 16 bytes of bookkeeping per slot, no copies or moves.
- **Clean**: no `new`/`delete`/`malloc`/`free`, no containers or smart pointers, `construct_at`/`destroy_at`/`forward` used, no try/catch.

## Tips

Mark a slot as occupied by storing a special value (say `kUsed`) in `next_free_` instead of keeping a separate flag array. A big pool is a big object: the tests create large pools on the heap (or as `static`), and so should you.
