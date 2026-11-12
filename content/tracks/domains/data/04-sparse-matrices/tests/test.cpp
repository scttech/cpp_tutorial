#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "sparse.hpp"
#include "zth_test.hpp"

using sparse::Csr;
using sparse::SparseVector;
using sparse::Triplet;
using Vec = std::vector<double>;
using Sizes = std::vector<std::size_t>;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();

Sizes sizes(std::span<const std::size_t> s) { return Sizes(s.begin(), s.end()); }
Vec doubles(std::span<const double> s) { return Vec(s.begin(), s.end()); }
double val(const std::optional<double>& o) { return o.value_or(kNan); }

// canonical form: row_ptr well formed, columns strictly increasing in a row and inside the matrix, no stored zero
bool canonical(const Csr& m) {
    const auto rp = m.row_ptr();
    if (rp.size() != m.rows() + 1 || rp[0] != 0 || rp[m.rows()] != m.nnz()) return false;
    if (m.col_idx().size() != m.nnz() || m.values().size() != m.nnz()) return false;
    for (std::size_t r = 0; r < m.rows(); ++r) {
        if (rp[r] > rp[r + 1]) return false;
        for (std::size_t k = rp[r]; k < rp[r + 1]; ++k) {
            if (m.col_idx()[k] >= m.cols()) return false;
            if (k > rp[r] && m.col_idx()[k] <= m.col_idx()[k - 1]) return false;
            if (m.values()[k] == 0.0) return false;
        }
    }
    return true;
}

struct Dense {
    std::size_t rows, cols;
    Vec a;
    Dense(std::size_t r, std::size_t c) : rows(r), cols(c), a(r * c, 0.0) {}
    double& at(std::size_t r, std::size_t c) { return a[r * cols + c]; }
    double at(std::size_t r, std::size_t c) const { return a[r * cols + c]; }
};

Dense random_dense(zth::Lcg& rng, std::size_t rows, std::size_t cols, int percent) {
    Dense d(rows, cols);
    for (std::size_t r = 0; r < rows; ++r)
        for (std::size_t c = 0; c < cols; ++c)
            if (rng.below(100) < percent) d.at(r, c) = static_cast<double>(rng.below(9) - 4) * (rng.below(3) == 0 ? 0.5 : 1.0);
    return d;
}

// triplets for a dense matrix, shuffled, with some entries split into two parts that sum to the value
std::vector<Triplet> triplets_of(zth::Lcg& rng, const Dense& d) {
    std::vector<Triplet> t;
    for (std::size_t r = 0; r < d.rows; ++r)
        for (std::size_t c = 0; c < d.cols; ++c) {
            const double v = d.at(r, c);
            if (v == 0.0) {
                if (rng.below(5) == 0) t.push_back({r, c, 0.0});   // an explicit zero
                continue;
            }
            if (rng.below(4) == 0) {
                t.push_back({r, c, v + 3.0});
                t.push_back({r, c, -3.0});
            } else {
                t.push_back({r, c, v});
            }
        }
    for (std::size_t i = t.size(); i > 1; --i) std::swap(t[i - 1], t[static_cast<std::size_t>(rng.below(static_cast<int>(i)))]);
    return t;
}

Csr build(std::size_t r, std::size_t c, std::vector<Triplet> t) {
    auto m = Csr::from_triplets(r, c, std::move(t));
    return m ? *m : Csr();
}

Vec dense_mul(const Dense& d, const Vec& x) {
    Vec y(d.rows, 0.0);
    for (std::size_t r = 0; r < d.rows; ++r) {
        double s = 0.0;
        for (std::size_t c = 0; c < d.cols; ++c)
            if (d.at(r, c) != 0.0) s += d.at(r, c) * x[c];   // ascending columns, skipping the zeros
        y[r] = s;
    }
    return y;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "from_triplets builds the three CSR arrays, with empty rows") {
    const auto m = Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    ZTH_EXPECT_TRUE(m.has_value());
    ZTH_EXPECT_EQ(m->rows(), std::size_t{4});
    ZTH_EXPECT_EQ(m->cols(), std::size_t{5});
    ZTH_EXPECT_EQ(m->nnz(), std::size_t{6});
    ZTH_EXPECT_TRUE((sizes(m->row_ptr()) == Sizes{0, 2, 2, 4, 6}));
    ZTH_EXPECT_TRUE((sizes(m->col_idx()) == Sizes{0, 2, 1, 3, 0, 4}));
    ZTH_EXPECT_TRUE((doubles(m->values()) == Vec{1, 2, 3, 4, 5, 6}));
}
ZTH_TEST("works", "triplets in any order give the same canonical arrays") {
    const auto m = Csr::from_triplets(3, 4, {{2, 3, 4}, {0, 2, 2}, {2, 1, 3}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(m.has_value());
    ZTH_EXPECT_TRUE((sizes(m->row_ptr()) == Sizes{0, 2, 2, 4}));
    ZTH_EXPECT_TRUE((sizes(m->col_idx()) == Sizes{0, 2, 1, 3}));
    ZTH_EXPECT_TRUE((doubles(m->values()) == Vec{1, 2, 3, 4}));
}
ZTH_TEST("works", "duplicates are summed; zeros and cancelled entries leave nothing behind; NaN stays") {
    const auto m = Csr::from_triplets(2, 3, {{0, 1, 2}, {0, 1, 3}, {1, 0, 5}, {1, 0, -5}, {1, 2, 0}, {0, 0, -0.0}, {1, 1, 7}});
    ZTH_EXPECT_TRUE(m.has_value());
    ZTH_EXPECT_TRUE((sizes(m->row_ptr()) == Sizes{0, 1, 2}));
    ZTH_EXPECT_TRUE((sizes(m->col_idx()) == Sizes{1, 1}));
    ZTH_EXPECT_TRUE((doubles(m->values()) == Vec{5, 7}));
    const auto n = Csr::from_triplets(1, 2, {{0, 0, kNan}, {0, 1, 1}});
    ZTH_EXPECT_TRUE(n.has_value() && n->nnz() == 2 && std::isnan(n->values()[0]));
}
ZTH_TEST("works", "duplicates are summed in the order they were given") {
    const auto a = Csr::from_triplets(1, 1, {{0, 0, 1e16}, {0, 0, 1.0}, {0, 0, -1e16}});   // (1e16 + 1) - 1e16 = 0 in doubles
    ZTH_EXPECT_TRUE(a.has_value());
    ZTH_EXPECT_EQ(a->nnz(), std::size_t{0});
    const auto b = Csr::from_triplets(1, 1, {{0, 0, 1e16}, {0, 0, -1e16}, {0, 0, 1.0}});   // 0 + 1 = 1
    ZTH_EXPECT_TRUE(b.has_value() && b->nnz() == 1 && b->values()[0] == 1.0);
    const auto c = Csr::from_triplets(2, 2, {{1, 1, 1.0}, {0, 0, 5}, {1, 1, 1e16}, {1, 1, -1e16}});   // other entries in between do not matter
    ZTH_EXPECT_TRUE(c.has_value() && c->at(1, 1).value_or(-1) == 0.0 && c->nnz() == 1);
}
ZTH_TEST("works", "from_triplets refuses what does not fit, and huge dimensions without allocating them") {
    ZTH_EXPECT_FALSE(Csr::from_triplets(2, 2, {{2, 0, 1}}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(2, 2, {{0, 2, 1}}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(0, 5, {{0, 0, 1}}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(std::numeric_limits<std::size_t>::max(), 3, {}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(3, std::size_t{1} << 40, {}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(sparse::kMaxDimension + 1, 1, {}).has_value());
    ZTH_EXPECT_FALSE(Csr::from_triplets(1, sparse::kMaxDimension + 1, {}).has_value());
    const auto e = Csr::from_triplets(0, 0, {});
    ZTH_EXPECT_TRUE(e.has_value() && e->nnz() == 0 && canonical(*e));
    const auto rows_only = Csr::from_triplets(3, 0, {});
    ZTH_EXPECT_TRUE(rows_only.has_value() && canonical(*rows_only) && rows_only->row_ptr().size() == 4);
    const auto cols_only = Csr::from_triplets(0, 3, {});
    ZTH_EXPECT_TRUE(cols_only.has_value() && canonical(*cols_only) && cols_only->row_ptr().size() == 1);
    const auto big = Csr::from_triplets(sparse::kMaxDimension, 5, {{sparse::kMaxDimension - 1, 4, 2.5}});   // the largest allowed
    ZTH_EXPECT_TRUE(big.has_value() && big->nnz() == 1 && big->at(sparse::kMaxDimension - 1, 4).value_or(0) == 2.5);
}
ZTH_TEST("works", "at(): stored entries, absent entries (0), and positions outside the matrix") {
    const auto m = Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    ZTH_EXPECT_TRUE(m.has_value());
    ZTH_EXPECT_NEAR(val(m->at(0, 0)), 1.0, 0);   // first of its row
    ZTH_EXPECT_NEAR(val(m->at(0, 2)), 2.0, 0);   // last of its row
    ZTH_EXPECT_NEAR(val(m->at(3, 4)), 6.0, 0);   // the very last stored
    ZTH_EXPECT_NEAR(val(m->at(2, 3)), 4.0, 0);
    ZTH_EXPECT_NEAR(val(m->at(0, 1)), 0.0, 0);   // absent
    ZTH_EXPECT_NEAR(val(m->at(0, 4)), 0.0, 0);   // beyond the last stored column of the row
    ZTH_EXPECT_NEAR(val(m->at(1, 3)), 0.0, 0);   // an empty row
    ZTH_EXPECT_NEAR(val(m->at(2, 0)), 0.0, 0);   // before the first stored column
    ZTH_EXPECT_TRUE(m->at(1, 3).has_value());
    ZTH_EXPECT_FALSE(m->at(4, 0).has_value());
    ZTH_EXPECT_FALSE(m->at(0, 5).has_value());
    ZTH_EXPECT_FALSE(m->at(std::numeric_limits<std::size_t>::max(), 0).has_value());
}
ZTH_TEST("works", "multiply, multiply_into: a known product, empty rows give 0, stale output is overwritten, wrong sizes are refused") {
    const auto m = Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    ZTH_EXPECT_TRUE(m.has_value());
    const Vec x = {1, 2, 3, 4, 5};
    const auto y = m->multiply(x);
    ZTH_EXPECT_TRUE(y.has_value());
    ZTH_EXPECT_TRUE((*y == Vec{7, 0, 22, 35}));
    Vec out(4, 99.0);
    ZTH_EXPECT_TRUE(m->multiply_into(x, out));
    ZTH_EXPECT_TRUE((out == Vec{7, 0, 22, 35}));
    Vec small(3, 99.0), shortx(4, 1.0);
    ZTH_EXPECT_FALSE(m->multiply_into(x, small));
    ZTH_EXPECT_TRUE((small == Vec{99, 99, 99}));   // untouched
    ZTH_EXPECT_FALSE(m->multiply_into(shortx, out));
    ZTH_EXPECT_FALSE(m->multiply(shortx).has_value());
    ZTH_EXPECT_FALSE(m->multiply(Vec(6, 1.0)).has_value());
}
ZTH_TEST("works", "multiply_transposed_into gives A^T x without forming A^T") {
    const auto m = Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    ZTH_EXPECT_TRUE(m.has_value());
    const Vec x = {1, 2, 3, 4};
    Vec y(5, 77.0);
    ZTH_EXPECT_TRUE(m->multiply_transposed_into(x, y));
    ZTH_EXPECT_TRUE((y == Vec{21, 9, 2, 12, 24}));
    Vec bad(4, 0.0);
    ZTH_EXPECT_FALSE(m->multiply_transposed_into(x, bad));
    ZTH_EXPECT_FALSE(m->multiply_transposed_into(Vec(5, 1.0), y));
}
ZTH_TEST("works", "transpose: the arrays of a known example, and twice is the identity") {
    const auto m = Csr::from_triplets(4, 5, {{0, 0, 1}, {0, 2, 2}, {2, 1, 3}, {2, 3, 4}, {3, 0, 5}, {3, 4, 6}});
    ZTH_EXPECT_TRUE(m.has_value());
    const Csr t = m->transpose();
    ZTH_EXPECT_EQ(t.rows(), std::size_t{5});
    ZTH_EXPECT_EQ(t.cols(), std::size_t{4});
    ZTH_EXPECT_TRUE((sizes(t.row_ptr()) == Sizes{0, 2, 3, 4, 5, 6}));
    ZTH_EXPECT_TRUE((sizes(t.col_idx()) == Sizes{0, 3, 2, 0, 2, 3}));
    ZTH_EXPECT_TRUE((doubles(t.values()) == Vec{1, 5, 3, 2, 4, 6}));
    const Csr tt = t.transpose();
    ZTH_EXPECT_TRUE(sizes(tt.row_ptr()) == sizes(m->row_ptr()) && sizes(tt.col_idx()) == sizes(m->col_idx()) && doubles(tt.values()) == doubles(m->values()));
    const Csr empty = build(0, 0, {});
    ZTH_EXPECT_TRUE(canonical(empty.transpose()));
}
ZTH_TEST("works", "plus: alpha * A + beta * B, merged per row, cancellations dropped, shapes checked") {
    const auto a = Csr::from_triplets(2, 3, {{0, 0, 1}, {0, 2, 2}, {1, 1, 3}});
    const auto b = Csr::from_triplets(2, 3, {{0, 1, 4}, {0, 2, -2}, {1, 1, 1}, {1, 2, 5}});
    ZTH_EXPECT_TRUE(a.has_value() && b.has_value());
    const auto s = a->plus(*b);
    ZTH_EXPECT_TRUE(s.has_value() && canonical(*s));
    ZTH_EXPECT_TRUE((sizes(s->row_ptr()) == Sizes{0, 2, 4}));
    ZTH_EXPECT_TRUE((sizes(s->col_idx()) == Sizes{0, 1, 1, 2}));   // (0,2): 2 + -2 = 0 dropped
    ZTH_EXPECT_TRUE((doubles(s->values()) == Vec{1, 4, 4, 5}));
    const auto d = a->plus(*b, 2.0, -1.0);   // 2a - b
    ZTH_EXPECT_TRUE(d.has_value() && canonical(*d));
    ZTH_EXPECT_NEAR(val(d->at(0, 0)), 2.0, 0);
    ZTH_EXPECT_NEAR(val(d->at(0, 1)), -4.0, 0);
    ZTH_EXPECT_NEAR(val(d->at(0, 2)), 6.0, 0);   // 2 * 2 - (-2)
    ZTH_EXPECT_NEAR(val(d->at(1, 1)), 5.0, 0);   // 2 * 3 - 1
    ZTH_EXPECT_NEAR(val(d->at(1, 2)), -5.0, 0);
    const auto self = a->plus(*a, 1.0, -1.0);   // a - a = nothing
    ZTH_EXPECT_TRUE(self.has_value() && self->nnz() == 0 && canonical(*self));
    const auto other_shape = Csr::from_triplets(3, 2, {});
    ZTH_EXPECT_TRUE(other_shape.has_value());
    ZTH_EXPECT_FALSE(a->plus(*other_shape).has_value());
    const auto same_rows = Csr::from_triplets(2, 4, {});
    ZTH_EXPECT_TRUE(same_rows.has_value());
    ZTH_EXPECT_FALSE(a->plus(*same_rows).has_value());
}
ZTH_TEST("works", "sparse vectors: construction, dot products with sparse and dense vectors") {
    const auto a = SparseVector::from_pairs(10, {{7, 2.0}, {1, 3.0}, {4, -1.0}, {7, 1.0}, {9, 0.0}});
    ZTH_EXPECT_TRUE(a.has_value());
    ZTH_EXPECT_EQ(a->dim(), std::size_t{10});
    ZTH_EXPECT_TRUE((sizes(a->indices()) == Sizes{1, 4, 7}));
    ZTH_EXPECT_TRUE((doubles(a->values()) == Vec{3, -1, 3}));
    const auto b = SparseVector::from_pairs(10, {{4, 5.0}, {7, 0.5}, {9, 8.0}});
    ZTH_EXPECT_TRUE(b.has_value());
    ZTH_EXPECT_NEAR(val(a->dot(*b)), -5.0 + 1.5, 1e-15);
    ZTH_EXPECT_NEAR(val(b->dot(*a)), -3.5, 1e-15);
    Vec dense(10, 1.0);
    dense[1] = 2;
    ZTH_EXPECT_NEAR(val(a->dot(dense)), 6.0 - 1.0 + 3.0, 1e-15);
    ZTH_EXPECT_FALSE(a->dot(Vec(9, 1.0)).has_value());
    const auto c = SparseVector::from_pairs(11, {});
    ZTH_EXPECT_TRUE(c.has_value());
    ZTH_EXPECT_FALSE(a->dot(*c).has_value());
    ZTH_EXPECT_FALSE(SparseVector::from_pairs(5, {{5, 1.0}}).has_value());
    const auto cancel = SparseVector::from_pairs(3, {{1, 4.0}, {1, -4.0}});
    ZTH_EXPECT_TRUE(cancel.has_value() && cancel->nnz() == 0);
    const auto nine = SparseVector::from_pairs(3, {{1, 9.0}});
    ZTH_EXPECT_TRUE(nine.has_value());
    ZTH_EXPECT_NEAR(val(cancel->dot(*nine)), 0.0, 0);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "against a dense reference: build, at, multiply, transpose, multiply_transposed and plus on 400 random matrices") {
    zth::Lcg rng(2025);
    for (int round = 0; round < 400; ++round) {
        const std::size_t rows = static_cast<std::size_t>(rng.below(13)), cols = static_cast<std::size_t>(rng.below(13));
        const int percent = rng.below(4) == 0 ? 0 : rng.below(4) == 0 ? 100 : 5 + rng.below(50);
        const Dense d = random_dense(rng, rows, cols, percent);
        const auto m = Csr::from_triplets(rows, cols, triplets_of(rng, d));
        ZTH_EXPECT_TRUE(m.has_value() && canonical(*m));
        if (!m) return;
        std::size_t stored = 0;
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c) {
                ZTH_EXPECT_EQ(val(m->at(r, c)), d.at(r, c));
                if (d.at(r, c) != 0.0) ++stored;
            }
        ZTH_EXPECT_EQ(m->nnz(), stored);
        Vec x(cols), z(rows);
        for (double& v : x) v = static_cast<double>(rng.below(7) - 3) * 0.25;
        for (double& v : z) v = static_cast<double>(rng.below(7) - 3) * 0.25;
        const auto y = m->multiply(x);
        ZTH_EXPECT_TRUE(y.has_value() && *y == dense_mul(d, x));
        const Csr t = m->transpose();
        ZTH_EXPECT_TRUE(canonical(t) && t.rows() == cols && t.cols() == rows && t.nnz() == stored);
        Dense dt(cols, rows);
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c) dt.at(c, r) = d.at(r, c);
        const auto ty = t.multiply(z);
        ZTH_EXPECT_TRUE(ty.has_value() && *ty == dense_mul(dt, z));
        Vec direct(cols, -1.0);
        ZTH_EXPECT_TRUE(m->multiply_transposed_into(z, direct));
        ZTH_EXPECT_TRUE(direct == *ty);   // the same bits
        const Dense d2 = random_dense(rng, rows, cols, 5 + rng.below(40));
        const auto m2 = Csr::from_triplets(rows, cols, triplets_of(rng, d2));
        const double alpha = rng.below(3) - 1.0, beta = rng.below(3) - 1.0;
        const auto s = m->plus(*m2, alpha, beta);
        ZTH_EXPECT_TRUE(s.has_value() && canonical(*s));
        for (std::size_t r = 0; s && r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c) ZTH_EXPECT_EQ(val(s->at(r, c)), alpha * d.at(r, c) + beta * d2.at(r, c));
    }
}
ZTH_TEST("robust", "the terms of a row are added in ascending column order: floating-point results are bit-exact against the reference") {
    zth::Lcg rng(8);
    for (int round = 0; round < 100; ++round) {
        const std::size_t rows = 1 + static_cast<std::size_t>(rng.below(6)), cols = 2 + static_cast<std::size_t>(rng.below(10));
        Dense d(rows, cols);
        Vec x(cols);
        for (std::size_t c = 0; c < cols; ++c) x[c] = (rng.below(2) ? 1.0 : -1.0) * (1.0 + rng.below(1000) / 997.0) * (rng.below(3) == 0 ? 1e16 : 1.0);
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c)
                if (rng.below(3) != 0) d.at(r, c) = 0.1 + rng.below(1000) / 331.0;
        std::vector<Triplet> plain;   // one triplet per entry: no splitting into parts, which would change the bits of the values
        for (std::size_t r = 0; r < rows; ++r)
            for (std::size_t c = 0; c < cols; ++c)
                if (d.at(r, c) != 0.0) plain.push_back({r, c, d.at(r, c)});
        for (std::size_t i = plain.size(); i > 1; --i) std::swap(plain[i - 1], plain[static_cast<std::size_t>(rng.below(static_cast<int>(i)))]);
        const Csr m = build(rows, cols, plain);
        Vec y(rows);
        ZTH_EXPECT_TRUE(m.multiply_into(x, y));
        ZTH_EXPECT_TRUE(y == dense_mul(d, x));
    }
}
ZTH_TEST("robust", "empty rows and columns, a single row, a single column") {
    const Csr one_row = build(1, 6, {{0, 5, 2}, {0, 0, 1}});
    ZTH_EXPECT_TRUE(canonical(one_row) && one_row.nnz() == 2);
    ZTH_EXPECT_TRUE((*one_row.multiply(Vec{1, 1, 1, 1, 1, 1}) == Vec{3}));
    const Csr col = one_row.transpose();
    ZTH_EXPECT_TRUE(canonical(col) && col.rows() == 6 && col.cols() == 1);
    ZTH_EXPECT_TRUE((sizes(col.row_ptr()) == Sizes{0, 1, 1, 1, 1, 1, 2}));
    const Csr none = build(5, 5, {});
    ZTH_EXPECT_TRUE(canonical(none) && canonical(none.transpose()) && none.transpose().rows() == 5);
    ZTH_EXPECT_TRUE((*none.multiply(Vec(5, 3.0)) == Vec(5, 0.0)));
    const Csr lastrow = build(1000, 3, {{999, 2, 1}});
    ZTH_EXPECT_TRUE(canonical(lastrow) && lastrow.row_ptr()[999] == 0 && lastrow.row_ptr()[1000] == 1);
    ZTH_EXPECT_NEAR(val(lastrow.at(999, 2)), 1.0, 0);
    ZTH_EXPECT_TRUE(canonical(lastrow.transpose()));
}
ZTH_TEST("robust", "sparse vector dot products against a dense reference, with disjoint, equal and interleaved index sets") {
    zth::Lcg rng(41);
    for (int round = 0; round < 300; ++round) {
        const std::size_t dim = 1 + static_cast<std::size_t>(rng.below(30));
        Vec da(dim, 0.0), db(dim, 0.0);
        std::vector<std::pair<std::size_t, double>> pa, pb;
        for (std::size_t i = 0; i < dim; ++i) {
            if (rng.below(3) == 0) {
                da[i] = static_cast<double>(rng.below(9) + 1) / 4.0;
                pa.push_back({i, da[i]});
            }
            if (rng.below(3) == 0) {
                db[i] = static_cast<double>(rng.below(9) + 1) / 4.0;
                pb.push_back({i, db[i]});
            }
        }
        for (std::size_t i = pa.size(); i > 1; --i) std::swap(pa[i - 1], pa[static_cast<std::size_t>(rng.below(static_cast<int>(i)))]);
        const auto a = SparseVector::from_pairs(dim, pa);
        const auto b = SparseVector::from_pairs(dim, pb);
        ZTH_EXPECT_TRUE(a.has_value() && b.has_value());
        double want = 0.0;
        for (std::size_t i = 0; i < dim; ++i) want += da[i] * db[i];
        ZTH_EXPECT_NEAR(val(a->dot(*b)), want, 1e-12);
        ZTH_EXPECT_NEAR(val(a->dot(db)), want, 1e-12);
        ZTH_EXPECT_NEAR(val(b->dot(da)), want, 1e-12);
    }
}
ZTH_TEST("robust", "garbage triplets never crash: out-of-range ones are refused, the rest build a canonical matrix") {
    zth::Lcg rng(5);
    for (int round = 0; round < 500; ++round) {
        const std::size_t rows = static_cast<std::size_t>(rng.below(6)), cols = static_cast<std::size_t>(rng.below(6));
        std::vector<Triplet> t;
        bool all_inside = true;
        for (int i = 0, n = rng.below(20); i < n; ++i) {
            const Triplet e = {static_cast<std::size_t>(rng.below(7)), static_cast<std::size_t>(rng.below(7)), static_cast<double>(rng.below(5) - 2)};
            all_inside = all_inside && e.row < rows && e.col < cols;
            t.push_back(e);
        }
        const auto m = Csr::from_triplets(rows, cols, t);
        ZTH_EXPECT_EQ(m.has_value(), all_inside);
        if (m) ZTH_EXPECT_TRUE(canonical(*m));
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "the kernels do not allocate: at, multiply_into, multiply_transposed_into; transpose makes at most 5 allocations; multiply makes one") {
    const Csr m = build(300, 400, [] {
        std::vector<Triplet> t;
        for (std::size_t r = 0; r < 300; ++r)
            for (std::size_t c = r % 7; c < 400; c += 13) t.push_back({r, c, 1.0 + static_cast<double>(c % 5)});
        return t;
    }());
    Vec x(400, 1.0), y(300), z(300, 2.0), w(400);
    zth::mem::Scope scope;
    double sink = 0;
    for (int round = 0; round < 200; ++round) {
        m.multiply_into(x, y);
        m.multiply_transposed_into(z, w);
        sink += y[round % 300] + w[round % 400] + m.at(static_cast<std::size_t>(round) % 300, static_cast<std::size_t>(round) % 400).value_or(0);
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sink > 0);
    zth::mem::Scope s2;
    const Csr t = m.transpose();
    ZTH_EXPECT_ALLOCS_AT_MOST(s2, 5);
    zth::mem::Scope s3;
    const auto p = m.multiply(x);
    ZTH_EXPECT_ALLOCS_AT_MOST(s3, 1);
    ZTH_EXPECT_TRUE(t.nnz() == m.nnz() && p.has_value());
}
ZTH_TEST_TIMEOUT("fast", "524288 rows by 524288 columns, 1.5 million entries: only the non-zeros are visited", 30) {
    const std::size_t n = std::size_t{1} << 19;
    std::vector<Triplet> t;
    t.reserve(3 * n);
    for (std::size_t r = 0; r < n; ++r) {
        t.push_back({r, r, 4.0});
        t.push_back({r, (r * 7 + 1) % n, -1.0});
        t.push_back({r, (r * 13 + 5) % n, -1.0});
    }
    const auto m = Csr::from_triplets(n, n, std::move(t));
    ZTH_EXPECT_TRUE(m.has_value());
    Vec x(n, 1.0), y(n);
    for (int i = 0; i < 20; ++i) m->multiply_into(x, y);
    ZTH_EXPECT_NEAR(y[12345], 2.0, 0);
    const Csr tr = m->transpose();
    ZTH_EXPECT_EQ(tr.nnz(), m->nnz());
    const auto sum = m->plus(tr, 1.0, 1.0);
    ZTH_EXPECT_TRUE(sum.has_value() && sum->nnz() > n);
}
ZTH_TEST_TIMEOUT("fast", "at() is a binary search: 200000 look-ups in a row of a million entries", 20) {
    const std::size_t cols = std::size_t{1} << 21;
    std::vector<Triplet> t;
    t.reserve(cols / 2);
    for (std::size_t c = 0; c < cols; c += 2) t.push_back({0, c, 1.0 + static_cast<double>(c % 3)});
    const auto m = Csr::from_triplets(1, cols, std::move(t));
    ZTH_EXPECT_TRUE(m.has_value());
    double sink = 0;
    zth::Lcg rng(1);
    for (int i = 0; i < 200000; ++i) sink += m->at(0, static_cast<std::size_t>(rng.next()) % cols).value_or(-1000);
    ZTH_EXPECT_TRUE(sink != 12345.678);
    ZTH_EXPECT_NEAR(val(m->at(0, cols - 2)), 1.0 + static_cast<double>((cols - 2) % 3), 0);
    ZTH_EXPECT_NEAR(val(m->at(0, cols - 1)), 0.0, 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("sparse.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("sparse.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "CSR arrays, not node-based containers; no linear search for an element") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("sparse.hpp"));
    for (const char* word : {"map", "unordered_map", "set", "unordered_set", "list", "deque", "find", "find_if", "count", "count_if"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "lower_bound").empty() && ::zth::find_word(code, "binary_search").empty());
}
