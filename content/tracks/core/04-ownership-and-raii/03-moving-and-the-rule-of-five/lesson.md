# Moving and the rule of five

Copying is expensive for a class that owns memory: a new buffer, a copy of every byte. And very often the copy is **pointless**,
because the original is about to be thrown away:

```cpp
Text make_greeting() {
    Text t("hello");
    t.append(", world");
    return t;                 // t dies right here. Why copy its buffer just to destroy the original?
}
std::vector<Text> v;
v.push_back(Text("temp"));    // the temporary dies at the semicolon
```

Since C++11 you can say "this object is about to die: **take its guts**". That is a **move**, and for an owner of a buffer it means
*stealing the pointer*: no allocation, no copy, a handful of instructions.

## Lvalues, rvalues, and `std::move`

- An **lvalue** has a name and lives on (`a`, `v[3]`). Copy from it.
- An **rvalue** is a temporary or something you have said you are finished with (`Text("x")`, `make_greeting()`, `std::move(a)`).
  The compiler may take its contents.

An rvalue **reference** `Text&&` binds to rvalues. Overload on it and the compiler picks the move version automatically for temporaries.
`std::move(a)` does not move anything: it is only a **cast** that says "treat `a` as an rvalue, I will not need its value any more".
The move itself is done by the move constructor or move assignment that the cast selects.

## The move operations

```cpp
Text(Text&& other) noexcept                      // move constructor: build a NEW object from `other`'s guts
    : size_(std::exchange(other.size_, 0)),      // std::exchange(x, v): returns x's old value and stores v in x
      data_(std::exchange(other.data_, nullptr)) {}

Text& operator=(Text&& other) noexcept {         // move assignment: this object already owns something
    if (this != &other) {                        // a = std::move(a) must not destroy a
        delete[] data_;                          // release our own buffer first
        size_ = std::exchange(other.size_, 0);
        data_ = std::exchange(other.data_, nullptr);
    }
    return *this;
}
```

Two rules make a move correct:

1. **Leave the source valid.** A moved-from object is still alive and will be destroyed (so its destructor must be safe), and may be
   assigned to or reused. Here it is left **empty**: size 0 and a null pointer, so its destructor's `delete[]` does nothing. If
   the source kept the pointer, both objects would free the same buffer: a double free.
2. **Make it `noexcept`.** It cannot fail (nothing is allocated), so say so. This is not decoration: when a `std::vector` grows
   it must relocate its elements, and it only **moves** them if the move constructor is `noexcept` (otherwise a throw halfway would leave the old
   elements damaged, so it copies instead). A move that is not `noexcept` silently makes `vector` growth copy everything.

(This is why `Text` lets an empty object own *no buffer at all*: leaving the source "empty with a fresh `new char[1]`" would allocate
inside a `noexcept` move, which is exactly what a move is supposed to avoid.)

## Rule of five

> If you write any of the destructor, copy constructor, copy assignment, **move constructor** or **move assignment**, think about all five.

Declaring a copy operation (as you did) suppresses the compiler-generated moves, so without your own move members `std::move(a)` quietly
**falls back to a copy**. It compiles, it works, and it is slow: the worst kind of bug. In the starter, the demo shows `move kept the buffer: no`.

## `swap`

Exchanging two objects should not copy anything either. Give the class a `noexcept` member `swap` that swaps the members, plus a `friend`
`swap(a, b)` so that generic code finds it. (A swap written as three copies is correct but allocates three times.)

## Your task

Complete `text.hpp`: copying, the destructor and `append` are done. Write the move constructor, the move assignment and a real `swap`.

| Member | Behavior |
|---|---|
| `Text(Text&&) noexcept` | Takes the buffer and the size; the source is left empty (size 0). Allocates nothing. |
| `operator=(Text&&) noexcept` | Frees its own buffer, takes the other's, leaves the source empty; safe for `a = std::move(a)`. |
| `swap(Text&) noexcept` | Exchanges buffers and sizes, with no allocation. |

The tests check the **pointer** (a move keeps the very same buffer), the moved-from state, `noexcept`, self-move, leaks, and under
the **Fast** tier they count allocations: a move, a swap and a sort of 200 `Text`s must allocate **nothing**, and 100 `push_back`s must
not copy.
