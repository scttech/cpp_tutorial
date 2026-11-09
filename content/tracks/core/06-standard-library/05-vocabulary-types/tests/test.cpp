#define ZTH_TRACK_ALLOCATIONS
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "expr.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using Vars = std::map<std::string, double>;
using Names = std::vector<std::string>;

// (2 + 3) * (x - 1)
ExprPtr sample() { return bin('*', bin('+', num(2), num(3)), bin('-', var("x"), num(1))); }

// 1 + (1 + (1 + ... )) with `depth` additions
ExprPtr chain(int depth) {
    ExprPtr e = num(1);
    for (int i = 0; i < depth; ++i) e = bin('+', num(1), std::move(e));
    return e;
}

// the value of an expression, or NaN when it has none
double ev(const Expr& e, const Vars& vars = {}) {
    const auto r = evaluate(e, vars);
    return r ? *r : std::nan("");
}

const Expr* find_address_of_lhs(const Expr& e) { return std::get<BinOp>(e.node).lhs.get(); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "evaluate: numbers and arithmetic") {
    ZTH_EXPECT_NEAR(ev(*num(2.5), {}), 2.5, 1e-12);
    ZTH_EXPECT_NEAR(ev(*bin('+', num(1), num(2)), {}), 3.0, 1e-12);
    ZTH_EXPECT_NEAR(ev(*bin('-', num(10), num(4)), {}), 6.0, 1e-12);
    ZTH_EXPECT_NEAR(ev(*bin('*', num(3), num(4)), {}), 12.0, 1e-12);
    ZTH_EXPECT_NEAR(ev(*bin('/', num(9), num(4)), {}), 2.25, 1e-12);
}
ZTH_TEST("works", "evaluate: nested expressions and negation") {
    ExprPtr e = sample();
    ZTH_EXPECT_NEAR(ev(*e, {{"x", 4.0}}), 15.0, 1e-12);
    ExprPtr n = neg(bin('+', num(1), num(2)));
    ZTH_EXPECT_NEAR(ev(*n, {}), -3.0, 1e-12);
}
ZTH_TEST("works", "evaluate: variables take their values from the map") {
    ExprPtr e = bin('+', var("x"), var("y"));
    const auto r = evaluate(*e, {{"x", 1.5}, {"y", 2.0}});
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) ZTH_EXPECT_NEAR(*r, 3.5, 1e-12);
}
ZTH_TEST("works", "evaluate: a variable without a value gives no value") {
    ExprPtr e = bin('+', var("x"), num(1));
    ZTH_EXPECT_FALSE(evaluate(*e, {}).has_value());
    ZTH_EXPECT_FALSE(evaluate(*e, {{"y", 1.0}}).has_value());
}
ZTH_TEST("works", "evaluate: division by zero gives no value") {
    ZTH_EXPECT_FALSE(evaluate(*bin('/', num(1), num(0)), {}).has_value());
    ExprPtr e = bin('/', num(1), var("x"));
    ZTH_EXPECT_FALSE(evaluate(*e, {{"x", 0.0}}).has_value());
    ZTH_EXPECT_TRUE(evaluate(*e, {{"x", 4.0}}).has_value());
}
ZTH_TEST("works", "to_string: fully parenthesized infix") {
    ZTH_EXPECT_EQ(to_string(*num(3)), S("3"));
    ZTH_EXPECT_EQ(to_string(*num(2.5)), S("2.5"));
    ZTH_EXPECT_EQ(to_string(*var("abc")), S("abc"));
    ZTH_EXPECT_EQ(to_string(*bin('+', var("x"), num(3))), S("(x + 3)"));
    ZTH_EXPECT_EQ(to_string(*neg(var("x"))), S("(-x)"));
    ZTH_EXPECT_EQ(to_string(*sample()), S("((2 + 3) * (x - 1))"));
}
ZTH_TEST("works", "node_count counts every node") {
    ZTH_EXPECT_EQ(node_count(*num(1)), std::size_t{1});
    ZTH_EXPECT_EQ(node_count(*neg(num(1))), std::size_t{2});
    ZTH_EXPECT_EQ(node_count(*sample()), std::size_t{7});
}
ZTH_TEST("works", "variables: each name once, sorted") {
    ExprPtr e = bin('+', bin('*', var("b"), var("a")), bin('-', var("b"), neg(var("c"))));
    ZTH_EXPECT_TRUE((variables(*e) == Names{"a", "b", "c"}));
    ZTH_EXPECT_TRUE(variables(*num(1)).empty());
}
ZTH_TEST("works", "clone makes an equal tree") {
    ExprPtr e = sample();
    ExprPtr c = clone(*e);
    ZTH_EXPECT_EQ(to_string(*c), to_string(*e));
    ZTH_EXPECT_EQ(node_count(*c), node_count(*e));
}
ZTH_TEST("works", "simplify folds constants") {
    ExprPtr e = sample();
    ZTH_EXPECT_EQ(to_string(*simplify(*e)), S("(5 * (x - 1))"));
    ZTH_EXPECT_EQ(to_string(*simplify(*bin('*', num(2), bin('+', num(3), num(4))))), S("14"));
    ZTH_EXPECT_EQ(to_string(*simplify(*neg(num(3)))), S("-3"));
}
ZTH_TEST("works", "min_max returns the smallest and the largest, from a vector, an array or a C array") {
    const std::vector<double> v{3.5, -1.0, 9.0, 2.0};
    const auto a = min_max(v);
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) {
        ZTH_EXPECT_NEAR(a->first, -1.0, 1e-12);
        ZTH_EXPECT_NEAR(a->second, 9.0, 1e-12);
    }
    const std::array<double, 2> arr{4.0, 5.0};
    const auto b = min_max(arr);
    if (b) ZTH_EXPECT_NEAR(b->second, 5.0, 1e-12);
    const double c[] = {2.0, 1.0, 3.0};
    const auto d = min_max(c);
    if (d) ZTH_EXPECT_NEAR(d->first, 1.0, 1e-12);
}
ZTH_TEST("works", "head_tail splits off the first element") {
    const std::vector<int> v{7, 8, 9};
    const auto r = head_tail(v);
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) {
        const auto& [head, tail] = *r;
        ZTH_EXPECT_EQ(head, 7);
        ZTH_EXPECT_EQ(tail.size(), std::size_t{2});
        ZTH_EXPECT_EQ(tail[0], 8);
        ZTH_EXPECT_EQ(tail[1], 9);
    }
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "evaluate: no value anywhere in the tree makes the whole result no value") {
    ExprPtr e = bin('+', num(1), bin('*', num(2), bin('/', num(3), num(0))));
    ZTH_EXPECT_FALSE(evaluate(*e, {}).has_value());
    ExprPtr n = neg(var("nope"));
    ZTH_EXPECT_FALSE(evaluate(*n, {}).has_value());
}
ZTH_TEST("robust", "evaluate: zero is a value (not 'no value'), and so is a negative zero result") {
    const auto z = evaluate(*bin('-', num(2), num(2)), {});
    ZTH_EXPECT_TRUE(z.has_value());
    if (z) ZTH_EXPECT_NEAR(*z, 0.0, 1e-12);
    const auto q = evaluate(*bin('/', num(0), num(5)), {});
    ZTH_EXPECT_TRUE(q.has_value());
}
ZTH_TEST("robust", "evaluate: division by a zero that comes from a subtraction or a variable") {
    ExprPtr e = bin('/', num(1), bin('-', var("x"), var("x")));
    ZTH_EXPECT_FALSE(evaluate(*e, {{"x", 5.0}}).has_value());
}
ZTH_TEST("robust", "evaluate: an unknown operator gives no value") {
    ZTH_EXPECT_FALSE(evaluate(*bin('^', num(2), num(3)), {}).has_value());
}
ZTH_TEST("robust", "evaluate: a variable that is a prefix of another name is a different variable") {
    ExprPtr e = bin('+', var("x"), var("x1"));
    ZTH_EXPECT_FALSE(evaluate(*e, {{"x", 1.0}}).has_value());
    ZTH_EXPECT_NEAR(ev(*e, {{"x", 1.0}, {"x1", 2.0}}), 3.0, 1e-12);
}
ZTH_TEST("robust", "evaluate: subtraction and division are not commutative") {
    ZTH_EXPECT_NEAR(ev(*bin('-', num(1), num(5)), {}), -4.0, 1e-12);
    ZTH_EXPECT_NEAR(ev(*bin('/', num(1), num(4)), {}), 0.25, 1e-12);
}
ZTH_TEST("robust", "to_string: negative numbers, nested negation and every operator") {
    ZTH_EXPECT_EQ(to_string(*num(-4)), S("-4"));
    ZTH_EXPECT_EQ(to_string(*neg(neg(var("x")))), S("(-(-x))"));
    ZTH_EXPECT_EQ(to_string(*bin('/', num(1), num(2))), S("(1 / 2)"));
    ZTH_EXPECT_EQ(to_string(*bin('-', var("a"), neg(num(1)))), S("(a - (-1))"));
}
ZTH_TEST("robust", "to_string of a deep left-leaning tree") {
    ExprPtr e = var("x");
    for (int i = 0; i < 5; ++i) e = bin('+', std::move(e), num(i));
    ZTH_EXPECT_EQ(to_string(*e), S("(((((x + 0) + 1) + 2) + 3) + 4)"));
}
ZTH_TEST("robust", "variables of a repeated variable, and of an expression with none") {
    ExprPtr e = bin('+', var("x"), bin('+', var("x"), var("x")));
    ZTH_EXPECT_TRUE((variables(*e) == Names{"x"}));
    ZTH_EXPECT_TRUE(variables(*bin('+', num(1), num(2))).empty());
}
ZTH_TEST("robust", "clone copies negations and every operator") {
    ExprPtr e = bin('/', neg(var("x")), bin('-', num(4), neg(num(2))));
    ExprPtr c = clone(*e);
    ZTH_EXPECT_EQ(to_string(*c), S("((-x) / (4 - (-2)))"));
    ZTH_EXPECT_EQ(node_count(*c), node_count(*e));
}
ZTH_TEST("robust", "clone is DEEP: no node is shared with the original") {
    ExprPtr e = sample();
    ExprPtr c = clone(*e);
    ZTH_EXPECT_TRUE(c.get() != e.get());
    ZTH_EXPECT_TRUE(find_address_of_lhs(*c) != find_address_of_lhs(*e));
}
ZTH_TEST("robust", "clone survives the original being destroyed") {
    ExprPtr c;
    {
        ExprPtr e = sample();
        c = clone(*e);
    }
    ZTH_EXPECT_EQ(to_string(*c), S("((2 + 3) * (x - 1))"));
}
ZTH_TEST("robust", "simplify does not change the original") {
    ExprPtr e = sample();
    ExprPtr s = simplify(*e);
    ZTH_EXPECT_EQ(to_string(*e), S("((2 + 3) * (x - 1))"));
    ZTH_EXPECT_EQ(to_string(*s), S("(5 * (x - 1))"));
}
ZTH_TEST("robust", "simplify folds all the way up, and leaves variables alone") {
    ExprPtr e = bin('+', bin('*', num(2), num(3)), bin('-', num(10), num(4)));
    ZTH_EXPECT_EQ(to_string(*simplify(*e)), S("12"));
    ExprPtr v = bin('+', var("x"), bin('*', num(2), num(3)));
    ZTH_EXPECT_EQ(to_string(*simplify(*v)), S("(x + 6)"));
}
ZTH_TEST("robust", "simplify leaves a division by zero unfolded (and folds the rest)") {
    ExprPtr e = bin('+', bin('/', num(1), num(0)), bin('*', num(2), num(2)));
    ZTH_EXPECT_EQ(to_string(*simplify(*e)), S("((1 / 0) + 4)"));
}
ZTH_TEST("robust", "simplify of a negated expression: a number folds, a variable stays negated") {
    ExprPtr e = neg(bin('+', num(1), num(2)));
    ZTH_EXPECT_EQ(to_string(*simplify(*e)), S("-3"));
    ZTH_EXPECT_EQ(to_string(*simplify(*neg(var("x")))), S("(-x)"));
}
ZTH_TEST("robust", "simplify and evaluate agree") {
    ExprPtr e = bin('-', bin('*', num(4), num(2.5)), bin('/', num(9), num(3)));
    ExprPtr s = simplify(*e);
    ZTH_EXPECT_NEAR(ev(*s, {}), ev(*e, {}), 1e-12);
    ZTH_EXPECT_EQ(node_count(*s), std::size_t{1});
}
ZTH_TEST("robust", "min_max of an empty sequence is nothing; of one value, that value twice") {
    ZTH_EXPECT_FALSE(min_max(std::span<const double>()).has_value());
    const std::vector<double> one{4.5};
    const auto r = min_max(one);
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) {
        ZTH_EXPECT_NEAR(r->first, 4.5, 1e-12);
        ZTH_EXPECT_NEAR(r->second, 4.5, 1e-12);
    }
}
ZTH_TEST("robust", "min_max with negative values, equal values and the extremes at either end") {
    const std::vector<double> v{-5.0, -5.0, -1.0};
    const auto r = min_max(v);
    if (r) {
        ZTH_EXPECT_NEAR(r->first, -5.0, 1e-12);
        ZTH_EXPECT_NEAR(r->second, -1.0, 1e-12);
    }
    const std::vector<double> w{9.0, 1.0, 5.0, 0.5};
    const auto q = min_max(w);
    if (q) {
        ZTH_EXPECT_NEAR(q->first, 0.5, 1e-12);
        ZTH_EXPECT_NEAR(q->second, 9.0, 1e-12);
    }
}
ZTH_TEST("robust", "head_tail of an empty sequence is nothing; of one element, that element and an empty tail") {
    ZTH_EXPECT_FALSE(head_tail(std::span<const int>()).has_value());
    const std::vector<int> v{42};
    const auto r = head_tail(v);
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) {
        ZTH_EXPECT_EQ(r->first, 42);
        ZTH_EXPECT_TRUE(r->second.empty());
    }
}
ZTH_TEST("robust", "head_tail's tail is a VIEW of the original elements (no copy)") {
    const std::vector<int> v{1, 2, 3, 4};
    const auto r = head_tail(v);
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) ZTH_EXPECT_TRUE(r->second.data() == v.data() + 1);
}
ZTH_TEST("robust", "head_tail can be applied repeatedly to walk a sequence") {
    const std::vector<int> v{1, 2, 3, 4, 5};
    std::span<const int> rest = v;
    int sum = 0;
    while (auto step = head_tail(rest)) {
        sum += step->first;
        rest = step->second;
    }
    ZTH_EXPECT_EQ(sum, 15);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "evaluate allocates nothing for a tree of numbers (no copy of the tree, no temporary strings)") {
    ExprPtr e = chain(500);
    zth::mem::Scope scope;
    const auto v = evaluate(*e, {});
    ZTH_EXPECT_TRUE(v.has_value());
    if (v) ZTH_EXPECT_NEAR(*v, 501.0, 1e-9);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "node_count allocates nothing") {
    ExprPtr e = chain(500);
    zth::mem::Scope scope;
    ZTH_EXPECT_EQ(node_count(*e), std::size_t{1001});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "min_max and head_tail do not copy the data (a span, not a vector)") {
    std::vector<double> big(100000, 1.0);
    big[5000] = 7.0;
    std::vector<int> ints(100000, 3);
    zth::mem::Scope scope;
    const auto r = min_max(big);
    ZTH_EXPECT_TRUE(r.has_value());
    if (r) ZTH_EXPECT_NEAR(r->second, 7.0, 1e-12);
    const auto h = head_tail(ints);
    ZTH_EXPECT_TRUE(h.has_value());
    if (h) ZTH_EXPECT_EQ(h->second.size(), std::size_t{99999});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "a tree of 2000 nodes is handled (cloned, simplified, evaluated) quickly") {
    ExprPtr e = chain(1000);
    ExprPtr c = clone(*e);
    ExprPtr s = simplify(*e);
    ZTH_EXPECT_EQ(node_count(*c), std::size_t{2001});
    ZTH_EXPECT_EQ(node_count(*s), std::size_t{1});
}
