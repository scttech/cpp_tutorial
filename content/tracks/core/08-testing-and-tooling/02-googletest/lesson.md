# GoogleTest

**GoogleTest** (often called *gtest*) is the most widely used C++ testing framework. It is free and open source (BSD license), it is what Google, Chromium and LLVM use, and you will meet it in many code bases. It is the framework from this module's first test-out, and now you look at it properly.

## The basics

```cpp
#include <gtest/gtest.h>

TEST(SuiteName, TestName) {
    // arrange, act, assert
    EXPECT_EQ(add(2, 3), 5);
}
```

There is no `main` to write: the library `gtest_main` provides one that finds every `TEST`, runs them all and prints a report. Press **Run** on any lesson in this module that uses GoogleTest to see it:

```
[ RUN      ] RingBufferTest.FrontIsTheOldestItem
[       OK ] RingBufferTest.FrontIsTheOldestItem (0 ms)
[ RUN      ] RingBufferTest.BackIsTheNewestItem
ring_buffer_test.cpp:31: Failure
Expected equality of these values:
  buffer.back()
    Which is: 1
  2
[  FAILED  ] RingBufferTest.BackIsTheNewestItem (0 ms)
```

A good failure message names the file and line, the expression, and the actual value. Read it from the top: *which test, where, what was compared, and what came out*.

## Assertions

| | meaning |
|---|---|
| `EXPECT_EQ(a, b)`, `EXPECT_NE`, `EXPECT_LT`, `EXPECT_LE`, `EXPECT_GT`, `EXPECT_GE` | compare two values (the first argument is the one you computed) |
| `EXPECT_TRUE(c)`, `EXPECT_FALSE(c)` | a condition |
| `EXPECT_THROW(stmt, Type)`, `EXPECT_ANY_THROW(stmt)`, `EXPECT_NO_THROW(stmt)` | exceptions: the type must match |
| `EXPECT_NEAR(a, b, tolerance)`, `EXPECT_DOUBLE_EQ(a, b)` | floating-point numbers. Never compare doubles with `EXPECT_EQ`: `0.1 + 0.2 != 0.3` |
| `EXPECT_STREQ(a, b)` | C strings (`EXPECT_EQ` would compare pointers) |
| `EXPECT_DEATH(stmt, regex)` | the statement must crash the process (it runs in a child process) |

Every `EXPECT_*` has an `ASSERT_*` twin. **`EXPECT_` records the failure and the test goes on; `ASSERT_` records it and leaves the test at once.** Use `ASSERT_` when going on would be pointless or dangerous (a size check before indexing, a pointer check before dereferencing), and `EXPECT_` otherwise, so that one run shows all the problems. You can add context with `<<`: `EXPECT_EQ(x, 3) << "for input " << input;`.

`size()` returns an unsigned type: write the expected value as `1u`. Comparing a `std::size_t` with an `int` is a classic source of bugs (a negative number becomes a huge one), and some builds warn about it; warnings inside the framework's own headers are usually hidden, so do not count on the compiler to tell you.

## Fixtures: shared setup

When several tests start from the same state, put it in a **fixture**: a class derived from `::testing::Test`, used with `TEST_F`:

```cpp
class RingBufferTest : public ::testing::Test {
protected:
    RingBuffer buffer{3};                       // built fresh for every test
    void fill(std::initializer_list<int> values) { for (int v : values) buffer.push(v); }
};

TEST_F(RingBufferTest, FrontIsTheOldestItem) {  // a TEST_F body is a member function of the fixture
    fill({1, 2});
    EXPECT_EQ(buffer.front(), 1);
}
```

GoogleTest creates a **new fixture object for each test**, so tests stay independent. The constructor (or an overridden `SetUp()`) is the setup and the destructor (or `TearDown()`) is the cleanup. `SetUpTestSuite()` and `TearDownTestSuite()` (static) run once per suite, for something expensive; objects created there are shared, so use them with care.

## Parameterized tests: one body, many inputs

```cpp
class RingBufferCapacity : public ::testing::TestWithParam<std::size_t> {};

TEST_P(RingBufferCapacity, IsFullExactlyWhenItHoldsItsCapacity) {
    const std::size_t capacity = GetParam();
    ...
}

INSTANTIATE_TEST_SUITE_P(Capacities, RingBufferCapacity, ::testing::Values(1u, 2u, 3u, 8u));
```

Each value becomes its own test (`Capacities/RingBufferCapacity.IsFull.../2`), reported separately. `Values(...)`, `Range(a, b)`, `Combine(...)` and `ValuesIn(container)` generate the parameters. A `TEST_P` that is never instantiated silently does not run.

## Useful to know

- **Run a few tests**: a GoogleTest program takes `--gtest_filter=RingBufferTest.*` (and `--gtest_list_tests`, `--gtest_repeat=100`, `--gtest_shuffle` to check that the tests are independent).
- **Naming**: `TEST(Suite, Name)` names are identifiers. `PopOnAnEmptyBufferThrows` tells the reader what the code promises; `Test3` does not.
- **One behavior per test.** A test that checks twelve things stops being readable when it fails.
- **GoogleMock** (next lessons) is part of the same project and adds matchers and mock objects.

## Your task

`ring_buffer.hpp` (read-only) holds a `RingBuffer`; its comment lists what it promises. Write the tests in `ring_buffer_test.cpp`. As in the test-out, **Run Tests** runs them against the real `RingBuffer`, then against several broken copies, each with one bug. A bug that your tests miss is listed by name with a hint.

- **Works**: the everyday behavior.
- **Robust**: wrap-around, the edges and the exceptions.
- **Clean**: your file contains a **fixture** (`TEST_F`, at least two), a **parameterized test** (`TEST_P` and `INSTANTIATE_TEST_SUITE_P`) and an **`ASSERT_`**, and compiles without warnings.
