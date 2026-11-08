#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "grid.hpp"
#include "zth_test.hpp"

namespace {

using std::size_t;

// ---- What can be done with a `const G&`? Each is false (not a compile error in this hidden file) if the class is not const-correct.
template <class G> constexpr bool c_rows = requires(const G& g) { g.rows(); };
template <class G> constexpr bool c_cols = requires(const G& g) { g.cols(); };
template <class G> constexpr bool c_at = requires(const G& g) { g.at(size_t{0}, size_t{0}); };
template <class G> constexpr bool c_total = requires(const G& g) { g.total(); };
template <class G> constexpr bool c_count = requires(const G& g) { g.count(0); };
template <class G> constexpr bool c_row_text = requires(const G& g) { g.row_text(size_t{0}); };
template <class G> constexpr bool c_recomputations = requires(const G& g) { g.recomputations(); };
template <class G>
constexpr bool all_const_reads = c_rows<G> && c_cols<G> && c_at<G> && c_total<G> && c_count<G> && c_row_text<G> && c_recomputations<G>;

template <class G> constexpr bool const_at_is_read_only = !requires(const G& g) { g.at(size_t{0}, size_t{0}) = 1; };
template <class G> constexpr bool at_is_writable = requires(G& g) { g.at(size_t{0}, size_t{0}) = 1; };
// the exact signature of row_sum: a const reference, not a copy and not a non-const reference
template <class G> constexpr bool row_sum_takes_const_ref = requires { static_cast<long long (*)(const G&, size_t)>(&row_sum); };

template <class G>
std::string missing_const() {
    std::string s;
    auto add = [&](bool ok, const char* name) {
        if (!ok) s += (s.empty() ? "" : ", ") + std::string(name);
    };
    add(c_rows<G>, "rows()");
    add(c_cols<G>, "cols()");
    add(c_at<G>, "at(r, c) (needs a const overload)");
    add(c_total<G>, "total()");
    add(c_count<G>, "count(v)");
    add(c_row_text<G>, "row_text(r)");
    add(c_recomputations<G>, "recomputations()");
    return s;
}

template <class G>
void const_reads_test() {
    if constexpr (!all_const_reads<G>) {
        ::zth::fail("these cannot be called on a const Grid: " + missing_const<G>() + ". Mark them const (and give at() a const overload).");
    } else {
        G g(2, 3, 1);
        g.at(1, 2) = 10;
        const G& cg = g;
        ZTH_EXPECT_EQ(cg.rows(), size_t{2});
        ZTH_EXPECT_EQ(cg.cols(), size_t{3});
        ZTH_EXPECT_EQ(cg.at(1, 2), 10);
        ZTH_EXPECT_EQ(cg.at(0, 0), 1);
        ZTH_EXPECT_EQ(cg.total(), 15LL);
        ZTH_EXPECT_EQ(cg.count(1), size_t{5});
        ZTH_EXPECT_EQ(cg.row_text(1), std::string("1 1 10"));
    }
}

template <class G>
void const_is_read_only_test() {
    if constexpr (!c_at<G>) {
        ::zth::fail("at() has no const overload, so a const Grid cannot be read at all");
    } else if constexpr (!const_at_is_read_only<G>) {
        ::zth::fail("at() on a const Grid returns something you can assign to: the const overload must return a const reference (or a copy)");
    } else if constexpr (!at_is_writable<G>) {
        ::zth::fail("at() on a non-const Grid must still return a reference you can assign to");
    } else {
        ZTH_EXPECT_TRUE(true);
    }
}

template <class G>
void row_sum_test() {
    if constexpr (!row_sum_takes_const_ref<G>) {
        ::zth::fail("row_sum must be declared `long long row_sum(const Grid&, std::size_t)`: a const reference, not a copy of the grid and not a non-const reference");
    } else {
        G g(2, 3, 1);
        g.at(1, 0) = 5;
        g.at(1, 2) = 10;
        const G& cg = g;
        ZTH_EXPECT_EQ(row_sum(cg, 0), 3LL);
        ZTH_EXPECT_EQ(row_sum(cg, 1), 16LL);
    }
}

template <class G>
void const_total_cached_test() {
    if constexpr (!(c_total<G> && c_recomputations<G>)) {
        ::zth::fail("total() and recomputations() must be callable on a const Grid");
    } else {
        G g(3, 3, 2);
        const G& cg = g;
        ZTH_EXPECT_EQ(cg.total(), 18LL);
        ZTH_EXPECT_EQ(cg.total(), 18LL);
        ZTH_EXPECT_EQ(cg.total(), 18LL);
        ZTH_EXPECT_EQ(cg.recomputations(), size_t{1});
    }
}

template <class G>
void const_at_keeps_cache_test() {
    if constexpr (!(c_at<G> && c_total<G> && c_recomputations<G>)) {
        ::zth::fail("at() (const overload), total() and recomputations() must be callable on a const Grid");
    } else {
        G g(2, 2, 3);
        const G& cg = g;
        ZTH_EXPECT_EQ(cg.total(), 12LL);
        ZTH_EXPECT_EQ(cg.at(0, 1), 3);
        ZTH_EXPECT_EQ(cg.at(1, 1), 3);
        ZTH_EXPECT_EQ(cg.total(), 12LL);
        ZTH_EXPECT_EQ(cg.recomputations(), size_t{1});  // only reading: the cached total is still valid
    }
}

template <class G>
void const_bounds_test() {
    if constexpr (!c_at<G>) {
        ::zth::fail("at() has no const overload");
    } else {
        const G g(2, 3, 0);
        ZTH_EXPECT_THROWS(std::out_of_range, (void)g.at(0, 3));
        ZTH_EXPECT_THROWS(std::out_of_range, (void)g.at(2, 0));
        ZTH_EXPECT_NO_THROW((void)g.at(1, 2));
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a new Grid has the requested size and every cell holds the fill value") {
    Grid g(2, 3, 7);
    ZTH_EXPECT_EQ(g.rows(), size_t{2});
    ZTH_EXPECT_EQ(g.cols(), size_t{3});
    for (size_t r = 0; r < 2; ++r)
        for (size_t c = 0; c < 3; ++c) ZTH_EXPECT_EQ(g.at(r, c), 7);
    Grid z(1, 1);
    ZTH_EXPECT_EQ(z.at(0, 0), 0);
}
ZTH_TEST("works", "at() on a Grid you can change returns a reference you can assign to") {
    Grid g(2, 2);
    g.at(1, 0) = 42;
    ZTH_EXPECT_EQ(g.at(1, 0), 42);
    ZTH_EXPECT_EQ(g.at(0, 0), 0);
    g.at(1, 0) += 8;
    ZTH_EXPECT_EQ(g.at(1, 0), 50);
}
ZTH_TEST("works", "fill, count, total and row_text give the right answers") {
    Grid g(2, 3, 1);
    g.at(0, 1) = -4;
    g.at(1, 2) = 10;
    ZTH_EXPECT_EQ(g.total(), 10LL);
    ZTH_EXPECT_EQ(g.count(1), size_t{4});
    ZTH_EXPECT_EQ(g.count(-4), size_t{1});
    ZTH_EXPECT_EQ(g.count(99), size_t{0});
    ZTH_EXPECT_EQ(g.row_text(0), std::string("1 -4 1"));
    ZTH_EXPECT_EQ(g.row_text(1), std::string("1 1 10"));
    g.fill(2);
    ZTH_EXPECT_EQ(g.total(), 12LL);
    ZTH_EXPECT_EQ(g.count(2), size_t{6});
}
ZTH_TEST("works", "a const Grid can be read: rows, cols, at, total, count, row_text and recomputations are const member functions") {
    const_reads_test<Grid>();
}
ZTH_TEST("works", "the const overload of at() is read-only, the other one still writes") { const_is_read_only_test<Grid>(); }
ZTH_TEST("works", "row_sum takes the grid by const reference and adds up one row") { row_sum_test<Grid>(); }

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "a Grid needs at least one row and one column (std::invalid_argument)") {
    ZTH_EXPECT_THROWS(std::invalid_argument, Grid g(0, 3));
    ZTH_EXPECT_THROWS(std::invalid_argument, Grid g(3, 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, Grid g(0, 0));
    ZTH_EXPECT_NO_THROW(Grid g(1, 1));
}
ZTH_TEST("robust", "at() outside the grid throws std::out_of_range (row AND column are checked)") {
    Grid g(2, 3);
    ZTH_EXPECT_THROWS(std::out_of_range, g.at(0, 3) = 1);
    ZTH_EXPECT_THROWS(std::out_of_range, g.at(2, 0) = 1);
    ZTH_EXPECT_THROWS(std::out_of_range, g.at(5, 5) = 1);
    ZTH_EXPECT_NO_THROW(g.at(1, 2) = 1);
    ZTH_EXPECT_THROWS(std::out_of_range, (void)g.row_text(2));
}
ZTH_TEST("robust", "at() on a const Grid also checks its bounds") { const_bounds_test<Grid>(); }
ZTH_TEST("robust", "total() is cached: asking again does not add the cells up again") {
    Grid g(3, 3, 2);
    ZTH_EXPECT_EQ(g.recomputations(), size_t{0});
    ZTH_EXPECT_EQ(g.total(), 18LL);
    ZTH_EXPECT_EQ(g.total(), 18LL);
    ZTH_EXPECT_EQ(g.recomputations(), size_t{1});
}
ZTH_TEST("robust", "total() is cached on a const Grid too (this is what `mutable` is for)") { const_total_cached_test<Grid>(); }
ZTH_TEST("robust", "writing through at() throws the cached total away") {
    Grid g(2, 2, 1);
    ZTH_EXPECT_EQ(g.total(), 4LL);
    g.at(0, 0) = 100;
    ZTH_EXPECT_EQ(g.total(), 103LL);
    ZTH_EXPECT_EQ(g.recomputations(), size_t{2});
    g.at(1, 1) += 1;
    ZTH_EXPECT_EQ(g.total(), 104LL);
}
ZTH_TEST("robust", "fill() throws the cached total away") {
    Grid g(2, 2, 1);
    ZTH_EXPECT_EQ(g.total(), 4LL);
    g.fill(5);
    ZTH_EXPECT_EQ(g.total(), 20LL);
    ZTH_EXPECT_EQ(g.recomputations(), size_t{2});
}
ZTH_TEST("robust", "only reading (the const overload of at) keeps the cache") { const_at_keeps_cache_test<Grid>(); }
ZTH_TEST("robust", "total() does not overflow an int") {
    Grid g(1, 3, 2000000000);
    ZTH_EXPECT_EQ(g.total(), 6000000000LL);
}
ZTH_TEST("robust", "row_text has single spaces between the numbers and nothing before or after them") {
    Grid one(1, 1, 5);
    ZTH_EXPECT_EQ(one.row_text(0), std::string("5"));
    Grid g(1, 4, -1);
    ZTH_EXPECT_EQ(g.row_text(0), std::string("-1 -1 -1 -1"));
}
ZTH_TEST("robust", "a long random sequence of writes, fills and reads always agrees with a plain vector") {
    const size_t R = 4, C = 5;
    Grid g(R, C, 0);
    std::vector<int> model(R * C, 0);
    zth::Lcg rng(12345);
    auto next = [&] { return rng.next(); };
    for (int step = 0; step < 600; ++step) {
        unsigned op = next() % 5;
        size_t r = next() % R, c = next() % C;
        if (op == 0) {
            int v = static_cast<int>(next() % 100) - 50;
            g.at(r, c) = v;
            model[r * C + c] = v;
        } else if (op == 1 && next() % 8 == 0) {
            int v = static_cast<int>(next() % 7);
            g.fill(v);
            for (int& m : model) m = v;
        } else if (op == 2) {
            long long sum = 0;
            for (int m : model) sum += m;
            ZTH_EXPECT_EQ(g.total(), sum);
        } else if (op == 3) {
            int v = model[r * C + c];
            size_t n = 0;
            for (int m : model) n += (m == v);
            ZTH_EXPECT_EQ(g.count(v), n);
            ZTH_EXPECT_EQ(g.at(r, c), v);
        } else {
            long long sum = 0;
            for (size_t cc = 0; cc < C; ++cc) sum += model[r * C + cc];
            ZTH_EXPECT_EQ(row_sum(g, r), sum);
        }
    }
}
