#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstddef>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "tree.hpp"
#include "zth_test.hpp"

namespace {

using V = std::vector<int>;

template <class T>
constexpr bool const_queries = requires(const T& t) {
    t.contains(1);
    t.size();
    t.height();
    t.to_vector();
    t.clone();
};

template <class T>
void const_queries_test() {
    if constexpr (!const_queries<T>) {
        ::zth::fail("contains, size, height, to_vector and clone must be const member functions");
    } else {
        T t;
        t.insert(2);
        t.insert(1);
        const T& c = t;
        ZTH_EXPECT_TRUE(c.contains(1));
        ZTH_EXPECT_EQ(c.size(), std::size_t{2});
        ZTH_EXPECT_EQ(c.height(), 2);
        ZTH_EXPECT_EQ(c.to_vector(), (V{1, 2}));
        ZTH_EXPECT_EQ(c.clone().size(), std::size_t{2});
    }
}

Tree build(std::initializer_list<int> values) {
    Tree t;
    for (int v : values) t.insert(v);
    return t;
}

//        50
//      30    70
//    20 40  60 80
Tree sample() { return build({50, 30, 70, 20, 40, 60, 80}); }

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a new tree is empty") {
    Tree t;
    ZTH_EXPECT_EQ(t.size(), std::size_t{0});
    ZTH_EXPECT_EQ(t.height(), 0);
    ZTH_EXPECT_EQ(t.to_vector(), V{});
    ZTH_EXPECT_FALSE(t.contains(1));
}
ZTH_TEST("works", "insert adds values and says whether the value was new") {
    Tree t;
    ZTH_EXPECT_TRUE(t.insert(5));
    ZTH_EXPECT_TRUE(t.insert(3));
    ZTH_EXPECT_TRUE(t.insert(8));
    ZTH_EXPECT_FALSE(t.insert(5));
    ZTH_EXPECT_EQ(t.size(), std::size_t{3});
}
ZTH_TEST("works", "contains finds exactly the values that were inserted") {
    Tree t = sample();
    for (int v : {50, 30, 70, 20, 40, 60, 80}) ZTH_EXPECT_TRUE(t.contains(v));
    for (int v : {0, 10, 25, 45, 55, 65, 75, 90}) ZTH_EXPECT_FALSE(t.contains(v));
}
ZTH_TEST("works", "to_vector lists the values in ascending order, whatever the insertion order") {
    ZTH_EXPECT_EQ(sample().to_vector(), (V{20, 30, 40, 50, 60, 70, 80}));
    ZTH_EXPECT_EQ(build({3, 1, 2}).to_vector(), (V{1, 2, 3}));
    ZTH_EXPECT_EQ(build({1, 2, 3}).to_vector(), (V{1, 2, 3}));
    ZTH_EXPECT_EQ(build({-5, 0, 5, -10}).to_vector(), (V{-10, -5, 0, 5}));
}
ZTH_TEST("works", "height counts levels: 0 for empty, 1 for one node, a degenerate tree is as tall as it is long") {
    ZTH_EXPECT_EQ(Tree().height(), 0);
    ZTH_EXPECT_EQ(build({1}).height(), 1);
    ZTH_EXPECT_EQ(sample().height(), 3);
    ZTH_EXPECT_EQ(build({1, 2, 3, 4, 5}).height(), 5);
}
ZTH_TEST("works", "erase removes a leaf, a node with one child, and a node with two children") {
    Tree t = sample();
    ZTH_EXPECT_TRUE(t.erase(20));  // leaf
    ZTH_EXPECT_EQ(t.to_vector(), (V{30, 40, 50, 60, 70, 80}));
    ZTH_EXPECT_TRUE(t.erase(30));  // one child (40)
    ZTH_EXPECT_EQ(t.to_vector(), (V{40, 50, 60, 70, 80}));
    ZTH_EXPECT_TRUE(t.erase(70));  // two children (60, 80)
    ZTH_EXPECT_EQ(t.to_vector(), (V{40, 50, 60, 80}));
    ZTH_EXPECT_EQ(t.size(), std::size_t{4});
}
ZTH_TEST("works", "erase of a value that is not there returns false and changes nothing") {
    Tree t = sample();
    ZTH_EXPECT_FALSE(t.erase(45));
    ZTH_EXPECT_FALSE(t.erase(100));
    ZTH_EXPECT_EQ(t.to_vector(), (V{20, 30, 40, 50, 60, 70, 80}));
}
ZTH_TEST("works", "clone is an independent copy: changing one tree does not change the other") {
    Tree a = sample();
    Tree b = a.clone();
    ZTH_EXPECT_EQ(b.to_vector(), a.to_vector());
    b.insert(99);
    b.erase(20);
    ZTH_EXPECT_EQ(a.to_vector(), (V{20, 30, 40, 50, 60, 70, 80}));
    ZTH_EXPECT_EQ(b.to_vector(), (V{30, 40, 50, 60, 70, 80, 99}));
    a.erase(50);
    ZTH_EXPECT_TRUE(b.contains(50));
}
ZTH_TEST("works", "the whole tree is freed: building, erasing, cloning and destroying leaks nothing") {
    zth::mem::Scope scope;
    {
        Tree t = sample();
        t.erase(50);
        t.erase(20);
        Tree c = t.clone();
        c.insert(1);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "erase handles the root in every shape") {
    Tree only = build({1});
    ZTH_EXPECT_TRUE(only.erase(1));
    ZTH_EXPECT_EQ(only.size(), std::size_t{0});
    ZTH_EXPECT_EQ(only.height(), 0);
    ZTH_EXPECT_TRUE(only.insert(2));
    Tree left_only = build({5, 3, 1});
    ZTH_EXPECT_TRUE(left_only.erase(5));
    ZTH_EXPECT_EQ(left_only.to_vector(), (V{1, 3}));
    Tree right_only = build({1, 3, 5});
    ZTH_EXPECT_TRUE(right_only.erase(1));
    ZTH_EXPECT_EQ(right_only.to_vector(), (V{3, 5}));
    Tree both = sample();
    ZTH_EXPECT_TRUE(both.erase(50));
    ZTH_EXPECT_EQ(both.to_vector(), (V{20, 30, 40, 60, 70, 80}));
}
ZTH_TEST("robust", "erasing a node whose right subtree has a deep left spine keeps every value") {
    Tree t = build({10, 5, 20, 15, 30, 12, 18, 11});
    ZTH_EXPECT_TRUE(t.erase(10));
    ZTH_EXPECT_EQ(t.to_vector(), (V{5, 11, 12, 15, 18, 20, 30}));
    ZTH_EXPECT_TRUE(t.erase(15));
    ZTH_EXPECT_EQ(t.to_vector(), (V{5, 11, 12, 18, 20, 30}));
}
ZTH_TEST("robust", "erase leaks nothing in any of its cases") {
    zth::mem::Scope scope;
    {
        Tree t = build({10, 5, 20, 15, 30, 12, 18, 11, 3, 7});
        for (int v : {3, 20, 10, 5, 7, 999, 15, 11, 12, 18, 30}) t.erase(v);
        ZTH_EXPECT_EQ(t.size(), std::size_t{0});
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "erasing a node with only a left child, only a right child, or none, leaks nothing") {
    zth::mem::Scope scope;
    {
        Tree t = build({10, 5, 3, 20, 25});
        ZTH_EXPECT_TRUE(t.erase(5));   // only a left child
        ZTH_EXPECT_TRUE(t.erase(20));  // only a right child
        ZTH_EXPECT_TRUE(t.erase(3));   // a leaf
        ZTH_EXPECT_EQ(t.to_vector(), (V{10, 25}));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a long random sequence of inserts and erases always agrees with std::set") {
    Tree t;
    std::set<int> model;
    zth::Lcg rng(31337);
    auto next = [&](int mod) { return rng.below(mod); };
    for (int step = 0; step < 1500; ++step) {
        const int v = next(60) - 20;
        if (next(3) != 0) {
            ZTH_EXPECT_EQ(t.insert(v), model.insert(v).second);
        } else {
            ZTH_EXPECT_EQ(t.erase(v), model.erase(v) == 1);
        }
        if (step % 50 == 0) {
            ZTH_EXPECT_EQ(t.to_vector(), V(model.begin(), model.end()));
            ZTH_EXPECT_EQ(t.size(), model.size());
            ZTH_EXPECT_TRUE(t.height() <= static_cast<int>(model.size()));
        }
        ZTH_EXPECT_EQ(t.contains(v), model.count(v) == 1);
    }
    ZTH_EXPECT_EQ(t.to_vector(), V(model.begin(), model.end()));
}
ZTH_TEST("robust", "clone keeps the shape (same height), and works for an empty tree and a degenerate one") {
    Tree s = sample();
    ZTH_EXPECT_EQ(s.clone().height(), 3);
    Tree e;
    Tree ec = e.clone();
    ZTH_EXPECT_EQ(ec.size(), std::size_t{0});
    ec.insert(1);
    ZTH_EXPECT_EQ(e.size(), std::size_t{0});
    Tree line = build({1, 2, 3, 4, 5, 6});
    ZTH_EXPECT_EQ(line.clone().height(), 6);
}
ZTH_TEST("robust", "a Tree can be moved: the new tree has the values, and the moved-from tree is a valid EMPTY tree") {
    Tree a = sample();
    Tree b = std::move(a);
    ZTH_EXPECT_EQ(b.to_vector(), (V{20, 30, 40, 50, 60, 70, 80}));
    ZTH_EXPECT_EQ(a.size(), std::size_t{0});  // not the old count: a moved-from tree owns nothing
    ZTH_EXPECT_EQ(a.height(), 0);
    ZTH_EXPECT_EQ(a.to_vector(), V{});
    ZTH_EXPECT_FALSE(a.contains(50));
    ZTH_EXPECT_TRUE(a.insert(7));  // and it can be used again
    ZTH_EXPECT_EQ(a.to_vector(), (V{7}));
    Tree c;
    c = std::move(b);
    ZTH_EXPECT_EQ(c.size(), std::size_t{7});
    ZTH_EXPECT_EQ(b.size(), std::size_t{0});
}
ZTH_TEST("robust", "Tree follows the rule of zero: it is nothrow-movable, so a vector of trees moves them") {
    ZTH_EXPECT_TRUE((std::is_move_constructible_v<Tree>));
    ZTH_EXPECT_TRUE((std::is_nothrow_move_constructible_v<Tree>));
    ZTH_EXPECT_TRUE((std::is_nothrow_move_assignable_v<Tree>));
    ZTH_EXPECT_TRUE((std::is_nothrow_destructible_v<Tree>));
}
ZTH_TEST("robust", "trees in a vector, moved around, leak nothing") {
    zth::mem::Scope scope;
    {
        std::vector<Tree> forest;
        for (int i = 0; i < 20; ++i) {
            Tree t;
            for (int j = 0; j < 10; ++j) t.insert((i * 7 + j * 3) % 25);
            forest.push_back(std::move(t));
        }
        std::reverse(forest.begin(), forest.end());
        ZTH_EXPECT_EQ(forest.size(), std::size_t{20});
        ZTH_EXPECT_TRUE(forest[0].size() > 0);
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "a deep (degenerate) tree is built, copied and freed correctly") {
    zth::mem::Scope scope;
    {
        Tree t;
        for (int i = 0; i < 2000; ++i) t.insert(i);
        ZTH_EXPECT_EQ(t.height(), 2000);
        ZTH_EXPECT_EQ(t.size(), std::size_t{2000});
        Tree c = t.clone();
        ZTH_EXPECT_EQ(c.to_vector(), t.to_vector());
        for (int i = 0; i < 2000; i += 2) c.erase(i);
        ZTH_EXPECT_EQ(c.size(), std::size_t{1000});
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "everything that only looks works on a const Tree") { const_queries_test<Tree>(); }

// ---- Clean ---------------------------------------------------------------------------------------
ZTH_TEST("clean", "tree.hpp does not use new, delete, malloc or free: unique_ptr and make_unique do it all") {
    ZTH_EXPECT_NO_RAW_MEMORY("tree.hpp");
}
