#define ZTH_TRACK_ALLOCATIONS
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "matrix.hpp"
#include "stack.hpp"
#include "zth_test.hpp"

namespace {

using std::size_t;

// ---- shape checks as requires-expressions: a wrong shape is a failed test, not a compile error in this hidden file ----------
template <class A, class B>
constexpr bool can_add = requires(A a, B b) { a + b; };
template <class A, class B>
constexpr bool can_multiply = requires(A a, B b) { a * b; };

// the value pop() returns, as a copy (a reference into the temporary optional would dangle)
template <class S>
auto take(S& s) {
    auto v = s.pop();
    return *v;
}

// ================================================ Stack ==============================================================
template <class T>
void lifo_test(T a, T b, T c) {
    Stack<T> s;
    ZTH_EXPECT_TRUE(s.empty());
    ZTH_EXPECT_EQ(s.size(), size_t{0});
    s.push(a);
    s.push(b);
    s.push(c);
    ZTH_EXPECT_FALSE(s.empty());
    ZTH_EXPECT_EQ(s.size(), size_t{3});
    ZTH_EXPECT_TRUE(s.top() != nullptr && *s.top() == c);
    auto x = s.pop();
    auto y = s.pop();
    auto z = s.pop();
    ZTH_EXPECT_TRUE(x && *x == c);
    ZTH_EXPECT_TRUE(y && *y == b);
    ZTH_EXPECT_TRUE(z && *z == a);
    ZTH_EXPECT_TRUE(s.empty());
}

template <class Int>
void ctad_test() {
    // `Stack s{1, 2, 3};` must deduce Stack<int> from the braces, with 3 on top
    if constexpr (!requires { Stack{Int{1}, Int{2}, Int{3}}; }) {
        ::zth::fail("Stack{1, 2, 3} does not compile: add a constructor taking std::initializer_list<T> (and initialize items_ from it)");
    } else {
        auto s = Stack{Int{1}, Int{2}, Int{3}};
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(s), Stack<Int>>));
        ZTH_EXPECT_EQ(s.size(), size_t{3});
        ZTH_EXPECT_TRUE(s.top() != nullptr && *s.top() == 3);
        ZTH_EXPECT_EQ(take(s), 3);
        ZTH_EXPECT_EQ(take(s), 2);
        ZTH_EXPECT_EQ(take(s), 1);
    }
}

// ================================================ Matrix =============================================================
template <class M>
void fill_counting(M& m) {
    int n = 1;
    for (size_t r = 0; r < M::rows(); ++r)
        for (size_t c = 0; c < M::cols(); ++c) m(r, c) = n++;
}

template <class M>
std::vector<int> flat(const M& m) {
    std::vector<int> out;
    for (size_t r = 0; r < M::rows(); ++r)
        for (size_t c = 0; c < M::cols(); ++c) out.push_back(m(r, c));
    return out;
}

template <class A, class B>
void product_test() {
    if constexpr (!can_multiply<A, B>) {
        ::zth::fail("a * b does not compile for a 2x3 and a 3x2 matrix: write operator* as a template over R, K and C");
    } else {
        A a;
        B b;
        fill_counting(a);
        fill_counting(b);
        auto p = a * b;  // [1 2 3; 4 5 6] * [1 2; 3 4; 5 6] = [22 28; 49 64]
        ZTH_EXPECT_TRUE((std::is_same_v<decltype(p), Matrix<int, 2, 2>>));
        ZTH_EXPECT_EQ(flat(p), (std::vector<int>{22, 28, 49, 64}));
    }
}

template <class M>
void transpose_test() {
    M m;
    fill_counting(m);
    auto t = m.transposed();
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(t), Matrix<int, 3, 2>>));
    ZTH_EXPECT_EQ(flat(t), (std::vector<int>{1, 4, 2, 5, 3, 6}));
    ZTH_EXPECT_TRUE(t.transposed() == m);
}

template <class M>
void identity_test() {
    M m;
    fill_counting(m);
    const M id = M::identity();
    ZTH_EXPECT_TRUE(id * m == m);
    ZTH_EXPECT_TRUE(m * id == m);
    ZTH_EXPECT_EQ(id(0, 0), 1);
    ZTH_EXPECT_EQ(id(0, 1), 0);
    ZTH_EXPECT_EQ(id(2, 2), 1);
}

template <class A, class B>
void add_test() {
    if constexpr (!can_add<A, B>) {
        ::zth::fail("a + b does not compile for two matrices of the same dimensions");
    } else {
        A a, b;
        fill_counting(a);
        fill_counting(b);
        A sum = a + b;
        ZTH_EXPECT_EQ(flat(sum), (std::vector<int>{2, 4, 6, 8, 10, 12}));
        a += b;
        ZTH_EXPECT_TRUE(a == sum);
        ZTH_EXPECT_EQ(flat(b), (std::vector<int>{1, 2, 3, 4, 5, 6}));  // b unchanged
    }
}

template <class A, class B>
void dimensions_are_checked_test() {
    // adding a 2x3 to a 3x2, and multiplying a 2x3 by a 2x3, must not compile
    ZTH_EXPECT_FALSE((can_add<A, B>));
    ZTH_EXPECT_FALSE((can_multiply<A, A>));
    ZTH_EXPECT_TRUE((can_multiply<A, B>));  // 2x3 * 3x2 is fine
}

}  // namespace

// ============================================ Stack: Works ===========================================================
ZTH_TEST("works", "a Stack of ints is last-in, first-out") { lifo_test<int>(1, 2, 3); }
ZTH_TEST("works", "a Stack of std::string, and of double") {
    lifo_test<std::string>("a", "bb", "ccc");
    lifo_test<double>(1.5, 2.5, 3.5);
}
ZTH_TEST("works", "pop() on an empty stack gives an empty optional, top() gives nullptr") {
    Stack<int> s;
    ZTH_EXPECT_FALSE(s.pop().has_value());
    ZTH_EXPECT_TRUE(s.top() == nullptr);
    s.push(1);
    (void)s.pop();
    ZTH_EXPECT_FALSE(s.pop().has_value());
    ZTH_EXPECT_TRUE(s.top() == nullptr);
}
ZTH_TEST("works", "top() gives access to the top element, const and non-const") {
    Stack<int> s;
    s.push(5);
    *s.top() = 50;
    const Stack<int>& c = s;
    ZTH_EXPECT_TRUE(c.top() != nullptr);
    ZTH_EXPECT_EQ(*c.top(), 50);
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(c.top()), const int*>));
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(s.top()), int*>));
}
ZTH_TEST("works", "Stack{1, 2, 3} deduces Stack<int> and holds 1, 2, 3 with 3 on top") { ctad_test<int>(); }
ZTH_TEST("works", "a Stack of move-only elements (std::unique_ptr) works") {
    Stack<std::unique_ptr<int>> s;
    s.push(std::make_unique<int>(1));
    s.push(std::make_unique<int>(2));
    auto top = s.pop();
    ZTH_EXPECT_TRUE(top.has_value() && **top == 2);
    ZTH_EXPECT_EQ(s.size(), size_t{1});
    ZTH_EXPECT_EQ(**s.top(), 1);
}
ZTH_TEST("works", "the member typedefs value_type and size_type exist") {
    ZTH_EXPECT_TRUE((std::is_same_v<Stack<int>::value_type, int>));
    ZTH_EXPECT_TRUE((std::is_same_v<Stack<std::string>::value_type, std::string>));
    ZTH_EXPECT_TRUE((std::is_same_v<Stack<int>::size_type, size_t>));
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(Stack<int>().size()), size_t>));
}

// ============================================ Stack: Robust ==========================================================
ZTH_TEST("robust", "push of an lvalue copies, push of an rvalue moves: both work") {
    Stack<std::string> s;
    std::string keep = "keep me";
    s.push(keep);
    s.push(std::string("temporary"));
    s.push(std::move(keep));
    ZTH_EXPECT_EQ(s.size(), size_t{3});
    ZTH_EXPECT_EQ(take(s), std::string("keep me"));
    ZTH_EXPECT_EQ(take(s), std::string("temporary"));
    ZTH_EXPECT_EQ(take(s), std::string("keep me"));
}
ZTH_TEST("robust", "clear() empties the stack, and it can be used again") {
    Stack<int> s{1, 2, 3};
    s.clear();
    ZTH_EXPECT_TRUE(s.empty());
    ZTH_EXPECT_EQ(s.size(), size_t{0});
    s.push(9);
    ZTH_EXPECT_EQ(*s.top(), 9);
}
ZTH_TEST("robust", "a copy of a Stack is independent of the original") {
    Stack<int> a{1, 2, 3};
    Stack<int> b = a;
    b.push(4);
    (void)a.pop();
    ZTH_EXPECT_EQ(a.size(), size_t{2});
    ZTH_EXPECT_EQ(b.size(), size_t{4});
    ZTH_EXPECT_EQ(*a.top(), 2);
    ZTH_EXPECT_EQ(*b.top(), 4);
}
ZTH_TEST("robust", "stacks of stacks, and a large stack") {
    Stack<Stack<int>> outer;
    outer.push(Stack<int>{1, 2});
    outer.push(Stack<int>{3});
    ZTH_EXPECT_EQ(outer.top()->size(), size_t{1});
    auto inner = outer.pop();
    ZTH_EXPECT_EQ(*inner->top(), 3);
    ZTH_EXPECT_EQ(outer.top()->size(), size_t{2});
    Stack<int> big;
    for (int i = 0; i < 50000; ++i) big.push(i);
    ZTH_EXPECT_EQ(big.size(), size_t{50000});
    ZTH_EXPECT_EQ(*big.top(), 49999);
    long long sum = 0;
    while (auto v = big.pop()) sum += *v;
    ZTH_EXPECT_EQ(sum, 1249975000LL);
}
ZTH_TEST("robust", "a Stack frees its elements: nothing leaks") {
    zth::mem::Scope scope;
    {
        Stack<std::string> s;
        for (int i = 0; i < 100; ++i) s.push(std::string(40, 'x'));
        (void)s.pop();
        Stack<std::unique_ptr<int>> u;
        for (int i = 0; i < 20; ++i) u.push(std::make_unique<int>(i));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ============================================ Stack: Fast ============================================================
ZTH_TEST("fast", "push of an rvalue moves the element; push of an lvalue copies it once") {
    Stack<zth::Tracked> s;
    zth::Tracked t(5);
    zth::Tracked::reset();
    s.push(std::move(t));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 1);
    zth::Tracked u(6);
    zth::Tracked::reset();
    s.push(u);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 1);
}
ZTH_TEST("fast", "pop() moves the element out (it never copies it)") {
    Stack<zth::Tracked> s;
    s.push(zth::Tracked(1));
    s.push(zth::Tracked(2));
    zth::Tracked::reset();
    auto v = s.pop();
    ZTH_EXPECT_TRUE(v.has_value() && v->value == 2);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
}

// ============================================ Matrix: Works ==========================================================
ZTH_TEST("works", "the dimensions are part of the type: rows() and cols()") {
    ZTH_EXPECT_EQ((Matrix<int, 2, 3>::rows()), size_t{2});
    ZTH_EXPECT_EQ((Matrix<int, 2, 3>::cols()), size_t{3});
    ZTH_EXPECT_EQ((Matrix<double, 5, 1>::rows()), size_t{5});
    ZTH_EXPECT_FALSE((std::is_same_v<Matrix<int, 2, 3>, Matrix<int, 3, 2>>));
}
ZTH_TEST("works", "elements are read and written with (row, col); the fill constructor sets every element") {
    Matrix<int, 2, 3> m;
    m(0, 0) = 1;
    m(1, 2) = 6;
    ZTH_EXPECT_EQ(m(0, 0), 1);
    ZTH_EXPECT_EQ(m(1, 2), 6);
    ZTH_EXPECT_EQ(m(1, 1), 0);
    Matrix<int, 2, 2> f(7);
    ZTH_EXPECT_EQ(flat(f), (std::vector<int>{7, 7, 7, 7}));
    const Matrix<int, 2, 2>& c = f;
    ZTH_EXPECT_EQ(c(1, 0), 7);
}
ZTH_TEST("works", "transposed() swaps the rows and columns, and its type") { transpose_test<Matrix<int, 2, 3>>(); }
ZTH_TEST("works", "matrices of the same dimensions can be added") { add_test<Matrix<int, 2, 3>, Matrix<int, 2, 3>>(); }
ZTH_TEST("works", "a (2x3) times a (3x2) is a (2x2), with the right values") { product_test<Matrix<int, 2, 3>, Matrix<int, 3, 2>>(); }
ZTH_TEST("works", "identity() leaves a matrix unchanged when multiplied from either side") { identity_test<Matrix<int, 3, 3>>(); }
ZTH_TEST("works", "== compares every element") {
    Matrix<int, 2, 2> a(1), b(1);
    ZTH_EXPECT_TRUE(a == b);
    b(1, 1) = 2;
    ZTH_EXPECT_FALSE(a == b);
    ZTH_EXPECT_TRUE((Matrix<int, 1, 1>() == Matrix<int, 1, 1>()));
}

// ============================================ Matrix: Robust =========================================================
ZTH_TEST("robust", "a size mistake is a COMPILE error: adding a 2x3 to a 3x2, multiplying a 2x3 by a 2x3") {
    dimensions_are_checked_test<Matrix<int, 2, 3>, Matrix<int, 3, 2>>();
}
ZTH_TEST("robust", "a default Matrix starts with every element zero (even when the stack held junk)") {
    zth::dirty_stack();
    Matrix<int, 3, 4> m;
    ZTH_EXPECT_EQ(flat(m), std::vector<int>(12, 0));
    Matrix<double, 2, 2> d;
    ZTH_EXPECT_TRUE(d(0, 0) == 0.0 && d(1, 1) == 0.0);
}
ZTH_TEST("robust", "(A * B) * C == A * (B * C), and (A * B) transposed == B transposed * A transposed") {
    if constexpr (!can_multiply<Matrix<int, 2, 3>, Matrix<int, 3, 2>>) {
        ::zth::fail("operator* does not compile");
    } else {
        Matrix<int, 2, 3> a;
        Matrix<int, 3, 4> b;
        Matrix<int, 4, 2> c;
        fill_counting(a);
        fill_counting(b);
        fill_counting(c);
        ZTH_EXPECT_TRUE((a * b) * c == a * (b * c));
        ZTH_EXPECT_TRUE((a * b).transposed() == b.transposed() * a.transposed());
    }
}
ZTH_TEST("robust", "1x1, a row vector times a column vector, and a column vector times a row vector") {
    Matrix<int, 1, 3> row;
    Matrix<int, 3, 1> col;
    fill_counting(row);
    fill_counting(col);
    auto dot = row * col;  // 1*1 + 2*2 + 3*3
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(dot), Matrix<int, 1, 1>>));
    ZTH_EXPECT_EQ(dot(0, 0), 14);
    auto outer = col * row;
    ZTH_EXPECT_TRUE((std::is_same_v<decltype(outer), Matrix<int, 3, 3>>));
    ZTH_EXPECT_EQ(outer(2, 1), 6);
    Matrix<int, 1, 1> one(5);
    ZTH_EXPECT_EQ((one * one)(0, 0), 25);
}
ZTH_TEST("robust", "works for other element types: double and long long") {
    Matrix<double, 2, 2> a;
    a(0, 0) = 0.5;
    a(0, 1) = 1.5;
    a(1, 0) = 2.0;
    a(1, 1) = 0.25;
    auto p = a * a;
    ZTH_EXPECT_NEAR(p(0, 0), 0.5 * 0.5 + 1.5 * 2.0, 1e-12);
    ZTH_EXPECT_NEAR(p(1, 1), 2.0 * 1.5 + 0.25 * 0.25, 1e-12);
    Matrix<long long, 1, 2> big;
    big(0, 0) = 3000000000LL;
    big(0, 1) = 2;
    auto sq = big * big.transposed();
    ZTH_EXPECT_EQ(sq(0, 0), 9000000000000000004LL);
}

// ============================================ Matrix: Fast ===========================================================
ZTH_TEST("fast", "a Matrix lives entirely inside the object: no heap allocation, and no space beyond the elements") {
    zth::mem::Scope scope;
    {
        Matrix<int, 4, 4> a(2), b(3);
        auto c = a * b;
        auto t = c.transposed();
        auto s = t + c;
        auto id = Matrix<int, 4, 4>::identity();
        ZTH_EXPECT_EQ(s(0, 0), 48);
        ZTH_EXPECT_TRUE((id == Matrix<int, 4, 4>::identity()));
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_EQ(sizeof(Matrix<int, 3, 4>), sizeof(int) * 12);
    ZTH_EXPECT_EQ(sizeof(Matrix<double, 2, 2>), sizeof(double) * 4);
}
