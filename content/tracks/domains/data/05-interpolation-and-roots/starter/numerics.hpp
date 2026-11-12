#pragma once
// YOUR INTERPOLATION AND ROOT FINDING.
//
// numeric::Interp          piecewise-linear interpolation through a table of points
//   Interp::make(xs, ys) -> optional<Interp>      nullopt unless: at least 2 points, xs.size() == ys.size(), every value finite, xs STRICTLY increasing
//   size()
//   operator()(x) -> optional<double>             nullopt for x outside [xs.front(), xs.back()] and for NaN. Find the segment by BINARY SEARCH. Interpolate as
//                                                  y0 * (1 - t) + y1 * t  with  t = (x - x0) / (x1 - x0)  (not y0 + t * (y1 - y0), which overflows for -1e308 and 1e308).
//                                                  At a knot the table's y is returned exactly; the last knot belongs to the last segment.
//
// numeric::Spline          the natural cubic spline (second derivative 0 at both ends) through the same kind of table
//   Spline::make(xs, ys) -> optional<Spline>      the same refusals as Interp. With 2 points the spline is the straight line. The second derivatives m_i at the knots solve the
//                                                  tridiagonal system  h_(i-1) m_(i-1) + 2 (h_(i-1) + h_i) m_i + h_i m_(i+1) = 6 ((y_(i+1) - y_i) / h_i - (y_i - y_(i-1)) / h_(i-1))
//                                                  (h_i = x_(i+1) - x_i, m_0 = m_(n-1) = 0) by the Thomas algorithm: O(n), no matrix.
//   operator()(x), derivative(x) -> optional<double>    nullopt outside the table. On the segment [x0, x1] of width h:
//                                                  S(x)  = (m0 (x1 - x)^3 + m1 (x - x0)^3) / (6 h) + (y0 / h - m0 h / 6) (x1 - x) + (y1 / h - m1 h / 6) (x - x0)
//                                                  S'(x) = (-m0 (x1 - x)^2 + m1 (x - x0)^2) / (2 h) + (y1 - y0) / h - h (m1 - m0) / 6
//
// numeric::Root { double x; int iterations; }
// numeric::bisect(f, a, b, tol = 1e-12, max_iter = 200) -> optional<Root>
//     f is any callable double -> double. a and b may be given in either order. Needs a sign change (f(a), f(b) of opposite signs; an exact zero at an end is returned at once with
//     iterations 0). nullopt if the end values are not finite, there is no sign change, f is ever NaN/infinite, tol is negative/NaN, or max_iter iterations do not shrink the interval
//     to width 2 * tol. tol == 0 must still terminate: stop when the midpoint equals an end (the interval cannot shrink any more) and return that. The midpoint is std::midpoint(a, b)
//     (a + b overflows). A sign change caused by a POLE (1/x on [-1, 2], tan on [1, 2]) is not a root: if |f(x)| of the answer is larger than both |f(a)| and |f(b)|, return nullopt.
// numeric::newton(f, df, x0, tol = 1e-12, max_iter = 50) -> optional<Root>
//     x <- x - f(x) / df(x). Stops when |step| <= tol * max(1, |x|) (iterations = steps taken), or at once when f(x) == 0 (iterations counted before it). nullopt if f or df is not finite
//     (df == 0 too), if x becomes infinite or NaN, or max_iter steps do not converge. Newton diverges for atan from 1.5: that must come back as nullopt, never an infinite loop.
// numeric::safe_newton(f, df, a, b, tol = 1e-12, max_iter = 100) -> optional<Root>
//     Newton inside a bracket that always keeps the sign change (Numerical Recipes' rtsafe): the Newton step is used only if it stays inside the bracket and at least halves
//     the previous step; otherwise bisect. A zero or non-finite df forces a bisection. Same refusals as bisect (no sign change, non-finite values); same stopping rule as newton.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

namespace numeric {

struct Root {
    double x;
    int iterations;
};

class Interp {
public:
    static std::optional<Interp> make(std::vector<double>, std::vector<double>) { return std::nullopt; }   // TODO
    std::size_t size() const { return xs_.size(); }
    std::optional<double> operator()(double) const { return std::nullopt; }   // TODO

private:
    std::vector<double> xs_, ys_;
};

class Spline {
public:
    static std::optional<Spline> make(std::vector<double>, std::vector<double>) { return std::nullopt; }   // TODO
    std::optional<double> operator()(double) const { return std::nullopt; }
    std::optional<double> derivative(double) const { return std::nullopt; }

private:
    std::vector<double> xs_, ys_, m_;
};

template <class F>
std::optional<Root> bisect(F, double, double, double = 1e-12, int = 200) { return std::nullopt; }   // TODO

template <class F, class D>
std::optional<Root> newton(F, D, double, double = 1e-12, int = 50) { return std::nullopt; }   // TODO

template <class F, class D>
std::optional<Root> safe_newton(F, D, double, double, double = 1e-12, int = 100) { return std::nullopt; }   // TODO

}  // namespace numeric
