# Test-out: Test the Stack

> **Already write unit tests as a habit?** Pass this challenge (the **Works** and **Robust** tiers) and Module 8 is marked complete. You can still open any of its lessons later.

## The task

This time you do not write the code, you write the **tests**. `stack.hpp` (read-only) holds `BoundedStack`, a stack of `int` with a fixed capacity. Its comment at the top says what it promises. Write unit tests in `stack_test.cpp` with **GoogleTest**.

```cpp
TEST(BoundedStack, PopReturnsTheNewestItem) {
    BoundedStack s(3);
    s.push(1);
    s.push(2);
    EXPECT_EQ(s.pop(), 2);
}
```

Press **Run** to build and run your tests like any GoogleTest program (`[  PASSED  ]` or a list of failures). Press **Run Tests** to have them graded.

## How the grading works

Good tests do two things: they **pass on correct code**, and they **fail when the code is wrong**. Run Tests checks both.

1. Your tests run against the real `BoundedStack`. They must all pass.
2. Then they run against several **broken copies** of `BoundedStack`, each with one realistic bug (a pop that returns the wrong end, a push that silently ignores a full stack, ...). Your tests must **fail** on each one. A copy that your tests do not notice is listed by name, with a hint.

This is called *mutation testing*: it measures whether your tests can actually find bugs, which is not the same as whether they run the code.

## The tiers

- **Works**: the everyday behavior (what `pop`, `top`, `size` and `empty` return).
- **Robust**: the edges: a full stack, an empty stack, `clear`, and a capacity of zero. The documentation promises exceptions there; a test for an exception uses `EXPECT_THROW(statement, ExceptionType)`.
- **Clean**: your test file compiles without warnings.

## Hints about the tools

- `TEST(SuiteName, TestName) { ... }` defines a test; each test gets a fresh stack, so tests cannot affect one another.
- `EXPECT_EQ(actual, expected)`, `EXPECT_TRUE(x)`, `EXPECT_FALSE(x)` record a failure and **carry on**; `ASSERT_*` stops the test at the first failure.
- `size()` returns `std::size_t`, an unsigned type: compare with `1u` (comparing signed and unsigned numbers is a classic source of bugs).
- One test per behavior, named after the behavior, is easier to read than one long test.
