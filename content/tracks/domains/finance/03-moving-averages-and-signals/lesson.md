# Moving Averages and Signals

> **You're an engineer at a trading firm.** The strategy needs the 50-price and 200-price moving averages of a stream of ticks, thousands of times per second, for hundreds of instruments. The research prototype recomputes each average from scratch with a loop over the window; it burns a CPU core. The traders also noticed that after a long run the averages are slightly *wrong*. You are asked for streaming versions that update in constant time and never go wrong.

## The indicators

- **`Sma(window)`**: the simple moving average of the last `window` prices. Keep a **ring buffer** of the last prices and a **running sum**: when a price arrives, add it and subtract the one that falls out; the update is O(1) whatever the window, and the memory is fixed.
- **`Ema(period)`**: the exponential moving average, `ema = α·price + (1−α)·ema` with `α = 2 / (period + 1)`; the first price seeds it. One number of state.
- **`Crossover(fast, slow)`**: two SMAs over the same prices. When the fast one rises above the slow one the trend is turning up (**Buy**); when it falls below, **Sell**. One signal per crossing, none while it stays on one side.

## The two traps

1. **Drift.** A running sum is exact in mathematics and *not* in floating point: every `sum += x; sum -= old` rounds, and the errors accumulate. If a price of 1,000,000,000,000 passes through the window, the sum keeps a rounding error of about 10⁻⁴ for ever, and after 200000 such prices the average is visibly off. The cure is **compensated summation** (Kahan or, better here, **Neumaier**): keep a second variable that collects the low-order bits lost by each addition and add it back when you read the sum. The Robust tests build exactly the scenario that breaks a plain running sum.
2. **Bad input.** A `NaN` in the stream would poison a running sum for ever (NaN + anything = NaN, and subtracting it later does not repair it). Ignore non-finite prices: no change of state.

Edge rules: a `window` of 0 counts as 1; before the window is full `push` returns `nullopt`; a crossover needs both averages ready, and the first difference only decides which side we start on (no signal); a difference of exactly zero is neutral (`-1, 0, +1` is one Buy).

## The tiers

- **Works**: SMA, EMA with exact values, windows of 0/1, the crossover, reset.
- **Robust**: the drift scenario (200000 × a huge price and a small one), a 300000-price random walk against a from-scratch sum, NaN/infinity, a window of a million, EMA convergence, crossover neutral differences, no repeated signals, and a sine wave checked against the sign changes of the averages.
- **Fast**: 2 million updates through a window of 1 million (O(1)); no allocation at all after construction; memory bounded by the window.
- **Clean**: no raw memory, no `float`, no `accumulate`/`deque`/`erase` in the update, `std::isfinite` for validation.

## Tips

`Neumaier`: `t = sum + v; if (|sum| >= |v|) c += (sum - t) + v; else c += (v - t) + sum; sum = t;` and the true sum is `sum + c`. Use `head_ = (head_ + 1) % window_` for the ring; when the buffer is full, `buffer_[head_]` is the price about to be overwritten, i.e. the oldest.
