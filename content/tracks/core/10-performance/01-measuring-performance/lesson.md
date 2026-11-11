# Measuring performance

> **The rule of this module: measure first.** Programmers are bad at guessing where a program spends its time. A function can *look* wasteful (a vector passed by value, a string built line by line) and cost almost nothing, while an innocent-looking loop does millions of operations. Tuning code you have not measured is just rewriting it.

## Big-O and constants

Two numbers describe the cost of code:

- **How it grows** (big-O): looking for a duplicate by scanning a list is `O(n)` per item, so deduplicating `n` items is `O(n²)`: 10 times more items, 100 times more work. A balanced tree (`std::set`) makes each lookup `O(log n)`; a hash table (`std::unordered_set`) `O(1)` on average.
- **The constant factor**: what one step costs. A copy of a big object costs more than a comparison of two ints; a cache miss costs more than a cache hit. Constants matter for fixing a small program; **growth decides whether a big program is usable at all.** Fix the growth first.

## Measuring without a stopwatch

A stopwatch is the obvious tool, and a poor one: the number changes with the computer, the compiler flags, what else is running, and (in a browser) the page itself. On this site speed is measured with **counters**: a type that counts the `==`, `<`, copies or allocations done to it (you met one in the test-out). The counts do not depend on the machine, so a test can say "at most 150 000 comparisons" and mean it. And a **scaling test** compares the cost at `n` and at `2n`: about double means `O(n)` or `O(n log n)`, about four times means `O(n²)`.

(In your own projects you will still use real timers and profilers, with optimization on, on a quiet machine, repeated several times. Lesson 5 shows the tools. The idea is the same: *find where the cost is before you change anything.*)

## The exercise: find the hot spot, then fix it

`report.hpp` builds a report in three stages: `dedupe` (one item per id), `rank` (best quantity first) and `render` (text). Press **Run**: `main.cpp` measures each stage with the counting `Item` from `meter.hpp` and prints a table of comparisons and copies.

1. **Find it.** Read the table. One stage does far more work than the others. Make `hot_stage()` return its name. (`rank` takes its vector *by value*, which looks wasteful: look at its copies in the table before you decide.)
2. **Fix it.** Make that stage cheap and keep the report **exactly** the same: the first item of each id is kept, in the order the ids first appeared; ties in `rank` keep their original order. The Fast tier has a comparison budget, a scaling test and a limit on copies.

The usual tools for "have I seen this one before?": a `std::set` or `std::unordered_set` of what you have seen (remember what `insert` returns), or sorting a copy.

## The tiers

- **Works**: the right hot stage, the right report.
- **Robust**: empty input, first occurrence kept, order of first appearance, stable ranking.
- **Fast**: a budget for 4000 items, twice the items at about twice the cost, and few copies.
