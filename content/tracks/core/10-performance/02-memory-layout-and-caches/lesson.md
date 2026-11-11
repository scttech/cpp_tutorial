# Memory layout and caches

> **A modern processor is fast; memory is slow.** Reading a value from RAM takes about as long as hundreds of arithmetic operations. The processor hides this with **caches**: small, fast memories that keep what was used recently. Code that uses memory *the way the cache expects* can be several times faster than code that does the same arithmetic with a bad layout, and no algorithm change is needed.

## Cache lines

Memory is not read byte by byte. It moves in **cache lines**, 64 bytes on almost every desktop and server processor. Touch one `float` and the 64 bytes around it come along: 16 floats, whether you wanted them or not.

That gives two rules:

- **Spatial locality**: things used together should sit together. A loop over a contiguous `std::vector<int>` uses all 16 ints of every line it loads. A loop over a `std::list<int>` follows a pointer to a node that can be anywhere, and uses 4 bytes of each line it loads.
- **Do not load what you do not use.** If a loop needs 4 bytes out of a 52-byte struct, the other 48 bytes still travel through the cache.

The processor also *guesses ahead*: when you walk through memory in order it fetches the next lines before you ask. That prefetching only works for predictable patterns, which is another reason `std::vector` beats pointer-based containers for most loops, even when the textbook big-O says they are equal.

## Counting cache lines

You cannot see the cache from C++, but you can count **how many different lines a loop touches**: the addresses are right there (`&x`). `cacheline.hpp` does this for you, and `main.cpp` prints the numbers. Fewer lines means less memory traffic, and the number is the same on every machine.

## Array of structs, struct of arrays

```cpp
struct Particle { float x, y, vx, vy, mass; char name[32]; };
std::vector<Particle> particles;                 // array of structs (AoS)
```

is the natural way to write it, and each particle's data is together. But `step()` needs 16 of the 52 bytes, and `sum_x()` needs 4. The other layout:

```cpp
struct Particles { std::vector<float> x, y, vx, vy, mass; std::vector<std::array<char, 32>> name; };   // struct of arrays (SoA)
```

keeps each field in its own array: a loop over `x` reads only `x`, 16 values per line, and the cold `name` data is never in the way. The price is that "one particle" is spread over several arrays. Rules of thumb: AoS when you usually need *all* the fields of one object together; SoA (or splitting off the **cold** fields) when hot loops need a few fields of *many* objects.

## Padding and member order

Every type has an **alignment** (a `double` must sit at a multiple of 8, an `int` at a multiple of 4), so the compiler inserts **padding** between members, and at the end so that arrays of the struct stay aligned. Putting members in a different order can remove it:

```cpp
struct A { char a; double b; char c; };   // 24 bytes: 1 + 7 pad + 8 + 1 + 7 pad
struct B { double b; char a; char c; };   // 16 bytes
```

The usual rule is *largest alignment first*. `sizeof` and `alignof` tell you the truth; `static_assert(sizeof(B) == 16)` keeps it that way.

## False sharing

Two threads write two *different* variables that happen to live in the **same cache line**. The variables are independent, but the hardware moves whole lines: each write forces the other processor to drop and reload the line, and the two threads slow each other down for no reason. The cure is to give each hot variable a line of its own: `struct alignas(64) Counter { ... };` (C++17 offers `std::hardware_destructive_interference_size` for the number; it is 64 on the usual machines). The price is memory, so use it only for per-thread data that is written often.

## The exercise

`layout.hpp` has three parts (the comments say what to do): reorder `Record` so that `sizeof` is 16; give `ShardCounter` a cache line of its own; turn `ParticleSystem` from an array of structs into a struct of arrays **without changing a single function signature or result**. Press **Run** first: `main.cpp` prints what the starter costs.

## The tiers

- **Works**: the same values and results as before.
- **Robust**: empty system, name length rules, data kept when the arrays grow, writes through the accessors, counters on separate lines.
- **Fast**: `sizeof(Record)` is 16, and the three loops touch at most 1100, 270 and 270 cache lines.
