#pragma once
// YOUR STREAMING INDICATORS. Each takes one price at a time and updates in constant time, using fixed memory.
//
// Sma(window)     simple moving average of the last `window` prices (window 0 counts as 1).
//   push(x)       adds a price; returns the average once `window` prices have been seen (nullopt before that).
//                 A price that is NaN or infinite is IGNORED: nothing changes, the current value is returned.
//   value(), count(), ready(), reset()
//   The update must be O(1): keep a running sum and a ring buffer of the last prices. Over millions of updates a plain running sum
//   DRIFTS (rounding errors pile up when big and small prices mix): compensate it (Kahan / Neumaier summation).
//
// Ema(period)     exponential moving average with alpha = 2 / (period + 1) (period 0 counts as 1).
//   push(x)       the first price is the first average; after that ema = alpha * x + (1 - alpha) * ema. Returns the new average.
//                 A NaN / infinite price is ignored (returns the current average, or 0 before any price).
//   value(), ready(), reset()
//
// Crossover(fast, slow)   two Smas over the same prices.
//   push(price)   Signal::Buy when fast - slow turns from <= 0 to > 0, Signal::Sell when it turns from >= 0 to < 0, else None.
//                 Nothing before BOTH averages are ready; the first difference only sets the starting side (no signal).
//                 A difference of exactly 0 is neutral: -1, 0, +1 gives one Buy (at +1). Bad prices are ignored (None).
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "types.hpp"

namespace ma {

class Sma {
public:
    explicit Sma(std::size_t window) : window_(window) {}
    std::optional<double> push(double x) {
        (void)x;
        return std::nullopt;  // TODO
    }
    std::optional<double> value() const { return std::nullopt; }  // TODO
    std::size_t count() const { return 0; }
    bool ready() const { return false; }
    void reset() {}

private:
    std::size_t window_;
    // TODO: ring buffer, position, count, running sum (+ compensation)
};

class Ema {
public:
    explicit Ema(std::size_t period) : period_(period) {}
    double push(double x) {
        (void)x;
        return 0;  // TODO
    }
    std::optional<double> value() const { return std::nullopt; }  // TODO
    bool ready() const { return false; }
    void reset() {}

private:
    std::size_t period_;
};

class Crossover {
public:
    Crossover(std::size_t fast, std::size_t slow) : fast_(fast), slow_(slow) {}
    Signal push(double price) {
        (void)price;
        return Signal::None;  // TODO
    }

private:
    Sma fast_, slow_;
};

}  // namespace ma
