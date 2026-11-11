// MUTANT: the previous difference is never updated
#pragma once
// Streaming indicators: a ring buffer + a Neumaier-compensated running sum (SMA), the recursive formula (EMA), and a crossover detector.
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "types.hpp"

namespace ma {

class Sma {
public:
    explicit Sma(std::size_t window) : window_(window == 0 ? 1 : window), buffer_(window_, 0.0) {}

    std::optional<double> push(double x) {
        if (!std::isfinite(x)) return value();
        if (count_ == window_) add(-buffer_[head_]);  // the oldest price leaves
        else ++count_;
        buffer_[head_] = x;
        head_ = (head_ + 1) % window_;
        add(x);
        return value();
    }

    std::optional<double> value() const {
        if (count_ < window_) return std::nullopt;
        return (sum_ + compensation_) / static_cast<double>(window_);
    }
    std::size_t count() const { return count_; }
    bool ready() const { return count_ == window_; }

    void reset() {
        count_ = 0;
        head_ = 0;
        sum_ = compensation_ = 0;
    }

private:
    std::size_t window_;
    std::vector<double> buffer_;  // the last prices, a ring: allocated once
    std::size_t head_ = 0;        // where the next price goes (= the oldest price when full)
    std::size_t count_ = 0;
    double sum_ = 0;
    double compensation_ = 0;     // the rounding errors of the additions so far (Neumaier)

    void add(double v) {
        const double t = sum_ + v;
        if (std::fabs(sum_) >= std::fabs(v)) compensation_ += (sum_ - t) + v;
        else compensation_ += (v - t) + sum_;
        sum_ = t;
    }
};

class Ema {
public:
    explicit Ema(std::size_t period) : alpha_(2.0 / (static_cast<double>(period == 0 ? 1 : period) + 1.0)) {}

    double push(double x) {
        if (!std::isfinite(x)) return value_;
        value_ = seeded_ ? alpha_ * x + (1.0 - alpha_) * value_ : x;
        seeded_ = true;
        return value_;
    }
    std::optional<double> value() const {
        if (!seeded_) return std::nullopt;
        return value_;
    }
    bool ready() const { return seeded_; }
    void reset() {
        seeded_ = false;
        value_ = 0;
    }

private:
    double alpha_;
    double value_ = 0;
    bool seeded_ = false;
};

class Crossover {
public:
    Crossover(std::size_t fast, std::size_t slow) : fast_(fast), slow_(slow) {}

    Signal push(double price) {
        if (!std::isfinite(price)) return Signal::None;
        const auto f = fast_.push(price);
        const auto s = slow_.push(price);
        if (!f || !s) return Signal::None;
        const double diff = *f - *s;
        Signal signal = Signal::None;
        if (have_previous_) {
            if (previous_diff_ <= 0 && diff > 0) signal = Signal::Buy;
            else if (previous_diff_ >= 0 && diff < 0) signal = Signal::Sell;
        }
        if (!have_previous_) previous_diff_ = diff;
        have_previous_ = true;
        return signal;
    }

private:
    Sma fast_, slow_;
    double previous_diff_ = 0;
    bool have_previous_ = false;
};

}  // namespace ma
