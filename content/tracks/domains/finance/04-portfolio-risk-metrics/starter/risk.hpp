#pragma once
// YOUR RISK LIBRARY. Invalid input gives std::nullopt, never a crash or a NaN.
//
//   mean(values)                nullopt for an empty vector (or a NaN / infinity in it)
//   stdev(values)               SAMPLE standard deviation (divide by n - 1); nullopt for fewer than 2 values (or non-finite ones).
//                               Must stay accurate when the values are large and close together: use Welford's algorithm, not
//                               sum(x^2) - n*mean^2 (catastrophic cancellation).
//   summarize(prices)           ONE pass over the series: nullopt if there are fewer than 2 prices or any price is <= 0 or not finite.
//                               Fill every field of Summary (types.hpp). Read each price at most twice in total.
//   historical_var(returns, confidence_percent)
//                               the loss that is exceeded in (100 - confidence)% of the periods, as a positive number (0 if there
//                               is no loss at that level). Sort the returns ascending and take the element at position
//                               k = ceil(n * (100 - c) / 100) - 1, computed with INTEGERS (floating point turns 5.000000000000004
//                               into 6). nullopt for no returns, a non-finite return, or c outside 1..99.
//   sharpe_ratio(summary, risk_free_per_period, periods_per_year)
//                               (mean - risk_free) / stdev * sqrt(periods_per_year); nullopt if stdev is 0.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "types.hpp"

namespace risk {

inline std::optional<double> mean(const std::vector<double>& values) {
    (void)values;
    return std::nullopt;  // TODO
}
inline std::optional<double> stdev(const std::vector<double>& values) {
    (void)values;
    return std::nullopt;  // TODO
}
inline std::optional<Summary> summarize(const Series& prices) {
    (void)prices;
    return std::nullopt;  // TODO
}
inline std::optional<double> historical_var(std::vector<double> returns, int confidence_percent) {
    (void)returns;
    (void)confidence_percent;
    return std::nullopt;  // TODO
}
inline std::optional<double> sharpe_ratio(const Summary& s, double risk_free_per_period, double periods_per_year) {
    (void)s;
    (void)risk_free_per_period;
    (void)periods_per_year;
    return std::nullopt;  // TODO
}

}  // namespace risk
