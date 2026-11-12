# Const correctness

`const` is a promise the compiler checks for you. Used everywhere it can be, it turns a whole class of bugs ("who changed
this?") into compile errors, and it is the difference between a class that works with `const&` parameters and one that
does not. **Const correctness** means: if something is not supposed to change, say so, and let the compiler enforce it.

## const member functions

A `const` after the parameter list says "this function does not modify the object":

```cpp
class Counter {
public:
    int value() const { return n_; }   // can be called on a const Counter
    void bump()       { ++n_; }        // cannot
private:
    int n_ = 0;
};

void show(const Counter& c) {
    c.value();   // fine
    c.bump();    // error: 'this' argument to member function 'bump' has type 'const Counter', but function is not marked const
}
```

Inside a `const` member function, `this` is a `const Counter*`, so assigning to a member is an error. The error you
get when you forget it is the one above: a `const T&` parameter (which you should use for anything
big that a function only reads) can only call member functions marked `const`. **One missing `const` spreads**: every
function that takes a `const&` to your class loses the ability to use it.

Rule of thumb: **every member function that does not change the object is `const`.** Getters, `size()`, `count()`,
`to_string()`, `operator==`: all of them.

## Const and non-const overloads

Containers such as `std::vector` have two `operator[]`s, and so should your classes where it makes sense:

```cpp
int&       at(std::size_t r, std::size_t c);        // for a Grid you may change: returns a reference you can assign to
const int& at(std::size_t r, std::size_t c) const;  // for a const Grid: read-only
```

They are different functions, and the compiler picks by the constness of the *object*. `Grid g; g.at(0, 0) = 5;` works.
`const Grid& cg = g; cg.at(0, 0) = 5;` is a compile error, which is exactly what you want. Do **not** implement the const
version with `const_cast` to return a mutable reference: that throws the promise away.

## const parameters

A function that only reads its argument takes `const T&`:

```cpp
long long row_sum(const Grid& g, std::size_t r);   // no copy, and it cannot modify the grid
long long row_sum(Grid g, std::size_t r);          // copies every cell on every call
long long row_sum(Grid& g, std::size_t r);         // cannot be called with a const Grid or a temporary
```

(Cheap types such as `int` and `double` are passed by value; the rule is for anything that is expensive to copy.)
One more detail: a function **defined in a header** must be `inline`, or two files that include the header define it twice.

## `mutable`: logically const, physically not

Sometimes a function does not change what the object *is*, but does change a private detail: a cache, a counter, a lock.
`total()` below does not change the grid's contents; it just remembers the answer. `mutable` marks the members that a
`const` function may modify:

```cpp
long long total() const {
    if (!cache_) { cache_ = compute(); ++recomputations_; }   // allowed: both members are mutable
    return *cache_;
}
mutable std::optional<long long> cache_;
mutable std::size_t recomputations_ = 0;
```

`mutable` is for **hidden implementation detail only**. If you find yourself marking real data `mutable` to make a
compile error go away, the function is not really `const`.

A cache brings its own invariant: *the cached value is either absent or correct*. Everything that can change the cells
has to throw the cache away. That includes the **non-const `at()`**: it hands out a reference, so you cannot know
whether the caller writes through it. The const overload cannot be written through, so it leaves the cache alone.

## Your task

Complete `grid.hpp`. The `Grid` class is a rectangle of `int`s; `main.cpp` (read-only) is a small demo. As a
warm-up, change `print`'s parameter in your head (or in a scratch copy) to `const Grid&` and imagine the errors: with the
starter every call inside it is rejected. Then make the class const-correct:

| Member | Behavior |
|---|---|
| `Grid(rows, cols, fill = 0)` | Both sizes must be at least 1 (`std::invalid_argument`). |
| `rows()`, `cols()`, `count(v)`, `row_text(r)`, `recomputations()` | `const`. `row_text` is the row with single spaces between numbers, e.g. `1 -4 1`. |
| `at(r, c)` | Two overloads (a mutable reference and a const one). Out of range: `std::out_of_range` (check **both** indices). |
| `fill(v)` | Sets every cell. |
| `total()` | `const`. Cached: it is only computed when there is no cached value, and each real computation adds one to `recomputations()`. Writing through `at()` or calling `fill()` throws the cache away; reading does not. |
| `row_sum(const Grid&, r)` | A free function that takes the grid by **const reference**. |

The tests check the *shape* of your class (what a `const Grid&` can and cannot do) as well as its behavior.

## Tiers

- **Works** and **Robust**: the class is const-correct (the shape tests above), behaves correctly, checks its bounds and caches the total.
- **Clean**: no `const_cast` anywhere in `grid.hpp`; only the cache and the call counter are `mutable` (real data never is); all the data members are `private`; and no `using namespace` in the header.
