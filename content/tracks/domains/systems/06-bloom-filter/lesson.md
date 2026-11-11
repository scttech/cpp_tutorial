# Bloom Filter

> **You're an engineer on a systems team.** Every request to your service looks up a user, a URL or a product in a database that lives on another machine, and 95% of those lookups are for things that do not exist (crawlers, typos, attackers). Each miss costs a network round trip. A **Bloom filter** sits in front: a tiny structure in memory that answers *"definitely not present"* (skip the database) or *"probably present"* (go and check). It is wrong only in one direction, and only occasionally, and you decide how often.

## How it works

A bit array of `m` bits, all zero, and `k` hash functions.

- `add(x)`: set the `k` bits `h_0(x) .. h_{k-1}(x)`.
- `might_contain(x)`: are all `k` bits set? If any is clear, `x` was **certainly never added**. If all are set, it *probably* was (other items may have set those bits: a **false positive**).
- There is **no false negative**, ever, and no deletion (clearing a bit could forget another item).

**Double hashing.** You do not need `k` independent hash functions: two are enough. Bit `i` of item `x` is `(h1 + i * h2) mod m` (Kirsch and Mitzenmacher, 2006). `types.hpp` gives you `hash_pair`; the formula and the unsigned 64-bit wrap-around are part of the contract, so the tests can check exactly which bits you set.

## Sizing: the two formulas

For `n` expected items and a false-positive rate `p`:

```
m = ceil( -n * ln(p) / (ln 2)^2 )          k = round( m / n * ln 2 )          (at least 1)
```

Rounding matters and the tests pin it down: `m` rounds **up** (never under-provision) and `k` to the **nearest** integer. Examples: `n = 1000, p = 1%` gives 9586 bits (about 1.2 KB) and 7 hashes; one million items at 1% need only 1.2 MB, 9.6 bits per item, no matter how long the items themselves are. An optimally sized filter is **half full** when it holds its `n` items.

## Statistics from the bits alone

- `bits_set()` must be **O(1)**: keep a running count and increment it only when a bit goes from 0 to 1 (an item added twice must not count twice). Recounting with a loop is O(m).
- `fill_ratio = bits_set / m`; `estimated_fpr = fill_ratio^k`;
- `estimated_count = -(m / k) * ln(1 - fill_ratio)`: you can estimate how many *distinct* items were added from the number of set bits, without having stored them. (Infinite when every bit is set: the filter has saturated.)

## Merging and saving

- **`merge`** is a bitwise OR, valid only for filters of the **same shape** (`m` and `k`); otherwise return `false` and change nothing. Because the filter is a union-friendly structure, you can build partial filters on many machines and combine them.
- **`to_bytes` / `from_bytes`**: 8 bytes `m` (little endian), 4 bytes `k`, then `ceil(m/8)` bytes with bit `i` in bit `i % 8` of byte `i / 8`. `from_bytes` reads **untrusted bytes** (a file, a network packet), so it must reject everything malformed: too short, `m == 0`, `k == 0`, wrong length, set **padding bits** in the last byte. And it must do so **before allocating**: a 12-byte header that claims `m = 2^62` must not make you try to allocate 512 petabytes (compare `m` with the data you actually have, and beware that `m + 7` can overflow).

## The tiers

- **Works**: the exact bits set (checked against an independent model through `to_bytes`), the formulas for `m` and `k`, edge cases, statistics, union, layout and validation of the serialized form.
- **Robust**: 50000 members with no false negative, the measured false-positive rate against the requested one (1%, 0.1% and 5%), `estimated_count` within 3%, word boundaries at 63/64/65 bits, 150 random filters through add, merge, save and load, hostile headers.
- **Fast**: no allocation in `add`/`might_contain` (even for long items), `bits_set` in O(1) (200000 statistics calls on a 2-million-bit filter), 3 million adds and queries, a single allocation for `to_bytes`, no allocation for a rejected header.
- **Clean**: no raw memory or exceptions, a hand-made bit array (no `std::bitset`, no `vector<bool>`), and `std::popcount`.
