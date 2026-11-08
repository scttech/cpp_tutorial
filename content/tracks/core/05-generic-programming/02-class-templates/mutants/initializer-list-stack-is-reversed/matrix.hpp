#pragma once

#include <array>
#include <cstddef>

// A matrix whose size is part of its TYPE: Matrix<int, 2, 3> (2 rows, 3 columns) and Matrix<int, 3, 2> are different types.
// The dimensions are template parameters ("non-type template parameters": values, not types), so
//   - the storage is a fixed array inside the object: no heap allocation at all,
//   - and a size mistake (adding a 2x3 to a 3x2, multiplying 2x3 by 2x3) is a COMPILE error, not a wrong answer.
template <class T, std::size_t R, std::size_t C>
class Matrix {
public:
    static constexpr std::size_t rows() { return R; }
    static constexpr std::size_t cols() { return C; }

    Matrix() = default;  // every element is value-initialized (0 for numbers): see data_
    explicit Matrix(const T& fill) { data_.fill(fill); }

    T& operator()(std::size_t row, std::size_t col) { return data_[row * C + col]; }
    const T& operator()(std::size_t row, std::size_t col) const { return data_[row * C + col]; }

    // The transpose of an R x C matrix is a C x R matrix: a DIFFERENT type.
    Matrix<T, C, R> transposed() const;

    // The identity matrix. Only a square matrix has one (checked when it is used).
    static Matrix identity() {
        static_assert(R == C, "identity() needs a square matrix");
        Matrix m;
        for (std::size_t i = 0; i < R; ++i) m(i, i) = T{1};
        return m;
    }

    Matrix& operator+=(const Matrix& other) {
        for (std::size_t i = 0; i < R * C; ++i) data_[i] += other.data_[i];
        return *this;
    }
    friend Matrix operator+(Matrix a, const Matrix& b) { return a += b; }  // same dimensions only: it takes Matrix, not a template
    friend bool operator==(const Matrix&, const Matrix&) = default;

private:
    std::array<T, R * C> data_{};  // the {} value-initializes every element
};

template <class T, std::size_t R, std::size_t C>
Matrix<T, C, R> Matrix<T, R, C>::transposed() const {
    Matrix<T, C, R> result;
    for (std::size_t r = 0; r < R; ++r)
        for (std::size_t c = 0; c < C; ++c) result(c, r) = (*this)(r, c);
    return result;
}

// (R x K) * (K x C) = (R x C). K appears in BOTH parameters, so the compiler only accepts the pair if the inner dimensions agree.
template <class T, std::size_t R, std::size_t K, std::size_t C>
Matrix<T, R, C> operator*(const Matrix<T, R, K>& a, const Matrix<T, K, C>& b) {
    Matrix<T, R, C> result;
    for (std::size_t r = 0; r < R; ++r)
        for (std::size_t c = 0; c < C; ++c) {
            T sum{};
            for (std::size_t k = 0; k < K; ++k) sum += a(r, k) * b(k, c);
            result(r, c) = sum;
        }
    return result;
}
