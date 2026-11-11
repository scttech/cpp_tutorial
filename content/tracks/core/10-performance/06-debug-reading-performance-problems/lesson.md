# Debug challenge: Reading performance problems

> **The numbers tell you where; you have to work out why.** Each of the six functions in `slow.hpp` is correct and wastes work in a different, very common way. `main.cpp` prints what each one costs. Your job is the one you will do on real code: read the numbers, find the cause behind each one, and fix the cause without changing a single result.

## How to work

1. Press **Run**. The table shows copies, moves, comparisons and heap allocations for realistic inputs. Do not start with the code: start with the numbers. Which ones are out of proportion for the amount of data? (2000 numbers should not need two million moves.)
2. For each suspect, ask: *what does this code do on every iteration that it could do once?*
3. Fix it, run again, and check that the number dropped and the answers did not change.

## The six suspects (the usual ones)

- **A `std::vector` that is changed at the front.** `insert(begin(), x)` moves every element after it: `n` insertions move `n²/2` elements. Build in the order the data comes, or construct the result from a range.
- **A big object passed by value into a helper called in a loop.** Each call copies the whole container. Take `const&`.
- **String building with `+`.** `s = s + w` builds a brand-new string for the sum, then replaces `s`; in a loop that is a copy of everything so far, every time. `reserve()` once, then `+=`/`append`.
- **A search that is repeated for every element.** Looking for duplicates with two nested loops is `n²` comparisons; sorting a copy and looking at the neighbours is `n log n` (or use a hash set).
- **Asking twice.** `count(key)` then `at(key)` is two tree searches; `find(key)` once gives the answer and the position.
- **A loop variable that is a copy.** `for (std::string word : words)` copies every string (a heap allocation for each one that is longer than the small-string buffer). `for (const std::string& word : words)` copies nothing.

## The tiers

- **Works / Robust**: the answers stay the same, including empty inputs, ties, empty words and the position of separators.
- **Fast**: each function has its own budget (copies, moves, comparisons or allocations). A budget that is missed names the function.
- **Clean**: no raw `new`/`delete`, no `std::endl`, no `using namespace`.
