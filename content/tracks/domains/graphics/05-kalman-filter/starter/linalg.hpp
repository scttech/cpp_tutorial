#pragma once
// Read-only: tiny fixed-size linear algebra for the Kalman filter lessons.
#include <array>
#include <cmath>
#include <cstddef>

namespace kf {

template <std::size_t N>
using Vec = std::array<double, N>;

// An N x N matrix, row-major.
template <std::size_t N>
struct Mat {
    std::array<double, N * N> a{};
    double& operator()(std::size_t r, std::size_t c) { return a[r * N + c]; }
    double operator()(std::size_t r, std::size_t c) const { return a[r * N + c]; }
};

template <std::size_t N>
Mat<N> identity() {
    Mat<N> m;
    for (std::size_t i = 0; i < N; ++i) m(i, i) = 1.0;
    return m;
}
template <std::size_t N>
Mat<N> operator*(const Mat<N>& x, const Mat<N>& y) {
    Mat<N> r;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < N; ++k) s += x(i, k) * y(k, j);
            r(i, j) = s;
        }
    return r;
}
template <std::size_t N>
Vec<N> operator*(const Mat<N>& m, const Vec<N>& v) {
    Vec<N> r{};
    for (std::size_t i = 0; i < N; ++i) {
        double s = 0.0;
        for (std::size_t k = 0; k < N; ++k) s += m(i, k) * v[k];
        r[i] = s;
    }
    return r;
}
template <std::size_t N>
Mat<N> operator+(const Mat<N>& x, const Mat<N>& y) {
    Mat<N> r;
    for (std::size_t i = 0; i < N * N; ++i) r.a[i] = x.a[i] + y.a[i];
    return r;
}
template <std::size_t N>
Mat<N> operator-(const Mat<N>& x, const Mat<N>& y) {
    Mat<N> r;
    for (std::size_t i = 0; i < N * N; ++i) r.a[i] = x.a[i] - y.a[i];
    return r;
}
template <std::size_t N>
Mat<N> operator*(const Mat<N>& x, double s) {
    Mat<N> r;
    for (std::size_t i = 0; i < N * N; ++i) r.a[i] = x.a[i] * s;
    return r;
}
template <std::size_t N>
Mat<N> transposed(const Mat<N>& x) {
    Mat<N> r;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j) r(i, j) = x(j, i);
    return r;
}
template <std::size_t N>
double dot(const Vec<N>& x, const Vec<N>& y) {
    double s = 0.0;
    for (std::size_t i = 0; i < N; ++i) s += x[i] * y[i];
    return s;
}
// the outer product x y^T
template <std::size_t N>
Mat<N> outer(const Vec<N>& x, const Vec<N>& y) {
    Mat<N> r;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = 0; j < N; ++j) r(i, j) = x[i] * y[j];
    return r;
}
template <std::size_t N>
bool is_finite(const Vec<N>& v) {
    for (double e : v)
        if (!std::isfinite(e)) return false;
    return true;
}
template <std::size_t N>
bool is_finite(const Mat<N>& m) {
    for (double e : m.a)
        if (!std::isfinite(e)) return false;
    return true;
}

}  // namespace kf
