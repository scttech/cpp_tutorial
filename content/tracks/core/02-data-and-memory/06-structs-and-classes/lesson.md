# Structs and basic classes

So far your data has been separate variables. A **struct** or **class** bundles values that belong together into one
type you can name, copy, pass around and give behavior to.

```cpp
struct Point {
    double x = 0;
    double y = 0;
    double length() const { return std::hypot(x, y); }   // a member function
};

Point p{3, 4};      // aggregate initialization: x = 3, y = 4
p.length();         // 5
```

## `struct` vs `class`

They are the same thing with one difference: members of a `struct` are **public** by default, members of a `class` are
**private** by default. By convention:

- use `struct` for plain bundles of data where every member can be touched freely (like `Point`);
- use `class` when the type has an **invariant** to protect, and keep the data `private`.

```cpp
class Interval {
public:                                   // the interface other code may use
    Interval(int a, int b);
    int lo() const;
private:                                  // the implementation: only member functions can touch these
    int lo_, hi_;
};
```

Making the data private is what lets a class *promise* something (for example, `lo_ <= hi_`): nobody outside can
break it. The trailing underscore is just a common naming convention for members.

## Member functions, `const` and `this`

- A member function is called on an object: `r.area()`. Inside it, the object's members are in scope, and **`this`** is
  a pointer to that object. `return *this;` hands the object itself back.
- A function that does not change the object should be marked **`const`** (written after the parameter list). Then
  it can be called on `const` objects, and the compiler stops you changing members by accident.
- Return `Rectangle&` (a reference) to let calls chain: `r.scale(2).scale(3)` works because `scale` returns the very
  same rectangle. Return `Rectangle` (a value) and the second call would work on a *copy*.

## Constructors and the initializer list

A constructor sets up a new object. Use the **member initializer list** (after the `:`) to initialize members
directly, instead of assigning in the body:

```cpp
Rectangle(Point top_left, double width, double height)
    : top_left_(top_left), width_(width), height_(height) {}
```

**Members are always initialized in the order they are *declared in the class*, not the order you write them in the
list.** If one member's initializer uses another, the other must be declared first. This is a classic bug:

```cpp
class R {
    double area_;       // declared first, so initialized first...
    double width_;
    R(double w) : width_(w), area_(width_ * 2) {}   // ...but width_ is still garbage when area_ reads it
};
```

The compiler warns about it (`-Wreorder`, and often `-Wuninitialized`). Read your warnings: this is one of the many
cases where they are right.

## Padding

The compiler places each member at an address that suits its type (an `int` on a multiple of 4, a `double` on a
multiple of 8) and leaves gaps. So `sizeof` a struct can be bigger than the sum of its members:

```cpp
struct A { char c; int i; char d; };   // c, 3 bytes padding, i, d, 3 bytes padding  -> 12
struct B { int i; char c; char d; };   // i, c, d, 2 bytes padding                   -> 8
```

Putting the largest members first usually wastes the least space. It rarely matters for a few objects, and matters a
lot for millions.

## Your task

Edit `geometry.hpp` (`main.cpp` is a read-only demo).

- **`Point`** (keep it an aggregate): `distance_to` and `translated`.
- **`Rectangle`**: a negative width or height becomes 0. Implement `perimeter`, `contains` (edges count as inside),
  `scale` (returns the same rectangle; ignores a negative factor; **keeps the cached `area_` correct**) and `translated`.
  The starter has an initialization-order bug: the compiler is telling you about it.
- **`Interval`**: normalize the endpoints in the constructor, then `length` (the difference can exceed `int`),
  `contains`, `overlaps` and `merged_with`.
- **`Reading`**: reorder its three members so that `sizeof(Reading)` is 8.

The tests also check that `Rectangle` and `Interval` keep their data private.
