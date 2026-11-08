#define ZTH_TRACK_ALLOCATIONS
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "text.hpp"
#include "zth_test.hpp"

namespace {

template <class Ex, class F>
bool throws(F&& f) {
    try {
        f();
    } catch (const Ex&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

std::string s(const Text& t) { return std::string(t.c_str()); }

// `a = b` must give back `a` itself, by reference. Written as a template so a wrong shape is a failed test, not a compile error.
template <class T>
constexpr bool assignment_returns_self = requires(T& a, const T& b) { { a = b } -> std::same_as<T&>; };

template <class T>
void chained_assignment_test() {
    if constexpr (!assignment_returns_self<T>) {
        ::zth::fail("operator= must return `Text&` (a reference to *this), so that `a = b = c` works like it does for int");
    } else {
        T a("a"), b("b"), c("ccc");
        a = b = c;
        ZTH_EXPECT_EQ(std::string(a.c_str()), std::string("ccc"));
        ZTH_EXPECT_EQ(std::string(b.c_str()), std::string("ccc"));
        T x("x"), y("yy");
        T& r = (x = y);
        ZTH_EXPECT_TRUE(&r == &x);
        r.append("!");
        ZTH_EXPECT_EQ(std::string(x.c_str()), std::string("yy!"));
        ZTH_EXPECT_EQ(std::string(y.c_str()), std::string("yy"));
    }
}

Text pass_through(Text t) {
    t.append("!");
    return t;
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a default Text is empty, and a Text made from a C string holds a copy of it") {
    Text empty;
    ZTH_EXPECT_EQ(empty.size(), std::size_t{0});
    ZTH_EXPECT_EQ(s(empty), std::string(""));
    Text t("hello");
    ZTH_EXPECT_EQ(t.size(), std::size_t{5});
    ZTH_EXPECT_EQ(s(t), std::string("hello"));
    Text from_null(static_cast<const char*>(nullptr));
    ZTH_EXPECT_EQ(from_null.size(), std::size_t{0});
    ZTH_EXPECT_EQ(s(from_null), std::string(""));
}
ZTH_TEST("works", "the copy constructor makes an equal Text with a buffer of its own") {
    Text a("hello");
    Text b = a;
    ZTH_EXPECT_EQ(s(b), std::string("hello"));
    ZTH_EXPECT_EQ(b.size(), std::size_t{5});
    ZTH_EXPECT_TRUE(a.c_str() != b.c_str());  // not the same buffer
}
ZTH_TEST("works", "changing a copy does not change the original (and the other way round)") {
    Text a("hello");
    Text b(a);
    b.at(0) = 'J';
    ZTH_EXPECT_EQ(s(a), std::string("hello"));
    ZTH_EXPECT_EQ(s(b), std::string("Jello"));
    a.at(4) = 'a';
    ZTH_EXPECT_EQ(s(b), std::string("Jello"));
    ZTH_EXPECT_EQ(s(a), std::string("hella"));
}
ZTH_TEST("works", "copy assignment copies the characters into the existing Text, with a buffer of its own") {
    Text a("hello"), b("x");
    b = a;
    ZTH_EXPECT_EQ(s(b), std::string("hello"));
    ZTH_EXPECT_EQ(b.size(), std::size_t{5});
    ZTH_EXPECT_TRUE(a.c_str() != b.c_str());
    b.at(0) = 'J';
    ZTH_EXPECT_EQ(s(a), std::string("hello"));
}
ZTH_TEST("works", "append adds characters at the end") {
    Text t("ab");
    t.append("cd");
    ZTH_EXPECT_EQ(s(t), std::string("abcd"));
    ZTH_EXPECT_EQ(t.size(), std::size_t{4});
    Text e;
    e.append("xyz");
    ZTH_EXPECT_EQ(s(e), std::string("xyz"));
}
ZTH_TEST("works", "copying, assigning and destroying Texts leaks nothing and frees nothing twice") {
    zth::mem::Scope scope;
    {
        Text a("one");
        Text b = a;
        Text c;
        c = b;
        c = a;
        Text d(c);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "assigning a Text to itself leaves it intact") {
    Text a("hello");
    Text& same = a;  // (a = a would be flagged by the compiler; this is the same thing through a reference)
    a = same;
    ZTH_EXPECT_EQ(s(a), std::string("hello"));
    ZTH_EXPECT_EQ(a.size(), std::size_t{5});
    a.append("!");
    ZTH_EXPECT_EQ(s(a), std::string("hello!"));
}
ZTH_TEST("robust", "assignment works between Texts of different lengths, including empty ones") {
    Text big("a longer text than the other one");
    Text small("hi");
    Text empty;
    Text t("start");
    t = big;
    ZTH_EXPECT_EQ(s(t), std::string("a longer text than the other one"));
    t = small;
    ZTH_EXPECT_EQ(s(t), std::string("hi"));
    ZTH_EXPECT_EQ(t.size(), std::size_t{2});
    t = empty;
    ZTH_EXPECT_EQ(s(t), std::string(""));
    ZTH_EXPECT_EQ(t.size(), std::size_t{0});
    t = big;
    ZTH_EXPECT_EQ(t.size(), big.size());
}
ZTH_TEST("robust", "operator= returns the object itself by reference, so assignments chain") { chained_assignment_test<Text>(); }
ZTH_TEST("robust", "assignment releases the old buffer: many assignments leak nothing") {
    zth::mem::Scope scope;
    {
        Text a("0123456789"), b("abc");
        for (int i = 0; i < 100; ++i) {
            a = b;
            b = a;
        }
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "the terminating NUL character is copied too: c_str() ends where size() says, after a copy and after an assignment") {
    Text a("abc");
    Text b = a;
    Text c;
    c = a;
    ZTH_EXPECT_EQ(std::string(b.c_str()).size(), std::size_t{3});
    ZTH_EXPECT_EQ(std::string(c.c_str()).size(), std::size_t{3});
    ZTH_EXPECT_EQ(b.c_str()[3], '\0');
    ZTH_EXPECT_EQ(c.c_str()[3], '\0');
}
ZTH_TEST("robust", "a copy of an empty Text, and a copy of a copy, are independent Texts") {
    Text empty;
    Text e2 = empty;
    e2.append("x");
    ZTH_EXPECT_EQ(s(empty), std::string(""));
    ZTH_EXPECT_EQ(s(e2), std::string("x"));
    Text a("one");
    Text b = a;
    Text c = b;
    c.at(0) = 'O';
    b.at(1) = 'N';
    ZTH_EXPECT_EQ(s(a), std::string("one"));
    ZTH_EXPECT_EQ(s(b), std::string("oNe"));
    ZTH_EXPECT_EQ(s(c), std::string("One"));
}
ZTH_TEST("robust", "passing a Text by value and returning it works (copies are made and destroyed)") {
    zth::mem::Scope scope;
    {
        Text a("hey");
        Text b = pass_through(a);
        ZTH_EXPECT_EQ(s(a), std::string("hey"));
        ZTH_EXPECT_EQ(s(b), std::string("hey!"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a vector of Texts copies them correctly when it grows") {
    zth::mem::Scope scope;
    {
        std::vector<Text> v;
        for (int i = 0; i < 100; ++i) v.push_back(Text(("item " + std::to_string(i)).c_str()));
        ZTH_EXPECT_EQ(s(v[0]), std::string("item 0"));
        ZTH_EXPECT_EQ(s(v[57]), std::string("item 57"));
        ZTH_EXPECT_EQ(s(v[99]), std::string("item 99"));
        std::vector<Text> w = v;
        w[3].at(0) = 'X';
        ZTH_EXPECT_EQ(s(v[3]), std::string("item 3"));
        ZTH_EXPECT_EQ(s(w[3]), std::string("Xtem 3"));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "append of text that lives inside the Text itself works (a.append(a.c_str()))") {
    Text a("ab");
    a.append(a.c_str());
    ZTH_EXPECT_EQ(s(a), std::string("abab"));
    a.append(a.c_str() + 2);
    ZTH_EXPECT_EQ(s(a), std::string("ababab"));
}
ZTH_TEST("robust", "append handles nothing, empty text and many characters; the old buffer is released") {
    zth::mem::Scope scope;
    {
        Text a("x");
        a.append(nullptr);
        a.append("");
        ZTH_EXPECT_EQ(s(a), std::string("x"));
        for (int i = 0; i < 300; ++i) a.append("yz");
        ZTH_EXPECT_EQ(a.size(), std::size_t{601});
        ZTH_EXPECT_EQ(a.c_str()[600], 'z');
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "at() checks its index (const and non-const)") {
    Text a("abc");
    const Text& c = a;
    ZTH_EXPECT_EQ(a.at(2), 'c');
    ZTH_EXPECT_EQ(c.at(0), 'a');
    ZTH_EXPECT_TRUE(throws<std::out_of_range>([&] { (void)a.at(3); }));
    ZTH_EXPECT_TRUE(throws<std::out_of_range>([&] { (void)c.at(3); }));
    Text e;
    ZTH_EXPECT_TRUE(throws<std::out_of_range>([&] { (void)e.at(0); }));
}
ZTH_TEST("robust", "long texts are copied completely") {
    std::string big(100000, 'q');
    big[99999] = 'Z';
    Text a(big.c_str());
    Text b = a;
    Text c;
    c = a;
    ZTH_EXPECT_EQ(b.size(), std::size_t{100000});
    ZTH_EXPECT_EQ(b.c_str()[99999], 'Z');
    ZTH_EXPECT_EQ(c.c_str()[99999], 'Z');
}
