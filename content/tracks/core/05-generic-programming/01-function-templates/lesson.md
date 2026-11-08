# Function templates

`int clamp_value(int value, int low, int high)` works for `int`. To use it for `double` you copy it. Then for `std::string`.
Then it is three functions that must stay in sync. A **template** lets you write the function once, with a placeholder for the type:

```cpp
template <class T>
const T& clamp_value(const T& value, const T& low, const T& high) {
    return value < low ? low : (high < value ? high : value);
}

clamp_value(15, 0, 10);                          // T is int
clamp_value(0.5, 0.0, 1.0);                      // T is double
clamp_value(std::string("m"), "a", "f");         // see below: this one does not compile!
```

A template is not a function. It is a **recipe**: each time you call it with new types, the compiler writes a real function for those types
(**instantiation**). Nothing is generated for types nobody uses, and the generated code is as fast as if you had written it by hand.
This also explains two rules you will see everywhere:

- **Templates go in headers.** To instantiate, the compiler needs the whole recipe at the point of use, in every file that uses it.
  (A function that is *not* a template and is defined in a header must be `inline`; templates are exempt from the one-definition rule's usual trouble.)
- **Errors come from the instantiation.** The compiler cannot fully check a template until it knows `T`. `value < low` is only an error for a
  `T` that has no `<`, and you find out when you use that `T`.

(The function is called `clamp_value` because `std::clamp` already exists. A call such as `clamp(std::string(...), ...)` would be ambiguous between yours and the standard one: argument-dependent lookup, which you will meet again below, finds the standard version because the arguments are `std` types.)

## Template argument deduction

The compiler works out `T` **from the arguments**, and all uses of one parameter must agree:

```cpp
clamp_value(5, 0, 3);        // T = int, int, int: fine
clamp_value(5, 0, 3.5);      // error: T is int from the first two arguments but double from the third
```

The error reads *"no matching function ... candidate template ignored: deduced conflicting types for parameter 'T' ('int' vs. 'double')"*.
Two fixes: make the arguments the same type, or **name** the type yourself, which turns off deduction and converts the arguments:
`clamp_value<double>(5, 0, 3.5)`.

When the arguments are *allowed* to differ, give each its own parameter: `template <class A, class B> ... min_of(const A& a, const B& b)`.
The result type is then `std::common_type_t<A, B>`: the type that `?:` would produce for them (`int` and `double` give `double`).
Without it you would have to choose one of the two types, and `min_of(3, 2.5)` would return `2`.

A parameter of type `std::initializer_list<T>` deduces `T` from a braced list: `min_of({4, 2, 8})`.

`const T&` parameters accept lvalues and temporaries and copy nothing. Remember that a function returning `const T&` (like `clamp_value`) returns a
reference to *one of its arguments*: if you pass temporaries, the result is dead at the end of the statement.

Only ask of `T` what you need. `clamp_value` and `min_of` use `<` and nothing else: no `>`, no `==`. That is why they also work for a type that
defines only `operator<`, which is all the standard library asks of keys.

## Two-phase lookup and `swap`

In a template, a call such as `swap(a, b)` depends on the type of `a`. The compiler looks up *independent* names when it reads the template, and
*dependent* ones again at instantiation, where **argument-dependent lookup (ADL)** adds the namespaces of the argument types (you met ADL in
Module 3: it is how `std::cout << point` finds your `operator<<`). This gives the standard idiom for "call the type's own `swap` if it has one":

```cpp
template <class T>
void swap_values(T& a, T& b) {
    using std::swap;     // std::swap is a candidate: the fallback, which moves
    swap(a, b);          // unqualified: ADL also finds a swap that the type's own namespace provides
}
```

Writing `std::swap(a, b)` would skip the type's own `swap`, which is often much faster (a container swaps two pointers instead of moving each element).

## Your task

`generic.hpp` has three functions that work for `int` only. Make them templates:

| Function | Behavior |
|---|---|
| `clamp_value(value, low, high)` | One type parameter. `low` if `value < low`, `high` if `high < value`, otherwise `value`. Returns `const T&`: a reference to one of the arguments. Only `<`. |
| `min_of(a, b)` | Two type parameters, result `std::common_type_t<A, B>`; the first wins when equal. |
| `min_of(std::initializer_list<T>)` | The smallest element of a non-empty braced list. |
| `swap_values(a, b)` | Exchanges two objects by moving, using `using std::swap; swap(a, b);`. |

The tests call each function with many types (ints, doubles, strings, move-only `std::unique_ptr`, a type with only `operator<`, a type with its own
`swap`) and, in the **Fast** tier, count copies and moves.
