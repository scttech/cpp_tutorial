// MUTANT: NaN returns are accepted
#pragma once
// Welford's algorithm for the mean and variance in one pass; a running peak for the drawdown; nth_element for the quantile.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "types.hpp"

namespace risk {
namespace detail {

// running mean and sum of squared deviations, updated one value at a time (Welford)
struct Welford {
    std::size_t n = 0;
    double mean = 0;
    double m2 = 0;  // sum of (x - mean)^2
    void add(double x) {
        ++n;
        const double delta = x - mean;
        mean += delta / static_cast<double>(n);
        m2 += delta * (x - mean);
    }
    double sample_stdev() const { return n < 2 ? 0.0 : std::sqrt(m2 / static_cast<double>(n - 1)); }
};

inline bool all_finite(const std::vector<double>& v) {
    for (const double x : v) {
        if (!std::isfinite(x)) return false;
    }
    return true;
}

}  // namespace detail

inline std::optional<double> mean(const std::vector<double>& values) {
    if (values.empty() || !detail::all_finite(values)) return std::nullopt;
    detail::Welford w;
    for (const double x : values) w.add(x);
    return w.mean;
}

inline std::optional<double> stdev(const std::vector<double>& values) {
    if (values.size() < 2 || !detail::all_finite(values)) return std::nullopt;
    detail::Welford w;
    for (const double x : values) w.add(x);
    return w.sample_stdev();
}

inline std::optional<Summary> summarize(const Series& prices) {
    const std::size_t n = prices.size();
    if (n < 2) return std::nullopt;
    detail::Welford w;
    Summary s;
    double first = prices[0];
    if (!(first > 0) || !std::isfinite(first)) return std::nullopt;
    double previous = first;
    double peak = first;
    for (std::size_t i = 1; i < n; ++i) {
        const double p = prices[i];  // one read per price
        if (!(p > 0) || !std::isfinite(p)) return std::nullopt;
        const double r = p / previous - 1.0;
        w.add(r);
        if (i == 1 || r > s.best) s.best = r;
        if (i == 1 || r < s.worst) s.worst = r;
        if (p > peak) peak = p;
        const double drawdown = (peak - p) / peak;
        if (drawdown > s.max_drawdown) s.max_drawdown = drawdown;
        previous = p;
    }
    s.returns = w.n;
    s.mean = w.mean;
    s.stdev = w.sample_stdev();
    s.total_return = previous / first - 1.0;
    return s;
}

inline std::optional<double> historical_var(std::vector<double> returns, int confidence_percent) {
    if (returns.empty() || confidence_percent < 1 || confidence_percent > 99) return std::nullopt;
    const std::size_t n = returns.size();
    const std::size_t tail = static_cast<std::size_t>(100 - confidence_percent);
    const std::size_t k = (n * tail + 99) / 100 - 1;  // ceil(n * tail / 100) - 1 with integers
    std::nth_element(returns.begin(), returns.begin() + static_cast<std::ptrdiff_t>(k), returns.end());
    const double quantile = returns[k];
    return quantile < 0 ? -quantile : 0.0;
}

inline std::optional<double> sharpe_ratio(const Summary& s, double risk_free_per_period, double periods_per_year) {
    if (!(s.stdev > 0) || !(periods_per_year > 0)) return std::nullopt;
    return (s.mean - risk_free_per_period) / s.stdev * std::sqrt(periods_per_year);
}

}  // namespace risk
