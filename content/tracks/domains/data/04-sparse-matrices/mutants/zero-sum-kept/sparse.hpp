// MUTANT: entries that sum to zero are stored
#pragma once
// YOUR SPARSE VECTORS AND CSR MATRICES.
//
// CSR (compressed sparse row) stores a rows x cols matrix in three arrays:
//   values   the non-zero values, row by row
//   col_idx  the column of each value (same length as values)
//   row_ptr  rows + 1 numbers: row r occupies the positions [row_ptr[r], row_ptr[r + 1]) of the two arrays above; row_ptr[0] = 0, row_ptr[rows] = nnz
// CANONICAL FORM (every Csr you return must be in it): inside a row the column indices are STRICTLY increasing, and no stored value is zero (0.0 or -0.0).
//
// constexpr std::size_t kMaxDimension = 1 << 22      rows and cols above this are refused (row_ptr has rows + 1 entries: a huge value must not allocate gigabytes or overflow)
//
// struct Triplet { std::size_t row, col; double value; }
// Csr::from_triplets(rows, cols, triplets) -> optional<Csr>
//     nullopt if rows or cols exceed kMaxDimension, or any triplet is outside the matrix. Triplets arrive in any order. Triplets with the same (row, col) are SUMMED, in the
//     order they were given (floating-point addition is not associative: 1e16 + 1 - 1e16 is 0 in that order and 1 in another). A sum that is zero (an explicit zero, or
//     x and -x) leaves NO entry. A NaN sum is kept.
// Csr::rows(), cols(), nnz(), row_ptr(), col_idx(), values()      (the three arrays as std::span<const ...>)
// Csr::at(r, c) -> optional<double>      nullopt if (r, c) is outside the matrix, otherwise the value (0.0 for an entry that is not stored); binary search inside the row
// Csr::multiply_into(x, y) -> bool       y = A x. false (and y untouched) unless x.size() == cols and y.size() == rows. Every y[r] is overwritten (empty rows give 0.0), the terms
//                                        of a row are added in ascending column order, and nothing is allocated.
// Csr::multiply(x) -> optional<vector<double>>      the same, allocating only the result
// Csr::multiply_transposed_into(x, y) -> bool       y = A^T x without building A^T: x.size() == rows, y.size() == cols; the same bits as transpose().multiply(x)
// Csr::transpose() -> Csr                canonical, built in O(nnz + cols) by counting (no sorting, at most 5 allocations)
// Csr::plus(other, alpha = 1, beta = 1) -> optional<Csr>      alpha * this + beta * other, merged row by row; nullopt if the shapes differ; entries that come out zero are dropped
//
// SparseVector::from_pairs(dim, pairs) -> optional<SparseVector>   (index, value) pairs in any order; duplicates summed in the given order; zero sums dropped; nullopt if an index >= dim
// SparseVector::dim(), nnz(), indices(), values()      indices strictly increasing
// SparseVector::dot(other) -> optional<double>        nullopt if the dimensions differ; a two-pointer merge over the two index lists: O(nnz_a + nnz_b)
// SparseVector::dot(dense) -> optional<double>        nullopt unless dense.size() == dim
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sparse {

inline constexpr std::size_t kMaxDimension = std::size_t{1} << 22;

struct Triplet {
    std::size_t row, col;
    double value;
};

class Csr {
public:
    Csr() : row_ptr_(1, 0) {}

    static std::optional<Csr> from_triplets(std::size_t rows, std::size_t cols, std::vector<Triplet> t) {
        if (rows > kMaxDimension || cols > kMaxDimension) return std::nullopt;
        for (const Triplet& e : t)
            if (e.row >= rows || e.col >= cols) return std::nullopt;
        std::stable_sort(t.begin(), t.end(), [](const Triplet& a, const Triplet& b) { return a.row != b.row ? a.row < b.row : a.col < b.col; });
        Csr m;
        m.rows_ = rows;
        m.cols_ = cols;
        m.row_ptr_.assign(rows + 1, 0);
        for (std::size_t i = 0; i < t.size();) {
            double sum = t[i].value;
            std::size_t j = i + 1;
            while (j < t.size() && t[j].row == t[i].row && t[j].col == t[i].col) sum += t[j++].value;
            if (true) {
                m.col_idx_.push_back(t[i].col);
                m.values_.push_back(sum);
                ++m.row_ptr_[t[i].row + 1];
            }
            i = j;
        }
        for (std::size_t r = 0; r < rows; ++r) m.row_ptr_[r + 1] += m.row_ptr_[r];
        return m;
    }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t nnz() const { return values_.size(); }
    std::span<const std::size_t> row_ptr() const { return row_ptr_; }
    std::span<const std::size_t> col_idx() const { return col_idx_; }
    std::span<const double> values() const { return values_; }

    std::optional<double> at(std::size_t r, std::size_t c) const {
        if (r >= rows_ || c >= cols_) return std::nullopt;
        const auto first = col_idx_.begin() + static_cast<std::ptrdiff_t>(row_ptr_[r]);
        const auto last = col_idx_.begin() + static_cast<std::ptrdiff_t>(row_ptr_[r + 1]);
        const auto it = std::lower_bound(first, last, c);
        if (it != last && *it == c) return values_[static_cast<std::size_t>(it - col_idx_.begin())];
        return 0.0;
    }

    bool multiply_into(std::span<const double> x, std::span<double> y) const {
        if (x.size() != cols_ || y.size() != rows_) return false;
        for (std::size_t r = 0; r < rows_; ++r) {
            double sum = 0.0;
            for (std::size_t k = row_ptr_[r]; k < row_ptr_[r + 1]; ++k) sum += values_[k] * x[col_idx_[k]];
            y[r] = sum;
        }
        return true;
    }

    std::optional<std::vector<double>> multiply(std::span<const double> x) const {
        if (x.size() != cols_) return std::nullopt;
        std::vector<double> y(rows_);
        multiply_into(x, y);
        return y;
    }

    bool multiply_transposed_into(std::span<const double> x, std::span<double> y) const {
        if (x.size() != rows_ || y.size() != cols_) return false;
        std::fill(y.begin(), y.end(), 0.0);
        for (std::size_t r = 0; r < rows_; ++r)
            for (std::size_t k = row_ptr_[r]; k < row_ptr_[r + 1]; ++k) y[col_idx_[k]] += values_[k] * x[r];
        return true;
    }

    Csr transpose() const {
        Csr t;
        t.rows_ = cols_;
        t.cols_ = rows_;
        t.row_ptr_.assign(cols_ + 1, 0);
        for (std::size_t c : col_idx_) ++t.row_ptr_[c + 1];
        for (std::size_t c = 0; c < cols_; ++c) t.row_ptr_[c + 1] += t.row_ptr_[c];
        t.col_idx_.resize(nnz());
        t.values_.resize(nnz());
        std::vector<std::size_t> next(t.row_ptr_.begin(), t.row_ptr_.end() - 1);
        for (std::size_t r = 0; r < rows_; ++r)
            for (std::size_t k = row_ptr_[r]; k < row_ptr_[r + 1]; ++k) {
                const std::size_t pos = next[col_idx_[k]]++;
                t.col_idx_[pos] = r;
                t.values_[pos] = values_[k];
            }
        return t;
    }

    std::optional<Csr> plus(const Csr& o, double alpha = 1, double beta = 1) const {
        if (rows_ != o.rows_ || cols_ != o.cols_) return std::nullopt;
        Csr s;
        s.rows_ = rows_;
        s.cols_ = cols_;
        s.row_ptr_.assign(rows_ + 1, 0);
        s.col_idx_.reserve(nnz() + o.nnz());
        s.values_.reserve(nnz() + o.nnz());
        for (std::size_t r = 0; r < rows_; ++r) {
            std::size_t i = row_ptr_[r], j = o.row_ptr_[r];
            const std::size_t ie = row_ptr_[r + 1], je = o.row_ptr_[r + 1];
            while (i < ie || j < je) {
                std::size_t col;
                double v;
                if (j >= je || (i < ie && col_idx_[i] < o.col_idx_[j])) {
                    col = col_idx_[i];
                    v = alpha * values_[i++];
                } else if (i >= ie || o.col_idx_[j] < col_idx_[i]) {
                    col = o.col_idx_[j];
                    v = beta * o.values_[j++];
                } else {
                    col = col_idx_[i];
                    v = alpha * values_[i++] + beta * o.values_[j++];
                }
                if (v != 0.0) {
                    s.col_idx_.push_back(col);
                    s.values_.push_back(v);
                }
            }
            s.row_ptr_[r + 1] = s.values_.size();
        }
        return s;
    }

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<std::size_t> row_ptr_, col_idx_;
    std::vector<double> values_;
};

class SparseVector {
public:
    static std::optional<SparseVector> from_pairs(std::size_t dim, std::vector<std::pair<std::size_t, double>> pairs) {
        for (const auto& p : pairs)
            if (p.first >= dim) return std::nullopt;
        std::stable_sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        SparseVector v;
        v.dim_ = dim;
        for (std::size_t i = 0; i < pairs.size();) {
            double sum = pairs[i].second;
            std::size_t j = i + 1;
            while (j < pairs.size() && pairs[j].first == pairs[i].first) sum += pairs[j++].second;
            if (sum != 0.0) {
                v.indices_.push_back(pairs[i].first);
                v.values_.push_back(sum);
            }
            i = j;
        }
        return v;
    }
    std::size_t dim() const { return dim_; }
    std::size_t nnz() const { return values_.size(); }
    std::span<const std::size_t> indices() const { return indices_; }
    std::span<const double> values() const { return values_; }

    std::optional<double> dot(const SparseVector& o) const {
        if (dim_ != o.dim_) return std::nullopt;
        double sum = 0.0;
        std::size_t i = 0, j = 0;
        while (i < indices_.size() && j < o.indices_.size()) {
            if (indices_[i] < o.indices_[j]) ++i;
            else if (indices_[i] > o.indices_[j]) ++j;
            else sum += values_[i++] * o.values_[j++];
        }
        return sum;
    }
    std::optional<double> dot(std::span<const double> dense) const {
        if (dense.size() != dim_) return std::nullopt;
        double sum = 0.0;
        for (std::size_t i = 0; i < indices_.size(); ++i) sum += values_[i] * dense[indices_[i]];
        return sum;
    }

private:
    std::size_t dim_ = 0;
    std::vector<std::size_t> indices_;
    std::vector<double> values_;
};

}  // namespace sparse
