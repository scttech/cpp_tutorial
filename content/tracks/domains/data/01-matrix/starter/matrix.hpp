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

    // TODO
    T& operator()(std::size_t, std::size_t) const { return *p_; }
    View transposed() const { return *this; }
    std::optional<View> block(std::size_t, std::size_t, std::size_t, std::size_t) const { return std::nullopt; }

private:
    T* p_ = nullptr;
    std::size_t rows_ = 0, cols_ = 0, rs_ = 0, cs_ = 0;
};

template <class T>
class Matrix {
public:
    Matrix() = default;
    Matrix(std::size_t rows, std::size_t cols, const T& fill = T{}) : rows_(rows), cols_(cols), data_(rows * cols, fill) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }

    // TODO
    static std::optional<Matrix> from_rows(const std::vector<std::vector<T>>&) { return std::nullopt; }
    static Matrix identity(std::size_t) { return Matrix(); }
    T& operator()(std::size_t, std::size_t) { return data_[0]; }
    const T& operator()(std::size_t, std::size_t) const { return data_[0]; }
    View<const T> view() const { return View<const T>(); }
    View<T> mview() { return View<T>(); }

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<T> data_;
};

// TODO
template <class T>
Matrix<T> to_matrix(View<const T>) { return Matrix<T>(); }
template <class T>
std::optional<Matrix<T>> multiply(View<const T>, View<const T>) { return std::nullopt; }
template <class T>
std::optional<Matrix<T>> multiply_blocked(View<const T>, View<const T>, std::size_t) { return std::nullopt; }

}  // namespace mat
