# Fast path: what's different in C++

- **GoogleTest**: `#include <gtest/gtest.h>`, `TEST(Suite, Name) { ... }`, and the macros `EXPECT_EQ`, `EXPECT_NE`, `EXPECT_LT`, `EXPECT_TRUE`, `EXPECT_THROW(stmt, Type)`, `EXPECT_NO_THROW(stmt)`. `EXPECT_*` records a failure and continues; `ASSERT_*` returns from the test function at once (use it when the rest of the test would crash without it).
- **No test registration, no `main`**: the macros register tests before `main` runs, and `gtest_main` provides `main`. The runner links both for you.
- **Unsigned comparisons**: `EXPECT_EQ(s.size(), 2)` compares `size_t` with `int`; write `2u` (or `std::size_t{2}`). Mixed signedness is a classic bug source (and a `-Wsign-compare` warning in many builds).
- **Test the contract, not the implementation**: what the documentation promises (including the exceptions), through the public interface only.
- **Edge cases are where the bugs live**: empty, full, one element, zero, the boundary on either side of a limit.
- **Mutation testing** is the honest measure of a test suite: change the code on purpose (the grader does) and see whether a test fails. Line coverage only says a line ran, not that anything checked its result.
