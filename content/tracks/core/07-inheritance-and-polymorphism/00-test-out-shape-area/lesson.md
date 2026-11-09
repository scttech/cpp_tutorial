# Test-out: Shape area

> **Already comfortable with inheritance and virtual functions?** Pass this challenge (the **Works** and **Robust** tiers) and Module 7 is marked complete. You can still open any of its lessons later.

## The task

Write a small **polymorphic shape hierarchy** in `shapes.hpp`, and the functions that work on a collection of shapes held through `std::unique_ptr<Shape>`.

```cpp
Shapes shapes;                                    // std::vector<std::unique_ptr<Shape>>
shapes.push_back(std::make_unique<Circle>(1.5));
shapes.push_back(std::make_unique<Square>(2));
shapes.push_back(make_shape("triangle", {3, 4, 5}));
double total = total_area(shapes);                // each area() call goes to the REAL class
Shapes copy = clone_all(shapes);                  // deep copies: a Square stays a Square
```

`main.cpp` (read-only) is a small demo: press **Run**.

### The classes

`Shape` is given: an interface with exactly **four** pure virtual functions (`area`, `perimeter`, `name`, `clone`). The hidden tests derive a shape of their own from it, so do not add more pure virtual functions. Look at its destructor, and think about what `std::unique_ptr<Shape>` does when it is destroyed.

| Class | Constructor | `area` | `perimeter` | `name` |
|---|---|---|---|---|
| `Circle` (final) | `(radius)` | π r² | 2 π r | `"circle"` |
| `Rectangle` | `(width, height)` | w × h | 2 (w + h) | `"rectangle"` |
| `Square` (final, **derives from `Rectangle`**) | `(side)` | (inherited) | (inherited) | `"square"` |
| `Triangle` (final) | `(a, b, c)` | Heron: `s = (a+b+c)/2`, `sqrt(s (s-a) (s-b) (s-c))` | a + b + c | `"triangle"` |

- A **negative** (or NaN) radius or side throws `std::invalid_argument`; zero is allowed for circles and rectangles.
- A triangle needs three **positive** sides that satisfy the **strict** triangle inequality (each side shorter than the other two together): a "triangle" with sides 1, 2, 3 is a line, and is rejected.
- `clone()` returns a copy of the **real** object. `Square::clone()` must produce a `Square`, not a `Rectangle`.

### The functions

| Function | Behavior |
|---|---|
| `make_shape(kind, args)` | `"circle"` (1 number), `"rectangle"` (2), `"square"` (1), `"triangle"` (3). An unknown kind, or the wrong number of arguments, throws `std::invalid_argument`. |
| `total_area(shapes)` | the sum of the areas; a **null pointer** in the vector is skipped |
| `largest(shapes)` | a pointer to the shape with the largest area, the **first** of equal ones; `nullptr` if there is none (empty, or only nulls) |
| `clone_all(shapes)` | a deep copy; a null entry stays null |

### The tiers

- **Works** and **Robust** are the behavior above, plus the part that is easy to get wrong: destroying a shape through a `Shape` pointer must run the *derived* destructor (the tests count live objects of a shape class they define themselves).
- **Fast**: `total_area` and `largest` allocate nothing, and `clone_all` allocates one block per shape plus one for the vector (`reserve` before the loop).
