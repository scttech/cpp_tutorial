// Statistics that stay accurate on awkward data. Read-only. Press Run.
#include <cmath>
#include <cstdio>
#include <vector>

#include "stats.hpp"

int main() {
    // four measurements with a huge common offset: the spread is tiny, the values are not
    const std::vector<double> xs = {1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16};
    stats::Running r;
    double sum = 0, sum_sq = 0;
    for (double x : xs) {
        r.add(x);
        sum += x;
        sum_sq += x * x;
    }
    const double n = static_cast<double>(xs.size());
    std::printf("variance by the textbook formula (sum_sq - sum^2 / n) / (n - 1): %.4f\n", (sum_sq - sum * sum / n) / (n - 1));
    std::printf("variance by Welford's update                                   : %.4f   (the true value is 30)\n", r.variance().value_or(-1));

    stats::CompensatedSum cs;
    double naive = 0;
    for (int i = 0; i < 1000000; ++i) {
        cs.add(0.1);
        naive += 0.1;
    }
    std::printf("one million times 0.1: naive %.10f, compensated %.10f\n", naive, cs.value());

    const std::vector<double> data = {15, 20, 35, 40, 50};
    std::printf("median %.1f, 25th percentile %.1f, 90th percentile %.1f\n", stats::median(data).value_or(-1), stats::percentile(data, 25).value_or(-1), stats::percentile(data, 90).value_or(-1));
}
