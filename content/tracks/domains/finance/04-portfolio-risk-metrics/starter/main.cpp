// Risk figures of a made-up price series. Read-only. Press Run.
#include <cmath>
#include <cstdio>
#include <vector>

#include "risk.hpp"

int main() {
    const std::vector<double> prices = {100, 102, 101, 105, 110, 104, 99, 103, 108, 112, 109, 115};
    const auto s = risk::summarize(risk::Series(prices));
    if (!s) {
        std::printf("invalid price series\n");
        return 1;
    }
    std::printf("%zu returns: mean %.4f, stdev %.4f, best %.4f, worst %.4f\n", s->returns, s->mean, s->stdev, s->best, s->worst);
    std::printf("total return %.4f, max drawdown %.4f\n", s->total_return, s->max_drawdown);
    if (const auto sharpe = risk::sharpe_ratio(*s, 0.0001, 252)) std::printf("Sharpe ratio (252 periods a year): %.3f\n", *sharpe);

    std::vector<double> r;
    for (std::size_t i = 1; i < prices.size(); ++i) r.push_back(prices[i] / prices[i - 1] - 1);
    if (const auto var = risk::historical_var(r, 90)) std::printf("90%% historical VaR: %.4f (lose more than this on 10%% of periods)\n", *var);

    // the precision trap: variance of numbers that are all about a billion
    const std::vector<double> big = {1e9 + 4, 1e9 + 7, 1e9 + 13, 1e9 + 16};
    std::printf("stdev of 1e9+{4,7,13,16}: %.6f (exact 5.477226)\n", *risk::stdev(big));
}
