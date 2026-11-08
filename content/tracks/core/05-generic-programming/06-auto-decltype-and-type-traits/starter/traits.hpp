#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

// A TYPE TRAIT is a class template used as a compile-time function from types to types (or to a constant): `std::remove_reference<T>::type`,
// `std::is_integral<T>::value`. It is written with (partial) specialization, and it is how generic code asks questions about T.
// Every definition below is a stub that compiles but answers wrongly: replace each one.

// ---- 1. Strip const and references: the "decay" of a parameter type ---------------------------------------------------
// TODO: plain_t<const int&> is int, plain_t<int&&> is int, plain_t<const int> is int. Only TOP-LEVEL const goes: plain_t<const int*> is still `const int*`.
// Specialize `plain<T&>`, `plain<T&&>` and `plain<const T>`: each one inherits from `plain<T>` (so the stripping repeats until nothing is left).
template <class T> struct plain { using type = T; };
template <class T> using plain_t = typename plain<T>::type;

// ---- 2. Detection: does T look like a container? (a member type value_type, and begin() and end()) ------------------------
// The primary template says no; a partial specialization `is_container<T, std::void_t<...>>` is only a candidate when ALL the expressions
// in void_t are valid (otherwise SFINAE just drops it) and then says yes.
// TODO: add the specialization:  typename T::value_type,  decltype(std::declval<T&>().begin()),  decltype(std::declval<T&>().end())
template <class T, class = void> struct is_container : std::false_type {};
// TODO: is_container_v<const std::vector<int>&> must be true: look at plain_t<T>.
template <class T> inline constexpr bool is_container_v = false;

// ---- 3. The element type of a container or of a C array ---------------------------------------------------------------
// TODO: element_type<std::vector<int>>::type is int, element_type<double[4]>::type is double, and element_type<int> has NO ::type at all.
// Use `template <class T, class = void> struct element_type {};` and two partial specializations (one chosen with std::enable_if_t<is_container<T>::value>,
// one for `T[N]`). element_type_t<T> should also accept references to those types (plain_t).
template <class T> struct element_type {};
template <class T> using element_type_t = typename element_type<T>::type;

// ---- 4. decltype: the type of an expression, without evaluating it ---------------------------------------------------------
// TODO: call_result_t<F, Args...> is the type of calling an F with Args: decltype(std::declval<F>()(std::declval<Args>()...)).
// std::declval<T>() pretends to be a value of type T, for use inside decltype only.
template <class F, class... Args> using call_result_t = void;

// ---- 5. auto drops references and const; decltype(auto) keeps the exact type ----------------------------------------------
// TODO: first_value returns a COPY of the first element (the return type `auto`); first_ref returns a REFERENCE to it (`decltype(auto)`),
// a const reference when the container is const. Both: `*c.begin()`.
template <class C>
int first_value(const C&) {
    return 0;
}

template <class C>
int first_ref(C&) {
    return 0;
}

// TODO: call_through(f, args...) calls f with the arguments (std::invoke) and returns EXACTLY what f returns: a reference stays a reference,
// a void function works too. Return type `decltype(auto)`, forward everything, and make it noexcept(std::is_nothrow_invocable_v<F, Args...>).
template <class F>
int call_through(F&&) {
    return 0;
}

// ---- 6. Traits + if constexpr: one function, different code for different types --------------------------------------------
// TODO: count_items(x): x.size() for a container, N for a C array (std::extent_v<T>), 1 for anything else.
template <class T>
std::size_t count_items(const T&) {
    return 0;
}

// TODO: copy_all(from, to, n) copies n elements. If std::is_trivially_copyable_v<T>, std::memcpy (when n > 0); otherwise a loop with the
// element's own copy assignment (`to[i] = from[i]`).
template <class T>
void copy_all(const T*, T*, std::size_t) {}
