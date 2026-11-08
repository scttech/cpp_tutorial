// MUTANT: pointers are removed too.
#pragma once

#include <cstddef>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

// A TYPE TRAIT is a class template used as a compile-time function from types to types (or to a constant): `std::remove_reference<T>::type`,
// `std::is_integral<T>::value`. It is written with (partial) specialization, and it is how generic code asks questions about T.

// ---- 1. Strip const and references: the "decay" of a parameter type ---------------------------------------------------
// plain_t<const int&> is int. Only TOP-LEVEL const goes: plain_t<const int*> is still a pointer to const int.
template <class T> struct plain { using type = T; };
template <class T> struct plain<T&> : plain<T> {};
template <class T> struct plain<T&&> : plain<T> {};
template <class T> struct plain<const T> : plain<T> {};
template <class T> struct plain<T*> : plain<T> {};
template <class T> using plain_t = typename plain<T>::type;

// ---- 2. Detection: does T look like a container? (a member type value_type, and begin() and end()) ------------------------
// The primary template says no; the partial specialization is only a candidate when ALL the expressions in void_t are valid
// (otherwise it is dropped by SFINAE, not an error) and then says yes.
template <class T, class = void> struct is_container : std::false_type {};
template <class T>
struct is_container<T, std::void_t<typename T::value_type, decltype(std::declval<T&>().begin()), decltype(std::declval<T&>().end())>>
    : std::true_type {};
template <class T> inline constexpr bool is_container_v = is_container<plain_t<T>>::value;

// ---- 3. The element type of a container or of a C array ---------------------------------------------------------------
// element_type<int> has NO ::type (so it can be used to ask "does this have elements?").
template <class T, class = void> struct element_type {};
template <class T> struct element_type<T, std::enable_if_t<is_container<T>::value>> { using type = typename T::value_type; };
template <class T, std::size_t N> struct element_type<T[N]> { using type = T; };
template <class T> using element_type_t = typename element_type<plain_t<T>>::type;

// ---- 4. decltype: the type of an expression, without evaluating it ---------------------------------------------------------
// The type you get by calling F with Args (as the given value categories). std::declval<T>() pretends to be a T value, for decltype only.
template <class F, class... Args>
using call_result_t = decltype(std::declval<F>()(std::declval<Args>()...));

// ---- 5. auto drops references and const; decltype(auto) keeps the exact type ----------------------------------------------
// first_value returns a COPY (auto: a plain value type); first_ref returns what `*c.begin()` really is (a reference into the container).
template <class C>
auto first_value(const C& c) {
    return *c.begin();
}

template <class C>
decltype(auto) first_ref(C& c) {
    return *c.begin();  // decltype(auto) of the expression `*c.begin()`: an lvalue, so a reference (const if c is const)
}

// A wrapper that is invisible to the caller: it returns EXACTLY what f returns (reference, value or void), forwards everything,
// and is noexcept when f is.
template <class F, class... Args>
decltype(auto) call_through(F&& f, Args&&... args) noexcept(std::is_nothrow_invocable_v<F, Args...>) {
    return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

// ---- 6. Traits + if constexpr: one function, different code for different types --------------------------------------------
// The number of items: size() for a container, N for an array, 1 for anything else.
template <class T>
std::size_t count_items(const T& x) {
    if constexpr (is_container_v<T>) {
        return x.size();
    } else if constexpr (std::is_array_v<T>) {
        return std::extent_v<T>;
    } else {
        (void)x;
        return 1;
    }
}

// Copies n elements. For a trivially copyable type the bytes can simply be copied (memcpy); any other type (one that counts its copies,
// owns memory...) must be copied element by element with its own copy assignment.
template <class T>
void copy_all(const T* from, T* to, std::size_t n) {
    if constexpr (std::is_trivially_copyable_v<T>) {
        if (n > 0) std::memcpy(to, from, n * sizeof(T));
    } else {
        for (std::size_t i = 0; i < n; ++i) to[i] = from[i];
    }
}
