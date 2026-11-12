// MUTANT: classic Kahan: assumes the running sum is the larger one
#pragma once
// YOUR NUMERICALLY STABLE STATISTICS.
//
// stats::CompensatedSum      a running sum that keeps the rounding error of every addition (Neumaier's variant of Kahan summation).
//   add(x), value()            value() = the running sum plus the accumulated compensation. It also works when an addend is LARGER than the running sum
//                              (the classic Kahan algorithm loses that case: 1e16 + 1 - 1e16 must give 1, not 0).
//
// stats::Running             Welford's online mean and variance, mergeable.
//   bool add(x)                false, and nothing changes, if x is NaN or infinite (a bad reading must not poison the statistics); true otherwise
//   merge(other)               afterwards *this describes both streams (Chan et al.): the means are combined and the squared-deviation sums get the
//                              extra term  delta^2 * n1 * n2 / (n1 + n2)  where delta = mean2 - mean1. Merging with an empty Running changes nothing, in either direction.
//   count()
//   mean()                     nullopt when empty
//   variance()                 the SAMPLE variance (divide by n - 1); nullopt when count < 2
//   population_variance()      divide by n; nullopt when empty
//   stddev()                   sqrt(variance()) ; nullopt like variance()
//   min(), max()               nullopt when empty
//   Never compute sum(x^2) - n * mean^2: it cancels catastrophically when the mean is large compared to the spread.
//
// stats::RunningCov          the same idea for two series.
//   bool add(x, y)             false (nothing changes) unless both are finite
//   count(), covariance()      the sample covariance (n - 1); nullopt when count < 2
//   correlation()              Pearson's r, always within [-1, 1] (rounding can make it 1.0000000000000002: clamp). nullopt when count < 2 or either series has zero spread.
//                              The products of the two sums of squared deviations can overflow even when each fits: take the two square roots first.
//
// stats::percentile(data, p)  linear interpolation between the closest ranks (NumPy's default): rank = p / 100 * (n - 1), lo = floor(rank), the value is
//                              data[lo] * (1 - f) + data[lo + 1] * f  with f = rank - lo  (written like this, not as a + f * (b - a): that overflows for -1e308 and 1e308).
//                              data is unsorted and must not be modified; use std::nth_element on a copy, not a full sort.
//                              nullopt if data is empty, p is NaN or outside [0, 100], or data contains a NaN (it has no place in an ordering).
// stats::median(data)         percentile(data, 50)
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace stats {

class CompensatedSum {
public:
    void add(double x) {
        const double t = sum_ + x;
        comp_ += (sum_ - t) + x;
        sum_ = t;
    }
    double value() const { return sum_ + comp_; }

private:
    double sum_ = 0;
    double comp_ = 0;
};

class Running {
public:
    bool add(double x) {
        if (!std::isfinite(x)) return false;
        ++n_;
        const double delta = x - mean_;
        mean_ += delta / static_cast<double>(n_);
        m2_ += delta * (x - mean_);
        if (n_ == 1 || x < min_) min_ = x;
        if (n_ == 1 || x > max_) max_ = x;
        return true;
    }
    void merge(const Running& o) {
        if (o.n_ == 0) return;
        if (n_ == 0) {
            *this = o;
            return;
        }
        const double na = static_cast<double>(n_), nb = static_cast<double>(o.n_), n = na + nb;
        const double delta = o.mean_ - mean_;
        mean_ += delta * (nb / n);
        m2_ += o.m2_ + delta * delta * (na * nb / n);
        n_ += o.n_;
        min_ = std::min(min_, o.min_);
        max_ = std::max(max_, o.max_);
    }
    std::size_t count() const { return n_; }
    std::optional<double> mean() const { return n_ ? std::optional<double>(mean_) : std::nullopt; }
    std::optional<double> variance() const { return n_ >= 2 ? std::optional<double>(m2_ / static_cast<double>(n_ - 1)) : std::nullopt; }
    std::optional<double> population_variance() const { return n_ >= 1 ? std::optional<double>(m2_ / static_cast<double>(n_)) : std::nullopt; }
    std::optional<double> stddev() const {
        const auto v = variance();
        return v ? std::optional<double>(std::sqrt(*v)) : std::nullopt;
    }
    std::optional<double> min() const { return n_ ? std::optional<double>(min_) : std::nullopt; }
    std::optional<double> max() const { return n_ ? std::optional<double>(max_) : std::nullopt; }

private:
    std::size_t n_ = 0;
    double mean_ = 0, m2_ = 0, min_ = 0, max_ = 0;
};

class RunningCov {
public:
    bool add(double x, double y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return false;
        ++n_;
        const double dx = x - mx_, dy = y - my_;
        mx_ += dx / static_cast<double>(n_);
        my_ += dy / static_cast<double>(n_);
        c_ += dx * (y - my_);
        m2x_ += dx * (x - mx_);
        m2y_ += dy * (y - my_);
        return true;
    }
    std::size_t count() const { return n_; }
    std::optional<double> covariance() const { return n_ >= 2 ? std::optional<double>(c_ / static_cast<double>(n_ - 1)) : std::nullopt; }
    std::optional<double> correlation() const {
        if (n_ < 2 || !(m2x_ > 0) || !(m2y_ > 0)) return std::nullopt;
        const double r = c_ / (std::sqrt(m2x_) * std::sqrt(m2y_));
        return std::clamp(r, -1.0, 1.0);
    }

private:
    std::size_t n_ = 0;
    double mx_ = 0, my_ = 0, c_ = 0, m2x_ = 0, m2y_ = 0;
};

inline std::optional<double> percentile(std::span<const double> data, double p) {
    if (data.empty() || !(p >= 0.0 && p <= 100.0)) return std::nullopt;
    for (double d : data)
        if (std::isnan(d)) return std::nullopt;
    std::vector<double> v(data.begin(), data.end());
    const double rank = p / 100.0 * static_cast<double>(v.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(rank));
    const double f = rank - static_cast<double>(lo);
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(lo), v.end());
    const double a = v[lo];
    if (lo + 1 >= v.size() || f == 0.0) return a;
    const double b = *std::min_element(v.begin() + static_cast<std::ptrdiff_t>(lo) + 1, v.end());
    return a * (1.0 - f) + b * f;
}
inline std::optional<double> median(std::span<const double> data) { return percentile(data, 50); }

}  // namespace stats
