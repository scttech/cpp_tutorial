# auto, decltype and type traits

Generic code keeps having to ask questions about a type it knows nothing about: *is it a container? what does it hold? what does calling this function give back? can I copy it with `memcpy`?* This lesson is the toolbox for asking, and for writing code whose types follow from other types.

## `auto` and `decltype(auto)`

`auto` deduces a type from an initializer **the way a template parameter passed by value would**: it drops references and top-level `const`.

```cpp
std::vector<int> v{1, 2, 3};
auto a = v[0];          // int      : a COPY
auto& b = v[0];         // int&     : the element itself
const auto& c = v[0];   // const int&
auto&& d = v[0];        // int&     : a forwarding reference: keeps whatever v[0] was
```

The same rule applies to a function's return type: `auto f() { return v[0]; }` returns a copy. When you need the exact type of the expression (a reference stays a reference), use **`decltype(auto)`**:

```cpp
template <class C> auto           first_value(const C& c) { return *c.begin(); }   // int  (a copy)
template <class C> decltype(auto) first_ref(C& c)         { return *c.begin(); }   // int& (the element; const int& for a const vector)
```

`decltype(e)` is the type of the expression `e`, without evaluating it. One trap: `decltype(x)` for a **bare name** is the type the name was declared with, but `decltype((x))` (extra parentheses) is an lvalue expression, so `T&`. So `decltype(auto) f() { int x = 0; return (x); }` returns a **dangling `int&`**.

`std::declval<T>()` is a pretend value of type `T`, usable only inside `decltype`, `noexcept` and `requires`, for asking about expressions on types you cannot construct:

```cpp
template <class F, class... Args>
using call_result_t = decltype(std::declval<F>()(std::declval<Args>()...));   // std::invoke_result_t, almost
```

A **forwarding wrapper** needs both: it must return *exactly* what the wrapped function returns, whatever that is.

```cpp
template <class F, class... Args>
decltype(auto) call_through(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}
```

## Type traits

A **type trait** is a class template that is a function from types to a type (`::type`, alias `_t`) or a value (`::value`, variable `_v`). It is written with specialization, as in the lesson on specialization: a primary template for the general case, and (partial) specializations for the special ones.

```cpp
template <class T> struct plain { using type = T; };
template <class T> struct plain<T&> : plain<T> {};      // strip a reference, then look again
template <class T> struct plain<const T> : plain<T> {}; // strip const, then look again
template <class T> using plain_t = typename plain<T>::type;
```

### The detection idiom

How do you ask "does `T` have a `value_type`, a `begin()` and an `end()`" without a compile error when it does not? Make the answer a specialization whose argument is **only valid when the answer is yes**:

```cpp
template <class T, class = void> struct is_container : std::false_type {};          // primary: no
template <class T>
struct is_container<T, std::void_t<typename T::value_type, decltype(std::declval<T&>().begin())>> : std::true_type {};  // yes, if the void_t is well-formed
```

`std::void_t<...>` is `void` for any valid arguments. If one of them is invalid, the specialization is not an error, it is **dropped** (SFINAE) and the primary template, `false`, remains. (C++20 concepts, the previous lesson, are the readable way to do this today. Traits are what the standard library and most existing code use, and what you read in error messages.)

### Traits and `if constexpr`

```cpp
template <class T>
std::size_t count_items(const T& x) {
    if constexpr (is_container_v<T>)  return x.size();
    else if constexpr (std::is_array_v<T>) return std::extent_v<T>;   // note: "abc" is a char[4]
    else return 1;
}
```

In a template the branches that are not taken are not instantiated: the `x.size()` need not compile for an `int`.

## Your task

Fill `traits.hpp` (each part is a stub that compiles but answers wrongly):

1. `plain_t<T>`: strip top-level `const` and references (`const int&` -> `int`, but `const int*` unchanged).
2. `is_container<T>` and `is_container_v<T>` (value_type, begin(), end(); looks through `const` and references).
3. `element_type<T>` / `element_type_t<T>`: a container's `value_type`, an array's element; **no `::type` at all** for anything else.
4. `call_result_t<F, Args...>`.
5. `first_value` (a copy), `first_ref` (a reference), `call_through` (exactly what `f` returns, `noexcept` when `f` is).
6. `count_items` and `copy_all` (`memcpy` only for trivially copyable types).

The tiers: **Works** (the traits and functions), **Robust** (the edges: references, empty ranges, noexcept, value categories), **Fast** (no copies that need not be made).
