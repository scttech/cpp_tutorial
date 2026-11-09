# Test-out: Top-K words

> **Already at home with the standard library?** Pass this challenge (the **Works** and **Robust** tiers) and Module 6 is marked complete. You can still open any of its lessons later.

## The task

Write `top_k_words` in `top_k.hpp`: the `k` most frequent words of a text, using **standard containers and algorithms**.

```cpp
top_k_words("The quick brown fox jumps over the lazy dog. The dog barks; the fox runs. A quick fox!", 4)
// -> { "the", "fox", "dog", "quick" }
```

`main.cpp` (read-only) is a small demo: press **Run**.

### The rules

| | |
|---|---|
| A word | a maximal run of **ASCII letters**. Digits, spaces, punctuation, apostrophes and bytes above 127 all separate words (`don't` is `don` and `t`). |
| Case | `The` and `the` are the same word; the result is in **lower case**. |
| Order | most frequent first; words with **equal counts in alphabetical order**. |
| Size | `k` words, or fewer if the text has fewer distinct words. `k == 0`, or no words: an empty vector. |
| Parameters | `std::string_view text` (a `std::string` or a literal both convert), `std::size_t k`. |

### The tiers

- **Works** and **Robust** are the behavior above, including the edge cases (empty text, huge `k`, UTF-8 bytes).
- **Fast** counts heap allocations on a text of 6000 *long* words: one `std::string` per word is too many. Count with `std::string_view` keys into one lower-cased copy of the text, and pick the winners with `std::partial_sort`.
