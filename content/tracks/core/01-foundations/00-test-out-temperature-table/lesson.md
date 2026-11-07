# Test-out: Temperature table

> **Already comfortable with C++ basics?** Pass this challenge (both the **Works** and **Robust** tiers) and Module 1 is
> marked complete. You can still open any of its lessons later.

## The task

Write a program that reads three integers from standard input: `start end step` (degrees Celsius), separated by
whitespace (spaces or newlines), and prints a table of Celsius and Fahrenheit temperatures from `start` to `end`
inclusive in increments of `step`.

Fahrenheit is `C * 9/5 + 32`.

### Format

First a header, then one row per temperature. Every line ends with a newline.

- The Celsius column is a right-aligned field **8** characters wide.
- The Fahrenheit column is a right-aligned field **12** characters wide, printed with exactly **one decimal place**.
- The header uses the same widths with the words `Celsius` and `Fahrenheit`.

```
Input:  0 30 10

Output:
 Celsius  Fahrenheit
       0        32.0
      10        50.0
      20        68.0
      30        86.0
```

Celsius values are integers. Fahrenheit values often are not: `1` degree Celsius is `33.8` Fahrenheit.

### Errors

Print exactly one line and nothing else (no header) for each of these, checked in this order:

| Problem | Output |
|---|---|
| The input does not start with three integers | `error: expected three integers: start end step` |
| `step` is zero or negative | `error: step must be positive` |
| `start` is greater than `end` | `error: start must not exceed end` |

### Things to watch

- `1 * 9 / 5` in `int` arithmetic is not `1.8`. You have met this before.
- `end` can be as large as `INT_MAX`. Think about what `c += step` does when `c` is near the top of the `int` range
  (signed overflow is undefined behavior).
- Stream formatting flags such as `std::fixed` stay set until changed: set them where you need them, and check that the
  Celsius integers are not affected.
