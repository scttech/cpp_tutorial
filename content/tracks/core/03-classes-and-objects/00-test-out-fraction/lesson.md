# Test-out: Fraction class

> **Already comfortable designing a class with operators?** Pass this challenge (both the **Works** and **Robust**
> tiers) and Module 3 is marked complete. You can still open any of its lessons later.

## The task

Complete the `Fraction` class in `fraction.hpp`. A `Fraction` is a rational number such as `3/4`. `main.cpp` (read-only)
is a small demo of it: press **Run** to see what it prints.

### The representation

A fraction is always stored **in lowest terms with a positive denominator**:

| You write | It is stored as |
|---|---|
| `Fraction(6, 8)` | `3/4` |
| `Fraction(1, -2)` and `Fraction(-1, -2)` | `-1/2` and `1/2` |
| `Fraction()`, `Fraction(0)`, `Fraction(0, 5)` | `0/1` |
| `Fraction(3)` | `3/1` |

Doing this once, in the constructor, is the whole trick: every other member can then assume the fraction is
normalized, and two fractions with the same value have exactly the same members. (That makes `==` a one-liner.)

A zero denominator is an error: throw **`std::invalid_argument`**.

### What to implement

| Part | Behavior |
|---|---|
| `numerator()`, `denominator()` | The stored parts. |
| `+ - * /`, unary `-` | The usual arithmetic; **results are normalized**. Dividing by a zero fraction throws **`std::domain_error`**. |
| `+= -= *= /=` | The same, in place. Build the binary operators from these. |
| `== !=  <  <=  >  >=` | By value: `1/2 == 2/4`. One `<=>` plus a defaulted `==` gives all six. |
| `operator<<` | `3/4`, `-3/4`; a whole number prints without the denominator: `2`, not `2/1`. |
| `explicit operator double()` | The value as a double, **only when asked for** with `static_cast<double>(f)`. |

### Things to watch

- **`2 + f` as well as `f + 2`.** If `operator+` is a member function, the left operand is `*this` and never gets
  converted, so `2 + f` does not compile. Make the binary operators non-members (or friends); the tests check this.
- **`explicit`.** A whole number should convert to a `Fraction` silently (`Fraction f = 3;`), but a `Fraction` should
  not turn into a `double` silently: that makes expressions like `f + 2` ambiguous and hides lost precision.
- **Overflow.** The numbers are `long long`. Multiplying `4000000000/4000000007` by `4000000007/3999999999` straight
  across needs `4000000000 * 4000000007`, about 1.6e19, which does not fit, even though the answer is a perfectly
  ordinary `4000000000/3999999999`. Cancel common factors (`std::gcd`) *before* multiplying. Adding fractions with the
  same big denominator has the same problem: use the **least** common denominator.
- **Compare exactly.** Converting both sides to `double` and comparing is tempting and wrong: two different fractions
  can become the same `double`. Cross-multiply instead (the denominators are positive, so the order is preserved).
