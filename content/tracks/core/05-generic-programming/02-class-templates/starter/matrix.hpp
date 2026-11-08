#pragma once

#include <array>
#include <cstddef>

// A matrix whose size is part of its TYPE: Matrix<int, 2, 3> (2 rows, 3 columns) and Matrix<int, 3, 2> are different types.
// The dimensions are template parameters ("non-type template parameters": values, not types), so
//   - the storage is a fixed array inside the object: no heap allocation at all,
//   - and a size mistake (adding a 2x3 to a 3x2, multiplying 2x3 by 2x3) is a COMPILE error, not a wrong answer.
// Access, rows() and cols() are written. The rest are stubs for you to fill in.
template <class T, std::size_t R, std::size_t C>
class Matrix {
public:
    static constexpr std::size_t rows() { return R; }
    static constexpr std::size_t cols() { return C; }

    Matrix() = default;  // data_ below is value-initialized: every element is 0 for numbers
    explicit Matrix(const T& fill) { data_.fill(fill); }

    T& operator()(std::size_t row, std::size_t col) { return data_[row * C + col]; }
    const T& operator()(std::size_t row, std::size_t col) const { return data_[row * C + col]; }

    // TODO: the transpose of an R x C matrix is a C x R matrix: a DIFFERENT type. Define it OUTSIDE the class (below), which needs
    //   template <class T, std::size_t R, std::size_t C>
    //   Matrix<T, C, R> Matrix<T, R, C>::transposed() const { ... }
    Matrix<T, C, R> transposed() const;

    // TODO: the identity matrix (ones on the diagonal, zeros elsewhere). Only a square matrix has one:
    // `static_assert(R == C, "identity() needs a square matrix");` checks it when the function is used.
    static Matrix identity() { return Matrix(); }

    // TODO: element-wise addition of two matrices of the SAME dimensions (+= changes this one; + returns a new one).
    Matrix& operator+=(const Matrix& other) {
        (void)other;
        return *this;
    }
    friend Matrix operator+(Matrix a, const Matrix& b) {
        (void)b;
        return a;
    }

    // TODO: equal when every element is equal (`= default` does it, if T has ==).
    friend bool operator==(const Matrix& a, const Matrix& b) {
        (void)a;
        (void)b;
        return false;
    }

private:
    std::array<T, R * C> data_{};  // the {} value-initializes every element
};

template <class T, std::size_t R, std::size_t C>
Matrix<T, C, R> Matrix<T, R, C>::transposed() const {
    return Matrix<T, C, R>();
}

// TODO: (R x K) * (K x C) = (R x C). K appears in BOTH parameters, so the compiler only accepts the pair when the inner
// dimensions agree: multiplying a 2x3 by a 2x3 is a compile error. result(r, c) = sum over k of a(r, k) * b(k, c).
template <class T, std::size_t R, std::size_t K, std::size_t C>
Matrix<T, R, C> operator*(const Matrix<T, R, K>& a, const Matrix<T, K, C>& b) {
    (void)a;
    (void)b;
    return Matrix<T, R, C>();
}
