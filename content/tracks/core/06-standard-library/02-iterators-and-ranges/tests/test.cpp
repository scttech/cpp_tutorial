#include <array>
#include <cstddef>
#include <cstring>
#include <deque>
#include <forward_list>
#include <iterator>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "iter_tools.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using V = std::vector<int>;
using VS = std::vector<std::string>;

// an iterator over an int array that counts how it is moved: `steps` (++ and --) and `jumps` (+= and -=)
template <bool RandomAccess>
struct CountIt {
    using iterator_category = std::conditional_t<RandomAccess, std::random_access_iterator_tag, std::bidirectional_iterator_tag>;
    using value_type = int;
    using difference_type = std::ptrdiff_t;
    using pointer = int*;
    using reference = int&;

    int* p = nullptr;
    static inline long steps = 0;
    static inline long jumps = 0;

    CountIt() = default;
    explicit CountIt(int* q) : p(q) {}
    int& operator*() const { return *p; }
    CountIt& operator++() {
        ++steps;
        ++p;
        return *this;
    }
    CountIt operator++(int) {
        CountIt t = *this;
        ++*this;
        return t;
    }
    CountIt& operator--() {
        ++steps;
        --p;
        return *this;
    }
    CountIt operator--(int) {
        CountIt t = *this;
        --*this;
        return t;
    }
    CountIt& operator+=(difference_type n)
        requires RandomAccess
    {
        ++jumps;
        p += n;
        return *this;
    }
    CountIt& operator-=(difference_type n)
        requires RandomAccess
    {
        ++jumps;
        p -= n;
        return *this;
    }
    friend CountIt operator+(CountIt a, difference_type n)
        requires RandomAccess
    {
        a += n;
        return a;
    }
    friend CountIt operator+(difference_type n, CountIt a)
        requires RandomAccess
    {
        a += n;
        return a;
    }
    friend CountIt operator-(CountIt a, difference_type n)
        requires RandomAccess
    {
        a -= n;
        return a;
    }
    friend difference_type operator-(const CountIt& a, const CountIt& b)
        requires RandomAccess
    {
        return a.p - b.p;
    }
    int& operator[](difference_type n) const
        requires RandomAccess
    {
        return p[n];
    }
    friend bool operator==(const CountIt& a, const CountIt& b) { return a.p == b.p; }
    friend auto operator<=>(const CountIt& a, const CountIt& b) { return a.p <=> b.p; }
};

template <class It>
void category_test(const char* what, const char* expected) {
    const char* got = category_name<It>();
    if (std::strcmp(got, expected) != 0) ::zth::fail(S(what) + " is " + expected + ", but category_name says " + got);
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "category_name: vector, array and string iterators are contiguous") {
    category_test<std::vector<int>::iterator>("std::vector<int>::iterator", "contiguous");
    category_test<std::array<int, 3>::const_iterator>("std::array<int, 3>::const_iterator", "contiguous");
    category_test<std::string::iterator>("std::string::iterator", "contiguous");
    category_test<int*>("int*", "contiguous");
}
ZTH_TEST("works", "category_name: a deque iterator is random access (it jumps, but is not contiguous)") {
    category_test<std::deque<int>::iterator>("std::deque<int>::iterator", "random access");
}
ZTH_TEST("works", "category_name: list and map iterators are bidirectional") {
    category_test<std::list<int>::iterator>("std::list<int>::iterator", "bidirectional");
    category_test<std::map<int, int>::iterator>("std::map<int, int>::iterator", "bidirectional");
}
ZTH_TEST("works", "category_name: a forward_list iterator is forward") {
    category_test<std::forward_list<int>::iterator>("std::forward_list<int>::iterator", "forward");
}
ZTH_TEST("works", "category_name: an istream_iterator is an input iterator, a back_inserter an output iterator") {
    category_test<std::istream_iterator<int>>("std::istream_iterator<int>", "input");
    category_test<std::back_insert_iterator<std::vector<int>>>("std::back_insert_iterator", "output");
    category_test<std::ostream_iterator<int>>("std::ostream_iterator<int>", "output");
}
ZTH_TEST("works", "advance_by moves a vector iterator forward and backward") {
    V v{10, 20, 30, 40, 50};
    auto it = v.begin();
    advance_by(it, 3);
    ZTH_EXPECT_EQ(*it, 40);
    advance_by(it, -2);
    ZTH_EXPECT_EQ(*it, 20);
    advance_by(it, 0);
    ZTH_EXPECT_EQ(*it, 20);
}
ZTH_TEST("works", "advance_by works on a list (bidirectional): forward and backward") {
    std::list<int> l{1, 2, 3, 4, 5};
    auto it = l.begin();
    advance_by(it, 4);
    ZTH_EXPECT_EQ(*it, 5);
    advance_by(it, -3);
    ZTH_EXPECT_EQ(*it, 2);
}
ZTH_TEST("works", "advance_by works on a forward_list and on a map") {
    std::forward_list<int> f{1, 2, 3};
    auto it = f.begin();
    advance_by(it, 2);
    ZTH_EXPECT_EQ(*it, 3);
    std::map<int, S> m{{1, "a"}, {2, "b"}, {3, "c"}};
    auto mit = m.begin();
    advance_by(mit, 2);
    ZTH_EXPECT_EQ(mit->second, S("c"));
}
ZTH_TEST("works", "squares_of_evens") {
    ZTH_EXPECT_TRUE((squares_of_evens({1, 2, 3, 4}) == V{4, 16}));
    ZTH_EXPECT_TRUE((squares_of_evens({2, 4, 6}) == V{4, 16, 36}));
    ZTH_EXPECT_TRUE((squares_of_evens({-2, 5, 0}) == V{4, 0}));
}
ZTH_TEST("works", "middle_section skips the leading negatives and stops at the first value >= 100") {
    ZTH_EXPECT_TRUE((middle_section({-3, -1, 5, 7, 120, 9}) == V{5, 7}));
    ZTH_EXPECT_TRUE((middle_section({1, 2, 3}) == V{1, 2, 3}));
    ZTH_EXPECT_TRUE((middle_section({-1, -2, 0, 99, 100, 5}) == V{0, 99}));
}
ZTH_TEST("works", "split_pieces splits at the separator") {
    ZTH_EXPECT_TRUE((split_pieces("a,b,c", ',') == VS{"a", "b", "c"}));
    ZTH_EXPECT_TRUE((split_pieces("one two", ' ') == VS{"one", "two"}));
    ZTH_EXPECT_TRUE((split_pieces("nosep", ',') == VS{"nosep"}));
}
ZTH_TEST("works", "first_n_matching: the first n integers that satisfy the predicate") {
    ZTH_EXPECT_TRUE((first_n_matching([](int x) { return x % 7 == 0; }, 5) == V{0, 7, 14, 21, 28}));
    ZTH_EXPECT_TRUE((first_n_matching([](int x) { return x > 100; }, 3) == V{101, 102, 103}));
}
ZTH_TEST("works", "duplicate_each doubles every element in place") {
    V v{1, 2, 3};
    duplicate_each(v);
    ZTH_EXPECT_TRUE((v == V{1, 1, 2, 2, 3, 3}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "category_name for const iterators, reverse iterators and pointers to const") {
    category_test<std::vector<int>::const_iterator>("vector::const_iterator", "contiguous");
    category_test<const char*>("const char*", "contiguous");
    category_test<std::list<int>::const_iterator>("list::const_iterator", "bidirectional");
    category_test<std::list<int>::reverse_iterator>("list::reverse_iterator", "bidirectional");
    category_test<std::deque<int>::const_iterator>("deque::const_iterator", "random access");
}
ZTH_TEST("robust", "category_name of something that is not an iterator at all") {
    category_test<int>("int", "none");
    category_test<S>("std::string", "none");
}
ZTH_TEST("robust", "advance_by 0 and by the whole length") {
    std::list<int> l{1, 2, 3};
    auto it = l.begin();
    advance_by(it, 0);
    ZTH_EXPECT_TRUE(it == l.begin());
    advance_by(it, 3);
    ZTH_EXPECT_TRUE(it == l.end());
    advance_by(it, -3);
    ZTH_EXPECT_TRUE(it == l.begin());
}
ZTH_TEST("robust", "advance_by works with a deque and an input iterator") {
    std::deque<int> d{5, 6, 7, 8};
    auto it = d.begin();
    advance_by(it, 3);
    ZTH_EXPECT_EQ(*it, 8);
    std::istringstream in("1 2 3 4");
    std::istream_iterator<int> in_it(in);
    advance_by(in_it, 2);
    ZTH_EXPECT_EQ(*in_it, 3);
}
ZTH_TEST("robust", "the test iterator really is random access (a sanity check of the test itself)") {
    ZTH_EXPECT_TRUE(std::random_access_iterator<CountIt<true>>);
    ZTH_EXPECT_TRUE(std::bidirectional_iterator<CountIt<false>>);
    ZTH_EXPECT_FALSE(std::random_access_iterator<CountIt<false>>);
}
ZTH_TEST("robust", "squares_of_evens of nothing, and of only odd numbers") {
    ZTH_EXPECT_TRUE(squares_of_evens({}).empty());
    ZTH_EXPECT_TRUE(squares_of_evens({1, 3, 5}).empty());
}
ZTH_TEST("robust", "squares_of_evens does not change its input") {
    const V in{1, 2, 3, 4};
    squares_of_evens(in);
    ZTH_EXPECT_TRUE((in == V{1, 2, 3, 4}));
}
ZTH_TEST("robust", "middle_section: nothing to skip, nothing to take, and negatives AFTER the start are kept") {
    ZTH_EXPECT_TRUE(middle_section({}).empty());
    ZTH_EXPECT_TRUE(middle_section({-1, -2}).empty());
    ZTH_EXPECT_TRUE(middle_section({500, 1}).empty());
    ZTH_EXPECT_TRUE((middle_section({-5, 3, -4, 8}) == V{3, -4, 8}));
}
ZTH_TEST("robust", "split_pieces keeps empty pieces, at the start, in the middle and at the end") {
    ZTH_EXPECT_TRUE((split_pieces("a,,b", ',') == VS{"a", "", "b"}));
    ZTH_EXPECT_TRUE((split_pieces(",a", ',') == VS{"", "a"}));
    ZTH_EXPECT_TRUE((split_pieces("a,", ',') == VS{"a", ""}));
    ZTH_EXPECT_TRUE((split_pieces(",", ',') == VS{"", ""}));
}
ZTH_TEST("robust", "split_pieces of an empty text has no pieces (std::views::split of an empty range is empty)") {
    ZTH_EXPECT_TRUE(split_pieces("", ',').empty());
}
ZTH_TEST("robust", "first_n_matching with n = 0, and with a predicate that matches the very first integer") {
    ZTH_EXPECT_TRUE(first_n_matching([](int) { return true; }, 0).empty());
    ZTH_EXPECT_TRUE((first_n_matching([](int) { return true; }, 3) == V{0, 1, 2}));
}
ZTH_TEST("robust", "duplicate_each of an empty vector and of one element") {
    V e;
    duplicate_each(e);
    ZTH_EXPECT_TRUE(e.empty());
    V one{9};
    duplicate_each(one);
    ZTH_EXPECT_TRUE((one == V{9, 9}));
}
ZTH_TEST("robust", "duplicate_each keeps the order of equal and negative values") {
    V v{-1, -1, 5, 0};
    duplicate_each(v);
    ZTH_EXPECT_TRUE((v == V{-1, -1, -1, -1, 5, 5, 0, 0}));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "advance_by on a random access iterator is ONE jump, not n steps") {
    int data[100] = {};
    CountIt<true> it(data);
    CountIt<true>::steps = CountIt<true>::jumps = 0;
    advance_by(it, 80);
    ZTH_EXPECT_TRUE(it.p == data + 80);
    ZTH_EXPECT_EQ(CountIt<true>::steps, 0L);
    ZTH_EXPECT_EQ(CountIt<true>::jumps, 1L);
}
ZTH_TEST("fast", "advance_by on a bidirectional iterator takes |n| steps (and never more)") {
    int data[100] = {};
    CountIt<false> it(data + 50);
    CountIt<false>::steps = 0;
    advance_by(it, 30);
    ZTH_EXPECT_TRUE(it.p == data + 80);
    ZTH_EXPECT_EQ(CountIt<false>::steps, 30L);
    CountIt<false>::steps = 0;
    advance_by(it, -45);
    ZTH_EXPECT_TRUE(it.p == data + 35);
    ZTH_EXPECT_EQ(CountIt<false>::steps, 45L);
}
ZTH_TEST("fast", "first_n_matching is lazy: the predicate is not called for the integers after the n-th match") {
    long calls = 0;
    auto pred = [&calls](int x) {
        ++calls;
        return x % 7 == 0;
    };
    const V r = first_n_matching(pred, 5);
    ZTH_EXPECT_TRUE((r == V{0, 7, 14, 21, 28}));
    ZTH_EXPECT_TRUE(calls >= 29);  // it did have to look at 0..28
    ZTH_EXPECT_TRUE(calls <= 40);  // but not much further
}
ZTH_TEST("fast", "duplicate_each handles 400000 elements (one pass, not an insert per element)") {
    V v;
    v.reserve(400000);
    for (int i = 0; i < 400000; ++i) v.push_back(i);
    duplicate_each(v);
    ZTH_EXPECT_EQ(v.size(), std::size_t{800000});
    ZTH_EXPECT_EQ(v[799998], 399999);
    ZTH_EXPECT_EQ(v[799999], 399999);
    ZTH_EXPECT_EQ(v[1], 0);
}
ZTH_TEST("fast", "duplicate_each is exact for 50000 elements") {
    V v;
    for (int i = 0; i < 50000; ++i) v.push_back(i % 10);
    duplicate_each(v);
    long sum = 0;
    for (int x : v) sum += x;
    ZTH_EXPECT_EQ(sum, 2L * 5000 * 45);
}
