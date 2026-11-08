# Concepts (C++20)

A template accepts *anything* that makes it compile. That is flexible, and it has a cost: when a type does not fit, you find out from an error **inside** the template,
three layers of instantiation deep (you will see exactly such an error in the last lesson of this module). A **concept** is a *named requirement* on a template argument:
a compile-time yes/no question about a type. With one, the mistake is reported where it was made, in one line:

```
error: no matching function for call to 'mean'
note: candidate template ignored: constraints not satisfied [with S = std::vector<std::string>]
note: because 'std::string' does not satisfy 'Number'
```

## Using concepts

The standard library has many (`std::integral`, `std::floating_point`, `std::same_as`, `std::convertible_to`, `std::invocable`, `std::sortable`, ...). There are four places to write a constraint:

```cpp
template <std::integral T>              T f(T x);                       // 1. instead of `class`: T must be integral
template <class T> requires Number<T>   T f(T x);                       // 2. a requires CLAUSE: any boolean combination of concepts
template <class T> T f(T x) requires Number<T>;                         // 3. a TRAILING requires clause
auto f(Number auto x);                                                  // 4. a constrained auto: the shortest form (each `auto` is a template parameter)
```

## Writing your own concept

A concept is built from other concepts and from **requires-expressions**: code that is *not run*, only asked "would this compile?":

```cpp
template <class T>
concept Integer = std::integral<T> && !std::same_as<T, bool>;       // combine with && || !

template <class T>
concept Printable = requires(std::ostream& os, const T& value) {    // "if I have an os and a value ..."
    os << value;                                                    // "... does this compile?"
};

template <class C>
concept Sequence = requires(const C& c) {
    typename C::value_type;                                         // this type must exist
    c.begin();                                                      // this must compile
    { c.size() } -> std::convertible_to<std::size_t>;               // this must compile AND its type must convert
};
```

A concept checks **the declaration, not the body**. A template constrained with `Printable` is rejected for a type that cannot print, but an *unconstrained* one is accepted
and fails inside. And the reverse: if you forget to constrain `join`, `join(std::vector<Opaque>{...})` is accepted by overload resolution as far as the signature goes,
and only the body would fail.

## Subsumption: the most constrained overload wins

Several overloads may be viable. If one is *more constrained* than the others, it wins:

```cpp
template <class T>   std::string describe(const T&);     // "something"
template <Number T>  std::string describe(const T&);     // "a number"
template <Integer T> std::string describe(const T&);     // "an integer"   <- wins for int
```

"More constrained" is decided by the compiler comparing the **atomic constraints**, not their meaning. `Integer` subsumes `Number` only because `Number` is *defined in terms of* `Integer`:

```cpp
concept Number = Integer<T> || std::floating_point<T>;                    // OK: the compiler sees Integer inside Number
concept Number = (std::integral<T> && !std::same_as<T, bool>) || std::floating_point<T>;   // same meaning, but the compiler cannot tell: describe(42) is AMBIGUOUS
```

So: *reuse a concept by name* rather than copying its definition.

## Your task

`constraints.hpp` has five templates and four concepts that say yes to everything. Write the concepts (`Integer`, `Number`, `Printable`, `Sequence`) and constrain the templates:

| | |
|---|---|
| `mean(values)` | A `Sequence` of `Number`s (`template <Sequence S> requires Number<typename S::value_type>`). The mean as a `double`; `0.0` for an empty sequence. |
| `larger(a, b)` | Two `Number`s as `Number auto` parameters; the larger, with their common type. |
| `describe(x)` | Three overloads by constraint: "something", "a number", "an integer". |
| `Accumulator<T>` | A class template for `Number`s only: `add`, `total`, `count`, `mean` (0.0 when empty). |
| `join(values, separator)` | A `Sequence` whose elements are `Printable`; no separator at the end. |

The tests check the concepts on many types, and that the constrained templates **reject** the wrong types (using a `requires` expression: a rejection is a compile error you cannot run).
