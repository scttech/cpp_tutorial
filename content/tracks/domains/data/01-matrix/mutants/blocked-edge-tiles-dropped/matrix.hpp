// MUTANT: the partial tile at the end of a row of tiles is skipped
#pragma once
// YOUR MATRIX LIBRARY. Row-major storage, non-owning strided views, a naive and a cache-blocked multiply.
//
// mat::View<T>          a NON-OWNING window onto a matrix: a pointer, a shape (rows, cols) and two STRIDES (the distance, in elements, between two
//                       neighbouring rows and between two neighbouring columns). Element (r, c) lives at ptr[r * row_stride + c * col_stride].
//                       T may be const (View<const double> is read-only); a View<U> converts implicitly to a View<const U>.
//   rows(), cols(), row_stride(), col_stride(), data()
//   operator()(r, c) const -> T&        unchecked
//   transposed() const -> View          the SAME elements with rows and columns swapped: no copy, no allocation (swap the shape AND the strides)
//   block(r0, c0, nr, nc) const -> std::optional<View>
//                       the nr x nc window whose top-left corner is (r0, c0); nullopt if it does not lie completely inside the view.
//                       The check must not overflow (r0 + nr can wrap around for huge values). Empty windows (nr == 0 or nc == 0) are fine as long as the corner is inside
//                       or exactly on the far edge (r0 <= rows, c0 <= cols).
//
// mat::Matrix<T>        owns rows * cols elements, row-major, in one std::vector.
//   Matrix() (0 x 0), Matrix(rows, cols, fill = T{})
//   static from_rows(vector<vector<T>>) -> optional<Matrix>    nullopt if the rows do not all have the same length (an empty list gives a 0 x 0 matrix)
//   static identity(n) -> Matrix        n x n, ones on the diagonal (T{1}) and T{} elsewhere
//   rows(), cols(), operator()(r, c) (const and non-const, unchecked), view() const -> View<const T>, mview() -> View<T>
//
// mat::to_matrix(View<const T>) -> Matrix<T>      a deep copy of whatever the view shows, in row-major order (so to_matrix(v.transposed()) is a real transpose)
// mat::multiply(a, b)            -> optional<Matrix<T>>   the product; nullopt if a.cols() != b.rows(). Starts every sum from T{} and adds a(i,k) * b(k,j) for k = 0, 1, 2, ...
//                                       in that order. Allocates exactly once (the result) and does exactly a.rows() * a.cols() * b.cols() multiplications.
// mat::multiply_blocked(a, b, block) the same result, computed tile by tile (block x block tiles) so that the working set stays in the cache. Every (i, j) sum must still add
//                                       its terms in ascending k, so floating-point results are bit-identical to multiply(). nullopt if block == 0 or the shapes do not fit.
//                                       block may be larger than the matrix, or as large as SIZE_MAX.
//
// T is any type with a default constructor (zero), operator+= and operator*; do not assume it is an arithmetic type.
#include <algorithm>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

namespace mat {

template <class T>
class View {
public:
    View() = default;
    View(T* p, std::size_t rows, std::size_t cols, std::size_t row_stride, std::size_t col_stride)
        : p_(p), rows_(rows), cols_(cols), rs_(row_stride), cs_(col_stride) {}
    template <class U, class = std::enable_if_t<std::is_same_v<const U, T> && !std::is_const_v<U>>>
    View(const View<U>& o) : p_(o.data()), rows_(o.rows()), cols_(o.cols()), rs_(o.row_stride()), cs_(o.col_stride()) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t row_stride() const { return rs_; }
    std::size_t col_stride() const { return cs_; }
    T* data() const { return p_; }

    T& operator()(std::size_t r, std::size_t c) const { return p_[r * rs_ + c * cs_]; }

    View transposed() const { return View(p_, cols_, rows_, cs_, rs_); }

    std::optional<View> block(std::size_t r0, std::size_t c0, std::size_t nr, std::size_t nc) const {
        if (r0 > rows_ || c0 > cols_ || nr > rows_ - r0 || nc > cols_ - c0) return std::nullopt;
        if (nr == 0 || nc == 0) return View(p_, nr, nc, rs_, cs_);   // never form a pointer outside the data
        return View(p_ + r0 * rs_ + c0 * cs_, nr, nc, rs_, cs_);
    }

private:
    T* p_ = nullptr;
    std::size_t rows_ = 0, cols_ = 0, rs_ = 0, cs_ = 0;
};

template <class T>
class Matrix {
public:
    Matrix() = default;
    Matrix(std::size_t rows, std::size_t cols, const T& fill = T{}) : rows_(rows), cols_(cols), data_(rows * cols, fill) {}

    static std::optional<Matrix> from_rows(const std::vector<std::vector<T>>& rows) {
        if (rows.empty()) return Matrix();
        const std::size_t width = rows.front().size();
        for (const auto& r : rows)
            if (r.size() != width) return std::nullopt;
        Matrix m(rows.size(), width);
        for (std::size_t i = 0; i < rows.size(); ++i)
            for (std::size_t j = 0; j < width; ++j) m(i, j) = rows[i][j];
        return m;
    }

    static Matrix identity(std::size_t n) {
        Matrix m(n, n);
        for (std::size_t i = 0; i < n; ++i) m(i, i) = T{1};
        return m;
    }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    T& operator()(std::size_t r, std::size_t c) { return data_[r * cols_ + c]; }
    const T& operator()(std::size_t r, std::size_t c) const { return data_[r * cols_ + c]; }
    View<const T> view() const { return View<const T>(data_.data(), rows_, cols_, cols_, 1); }
    View<T> mview() { return View<T>(data_.data(), rows_, cols_, cols_, 1); }

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<T> data_;
};

template <class T>
Matrix<T> to_matrix(View<const T> v) {
    Matrix<T> m(v.rows(), v.cols());
    for (std::size_t i = 0; i < v.rows(); ++i)
        for (std::size_t j = 0; j < v.cols(); ++j) m(i, j) = v(i, j);
    return m;
}

template <class T>
std::optional<Matrix<T>> multiply(View<const T> a, View<const T> b) {
    if (a.cols() != b.rows()) return std::nullopt;
    Matrix<T> c(a.rows(), b.cols());
    for (std::size_t i = 0; i < a.rows(); ++i)
        for (std::size_t j = 0; j < b.cols(); ++j) {
            T acc{};
            for (std::size_t k = 0; k < a.cols(); ++k) acc += a(i, k) * b(k, j);
            c(i, j) = acc;
        }
    return c;
}

template <class T>
std::optional<Matrix<T>> multiply_blocked(View<const T> a, View<const T> b, std::size_t block) {
    if (block == 0 || a.cols() != b.rows()) return std::nullopt;
    const std::size_t n = a.rows(), K = a.cols(), m = b.cols();
    Matrix<T> c(n, m);
    for (std::size_t ii = 0; ii < n; ii += block) {
        if (n - ii < block) break;
        const std::size_t ie = ii + std::min(block, n - ii);   // never ii + block: that can wrap around
        for (std::size_t kk = 0; kk < K; kk += block) {
            const std::size_t ke = kk + std::min(block, K - kk);
            for (std::size_t jj = 0; jj < m; jj += block) {
                const std::size_t je = jj + std::min(block, m - jj);
                for (std::size_t i = ii; i < ie; ++i)
                    for (std::size_t k = kk; k < ke; ++k)
                        for (std::size_t j = jj; j < je; ++j) c(i, j) += a(i, k) * b(k, j);
            }
        }
    }
    return c;
}

}  // namespace mat
