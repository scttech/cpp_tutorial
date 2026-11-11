# Portfolio Risk Metrics

> **You're an engineer at an asset manager.** The risk report needs, for every portfolio and every night: the volatility, the worst fall, the 95% value-at-risk and a Sharpe ratio, from a time series of prices. The spreadsheet version takes hours and its standard deviation is sometimes *negative under the square root* (that is, wrong). You are asked for a library that is fast, single-pass and numerically sound.

## The metrics

All "returns" are **simple returns**: `r[i] = price[i] / price[i-1] - 1`.

- **Mean** and **sample standard deviation** (volatility). *Sample* means dividing by `n - 1`, not `n`.
- **Total return**: `last / first - 1`.
- **Max drawdown**: the largest fall from a previous peak, as a fraction of that peak. Track the highest price so far; at each price `(peak - price) / peak`; keep the maximum. `100 → 120 → 60` is a 50% drawdown (from 120), regardless of what happened before.
- **Historical VaR** at confidence `c`%: sort the returns; the loss exceeded in `(100 - c)%` of the periods is the element at position `k = ceil(n * (100 - c) / 100) - 1`. Report it as a positive loss (0 if that quantile is a gain). **Compute `k` with integers**: in floating point `(1 - 0.95) * 100` is `5.000000000000004`, `ceil` gives 6, and your VaR is one element off.
- **Sharpe ratio**: `(mean - risk_free) / stdev * sqrt(periods_per_year)`, undefined (`nullopt`) when the stdev is 0.

## The numerical trap: the "textbook" variance

`variance = (Σx² − n·mean²) / (n−1)` is exact in mathematics and awful in floating point: for numbers around 10⁹ with a spread of 5, `Σx²` is around 10¹⁸ and the subtraction cancels all the significant digits (the result may even be **negative**). **Welford's algorithm** keeps a running mean and a running sum of squared deviations and never subtracts two huge numbers:

```
n += 1;  delta = x - mean;  mean += delta / n;  m2 += delta * (x - mean);   // variance = m2 / (n - 1)
```

It also works in a **single pass**, which matters when the series is read from disk or a database.

## Validation

Real price feeds contain zeros, negatives, `NaN`. Every function returns `std::optional` and refuses bad input instead of producing garbage: fewer than two prices, a price ≤ 0 or non-finite, a non-finite return, a confidence outside 1..99.

## The tiers

- **Works**: mean, sample stdev, `summarize` on a worked example, drawdown shapes, VaR, Sharpe.
- **Robust**: the cancellation test (1e9 + {4, 7, 13, 16}), constant series, NaN/∞ everywhere, invalid prices, a single return, all-negative/all-positive returns (the "best/worst start at 0" bug), a 99.9% crash, the integer quantile index at 100/20/7 values, validation of the confidence, caller's vector untouched, and 200 random paths checked against direct formulas.
- **Fast**: `summarize` reads each price once (≤ 2 reads per price on 200000 prices, counted by `Series`) and allocates nothing; the quantile uses `nth_element` rather than a full sort (5 million returns in time).
- **Clean**: no raw memory, no `float`, no `pow`/`accumulate` (variance by Welford), `std::optional`, no `std::sort`.

## Tips

`summarize` can do everything in one loop: keep `previous` (the last price), `peak`, the Welford state, `best` and `worst` (initialise them from the *first* return, not from 0). `std::nth_element(first, first + k, last)` puts the k-th smallest element at position k in O(n).
