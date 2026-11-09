# Fast path: what's different in C++

- **GoogleTest** is a library, not part of the language: `#include <gtest/gtest.h>`, link `gtest` and `gtest_main` (or write a `main` that calls `::testing::InitGoogleTest(&argc, argv)` and `RUN_ALL_TESTS()`). Tests register themselves during static initialization.
- **`TEST(Suite, Name)`**, **`TEST_F(Fixture, Name)`** (a new fixture object per test; constructor/`SetUp` and destructor/`TearDown`), **`TEST_P` + `INSTANTIATE_TEST_SUITE_P`** (value-parameterized tests; `TYPED_TEST` for type-parameterized ones).
- **`EXPECT_*`** continues after a failure, **`ASSERT_*`** returns from the current function (so it only works in functions returning `void`; in a helper that returns a value, use `ASSERT_NO_FATAL_FAILURE` at the call site or `EXPECT_` plus a return).
- **Argument order**: `EXPECT_EQ(actual, expected)` by convention; the failure message prints both, so the order only affects readability. Pointers compare by address, C strings need `EXPECT_STREQ`, doubles need `EXPECT_NEAR` / `EXPECT_DOUBLE_EQ` (4 ULPs).
- **`EXPECT_THROW(stmt, Type)`** needs the exact type or a derived one; `EXPECT_DEATH` forks a child, so death tests are slow and platform-sensitive.
- **Run options**: `--gtest_filter=Suite.*:-Suite.Slow`, `--gtest_repeat=N`, `--gtest_shuffle`, `--gtest_output=xml:report.xml` (for CI), `--gtest_break_on_failure` (for a debugger).
- **Comparing `size_t` with an `int` literal** is a signed/unsigned comparison inside the macro: use `1u` or `std::size_t{1}` (whether the compiler warns depends on whether the framework headers are treated as system headers).
- **Test code is code**: keep it small, named after the behavior and free of logic (`if`, loops that compute the expected value are a sign that the test may be wrong itself).
