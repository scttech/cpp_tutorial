# Specialization and `if constexpr`

A template does the same thing for every type. Sometimes one type needs different treatment: a `bool` should print as `true`, not `1`; a vector of
`T*` should say "pointer". C++ has three tools for that, and choosing the right one is most of the skill.

## 1. Specialization: a different definition for chosen types

For a **class template**, you can supply a different definition for particular arguments:

```cpp
template <class T>
struct TypeName { static std::string name() { return "unknown"; } };      // the PRIMARY template: the default

template <>                                                               // FULL specialization: one exact type
struct TypeName<int> { static std::string name() { return "int"; } };

template <class T>                                                        // PARTIAL specialization: a whole family
struct TypeName<T*> { static std::string name() { return "pointer to " + TypeName<T>::name(); } };

TypeName<int*>::name();         // "pointer to int":  T* matches with T = int, and the name of T is looked up recursively
```

`template <>` means "no parameters left: this is for exactly this type". A partial specialization keeps some parameters and matches a *pattern*
(`T*`, `std::vector<T>`, `std::pair<A, B>`). The compiler picks the **most specialized** match. This is how the standard library works too:
`std::vector<bool>` is a full specialization of `std::vector`, and `std::hash`, `std::numeric_limits` and `std::is_pointer` are specialization-based.

## 2. Overloading: for functions, instead of specialization

**A function template cannot be partially specialized.** For functions, write another *overload*; the compiler chooses the more specialized template by partial ordering:

```cpp
template <class T>                     std::string to_text(const T& value);                   // for anything
template <class A, class B>            std::string to_text(const std::pair<A, B>& pair);      // for pairs: more specialized, so it wins
```

(You *can* fully specialize a function template, but overloads are almost always the better tool: specializations do not take part in overload resolution, which gives surprises.)

## 3. `if constexpr`: one function, a different body per type

Inside one function, branch on the type at **compile time**:

```cpp
template <class T>
std::string to_text(const T& value) {
    if constexpr (std::is_same_v<T, bool>)         return value ? "true" : "false";
    else if constexpr (std::is_integral_v<T>)      return std::to_string(value);
    else if constexpr (std::is_pointer_v<T>)       return "&" + to_text(*value);       // *value is only compiled when T IS a pointer
    else                                           return "<unprintable>";
}
```

The difference from a plain `if` is the point: the branches that do not apply are **discarded**, not compiled for this `T`. With a plain `if`, `*value` would
be compiled for `int` too, and fail. It is also free: no runtime test, and no code for the other cases (less *instantiation bloat* than writing every type's code into every instantiation).

The conditions come from `<type_traits>` (`std::is_integral_v<T>`, `std::is_pointer_v<T>`, `std::is_convertible_v<A, B>`) or from a `requires` expression that asks "would this compile?":

```cpp
template <class T> constexpr bool is_range = requires(const T& t) { std::begin(t); std::end(t); };
```

**Order matters**: `bool` and `char` are integers, a `std::string` is a range of `char`, and `const char*` is a pointer. Put the special cases first.

## Two-phase lookup, again

`to_text(*value)` and `to_text(element)` inside the generic function are calls whose argument types depend on `T`, so the compiler looks for candidates in two places:
what is **declared above** the template, plus (at the call) **argument-dependent lookup**, which only searches the namespaces of the arguments. For a
`std::pair`, that is `std`, and your overload for pairs lives in the global namespace. So an overload that is *defined below* the generic function is invisible to it,
and a `std::map` (whose elements are pairs) prints `<unprintable>`. Fix: **declare** the overloads before the function that uses them.

## Your task

`pretty.hpp` has two parts:

1. **`TypeName<T>::name()`**: full specializations for `int`, `double`, `bool` and `std::string`, and partial ones for `T*` ("pointer to ..."), `const T` ("const ..."),
   `std::vector<T>` ("vector of ...") and `std::pair<A, B>` ("pair of A and B"). Anything else says `unknown`.
2. **`to_text(x)`**: the table in the comments of the file. One generic function with an `if constexpr` chain, plus overloads for `std::pair` and `std::optional`,
   which must be **declared** before the generic function.

The tests print every kind of value (including a `std::map<std::string, std::vector<int>>`, which needs all the pieces to work together) and, in the Fast tier,
check that printing a 20000-element vector does not build a new string for every element.
