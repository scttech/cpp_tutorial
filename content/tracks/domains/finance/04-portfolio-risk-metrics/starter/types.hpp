#pragma once
// READ-ONLY. A price series that counts how often it is read, and the summary you compute.
#include <cstddef>
#include <vector>

namespace risk {

namespace stats {
inline unsigned long long reads = 0;  // calls of Series::operator[]
inline void reset() { reads = 0; }
}  // namespace stats

// A series of prices. Every access to a price is counted.
class Series {
public:
    Series() = default;
    explicit Series(std::vector<double> prices) : prices_(std::move(prices)) {}
    std::size_t size() const { return prices_.size(); }
    double operator[](std::size_t i) const {
        ++stats::reads;
        return prices_[i];
    }

private:
    std::vector<double> prices_;
};

// What summarize() computes. "return" means the simple return of one period: price[i] / price[i-1] - 1.
struct Summary {
    std::size_t returns = 0;     // how many returns (prices - 1)
    double mean = 0;             // mean return
    double stdev = 0;            // SAMPLE standard deviation of the returns (divides by n - 1); 0 when there are fewer than 2 returns
    double best = 0;             // the largest return
    double worst = 0;            // the smallest return
    double total_return = 0;     // last price / first price - 1
    double max_drawdown = 0;     // the largest fall from a previous peak, as a fraction of that peak: 0.25 means -25 %
};

}  // namespace risk
