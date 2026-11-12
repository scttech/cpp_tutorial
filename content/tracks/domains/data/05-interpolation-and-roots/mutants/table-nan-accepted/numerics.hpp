// MUTANT: a NaN y is accepted
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

namespace detail {
inline bool valid_table(const std::vector<double>& xs, const std::vector<double>& ys) {
    if (xs.size() < 2 || xs.size() != ys.size()) return false;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (!std::isfinite(xs[i]) || std::isinf(ys[i])) return false;
        if (i > 0 && !(xs[i] > xs[i - 1])) return false;
    }
    return true;
}
// index i of the segment [xs[i], xs[i+1]] that holds x (x is inside the table)
inline std::size_t segment(const std::vector<double>& xs, double x) {
    const std::size_t k = static_cast<std::size_t>(std::upper_bound(xs.begin(), xs.end(), x) - xs.begin());
    return k == 0 ? 0 : std::min(k - 1, xs.size() - 2);
}
}  // namespace detail

class Interp {
public:
    static std::optional<Interp> make(std::vector<double> xs, std::vector<double> ys) {
        if (!detail::valid_table(xs, ys)) return std::nullopt;
        Interp r;
        r.xs_ = std::move(xs);
        r.ys_ = std::move(ys);
        return r;
    }
    std::size_t size() const { return xs_.size(); }
    std::optional<double> operator()(double x) const {
        if (!(x >= xs_.front() && x <= xs_.back())) return std::nullopt;   // also catches NaN
        const std::size_t i = detail::segment(xs_, x);
        const double t = (x - xs_[i]) / (xs_[i + 1] - xs_[i]);
        return ys_[i] * (1.0 - t) + ys_[i + 1] * t;
    }

private:
    std::vector<double> xs_, ys_;
};

class Spline {
public:
    static std::optional<Spline> make(std::vector<double> xs, std::vector<double> ys) {
        if (!detail::valid_table(xs, ys)) return std::nullopt;
        const std::size_t n = xs.size();
        std::vector<double> m(n, 0.0), cp(n, 0.0), dp(n, 0.0);
        for (std::size_t i = 1; i + 1 < n; ++i) {   // forward sweep of the Thomas algorithm
            const double h0 = xs[i] - xs[i - 1], h1 = xs[i + 1] - xs[i];
            const double diag = 2.0 * (h0 + h1);
            const double rhs = 6.0 * ((ys[i + 1] - ys[i]) / h1 - (ys[i] - ys[i - 1]) / h0);
            const double denom = i == 1 ? diag : diag - h0 * cp[i - 1];
            cp[i] = h1 / denom;
            dp[i] = (i == 1 ? rhs : rhs - h0 * dp[i - 1]) / denom;
        }
        for (std::size_t i = n - 2; i >= 1; --i) m[i] = dp[i] - cp[i] * m[i + 1];   // back substitution; m[0] = m[n-1] = 0
        Spline s;
        s.xs_ = std::move(xs);
        s.ys_ = std::move(ys);
        s.m_ = std::move(m);
        return s;
    }
    std::optional<double> operator()(double x) const {
        if (!(x >= xs_.front() && x <= xs_.back())) return std::nullopt;
        const std::size_t i = detail::segment(xs_, x);
        const double h = xs_[i + 1] - xs_[i], a = xs_[i + 1] - x, b = x - xs_[i];
        return (m_[i] * a * a * a + m_[i + 1] * b * b * b) / (6.0 * h) + (ys_[i] / h - m_[i] * h / 6.0) * a + (ys_[i + 1] / h - m_[i + 1] * h / 6.0) * b;
    }
    std::optional<double> derivative(double x) const {
        if (!(x >= xs_.front() && x <= xs_.back())) return std::nullopt;
        const std::size_t i = detail::segment(xs_, x);
        const double h = xs_[i + 1] - xs_[i], a = xs_[i + 1] - x, b = x - xs_[i];
        return (-m_[i] * a * a + m_[i + 1] * b * b) / (2.0 * h) + (ys_[i + 1] - ys_[i]) / h - h * (m_[i + 1] - m_[i]) / 6.0;
    }

private:
    std::vector<double> xs_, ys_, m_;
};

template <class F>
std::optional<Root> bisect(F f, double a, double b, double tol = 1e-12, int max_iter = 200) {
    if (!std::isfinite(a) || !std::isfinite(b) || !(tol >= 0.0)) return std::nullopt;
    if (a > b) std::swap(a, b);
    const double fa0 = f(a), fb0 = f(b);
    if (!std::isfinite(fa0) || !std::isfinite(fb0)) return std::nullopt;
    if (fa0 == 0.0) return Root{a, 0};
    if (fb0 == 0.0) return Root{b, 0};
    if ((fa0 < 0.0) == (fb0 < 0.0)) return std::nullopt;
    const double scale = std::max(std::fabs(fa0), std::fabs(fb0));
    const bool low_is_negative = fa0 < 0.0;
    for (int i = 1; i <= max_iter; ++i) {
        const double m = std::midpoint(a, b);
        const double fm = f(m);
        if (!std::isfinite(fm)) return std::nullopt;
        if (fm == 0.0) return Root{m, i};
        if ((fm < 0.0) == low_is_negative) a = m;
        else b = m;
        const double next = std::midpoint(a, b);
        if (b - a <= 2.0 * tol || next == a || next == b) {
            const double fx = f(next);
            if (!std::isfinite(fx) || std::fabs(fx) > scale) return std::nullopt;   // a pole, not a root
            return Root{next, i};
        }
    }
    return std::nullopt;
}

template <class F, class D>
std::optional<Root> newton(F f, D df, double x, double tol = 1e-12, int max_iter = 50) {
    for (int i = 0; i < max_iter; ++i) {
        const double fx = f(x);
        if (!std::isfinite(fx)) return std::nullopt;
        if (fx == 0.0) return Root{x, i};
        const double d = df(x);
        if (d == 0.0 || !std::isfinite(d)) return std::nullopt;
        const double step = fx / d;
        x -= step;
        if (!std::isfinite(x)) return std::nullopt;
        if (std::fabs(step) <= tol * std::max(1.0, std::fabs(x))) return Root{x, i + 1};
    }
    return std::nullopt;
}

template <class F, class D>
std::optional<Root> safe_newton(F f, D df, double a, double b, double tol = 1e-12, int max_iter = 100) {
    if (!std::isfinite(a) || !std::isfinite(b) || !(tol >= 0.0)) return std::nullopt;
    if (a > b) std::swap(a, b);
    const double fa = f(a), fb = f(b);
    if (!std::isfinite(fa) || !std::isfinite(fb)) return std::nullopt;
    if (fa == 0.0) return Root{a, 0};
    if (fb == 0.0) return Root{b, 0};
    if ((fa < 0.0) == (fb < 0.0)) return std::nullopt;
    double xl = fa < 0.0 ? a : b, xh = fa < 0.0 ? b : a;   // f(xl) < 0 < f(xh)
    double x = std::midpoint(a, b);
    double dxold = std::fabs(b - a), dx = dxold;
    double fx = f(x), d = df(x);
    for (int i = 1; i <= max_iter; ++i) {
        if (!std::isfinite(fx)) return std::nullopt;
        if (fx == 0.0) return Root{x, i};
        const bool newton_leaves = !std::isfinite(d) || d == 0.0 || ((x - xh) * d - fx) * ((x - xl) * d - fx) > 0.0;
        if (newton_leaves || std::fabs(2.0 * fx) > std::fabs(dxold * d)) {   // bisect
            dxold = dx;
            dx = (xh - xl) / 2.0;
            const double nx = xl + dx;
            if (nx == xl) return Root{nx, i};
            x = nx;
        } else {
            dxold = dx;
            dx = fx / d;
            const double nx = x - dx;
            if (nx == x) return Root{nx, i};
            x = nx;
        }
        if (std::fabs(dx) <= tol * std::max(1.0, std::fabs(x))) return Root{x, i};
        fx = f(x);
        d = df(x);
        if (fx < 0.0) xl = x;
        else xh = x;
    }
    return std::nullopt;
}

}  // namespace numeric
