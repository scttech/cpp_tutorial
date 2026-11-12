#pragma once
// YOUR KALMAN FILTER. A linear Kalman filter for an N-dimensional state observed through ONE scalar measurement at a time
// (a scalar measurement needs no matrix inverse: S below is a number). Matrices and vectors come from linalg.hpp (namespace kf).
//
// The model:  x' = F x + w   (w has covariance Q)       z = H . x + v   (v has variance R)
//
// kf::constant_velocity(dt, q) -> struct Model { Mat<2> F, Q; }     the standard "nearly constant velocity" model for state (position, velocity):
//        F = [[1, dt], [0, 1]]        Q = q * [[dt^4 / 4, dt^3 / 2], [dt^3 / 2, dt^2]]      (white-noise acceleration of intensity q)
//
// kf::Kalman<N>
//   static make(x0, P0) -> optional<Kalman>      nullopt if x0 or P0 is not finite, a diagonal entry of P0 is negative, or P0 is not symmetric
//                                                (|P(i,j) - P(j,i)| > 1e-12 * (1 + the largest diagonal entry)). The stored covariance is exactly symmetric.
//   state() -> const Vec<N>&,  covariance() -> const Mat<N>&
//   predict(F, Q) -> bool          x = F x,  P = F P F^T + Q.  Returns false and changes NOTHING if F or Q has a non-finite entry.
//   update(H, z, R, gate = infinity) -> optional<Innovation>      one scalar measurement z = H . x + noise of variance R.
//        Innovation { double y; double s; double nis; }          y = z - H . x (the innovation), s = H P H^T + R (its variance), nis = y^2 / s (normalised innovation squared)
//        Returns nullopt and changes NOTHING if: H is not finite; z is not finite; R is not a finite number > 0; the gate is NaN or negative; s is not a finite number > 0 (P is broken);
//        or nis > gate (an outlier: with gate = 9 a reading more than 3 standard deviations away is ignored; a reading with nis == gate is accepted).
//        Otherwise:  K = P H / s;   x += K y;   P = (I - K H^T) P (I - K H^T)^T + R K K^T      <- the JOSEPH form, NOT the shorter P = (I - K H^T) P
//   AFTER EVERY predict AND update the covariance is exactly symmetric (replace P by (P + P^T) / 2 at the end).
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

#include "linalg.hpp"

namespace kf {

struct Model {
    Mat<2> F, Q;
};

struct Innovation {
    double y;
    double s;
    double nis;
};

inline Model constant_velocity(double, double) { return Model{}; }   // TODO

template <std::size_t N>
class Kalman {
public:
    static std::optional<Kalman> make(Vec<N> x0, Mat<N> P0) {   // TODO: validate
        Kalman k;
        k.x_ = x0;
        k.P_ = P0;
        return k;
    }
    const Vec<N>& state() const { return x_; }
    const Mat<N>& covariance() const { return P_; }

    bool predict(const Mat<N>&, const Mat<N>&) { return false; }   // TODO
    std::optional<Innovation> update(const Vec<N>&, double, double, double = std::numeric_limits<double>::infinity()) { return std::nullopt; }   // TODO

private:
    Vec<N> x_{};
    Mat<N> P_{};
};

}  // namespace kf
