# Fixed-Point Money

> **You're an engineer at a payments company.** The invoice total is off by a cent on some invoices. Finance found the cause: prices are `double`. `0.1 + 0.2` is `0.30000000000000004` in binary floating point, and after a few thousand additions and a 8.25% tax the cents drift. Auditors do not accept "approximately". You are asked to write the one money type that the whole code base will use.

## The type

`Money` in `money.hpp` stores a whole number of **cents** in an `int64_t`. All arithmetic is integer arithmetic, so it is exact, and every operation that **cannot** be done exactly says so (`std::optional`) instead of silently wrapping around. **No `float` or `double` may appear in the file**, not even for an intermediate.

| Operation | Result |
|---|---|
| `checked_add / checked_sub / checked_mul(Money, n)` | the result, or `nullopt` when it does not fit an `int64_t` |
| `negated()`, `abs()` | `nullopt` for `INT64_MIN` (it has no positive twin) |
| `divided(n, mode)` | rounded to a cent; `nullopt` for `n == 0` and for `INT64_MIN / -1` |
| `percent(bp, mode)` | `amount * bp / 10000` rounded; the product can exceed 64 bits even when the answer fits: use `__int128` |
| `allocate(parts)` | shares that add up **exactly**; larger shares first (`100` in 3 → `34, 33, 33`) |
| `allocate_by_ratio(r)` | proportional shares, truncated, with the left-over cents handed out one by one from the first share |
| `to_string()` | `$12.34`, `-$0.05`; must work for `INT64_MIN` |
| `Money::parse(text)` | strict: `-`? `$`? digits (`.` one-or-two digits)?; nothing else; out-of-range is `nullopt` |

## Rounding modes

When a division leaves a remainder, which whole cent do you pick?

- **`Down`**: toward zero (`2.9 → 2`, `-2.9 → -2`). C++ integer division already does this.
- **`HalfUp`**: to the nearest, and an exact half goes **away from zero** (`2.5 → 3`, `-2.5 → -3`).
- **`HalfEven`** (banker's rounding): to the nearest, and an exact half goes to the **even** neighbour (`2.5 → 2`, `3.5 → 4`). Over many roundings the errors cancel instead of accumulating upward; it is the default in IEEE 754 and in many accounting rules.

The classic traps: C++ `%` and `/` **truncate toward zero**, so a "round half up" written as `(a + b/2) / b` is wrong for negative numbers; and "is the remainder exactly half?" must compare `2 * |remainder|` with `|divisor|`, never use floating point.

## Why `allocate` matters

Splitting $100.00 three ways is $33.33 × 3 = $99.99: a cent has vanished. Payment systems never allow that. `allocate` computes the truncated share and then hands out the **remaining cents one at a time**, so the shares always add up to the original amount.

## The tiers

- **Works**: construction and comparison, exact arithmetic, the formatting, the strict parser, all three rounding modes on positive and negative numbers, percent, allocation.
- **Robust**: overflow right at the edges of `int64_t`, `INT64_MIN` everywhere, `n = 0`, ties alternating for half-even, percent with a product beyond 64 bits, allocation invariants over 3000 random cases, 28 malformed strings, `parse(to_string(x)) == x`, and 4000 random divisions against an independent reference.
- **Fast**: an 8-byte trivially copyable value; no allocation in arithmetic or parsing (300000 iterations); a million additions of 0.10 are exact.
- **Clean**: no floating point at all, no exceptions or throwing conversions, `std::optional` for every failure.

## Tips

Do the arithmetic in `__int128` (a GCC/Clang extension, 128-bit integer) and check the result against the `int64_t` range: it makes every overflow test a one-liner. To print `INT64_MIN` convert to `uint64_t` *before* negating (`0 - static_cast<uint64_t>(x)`). `std::from_chars` parses digits without exceptions.
