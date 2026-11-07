# Control flow

## `if` / `else`

```cpp
if (n % 2 == 0) {
    std::cout << "even\n";
} else if (n > 100) {
    std::cout << "big and odd\n";
} else {
    std::cout << "odd\n";
}
```

**Always use braces**, even for one-line bodies. Without them, the "dangling else" and "added a second line but it is
not part of the if" bugs are easy to write. Since C++17 you can declare a variable in the condition:
`if (auto it = m.find(k); it != m.end()) { ... }`.

## `switch`

```cpp
switch (code) {
    case 1:
    case 2:
        std::cout << "low\n";
        break;          // without break, execution FALLS THROUGH into the next case
    case 3:
        std::cout << "three\n";
        break;
    default:
        std::cout << "other\n";
}
```

`switch` works on integers and enums, not strings. Stacking `case` labels (as with 1 and 2 above) is the intended use of
fallthrough. If you ever mean to fall through *with* code in between, mark it `[[fallthrough]];` so readers
and the compiler know it is deliberate.

## Loops

```cpp
for (int i = 0; i < 10; ++i) { }          // classic
for (const auto& x : container) { }       // range-for: read-only, no copies
while (cond) { }
do { } while (cond);                       // body runs at least once
```

Use `break` to leave a loop early and `continue` to skip to the next iteration.

**Index type trap.** `container.size()` returns `std::size_t`, an *unsigned* type:

```cpp
for (int i = v.size() - 1; i >= 0; --i) { }       // warning: signed/unsigned; breaks for huge sizes
for (std::size_t i = v.size(); i-- > 0; ) { }      // safe reverse loop
```

If `v` is empty, `v.size() - 1` is not `-1`; it wraps to a gigantic number. Remember the unsigned wraparound from the
last lesson.

## Your task

Implement three functions in `main.cpp`:

| Function | Behavior |
|---|---|
| `std::string fizzbuzz(int n)` | `"FizzBuzz"` if `n` is divisible by 15, `"Fizz"` if by 3, `"Buzz"` if by 5, otherwise the number as a string (`std::to_string`). |
| `std::string day_type(int day)` | Use a `switch`. Days 1-5 are `"weekday"`, 6-7 are `"weekend"`, anything else is `"invalid"`. |
| `std::vector<int> primes_up_to(int n)` | All primes `<= n` in increasing order, using the **Sieve of Eratosthenes**. Return an empty vector when `n < 2`. |

**Robust** adds edge inputs (0, negative numbers, out-of-range days). **Fast** asks for the primes up to 5,000,000 in
a couple of seconds, which rules out trial-dividing every number. Mark composites in a `std::vector<bool>` instead.
