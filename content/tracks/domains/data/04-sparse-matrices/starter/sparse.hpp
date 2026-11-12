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
    // TODO: from_triplets
    static std::optional<Csr> from_triplets(std::size_t, std::size_t, std::vector<Triplet>) { return std::nullopt; }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t nnz() const { return values_.size(); }
    std::span<const std::size_t> row_ptr() const { return row_ptr_; }
    std::span<const std::size_t> col_idx() const { return col_idx_; }
    std::span<const double> values() const { return values_; }

    // TODO
    std::optional<double> at(std::size_t, std::size_t) const { return std::nullopt; }
    bool multiply_into(std::span<const double>, std::span<double>) const { return false; }
    std::optional<std::vector<double>> multiply(std::span<const double>) const { return std::nullopt; }
    bool multiply_transposed_into(std::span<const double>, std::span<double>) const { return false; }
    Csr transpose() const { return Csr(); }
    std::optional<Csr> plus(const Csr&, double = 1, double = 1) const { return std::nullopt; }

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<std::size_t> row_ptr_, col_idx_;
    std::vector<double> values_;
};

class SparseVector {
public:
    // TODO
    static std::optional<SparseVector> from_pairs(std::size_t, std::vector<std::pair<std::size_t, double>>) { return std::nullopt; }
    std::size_t dim() const { return dim_; }
    std::size_t nnz() const { return values_.size(); }
    std::span<const std::size_t> indices() const { return indices_; }
    std::span<const double> values() const { return values_; }
    std::optional<double> dot(const SparseVector&) const { return std::nullopt; }
    std::optional<double> dot(std::span<const double>) const { return std::nullopt; }

private:
    std::size_t dim_ = 0;
    std::vector<std::size_t> indices_;
    std::vector<double> values_;
};

}  // namespace sparse
