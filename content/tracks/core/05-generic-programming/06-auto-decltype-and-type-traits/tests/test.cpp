#include <array>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "traits.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

// ---- test types ----
struct Opaque {
    int v;
};
struct OnlyValueType {  // has value_type but nothing to iterate
    using value_type = int;
};
struct OnlyBeginEnd {  // iterable, but no value_type
    int* begin();
    int* end();
};
struct NoEnd {
    using value_type = int;
    int* begin();
};
struct MyRange {  // a user-defined container (no size())
    using value_type = double;
    double* begin();
    double* end();
};

struct Pod {
    int a;
    double b;
};
bool operator==(const Pod& x, const Pod& y) { return x.a == y.a && x.b == y.b; }

int plain_function(double) { return 1; }
int& pick(int& a, int&) { return a; }
const int& pick_const(const int& a) { return a; }
int&& as_rvalue(int&& a) { return std::move(a); }
S make_text(int n) { return S(static_cast<std::size_t>(n), 'x'); }
int tick = 0;
void bump() { ++tick; }

struct Cat {  // a different result type for each value category of the argument
    char operator()(int&);
    short operator()(int&&);
    long operator()(const int&);
};

struct Probe {  // says which overload got the std::string
    int operator()(const S&) const { return 1; }
    int operator()(S&&) const { return 2; }
};

// ---- readable type mismatches ----
template <class T>
std::string type_name() {
    std::string s = __PRETTY_FUNCTION__;
    const auto b = s.find("T = ");
    const auto e = s.rfind(']');
    return b == std::string::npos ? s : s.substr(b + 4, e - b - 4);
}

template <class Expected, class Actual>
void expect_type(const char* what) {
    if (!std::is_same_v<Expected, Actual>) {
        ::zth::fail(std::string(what) + " has the wrong type: expected " + type_name<Expected>() + ", got " + type_name<Actual>());
    }
}

// ---- element_type_t, or void when there is none ----
template <class T>
consteval auto elem_probe() {
    if constexpr (requires { typename element_type_t<T>; }) {
        return std::type_identity<element_type_t<T>>{};
    } else {
        return std::type_identity<void>{};
    }
}
template <class T> using elem_t = typename decltype(elem_probe<T>())::type;

// ---- calls that report a missing signature instead of failing to compile ----
template <class F, class... Args> constexpr bool can_call = requires { call_through(std::declval<F>(), std::declval<Args>()...); };
template <class F, class... Args> constexpr bool nothrow_call = requires { requires noexcept(call_through(std::declval<F>(), std::declval<Args>()...)); };

// first_ref(c) = value, if first_ref returns something assignable
template <class C, class V>
void assign_first(C& c, const V& value) {
    if constexpr (requires { first_ref(c) = value; }) {
        first_ref(c) = value;
    } else {
        ::zth::fail("first_ref does not return a reference to the first element (it cannot be assigned to)");
    }
}

// does first_value(c) return a value_type equal to `expected`? (false for a stub that returns something else)
template <class C>
bool first_value_equals(const C& c, const typename C::value_type& expected) {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(first_value(c))>, typename C::value_type>) {
        return first_value(c) == expected;
    } else {
        return false;
    }
}

// call_through(f, args...) must equal `expected`
template <class Expected, class F, class... Args>
void expect_call_equals(const Expected& expected, F&& f, Args&&... args) {
    if constexpr (!can_call<F, Args...>) {
        ::zth::fail("call_through does not accept this function and these arguments");
    } else {
        ZTH_EXPECT_EQ(call_through(std::forward<F>(f), std::forward<Args>(args)...), expected);
    }
}

// call_through(f) for a void function
template <class F>
void call_void(F&& f) {
    if constexpr (!can_call<F>) {
        ::zth::fail("call_through(f) with no arguments does not compile");
    } else {
        call_through(std::forward<F>(f));
    }
}

// call_through(f, a, b) = value, if call_through returns an assignable reference
template <class F, class A, class B, class V>
void assign_through(F&& f, A& a, B& b, const V& value) {
    if constexpr (requires { call_through(f, a, b) = value; }) {
        call_through(f, a, b) = value;
    } else {
        ::zth::fail("call_through does not return the reference that the function returns (it cannot be assigned to)");
    }
}

// is call_through(f, a) the object a?
template <class F, class A>
bool returns_the_argument(F&& f, A& a) {
    if constexpr (can_call<F&, A&>) {
        auto&& r = call_through(f, a);
        return static_cast<const void*>(std::addressof(r)) == static_cast<const void*>(std::addressof(a));
    } else {
        return false;
    }
}

// is first_ref(c) the object at address p?
template <class C>
bool is_the_object(C& c, const void* p) {
    auto&& r = first_ref(c);
    return static_cast<const void*>(std::addressof(r)) == p;
}

template <class F, class... Args>
void expect_call_type(const char* what, F&& f, Args&&... args) {
    if constexpr (!can_call<F, Args...>) {
        ::zth::fail(std::string("call_through does not accept ") + what);
    } else {
        using Expected = std::invoke_result_t<F, Args...>;
        expect_type<Expected, decltype(call_through(std::forward<F>(f), std::forward<Args>(args)...))>(what);
    }
}

}  // namespace

// ---- Works: plain_t -------------------------------------------------------------------------------------
ZTH_TEST("works", "plain_t strips const") {
    expect_type<int, plain_t<const int>>("plain_t<const int>");
    expect_type<S, plain_t<const S>>("plain_t<const std::string>");
    expect_type<int, plain_t<int>>("plain_t<int>");
}
ZTH_TEST("works", "plain_t strips lvalue and rvalue references") {
    expect_type<int, plain_t<int&>>("plain_t<int&>");
    expect_type<int, plain_t<int&&>>("plain_t<int&&>");
    expect_type<S, plain_t<S&>>("plain_t<std::string&>");
}
ZTH_TEST("works", "plain_t strips const and a reference together") {
    expect_type<int, plain_t<const int&>>("plain_t<const int&>");
    expect_type<int, plain_t<const int&&>>("plain_t<const int&&>");
    expect_type<S, plain_t<const S&>>("plain_t<const std::string&>");
}
ZTH_TEST("works", "plain_t keeps what is not top-level: pointers, pointees, arrays") {
    expect_type<const int*, plain_t<const int*>>("plain_t<const int*>");
    expect_type<int*, plain_t<int* const>>("plain_t<int* const>");
    expect_type<int*, plain_t<int*&>>("plain_t<int*&>");
    expect_type<int[3], plain_t<int[3]>>("plain_t<int[3]>");
}

// ---- Works: is_container --------------------------------------------------------------------------------
ZTH_TEST("works", "is_container_v is true for the standard containers") {
    ZTH_EXPECT_TRUE(is_container_v<std::vector<int>>);
    ZTH_EXPECT_TRUE(is_container_v<std::list<S>>);
    ZTH_EXPECT_TRUE(is_container_v<std::deque<double>>);
    ZTH_EXPECT_TRUE(is_container_v<S>);
    ZTH_EXPECT_TRUE((is_container_v<std::map<int, int>>));
    ZTH_EXPECT_TRUE((is_container_v<std::array<int, 3>>));
}
ZTH_TEST("works", "is_container_v is false for everything else") {
    ZTH_EXPECT_FALSE(is_container_v<int>);
    ZTH_EXPECT_FALSE(is_container_v<double>);
    ZTH_EXPECT_FALSE(is_container_v<int*>);
    ZTH_EXPECT_FALSE(is_container_v<int[3]>);
    ZTH_EXPECT_FALSE(is_container_v<Opaque>);
    ZTH_EXPECT_FALSE((is_container_v<std::pair<int, int>>));
}
ZTH_TEST("works", "is_container_v needs ALL of value_type, begin() and end()") {
    ZTH_EXPECT_FALSE(is_container_v<OnlyValueType>);
    ZTH_EXPECT_FALSE(is_container_v<OnlyBeginEnd>);
    ZTH_EXPECT_FALSE(is_container_v<NoEnd>);
    ZTH_EXPECT_TRUE(is_container_v<MyRange>);
}
ZTH_TEST("works", "is_container_v sees through const and references") {
    ZTH_EXPECT_TRUE(is_container_v<const std::vector<int>>);
    ZTH_EXPECT_TRUE(is_container_v<const std::vector<int>&>);
    ZTH_EXPECT_TRUE(is_container_v<std::vector<int>&&>);
    ZTH_EXPECT_FALSE(is_container_v<const int&>);
}
ZTH_TEST("works", "is_container itself is a trait class: a std::true_type or std::false_type") {
    ZTH_EXPECT_TRUE((std::is_base_of_v<std::true_type, is_container<std::vector<int>>>));
    ZTH_EXPECT_TRUE((std::is_base_of_v<std::false_type, is_container<int>>));
}

// ---- Works: element_type --------------------------------------------------------------------------------
ZTH_TEST("works", "element_type_t of a container is its value_type") {
    expect_type<int, elem_t<std::vector<int>>>("element_type_t<std::vector<int>>");
    expect_type<S, elem_t<std::list<S>>>("element_type_t<std::list<std::string>>");
    expect_type<char, elem_t<S>>("element_type_t<std::string>");
    expect_type<char, elem_t<std::array<char, 3>>>("element_type_t<std::array<char, 3>>");
    expect_type<double, elem_t<MyRange>>("element_type_t<MyRange>");
}
ZTH_TEST("works", "element_type_t of a C array is the type of its elements") {
    expect_type<int, elem_t<int[4]>>("element_type_t<int[4]>");
    expect_type<double, elem_t<double[2]>>("element_type_t<double[2]>");
}
ZTH_TEST("works", "element_type_t sees through references") {
    expect_type<double, elem_t<const std::vector<double>&>>("element_type_t<const std::vector<double>&>");
    expect_type<int, elem_t<std::vector<int>&&>>("element_type_t<std::vector<int>&&>");
    expect_type<int, elem_t<int (&)[4]>>("element_type_t<int (&)[4]>");
}
ZTH_TEST("works", "element_type has NO ::type for types without elements (not a compile error to ask)") {
    expect_type<void, elem_t<int>>("element_type_t<int>");
    expect_type<void, elem_t<int*>>("element_type_t<int*>");
    expect_type<void, elem_t<Opaque>>("element_type_t<Opaque>");
    expect_type<void, elem_t<OnlyValueType>>("element_type_t<OnlyValueType>");
}

// ---- Works: call_result_t -------------------------------------------------------------------------------
ZTH_TEST("works", "call_result_t is the type of calling the callable") {
    expect_type<int, call_result_t<decltype(&plain_function), double>>("call_result_t<int(*)(double), double>");
    expect_type<S, call_result_t<decltype(&make_text), int>>("call_result_t<std::string(*)(int), int>");
    auto lambda = [](int x) { return x * 1.5; };
    expect_type<double, call_result_t<decltype(lambda), int>>("call_result_t<lambda, int>");
    expect_type<void, call_result_t<decltype(&bump)>>("call_result_t<void(*)()>");
}
ZTH_TEST("works", "call_result_t keeps a returned reference") {
    expect_type<int&, call_result_t<decltype(&pick), int&, int&>>("call_result_t<int&(*)(int&, int&), ...>");
    expect_type<const int&, call_result_t<decltype(&pick_const), int>>("call_result_t<const int&(*)(const int&), int>");
    expect_type<int&&, call_result_t<decltype(&as_rvalue), int>>("call_result_t<int&&(*)(int&&), int>");
}
ZTH_TEST("works", "call_result_t knows the value category of each argument") {
    expect_type<char, call_result_t<Cat, int&>>("call_result_t<Cat, int&>");
    expect_type<short, call_result_t<Cat, int>>("call_result_t<Cat, int>");
    expect_type<long, call_result_t<Cat, const int&>>("call_result_t<Cat, const int&>");
}

// ---- Works: auto vs decltype(auto) ----------------------------------------------------------------------
ZTH_TEST("works", "first_value returns a copy: a plain value type, not a reference") {
    std::vector<int> v{7, 8};
    expect_type<int, decltype(first_value(v))>("first_value(vector<int>)");
    ZTH_EXPECT_TRUE(first_value_equals(v, 7));
    std::list<S> words{"a", "b"};
    expect_type<S, decltype(first_value(words))>("first_value(list<string>)");
    ZTH_EXPECT_TRUE(first_value_equals(words, S("a")));
    const std::vector<int> cv{3};
    expect_type<int, decltype(first_value(cv))>("first_value(const vector<int>)");  // auto drops const too
}
ZTH_TEST("works", "first_ref returns a reference to the element itself") {
    std::vector<int> v{7, 8};
    expect_type<int&, decltype(first_ref(v))>("first_ref(vector<int>)");
    assign_first(v, 42);
    ZTH_EXPECT_EQ(v[0], 42);
    std::list<S> words{"a", "b"};
    expect_type<S&, decltype(first_ref(words))>("first_ref(list<string>)");
    assign_first(words, S("a!"));
    ZTH_EXPECT_EQ(words.front(), S("a!"));
}
ZTH_TEST("works", "first_ref of a const container is a const reference") {
    const std::vector<int> v{7, 8};
    expect_type<const int&, decltype(first_ref(v))>("first_ref(const vector<int>)");
    ZTH_EXPECT_TRUE(is_the_object(v, &v[0]));
}

// ---- Works: call_through --------------------------------------------------------------------------------
ZTH_TEST("works", "call_through returns what the function returns: a value") {
    expect_call_type("a function returning int", plain_function, 1.5);
    expect_call_type("a function returning std::string", make_text, 3);
    expect_call_equals(S("xxx"), make_text, 3);
}
ZTH_TEST("works", "call_through returns what the function returns: a reference stays a reference") {
    int a = 1, b = 2;
    expect_call_type("a function returning int&", pick, a, b);
    expect_call_type("a function returning const int&", pick_const, a);
    expect_call_type("a function returning int&&", as_rvalue, 5);
    assign_through(pick, a, b, 99);
    ZTH_EXPECT_EQ(a, 99);
    ZTH_EXPECT_EQ(b, 2);
}
ZTH_TEST("works", "call_through works for a void function, a lambda and a function object") {
    tick = 0;
    call_void(bump);
    call_void(bump);
    ZTH_EXPECT_EQ(tick, 2);
    auto twice = [](int x) { return 2 * x; };
    expect_call_equals(42, twice, 21);
}

// ---- Works: count_items ---------------------------------------------------------------------------------
ZTH_TEST("works", "count_items: size() for a container") {
    ZTH_EXPECT_EQ(count_items(std::vector<int>{1, 2, 3}), std::size_t{3});
    ZTH_EXPECT_EQ(count_items(std::list<S>{"a", "b"}), std::size_t{2});
    ZTH_EXPECT_EQ(count_items(S("hello")), std::size_t{5});
    ZTH_EXPECT_EQ(count_items(std::array<int, 5>{}), std::size_t{5});
    ZTH_EXPECT_EQ(count_items(std::vector<int>{}), std::size_t{0});
}
ZTH_TEST("works", "count_items: N for a C array, and a string literal is an array of its characters plus the terminator") {
    int numbers[4] = {1, 2, 3, 4};
    double ds[2] = {1.0, 2.0};
    ZTH_EXPECT_EQ(count_items(numbers), std::size_t{4});
    ZTH_EXPECT_EQ(count_items(ds), std::size_t{2});
    ZTH_EXPECT_EQ(count_items("abc"), std::size_t{4});
}
ZTH_TEST("works", "count_items: 1 for anything else") {
    ZTH_EXPECT_EQ(count_items(3.5), std::size_t{1});
    ZTH_EXPECT_EQ(count_items(42), std::size_t{1});
    ZTH_EXPECT_EQ(count_items(Opaque{1}), std::size_t{1});
    int x = 0;
    ZTH_EXPECT_EQ(count_items(&x), std::size_t{1});
    const char* p = "abc";  // a pointer, not an array
    ZTH_EXPECT_EQ(count_items(p), std::size_t{1});
}

// ---- Works: copy_all ------------------------------------------------------------------------------------
ZTH_TEST("works", "copy_all copies n ints, doubles and structs") {
    const int ia[4] = {1, 2, 3, 4};
    int ib[4] = {0, 0, 0, 0};
    copy_all(ia, ib, 4);
    for (int i = 0; i < 4; ++i) ZTH_EXPECT_EQ(ib[i], ia[i]);
    const double da[2] = {0.5, 1.5};
    double db[2] = {0, 0};
    copy_all(da, db, 2);
    ZTH_EXPECT_NEAR(db[0], 0.5, 1e-12);
    ZTH_EXPECT_NEAR(db[1], 1.5, 1e-12);
    const Pod pa[2] = {{1, 2.0}, {3, 4.0}};
    Pod pb[2] = {{0, 0}, {0, 0}};
    copy_all(pa, pb, 2);
    ZTH_EXPECT_TRUE(pb[0] == pa[0]);
    ZTH_EXPECT_TRUE(pb[1] == pa[1]);
}
ZTH_TEST("works", "copy_all copies a type that is NOT trivially copyable, element by element") {
    const zth::Tracked src[3] = {zth::Tracked(1), zth::Tracked(2), zth::Tracked(3)};
    zth::Tracked dst[3] = {zth::Tracked(0), zth::Tracked(0), zth::Tracked(0)};
    zth::Tracked::reset();
    copy_all(src, dst, 3);
    ZTH_EXPECT_EQ(dst[0].value, 1);
    ZTH_EXPECT_EQ(dst[1].value, 2);
    ZTH_EXPECT_EQ(dst[2].value, 3);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 3);  // the element's own copy assignment ran for each element
}

// ---- Robust ---------------------------------------------------------------------------------------------
ZTH_TEST("robust", "plain_t: const and references in any order and number") {
    expect_type<int, plain_t<const int&&>>("plain_t<const int&&>");
    expect_type<const int*, plain_t<const int* const&>>("plain_t<const int* const&>");
    expect_type<S, plain_t<const S&&>>("plain_t<const std::string&&>");
    expect_type<const int*, plain_t<const int* const>>("plain_t<const int* const>");
}
ZTH_TEST("robust", "is_container_v: a pointer to a container and an array of containers are not containers") {
    ZTH_EXPECT_FALSE(is_container_v<std::vector<int>*>);
    ZTH_EXPECT_FALSE(is_container_v<std::vector<int>[2]>);
    ZTH_EXPECT_FALSE(is_container_v<const char*>);
    ZTH_EXPECT_TRUE(is_container_v<const S&>);
}
ZTH_TEST("robust", "call_result_t with several arguments and with a function object with state") {
    int counter = 0;
    auto count = [&counter](int step) mutable { return counter += step; };
    expect_type<int, call_result_t<decltype(count), int>>("call_result_t<mutable lambda, int>");
    auto pair_up = [](int a, const S& b) { return std::pair<int, S>(a, b); };
    expect_type<std::pair<int, S>, call_result_t<decltype(pair_up), int, S>>("call_result_t<lambda, int, std::string>");
}
ZTH_TEST("robust", "call_through forwards each argument with its category: an rvalue std::string reaches the S&& overload") {
    Probe probe;
    S text = "t";
    expect_call_equals(2, probe, S("temp"));
    expect_call_equals(1, probe, text);
    expect_call_equals(2, probe, std::move(text));
}
ZTH_TEST("robust", "call_through is noexcept exactly when the call is") {
    auto quiet = [](int x) noexcept { return x; };
    auto loud = [](int x) { return x; };
    ZTH_EXPECT_TRUE((can_call<decltype(quiet)&, int>));
    ZTH_EXPECT_TRUE((nothrow_call<decltype(quiet)&, int>));
    ZTH_EXPECT_TRUE((can_call<decltype(loud)&, int>));
    ZTH_EXPECT_FALSE((nothrow_call<decltype(loud)&, int>));
}
ZTH_TEST("robust", "first_value and first_ref on a vector of strings") {
    std::vector<S> v{"first", "second"};
    ZTH_EXPECT_TRUE(first_value_equals(v, S("first")));
    assign_first(v, S("changed"));
    ZTH_EXPECT_EQ(v[0], S("changed"));
    ZTH_EXPECT_TRUE(first_value_equals(v, S("changed")));
}
ZTH_TEST("robust", "copy_all with n == 0 copies nothing (even from null pointers)") {
    int dst[2] = {7, 8};
    copy_all<int>(nullptr, dst, 0);
    ZTH_EXPECT_EQ(dst[0], 7);
    ZTH_EXPECT_EQ(dst[1], 8);
    zth::Tracked::reset();
    zth::Tracked t(5);
    copy_all<zth::Tracked>(nullptr, &t, 0);
    ZTH_EXPECT_EQ(t.value, 5);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 0);
}
ZTH_TEST("robust", "copy_all does not write past n elements") {
    const int src[5] = {1, 2, 3, 4, 5};
    int dst[5] = {0, 0, 0, 0, 99};
    copy_all(src, dst, 4);
    ZTH_EXPECT_EQ(dst[3], 4);
    ZTH_EXPECT_EQ(dst[4], 99);
    const zth::Tracked ts[3] = {zth::Tracked(1), zth::Tracked(2), zth::Tracked(3)};
    zth::Tracked td[3] = {zth::Tracked(0), zth::Tracked(0), zth::Tracked(77)};
    copy_all(ts, td, 2);
    ZTH_EXPECT_EQ(td[1].value, 2);
    ZTH_EXPECT_EQ(td[2].value, 77);
}

// ---- Fast -----------------------------------------------------------------------------------------------
ZTH_TEST("fast", "first_ref and count_items copy nothing") {
    std::vector<zth::Tracked> v;
    v.reserve(3);
    v.emplace_back(1);
    v.emplace_back(2);
    zth::Tracked::reset();
    ZTH_EXPECT_TRUE(is_the_object(v, &v[0]));
    ZTH_EXPECT_EQ(count_items(v), std::size_t{2});
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "first_value makes exactly one copy") {
    std::vector<zth::Tracked> v;
    v.reserve(2);
    v.emplace_back(4);
    zth::Tracked::reset();
    zth::Tracked first = first_value(v);
    ZTH_EXPECT_EQ(first.value, 4);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 1);
}
ZTH_TEST("fast", "call_through passes its arguments by reference: no copies, no moves") {
    zth::Tracked a(1);
    auto read = [](const zth::Tracked& t) { return t.value; };
    auto same = [](zth::Tracked& t) -> zth::Tracked& { return t; };
    zth::Tracked::reset();
    expect_call_equals(1, read, a);
    ZTH_EXPECT_TRUE(returns_the_argument(same, a));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "copy_all of a non-trivial type: exactly n copy assignments, no temporaries") {
    const zth::Tracked src[4] = {zth::Tracked(1), zth::Tracked(2), zth::Tracked(3), zth::Tracked(4)};
    zth::Tracked dst[4];
    zth::Tracked::reset();
    copy_all(src, dst, 4);
    ZTH_EXPECT_EQ(zth::Tracked::copy_assigns, 4);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
    ZTH_EXPECT_EQ(zth::Tracked::move_assigns, 0);
}
