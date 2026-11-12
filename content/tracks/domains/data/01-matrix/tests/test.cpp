#define ZTH_TRACK_ALLOCATIONS
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "matrix.hpp"
#include "zth_test.hpp"

using M = mat::Matrix<long long>;
using V = mat::View<const long long>;
using Grid = std::vector<std::vector<long long>>;

static_assert(std::is_convertible_v<mat::View<int>, mat::View<const int>>, "a View<T> must convert to a View<const T>");
static_assert(!std::is_convertible_v<mat::View<const int>, mat::View<int>>, "a View<const T> must NOT convert to a View<T>");

namespace {

constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();

std::size_t rnd(zth::Lcg& r, int n) { return static_cast<std::size_t>(r.below(n)); }

// ---- a cache model: 64 lines of 64 bytes, fully associative, LRU. Cell::operator* reports both operands to it.
struct Cache {
    std::array<std::uintptr_t, 64> lines{};
    std::size_t used = 0;
    std::uint64_t misses = 0;
    bool on = false;
    void touch(const void* p) {
        if (!on) return;
        const std::uintptr_t line = reinterpret_cast<std::uintptr_t>(p) >> 6;
        for (std::size_t i = 0; i < used; ++i) {
            if (lines[i] != line) continue;
            for (std::size_t j = i; j > 0; --j) lines[j] = lines[j - 1];
            lines[0] = line;
            return;
        }
        ++misses;
        if (used < lines.size()) ++used;
        for (std::size_t j = used - 1; j > 0; --j) lines[j] = lines[j - 1];
        lines[0] = line;
    }
    void reset() {
        used = 0;
        misses = 0;
    }
};
Cache g_cache;

struct Cell {
    int v = 0;
    static inline std::uint64_t muls = 0;
    Cell() = default;
    Cell(int x) : v(x) {}
    Cell& operator+=(const Cell& o) {
        v += o.v;
        return *this;
    }
    friend Cell operator*(const Cell& a, const Cell& b) {
        ++muls;
        g_cache.touch(&a);
        g_cache.touch(&b);
        return Cell(a.v * b.v);
    }
};

mat::Matrix<Cell> cells(std::size_t r, std::size_t c, int salt) {
    mat::Matrix<Cell> m(r, c);
    for (std::size_t i = 0; i < r; ++i)
        for (std::size_t j = 0; j < c; ++j) m(i, j) = Cell(static_cast<int>((i * 7 + j * 3 + static_cast<std::size_t>(salt)) % 5));
    return m;
}

// A random operand of the logical shape rows x cols, cut out of a larger storage and maybe transposed: strides that are not 1 and not the width.
struct Operand {
    std::unique_ptr<M> store;
    V view;
    Grid grid;
};
Operand make_operand(zth::Lcg& rng, std::size_t rows, std::size_t cols) {
    const bool tr = rng.below(2) == 1;
    const std::size_t wr = tr ? cols : rows, wc = tr ? rows : cols;
    const std::size_t pr = rnd(rng, 3), pc = rnd(rng, 3);
    const std::size_t R = wr + pr + rnd(rng, 3), Cc = wc + pc + rnd(rng, 3);
    Operand o;
    o.store = std::make_unique<M>(R, Cc);
    Grid full(R, std::vector<long long>(Cc, 0));
    for (std::size_t i = 0; i < R; ++i)
        for (std::size_t j = 0; j < Cc; ++j) {
            full[i][j] = static_cast<long long>(rng.below(7)) - 3;
            (*o.store)(i, j) = full[i][j];
        }
    const auto w = o.store->view().block(pr, pc, wr, wc);
    if (!w) {
        zth::fail("block() refused a window that lies inside the view");
        return o;
    }
    o.view = tr ? w->transposed() : *w;
    o.grid.assign(rows, std::vector<long long>(cols, 0));
    for (std::size_t i = 0; i < rows; ++i)
        for (std::size_t j = 0; j < cols; ++j) o.grid[i][j] = tr ? full[pr + j][pc + i] : full[pr + i][pc + j];
    return o;
}

bool same(const M& m, const Grid& g, std::size_t rows, std::size_t cols) {
    if (m.rows() != rows || m.cols() != cols) return false;
    for (std::size_t i = 0; i < rows; ++i)
        for (std::size_t j = 0; j < cols; ++j)
            if (m(i, j) != g[i][j]) return false;
    return true;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a matrix is row-major; its view reports the shape and the strides, and elements are found at r * row_stride + c * col_stride") {
    M m(2, 3);
    for (std::size_t r = 0; r < 2; ++r)
        for (std::size_t c = 0; c < 3; ++c) m(r, c) = static_cast<long long>(r * 10 + c);
    const V v = m.view();
    ZTH_EXPECT_EQ(v.rows(), std::size_t{2});
    ZTH_EXPECT_EQ(v.cols(), std::size_t{3});
    ZTH_EXPECT_EQ(v.row_stride(), std::size_t{3});
    ZTH_EXPECT_EQ(v.col_stride(), std::size_t{1});
    ZTH_EXPECT_EQ(v(1, 2), 12LL);
    ZTH_EXPECT_EQ(v(0, 1), 1LL);
    ZTH_EXPECT_TRUE(&v(1, 0) == &m(1, 0));   // a view, not a copy
    mat::View<long long> w = m.mview();
    w(0, 0) = 99;
    ZTH_EXPECT_EQ(m(0, 0), 99LL);
}
ZTH_TEST("works", "transposed() swaps the shape AND the strides and shows the same elements") {
    M m(2, 3);
    for (std::size_t r = 0; r < 2; ++r)
        for (std::size_t c = 0; c < 3; ++c) m(r, c) = static_cast<long long>(r * 10 + c);
    const V t = m.view().transposed();
    ZTH_EXPECT_EQ(t.rows(), std::size_t{3});
    ZTH_EXPECT_EQ(t.cols(), std::size_t{2});
    ZTH_EXPECT_EQ(t.row_stride(), std::size_t{1});
    ZTH_EXPECT_EQ(t.col_stride(), std::size_t{3});
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 2; ++c) ZTH_EXPECT_EQ(t(r, c), m(c, r));
    ZTH_EXPECT_TRUE(&t(2, 1) == &m(1, 2));
    const V back = t.transposed();
    ZTH_EXPECT_EQ(back.rows(), std::size_t{2});
    ZTH_EXPECT_TRUE(back.data() == m.view().data() && back.row_stride() == 3 && back.col_stride() == 1);
    mat::View<long long> mw = m.mview().transposed();   // writes go through to the matrix
    mw(2, 0) = -5;
    ZTH_EXPECT_EQ(m(0, 2), -5LL);
}
ZTH_TEST("works", "block() cuts a window, keeps the strides, nests, and works on a transposed view") {
    M m(4, 5);
    for (std::size_t r = 0; r < 4; ++r)
        for (std::size_t c = 0; c < 5; ++c) m(r, c) = static_cast<long long>(r * 10 + c);
    const auto b = m.view().block(1, 2, 2, 3);
    ZTH_EXPECT_TRUE(b.has_value());
    ZTH_EXPECT_EQ(b->rows(), std::size_t{2});
    ZTH_EXPECT_EQ(b->cols(), std::size_t{3});
    ZTH_EXPECT_EQ(b->row_stride(), std::size_t{5});   // the stride of the parent, not of the window
    ZTH_EXPECT_EQ((*b)(0, 0), 12LL);
    ZTH_EXPECT_EQ((*b)(1, 2), 24LL);
    const auto inner = b->block(1, 1, 1, 2);
    ZTH_EXPECT_TRUE(inner.has_value());
    ZTH_EXPECT_EQ((*inner)(0, 0), 23LL);
    ZTH_EXPECT_EQ((*inner)(0, 1), 24LL);
    const auto bt = m.view().transposed().block(2, 1, 3, 2);   // rows 2..4 of the 5x4 transpose, columns 1..2
    ZTH_EXPECT_TRUE(bt.has_value());
    ZTH_EXPECT_EQ((*bt)(0, 0), m(1, 2));
    ZTH_EXPECT_EQ((*bt)(2, 1), m(2, 4));
    ZTH_EXPECT_TRUE(m.view().block(0, 0, 4, 5).has_value());   // the whole matrix
}
ZTH_TEST("works", "block() refuses windows that are not completely inside, and accepts empty windows on the edge") {
    M m(4, 5);
    const V v = m.view();
    ZTH_EXPECT_FALSE(v.block(0, 0, 5, 5).has_value());
    ZTH_EXPECT_FALSE(v.block(0, 0, 4, 6).has_value());
    ZTH_EXPECT_FALSE(v.block(3, 0, 2, 5).has_value());
    ZTH_EXPECT_FALSE(v.block(0, 4, 4, 2).has_value());
    ZTH_EXPECT_FALSE(v.block(4, 0, 1, 1).has_value());
    ZTH_EXPECT_FALSE(v.block(5, 0, 0, 0).has_value());   // the corner is outside
    ZTH_EXPECT_FALSE(v.block(0, 6, 0, 0).has_value());
    const auto e1 = v.block(4, 0, 0, 5);   // an empty window exactly on the far edge
    ZTH_EXPECT_TRUE(e1.has_value() && e1->rows() == 0 && e1->cols() == 5);
    const auto e2 = v.block(2, 5, 2, 0);
    ZTH_EXPECT_TRUE(e2.has_value() && e2->rows() == 2 && e2->cols() == 0);
    ZTH_EXPECT_TRUE(v.block(3, 4, 1, 1).has_value());   // the last element
}
ZTH_TEST("works", "from_rows and identity") {
    const auto ok = M::from_rows({{1, 2, 3}, {4, 5, 6}});
    ZTH_EXPECT_TRUE(ok.has_value());
    ZTH_EXPECT_EQ(ok->rows(), std::size_t{2});
    ZTH_EXPECT_EQ(ok->cols(), std::size_t{3});
    ZTH_EXPECT_EQ((*ok)(1, 0), 4LL);
    ZTH_EXPECT_FALSE(M::from_rows({{1, 2, 3}, {4, 5}}).has_value());
    ZTH_EXPECT_FALSE(M::from_rows({{1}, {2, 3}}).has_value());
    ZTH_EXPECT_FALSE(M::from_rows({{}, {1}}).has_value());
    const auto empty = M::from_rows({});
    ZTH_EXPECT_TRUE(empty.has_value() && empty->rows() == 0 && empty->cols() == 0);
    const M id = M::identity(3);
    for (std::size_t r = 0; r < 3; ++r)
        for (std::size_t c = 0; c < 3; ++c) ZTH_EXPECT_EQ(id(r, c), r == c ? 1LL : 0LL);
    ZTH_EXPECT_EQ(M::identity(0).rows(), std::size_t{0});
}
ZTH_TEST("works", "to_matrix makes an independent deep copy of whatever a view shows") {
    M m(2, 3);
    for (std::size_t r = 0; r < 2; ++r)
        for (std::size_t c = 0; c < 3; ++c) m(r, c) = static_cast<long long>(r * 10 + c);
    M t = mat::to_matrix(m.view().transposed());
    ZTH_EXPECT_EQ(t.rows(), std::size_t{3});
    ZTH_EXPECT_EQ(t.cols(), std::size_t{2});
    ZTH_EXPECT_EQ(t(2, 1), 12LL);
    ZTH_EXPECT_EQ(t(1, 0), 1LL);
    t(0, 0) = 777;
    ZTH_EXPECT_EQ(m(0, 0), 0LL);   // the original is untouched
    const auto b = m.view().block(0, 1, 2, 2);
    ZTH_EXPECT_TRUE(b.has_value());
    const M c = mat::to_matrix(*b);
    ZTH_EXPECT_TRUE(c.rows() == 2 && c.cols() == 2 && c(0, 0) == 1 && c(1, 1) == 12);
}
ZTH_TEST("works", "multiply: a known product, identity, transposes, and a shape mismatch") {
    const auto a = M::from_rows({{1, 2, 3}, {4, 5, 6}});
    const auto b = M::from_rows({{7, 8}, {9, 10}, {11, 12}});
    ZTH_EXPECT_TRUE(a.has_value() && b.has_value());
    const auto p = mat::multiply(a->view(), b->view());
    ZTH_EXPECT_TRUE(p.has_value());
    ZTH_EXPECT_TRUE(p->rows() == 2 && p->cols() == 2);
    ZTH_EXPECT_TRUE((*p)(0, 0) == 58 && (*p)(0, 1) == 64 && (*p)(1, 0) == 139 && (*p)(1, 1) == 154);
    const auto aat = mat::multiply(a->view(), a->view().transposed());   // 2x3 * 3x2
    ZTH_EXPECT_TRUE(aat.has_value() && aat->rows() == 2 && (*aat)(0, 0) == 14 && (*aat)(0, 1) == 32 && (*aat)(1, 0) == 32 && (*aat)(1, 1) == 77);
    const auto ata = mat::multiply(a->view().transposed(), a->view());   // 3x2 * 2x3
    ZTH_EXPECT_TRUE(ata.has_value() && ata->rows() == 3 && ata->cols() == 3 && (*ata)(0, 0) == 17 && (*ata)(2, 2) == 45 && (*ata)(0, 2) == 27);
    const M id = M::identity(3);
    const auto same_a = mat::multiply(a->view(), id.view());
    ZTH_EXPECT_TRUE(same_a.has_value() && same_a->rows() == 2 && same_a->cols() == 3 && (*same_a)(1, 2) == 6);
    ZTH_EXPECT_FALSE(mat::multiply(a->view(), a->view()).has_value());   // 2x3 * 2x3
    ZTH_EXPECT_FALSE(mat::multiply(b->view(), b->view()).has_value());
}
ZTH_TEST("works", "multiply_blocked gives the same product for any block size, and refuses block == 0 and mismatched shapes") {
    const auto a = M::from_rows({{1, 2, 3}, {4, 5, 6}});
    const auto b = M::from_rows({{7, 8}, {9, 10}, {11, 12}});
    ZTH_EXPECT_TRUE(a.has_value() && b.has_value());
    for (std::size_t block : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{100}}) {
        const auto p = mat::multiply_blocked(a->view(), b->view(), block);
        ZTH_EXPECT_TRUE(p.has_value());
        ZTH_EXPECT_TRUE(p->rows() == 2 && p->cols() == 2);
        ZTH_EXPECT_TRUE((*p)(0, 0) == 58 && (*p)(0, 1) == 64 && (*p)(1, 0) == 139 && (*p)(1, 1) == 154);
    }
    ZTH_EXPECT_FALSE(mat::multiply_blocked(a->view(), b->view(), 0).has_value());
    ZTH_EXPECT_FALSE(mat::multiply_blocked(a->view(), a->view(), 2).has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "differential test: random shapes, strided/transposed/cut-out operands, many block sizes, against an independent reference") {
    zth::Lcg rng(2024);
    const std::size_t blocks[] = {1, 2, 3, 4, 5, 7, 8, 16, 1000, kMax / 2, kMax - 1, kMax};
    for (int round = 0; round < 400; ++round) {
        const std::size_t n = rnd(rng, 9), k = rnd(rng, 9), m = rnd(rng, 9);
        Operand a = make_operand(rng, n, k), b = make_operand(rng, k, m);
        Grid want(n, std::vector<long long>(m, 0));
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < m; ++j)
                for (std::size_t x = 0; x < k; ++x) want[i][j] += a.grid[i][x] * b.grid[x][j];
        const auto p = mat::multiply(a.view, b.view);
        ZTH_EXPECT_TRUE(p.has_value() && same(*p, want, n, m));
        for (std::size_t block : blocks) {
            const auto q = mat::multiply_blocked(a.view, b.view, block);
            ZTH_EXPECT_TRUE(q.has_value() && same(*q, want, n, m));
        }
        const M copy = mat::to_matrix(a.view);
        ZTH_EXPECT_TRUE(same(copy, a.grid, n, k));
    }
}
ZTH_TEST("robust", "blocked and naive multiply add the terms in the same order: doubles are bit-identical") {
    zth::Lcg rng(5);
    for (int round = 0; round < 60; ++round) {
        const std::size_t n = 1 + rnd(rng, 12), k = 1 + rnd(rng, 12), m = 1 + rnd(rng, 12);
        mat::Matrix<double> a(n, k), b(k, m);
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < k; ++j) a(i, j) = static_cast<double>(rng.below(2000) - 1000) * (rng.below(2) ? 1e-3 : 1e7) + 0.1;
        for (std::size_t i = 0; i < k; ++i)
            for (std::size_t j = 0; j < m; ++j) b(i, j) = static_cast<double>(rng.below(2000) - 1000) * (rng.below(2) ? 1e-9 : 1e5) - 0.3;
        const auto p = mat::multiply(a.view(), b.view());
        ZTH_EXPECT_TRUE(p.has_value());
        for (std::size_t block : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{5}, std::size_t{64}}) {
            const auto q = mat::multiply_blocked(a.view(), b.view(), block);
            ZTH_EXPECT_TRUE(q.has_value());
            bool equal = q.has_value() && p.has_value();
            for (std::size_t i = 0; equal && i < n; ++i)
                for (std::size_t j = 0; j < m; ++j)
                    if (!((*p)(i, j) == (*q)(i, j))) {
                        equal = false;
                        break;
                    }
            ZTH_EXPECT_TRUE(equal);
        }
    }
}
ZTH_TEST("robust", "zero-sized matrices: 0 x N, N x 0 and the empty inner dimension give correctly shaped results (all zeros)") {
    const M none(0, 4), tall(4, 0), wide(0, 0);
    const auto p = mat::multiply(tall.view(), none.view());   // 4x0 * 0x4 = 4x4 of zeros
    ZTH_EXPECT_TRUE(p.has_value() && p->rows() == 4 && p->cols() == 4);
    bool zeros = p.has_value();
    for (std::size_t i = 0; zeros && i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) zeros = zeros && (*p)(i, j) == 0;
    ZTH_EXPECT_TRUE(zeros);
    const auto q = mat::multiply(none.view(), tall.view());   // 0x4 * 4x0 = 0x0
    ZTH_EXPECT_TRUE(q.has_value() && q->rows() == 0 && q->cols() == 0);
    const auto q2 = mat::multiply_blocked(tall.view(), none.view(), 3);
    ZTH_EXPECT_TRUE(q2.has_value() && q2->rows() == 4 && q2->cols() == 4 && (*q2)(3, 3) == 0);
    ZTH_EXPECT_TRUE(mat::multiply(wide.view(), wide.view()).has_value());
    ZTH_EXPECT_FALSE(mat::multiply(none.view(), none.view()).has_value());   // 0x4 * 0x4
    ZTH_EXPECT_TRUE(mat::to_matrix(none.view().transposed()).rows() == 4);
}
ZTH_TEST("robust", "block() does not overflow: huge origins and sizes are refused, never wrapped around") {
    M m(4, 5);
    const V v = m.view();
    ZTH_EXPECT_FALSE(v.block(1, 0, kMax, 1).has_value());   // 1 + SIZE_MAX wraps to 0
    ZTH_EXPECT_FALSE(v.block(0, 1, 1, kMax).has_value());
    ZTH_EXPECT_FALSE(v.block(kMax, 0, 2, 1).has_value());
    ZTH_EXPECT_FALSE(v.block(2, 2, kMax - 1, 1).has_value());
    ZTH_EXPECT_FALSE(v.block(kMax, kMax, 0, 0).has_value());
    ZTH_EXPECT_FALSE(v.block(3, 3, kMax - 2, kMax - 2).has_value());
    ZTH_EXPECT_TRUE(v.block(4, 5, 0, 0).has_value());
}
ZTH_TEST("robust", "views of views of transposes keep addressing the right element, also at the far corner") {
    zth::Lcg rng(31);
    M m(9, 11);
    Grid g(9, std::vector<long long>(11));
    for (std::size_t i = 0; i < 9; ++i)
        for (std::size_t j = 0; j < 11; ++j) {
            g[i][j] = static_cast<long long>(rng.below(1000));
            m(i, j) = g[i][j];
        }
    const auto t = m.view().transposed();                  // 11 x 9
    const auto b1 = t.block(2, 1, 8, 7);                   // 8 x 7 : rows 2..9, columns 1..7 of the transpose
    ZTH_EXPECT_TRUE(b1.has_value());
    const auto b2 = b1->transposed().block(1, 2, 5, 6);    // 7x8 -> 5 x 6 : rows 1..5, columns 2..7
    ZTH_EXPECT_TRUE(b2.has_value());
    for (std::size_t i = 0; i < 5; ++i)
        for (std::size_t j = 0; j < 6; ++j) {
            // b2(i, j) = b1^T(1 + i, 2 + j) = b1(2 + j, 1 + i) = t(2 + 2 + j, 1 + 1 + i) = m(2 + i, 4 + j)
            ZTH_EXPECT_EQ((*b2)(i, j), g[2 + i][4 + j]);
        }
    const auto last = m.view().block(8, 10, 1, 1);
    ZTH_EXPECT_TRUE(last.has_value() && (*last)(0, 0) == g[8][10]);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "views, transposes and blocks allocate nothing; a product allocates exactly its result") {
    const M a(40, 30, 2), b(30, 50, 3);
    zth::mem::Scope scope;
    std::size_t sum = 0;
    for (int round = 0; round < 200; ++round) {
        const V t = a.view().transposed();
        const auto blk = t.block(1, 2, 20, 10);
        const auto blk2 = blk->transposed().block(0, 0, 5, 5);
        sum += t.rows() + blk->cols() + blk2->rows();
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sum > 0);
    zth::mem::Scope s1;
    const auto p = mat::multiply(a.view(), b.view());
    ZTH_EXPECT_ALLOCS_AT_MOST(s1, 1);
    zth::mem::Scope s2;
    const auto q = mat::multiply_blocked(a.view(), b.view(), 8);
    ZTH_EXPECT_ALLOCS_AT_MOST(s2, 1);
    zth::mem::Scope s3;
    const auto r = mat::multiply(a.view().transposed(), a.view());   // a transposed operand must not be copied first
    ZTH_EXPECT_ALLOCS_AT_MOST(s3, 1);
    ZTH_EXPECT_TRUE(p.has_value() && q.has_value() && r.has_value());
    ZTH_EXPECT_TRUE((*p)(5, 5) == 180 && (*q)(5, 5) == 180 && (*r)(0, 0) == 160);   // 30 * 2 * 3 and 40 * 2 * 2
}
ZTH_TEST("fast", "both multiplies do exactly rows * inner * cols multiplications: no tile is done twice") {
    const auto a = cells(7, 5, 1), b = cells(5, 9, 2);
    Cell::muls = 0;
    const auto p = mat::multiply(a.view(), b.view());
    ZTH_EXPECT_EQ(Cell::muls, std::uint64_t{7 * 5 * 9});
    for (std::size_t block : {std::size_t{1}, std::size_t{2}, std::size_t{4}, std::size_t{100}}) {
        Cell::muls = 0;
        const auto q = mat::multiply_blocked(a.view(), b.view(), block);
        ZTH_EXPECT_EQ(Cell::muls, std::uint64_t{7 * 5 * 9});
        bool equal = p.has_value() && q.has_value();
        for (std::size_t i = 0; equal && i < 7; ++i)
            for (std::size_t j = 0; j < 9; ++j)
                if ((*p)(i, j).v != (*q)(i, j).v) equal = false;
        ZTH_EXPECT_TRUE(equal);
    }
}
ZTH_TEST_TIMEOUT("fast", "blocked multiplication touches far fewer cache lines than the textbook triple loop (modelled L1: 64 lines of 64 bytes, LRU)", 30) {
    const std::size_t n = 96;
    const auto a = cells(n, n, 1), b = cells(n, n, 2);
    // the reference: i, j, k with a column walk through b
    g_cache.reset();
    g_cache.on = true;
    long long checksum = 0;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            Cell acc;
            for (std::size_t k = 0; k < n; ++k) acc += a.view()(i, k) * b.view()(k, j);
            checksum += acc.v;
        }
    g_cache.on = false;
    const std::uint64_t naive = g_cache.misses;
    g_cache.reset();
    g_cache.on = true;
    const auto q = mat::multiply_blocked(a.view(), b.view(), 8);
    g_cache.on = false;
    const std::uint64_t blocked = g_cache.misses;
    long long checksum2 = 0;
    for (std::size_t i = 0; q && i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) checksum2 += (*q)(i, j).v;
    ZTH_EXPECT_EQ(checksum, checksum2);
    ZTH_EXPECT_TRUE(naive > 200000);   // the model really does punish a column walk
    ZTH_EXPECT_TRUE(blocked * 4 < naive);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no C memory functions or casts that remove const") {
    ZTH_EXPECT_NO_RAW_MEMORY("matrix.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("matrix.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "memcpy", "memmove", "memset", "const_cast", "reinterpret_cast", "assert"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a transposed view is made by swapping strides, never by building a new Matrix; blocks never copy either") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("matrix.hpp"));
    const std::string view_part = code.substr(0, code.find("class Matrix"));
    for (const char* word : {"Matrix", "new", "copy", "to_matrix"}) ZTH_EXPECT_TRUE(::zth::find_word(view_part, word).empty());
}
