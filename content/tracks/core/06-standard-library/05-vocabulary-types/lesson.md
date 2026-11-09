# Vocabulary types: optional, variant, tuple and span

Some types appear in so many interfaces that the standard library gives them names everybody understands. They make function signatures say what they mean: "might not have a value", "is one of these kinds", "returns several things", "a view of a sequence".

## `std::optional<T>`: a value or nothing

```cpp
std::optional<double> evaluate(const Expr& e, const Vars& vars);   // no value: a missing variable, a division by zero
if (auto v = evaluate(e, vars)) use(*v);                           // true when there is a value; * and -> access it
double x = r.value_or(0.0);                                        // a default
```

Better than a sentinel (`-1`, NaN), a null pointer or an out-parameter. `*opt` on an empty optional is undefined behavior: test it first. `optional<T&>` is not allowed (use a pointer, or `std::reference_wrapper`).

## `std::variant<A, B, C>` and `std::visit`: one of several kinds

A **sum type**: a value that is exactly one of its alternatives, with no inheritance and no heap. The expression tree here is the classic use:

```cpp
struct Expr { std::variant<Num, Var, BinOp, Neg> node; };

std::visit(Overloaded{
    [](const Num& n)   { return n.value; },
    [&](const Var& v)  { ... },
    [&](const BinOp& b) { ... },
    [&](const Neg& n)  { ... },
}, e.node);
```

`std::visit` calls the lambda that matches the alternative currently held, and **the code does not compile if you leave a kind out**: when you add a `Mul` node later, the compiler lists every function you must update. `Overloaded` is the small helper (in `expr_types.hpp`) that glues lambdas into one visitor.

Other access: `std::get_if<Num>(&node)` (a pointer, or `nullptr` if it holds something else), `std::holds_alternative<Num>(node)`, `std::get<Num>(node)` (throws `std::bad_variant_access` on a mismatch), `node.index()`. A variant is *valueless by exception* only if a move/assignment throws halfway: rare, and `visit` on it throws.

A tree is **recursive** and a variant cannot contain itself, so `BinOp` holds `std::unique_ptr<Expr>` children. That makes the tree **move-only**: to copy one you write a deep `clone`, which is a `visit` too.

## `std::pair`, `std::tuple` and structured bindings

```cpp
std::optional<std::pair<double, double>> min_max(std::span<const double> v);
if (auto mm = min_max(data)) { auto [lo, hi] = *mm; ... }          // structured bindings give the parts names
```

A function that returns several values returns a `pair`/`tuple` (or, better when the parts have meaning, a small `struct`), and the caller unpacks with `auto [a, b, c] = f();`.

## `std::span<T>`: a view of a contiguous sequence

A `span` is a pointer and a length. A `vector`, a `std::array` and a C array all convert to `std::span<const double>` **without a copy**, so one function accepts all of them:

```cpp
std::optional<std::pair<int, std::span<const int>>> head_tail(std::span<const int> v);   // the tail is a view, not a new vector
```

A span does not own its elements: it dangles if the container is destroyed or reallocates (the same rule as for iterators and `string_view`).

## Your task

Fill in `expr.hpp`: `evaluate`, `to_string`, `node_count`, `variables`, `clone` and `simplify` over the tree in `expr_types.hpp` (read-only), then `min_max` and `head_tail`. The tiers: **Works**, **Robust** (no value propagating up through the tree, a deep copy, folding that must not change the original, empty and one-element spans), **Fast** (`evaluate` and `node_count` allocate nothing, `min_max` and `head_tail` do not copy a 100000-element vector).
