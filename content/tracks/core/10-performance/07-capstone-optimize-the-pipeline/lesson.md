# Capstone: Optimize the Pipeline

> **A rewrite you can trust.** Fast code that gives a different answer is a bug. This capstone is the real workflow of performance work: a correct but slow program, a test suite that pins down its behaviour, and budgets to hit. You may change *how* it works as much as you like; you may not change *what* it computes.

## The program

`analyze()` reads a log, one line per entry:

```
ERROR user42 1250
INFO  alice  17
```

A **level** (`INFO`, `WARN` or `ERROR`, case-sensitive), a **user name** and a number of **milliseconds**, separated by any amount of spaces or tabs. It counts the understood lines per level, the different users, and finds the three users with the largest total of milliseconds (ties: the name that sorts first). A line that is not exactly that is **skipped** and counted: an unknown level, a missing or extra field, or milliseconds that are not 1 to 9 digits (no sign, no letters, no decimal point).

`pipeline.hpp` implements it correctly, and slowly. `types.hpp` (read-only) has the `Report` it returns, a `UserId` that counts its comparisons and copies, and `make_log`, a generator for test data.

## What to do

1. **Press Run.** `main.cpp` prints the report and three numbers for a log of 5000 lines: `UserId` comparisons, `UserId` copies and heap allocations. One of them is *very* large for the size of the log. The others are too.
2. **Decide what to fix first** with the numbers, not with your taste in code. Everything you need was in this module:
   - *Measuring performance*: find the stage that dominates; algorithm before micro-tuning.
   - *Memory and allocation*: where do the allocations come from? What gets copied that could be looked at in place, or moved?
   - *Reading performance problems*: the usual suspects (a search per element, a copy per element, a temporary per element).
3. **Keep the tests green.** The Works and Robust tests are the safety net, and they are strict about the corners: tabs, `\r`, `+5`, `12x`, ten digits, level prefixes, ties.
4. **Re-run after every change.** One change at a time tells you which one paid off.

## The tiers

- **Works**: the right report on small and generated logs.
- **Robust**: every kind of malformed line, whitespace, a 9-digit number, big totals, ties, fewer than three users.
- **Fast**: at most 150 000 `UserId` comparisons, 150 copies and 1200 allocations for 5000 lines; and a scaling test.
- **Clean**: no streams or `sscanf`, no raw `new`/`delete`; the line is looked at in place with `std::string_view` and the number converted with `std::from_chars`.

## Tools you will want

`std::string_view` (a pointer and a length into the existing text, no copy), `std::from_chars` (number conversion that does not allocate, throw or look at the locale), `std::unordered_map` (find a user in constant time), `operator[]`/`try_emplace` with a temporary key (the key is moved in), and a small array that holds the best three (no need to sort everybody).
