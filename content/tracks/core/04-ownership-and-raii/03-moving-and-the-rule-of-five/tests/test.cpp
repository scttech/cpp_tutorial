#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "text.hpp"
#include "zth_test.hpp"

namespace {

std::string s(const Text& t) { return std::string(t.c_str()); }

// A function that takes its argument BY VALUE: given std::move(x), the parameter is move-constructed.
std::size_t consume(Text t) { return t.size(); }

Text greeting() {
    Text t("hello");
    t.append(", world");
    return t;
}

template <class T>
constexpr bool has_member_swap = requires(T& a, T& b) { a.swap(b); };
template <class T>
constexpr bool member_swap_is_noexcept = noexcept(std::declval<T&>().swap(std::declval<T&>()));

template <class T>
void swap_test() {
    if constexpr (!has_member_swap<T>) {
        ::zth::fail("Text needs a member function `void swap(Text&) noexcept`");
    } else {
        ZTH_EXPECT_TRUE(member_swap_is_noexcept<T>);
        T a("first"), b("second one");
        const char* pa = a.c_str();
        const char* pb = b.c_str();
        a.swap(b);
        ZTH_EXPECT_EQ(s(a), std::string("second one"));
        ZTH_EXPECT_EQ(s(b), std::string("first"));
        ZTH_EXPECT_EQ(a.size(), std::size_t{10});
        ZTH_EXPECT_EQ(b.size(), std::size_t{5});
        ZTH_EXPECT_TRUE(a.c_str() == pb && b.c_str() == pa);  // the buffers changed owners: nothing was copied
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "the move constructor takes the buffer: the new Text has the old one's characters AND its buffer") {
    Text a("hello world");
    const char* buffer = a.c_str();
    Text b(std::move(a));
    ZTH_EXPECT_EQ(s(b), std::string("hello world"));
    ZTH_EXPECT_EQ(b.size(), std::size_t{11});
    ZTH_EXPECT_TRUE(b.c_str() == buffer);  // stolen, not copied
}
ZTH_TEST("works", "after a move the source is empty (size 0, empty string), but still a valid Text") {
    Text a("hello");
    Text b(std::move(a));
    ZTH_EXPECT_EQ(a.size(), std::size_t{0});
    ZTH_EXPECT_TRUE(a.empty());
    ZTH_EXPECT_EQ(s(a), std::string(""));
    ZTH_EXPECT_EQ(s(b), std::string("hello"));
}
ZTH_TEST("works", "move assignment takes the buffer and empties the source") {
    Text a("hello world"), b("something else entirely");
    const char* buffer = a.c_str();
    b = std::move(a);
    ZTH_EXPECT_EQ(s(b), std::string("hello world"));
    ZTH_EXPECT_EQ(b.size(), std::size_t{11});
    ZTH_EXPECT_TRUE(b.c_str() == buffer);
    ZTH_EXPECT_TRUE(a.empty());
    ZTH_EXPECT_EQ(s(a), std::string(""));
}
ZTH_TEST("works", "a moved-from Text can be assigned to, appended to and destroyed") {
    Text a("old");
    Text b(std::move(a));
    a = Text("new");
    ZTH_EXPECT_EQ(s(a), std::string("new"));
    Text c(std::move(a));
    c.append("!");
    a.append("fresh");
    ZTH_EXPECT_EQ(s(a), std::string("fresh"));
    ZTH_EXPECT_EQ(s(c), std::string("new!"));
}
ZTH_TEST("works", "copies are still deep: a copy has its own buffer, a move does not need one") {
    Text a("abc");
    Text copy = a;
    ZTH_EXPECT_TRUE(copy.c_str() != a.c_str());
    ZTH_EXPECT_EQ(s(a), std::string("abc"));
    Text b = a;
    b.append("d");
    ZTH_EXPECT_EQ(s(a), std::string("abc"));
}
ZTH_TEST("works", "swap exchanges two Texts by exchanging their buffers") { swap_test<Text>(); }
ZTH_TEST("works", "moving, copying and assigning Texts leaks nothing and frees nothing twice") {
    zth::mem::Scope scope;
    {
        Text a("one"), b("two"), c;
        c = std::move(a);
        Text d(std::move(b));
        a = c;
        b = std::move(d);
        Text e = greeting();
        e = std::move(c);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "the move operations are noexcept (so containers dare to use them)") {
    ZTH_EXPECT_TRUE((std::is_nothrow_move_constructible_v<Text>));
    ZTH_EXPECT_TRUE((std::is_nothrow_move_assignable_v<Text>));
    ZTH_EXPECT_TRUE((std::is_nothrow_swappable_v<Text>));
}
ZTH_TEST("robust", "Text really is move-constructible and move-assignable (not just copyable)") {
    // an rvalue must pick the move overload: the pointer proves it
    Text a("proof");
    const char* p = a.c_str();
    Text b = std::move(a);
    ZTH_EXPECT_TRUE(b.c_str() == p);
    Text c;
    c = std::move(b);
    ZTH_EXPECT_TRUE(c.c_str() == p);
}
ZTH_TEST("robust", "moving from an empty Text, and into an empty Text, works") {
    Text empty;
    Text a(std::move(empty));
    ZTH_EXPECT_TRUE(a.empty());
    Text full("data");
    a = std::move(full);
    ZTH_EXPECT_EQ(s(a), std::string("data"));
    Text nothing;
    a = std::move(nothing);
    ZTH_EXPECT_TRUE(a.empty());
    ZTH_EXPECT_EQ(s(a), std::string(""));
}
ZTH_TEST("robust", "assigning a Text to itself by move leaves it intact") {
    Text a("keep me");
    Text& same = a;  // (a = std::move(a) is flagged by the compiler; this is the same thing through a reference)
    a = std::move(same);
    ZTH_EXPECT_EQ(s(a), std::string("keep me"));
    ZTH_EXPECT_EQ(a.size(), std::size_t{7});
    a.append("!");
    ZTH_EXPECT_EQ(s(a), std::string("keep me!"));
}
ZTH_TEST("robust", "move assignment releases the old buffer: many moves leak nothing") {
    zth::mem::Scope scope;
    {
        Text a("0123456789"), b("abc");
        for (int i = 0; i < 100; ++i) {
            a = std::move(b);
            b = Text("xyz");
        }
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a chain of moves passes the buffer along and never double-frees") {
    zth::mem::Scope scope;
    {
        Text a("hand me down");
        const char* p = a.c_str();
        Text b(std::move(a));
        Text c(std::move(b));
        Text d(std::move(c));
        ZTH_EXPECT_TRUE(d.c_str() == p);
        ZTH_EXPECT_TRUE(a.empty() && b.empty() && c.empty());
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a vector of Texts holds the right values after growing, erasing and inserting") {
    zth::mem::Scope scope;
    {
        std::vector<Text> v;
        for (int i = 0; i < 50; ++i) v.push_back(Text(("n" + std::to_string(i)).c_str()));
        v.erase(v.begin() + 10, v.begin() + 20);
        v.insert(v.begin() + 5, Text("inserted"));
        ZTH_EXPECT_EQ(v.size(), std::size_t{41});
        ZTH_EXPECT_EQ(s(v[4]), std::string("n4"));
        ZTH_EXPECT_EQ(s(v[5]), std::string("inserted"));
        ZTH_EXPECT_EQ(s(v[6]), std::string("n5"));
        ZTH_EXPECT_EQ(s(v[15]), std::string("n24"));
        ZTH_EXPECT_EQ(s(v[16]), std::string("n25"));
        ZTH_EXPECT_EQ(s(v.back()), std::string("n49"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "std::sort of Texts (it moves and swaps them around) gives the right order and leaks nothing") {
    zth::mem::Scope scope;
    {
        std::vector<Text> v;
        const char* words[] = {"pear", "apple", "fig", "banana", "cherry", "date", "elderberry", "grape"};
        for (int round = 0; round < 3; ++round)
            for (const char* w : words) v.push_back(Text(w));
        std::sort(v.begin(), v.end(), [](const Text& x, const Text& y) { return std::string(x.c_str()) < std::string(y.c_str()); });
        ZTH_EXPECT_EQ(s(v.front()), std::string("apple"));
        ZTH_EXPECT_EQ(s(v.back()), std::string("pear"));
        for (std::size_t i = 1; i < v.size(); ++i) ZTH_EXPECT_TRUE(s(v[i - 1]) <= s(v[i]));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a long Text moves intact") {
    std::string big(100000, 'z');
    big[0] = 'A';
    Text a(big.c_str());
    Text b(std::move(a));
    Text c;
    c = std::move(b);
    ZTH_EXPECT_EQ(c.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(c.c_str()[0], 'A');
    ZTH_EXPECT_EQ(c.c_str()[99999], 'z');
}

// ---- Fast: a move must not allocate -------------------------------------------------------------------
ZTH_TEST("fast", "moving into a function that takes a Text by value allocates nothing") {
    Text a("some text that is not tiny");
    zth::mem::Scope scope;
    const std::size_t n = consume(std::move(a));
    ZTH_EXPECT_EQ(n, std::size_t{26});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "move construction and move assignment allocate nothing") {
    Text a("one"), b("two"), c("three");
    zth::mem::Scope scope;
    Text d(std::move(a));
    b = std::move(c);
    d = std::move(b);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_EQ(s(d), std::string("three"));
}
ZTH_TEST("fast", "swap allocates nothing (std::swap too)") {
    Text a("first text"), b("second text");
    zth::mem::Scope scope;
    std::swap(a, b);
    swap(a, b);
    swap(b, a);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_EQ(s(a), std::string("second text"));
}
ZTH_TEST("fast", "returning a Text from a function allocates only the Text's own buffers") {
    zth::mem::Scope scope;
    Text g = greeting();
    ZTH_EXPECT_EQ(s(g), std::string("hello, world"));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);  // "hello" and then the grown buffer in append
}
ZTH_TEST("fast", "a vector moves its Texts when it grows: 100 push_backs allocate about 100 buffers, not 300") {
    std::vector<Text> v;
    zth::mem::Scope scope;
    for (int i = 0; i < 100; ++i) v.push_back(Text("an item of text"));
    ZTH_EXPECT_EQ(v.size(), std::size_t{100});
    // 100 buffers for the items + 8 reallocations of the vector's own array
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 115);
}
ZTH_TEST("fast", "sorting Texts allocates nothing: it only moves and swaps them") {
    std::vector<Text> v;
    for (int i = 0; i < 200; ++i) v.push_back(Text(("k" + std::to_string((i * 7919) % 200)).c_str()));
    zth::mem::Scope scope;
    std::sort(v.begin(), v.end(), [](const Text& x, const Text& y) { return std::string(x.c_str()) < std::string(y.c_str()); });
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
