# Namespaces and modular design

A real program is many pieces of code written by different people. If two of them both define a `Point`, or an `area`,
or a `Rect`, the program does not compile (or worse, links the wrong one). **Namespaces** give every library its own
space for names:

```cpp
namespace geo {
struct Point { int x = 0, y = 0; };
long long area(const Rect& r);
}

geo::Point p;          // qualified: "the Point in geo"
std::vector<int> v;    // std is a namespace too
```

Namespaces nest (`geo::detail::end_of`) and can be reopened in any file. A **namespace alias** gives a long name a short
local one: `namespace g = geo;`.

## What goes where

A module (a header, here) has a **public interface** (the types and functions its users are meant to call) and
**implementation details** (helpers nobody else should depend on). Two habits keep that line clear:

- Everything the header declares lives **inside the library's namespace**. Nothing at file scope: a global `intervals_overlap`
  would collide with the next library that has one.
- Helpers go in a nested **`detail`** namespace (`geo::detail`). It is a convention that says "not for you": the
  names are reachable, but anyone who uses them knows they are on their own. Do **not** use an *unnamed* namespace
  (`namespace { ... }`) in a header: that is for `.cpp` files. In a header, every file that includes it gets its own
  private copy of everything inside.

## Never `using namespace std;` in a header

`using namespace std;` makes every name in `std` visible without the prefix. In a `.cpp` file of your own that is a
matter of taste. In a **header** it is a trap: it is silently applied to *every file that ever includes it*, including code
that has its own `count`, `distance`, `max` or `list`. Those programs fail to compile (or pick the wrong function)
and the cause is in a file they never read. In headers, write `std::`.

## Argument-dependent lookup (ADL)

How does `std::cout << point` find *your* `operator<<`? You did not write `geo::operator<<`. The rule: when you call a
function (or use an operator) without qualifying it, the compiler looks in the current scope **and in the namespaces of the
arguments' types**. `point` is a `geo::Point`, so `geo` is searched, and `geo::operator<<` is found.

That is why **an operator belongs in the same namespace as the type it works on**. Put `operator<<(ostream&, const geo::Point&)`
in the global namespace instead and it works at first (the code you test with can see the global scope), then breaks
the moment a template in *another* namespace uses it, such as `std::ostream_iterator`, a generic `show(const T&)`, or the
unit-test framework's value printing: the compiler searches `std` and `geo`, never the global namespace.

```cpp
namespace geo {
struct Point {
    int x = 0, y = 0;
    friend bool operator==(const Point&, const Point&) = default;   // a "hidden friend": only found via ADL, and always right
};
inline std::ostream& operator<<(std::ostream& os, const Point& p) { return os << '(' << p.x << ", " << p.y << ')'; }
}
```

The same goes for `swap` and for any function that is part of a type's interface (`to_string(Status)` last lesson!).

## One more rule for headers: `inline`

A function **defined** in a header must be `inline` (or a template, or in a class body). Otherwise every `.cpp` file
that includes the header gets its own definition, and the linker complains about duplicates. `inline` tells the linker
"identical copies are fine".

## Your task

`geometry.hpp` already contains a working library, but it is a bad citizen. Make it a good one, without changing what
it computes (`main.cpp` is a read-only demo):

1. Delete `using namespace std;` and write `std::` where you need it.
2. Move `intervals_overlap` into **`geo::detail`**.
3. Move the four operators **into namespace `geo`** (`==` for `Point` and `Rect` as defaulted hidden friends inside the structs).
4. Nothing may remain at file scope (apart from `#include` and `#pragma once`), and there must be no unnamed namespace.

The tests check the behavior of the library (including the half-open rectangle rules and huge coordinates), then check the
**shape**: `operator<<` and `operator==` must be found by ADL from generic code, and the test reads your header to check its
hygiene (the **Clean** tier).

A rectangle covers `origin.x <= x < origin.x + width` and `origin.y <= y < origin.y + height`: the right and bottom edges are
*outside*, so two rectangles that only touch along an edge do not overlap, and an empty rectangle overlaps nothing.
