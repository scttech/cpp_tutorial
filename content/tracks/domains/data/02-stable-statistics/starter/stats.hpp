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
    void add(double x) { sum_ += x; }   // TODO: keep the rounding error
    double value() const { return sum_; }

private:
    double sum_ = 0;
};

class Running {
public:
    bool add(double x) {   // TODO: Welford
        sum_ += x;
        ++n_;
        return true;
    }
    void merge(const Running&) {}   // TODO
    std::size_t count() const { return n_; }
    std::optional<double> mean() const { return n_ ? std::optional<double>(sum_ / static_cast<double>(n_)) : std::nullopt; }
    std::optional<double> variance() const { return std::nullopt; }
    std::optional<double> population_variance() const { return std::nullopt; }
    std::optional<double> stddev() const { return std::nullopt; }
    std::optional<double> min() const { return std::nullopt; }
    std::optional<double> max() const { return std::nullopt; }

private:
    std::size_t n_ = 0;
    double sum_ = 0;
};

class RunningCov {
public:
    bool add(double, double) { return false; }   // TODO
    std::size_t count() const { return 0; }
    std::optional<double> covariance() const { return std::nullopt; }
    std::optional<double> correlation() const { return std::nullopt; }
};

inline std::optional<double> percentile(std::span<const double>, double) { return std::nullopt; }   // TODO
inline std::optional<double> median(std::span<const double> data) { return percentile(data, 50); }

}  // namespace stats
