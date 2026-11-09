# Fast path: what's different in C++

- **Catch2 v3**: `#include <catch2/catch_test_macros.hpp>`, link `Catch2Main` and `Catch2` (the runner provides `main`). `TEST_CASE("any string name", "[tag]")`, no classes needed.
- **`REQUIRE` vs `CHECK`**: `REQUIRE` stops the test case on failure, `CHECK` continues (the reverse names of GoogleTest's `ASSERT_`/`EXPECT_`). The failure message **decomposes** the expression: `REQUIRE(a == b)` prints both values. Do not combine conditions with `&&` inside one macro.
- **`SECTION`s**: the test case body runs once per leaf section, so code before a section is the per-section setup. Fixtures are rarely needed (`TEST_CASE_METHOD` if you do).
- **`GENERATE(...)`** re-runs the test case per generated value (`values`, `range`, `as<T>{}`, `random`, `take`, `chunk`): Catch2's parameterized tests. Use `as<std::string>{}` when the first value is a string literal.
- **Matchers**: `REQUIRE_THAT(x, StartsWith("a") && !EndsWith("z"))`, `Equals(vec)`, `Contains(x)`, `WithinRel`, `Catch::Approx` for floating point. Each family has its own header (`catch2/matchers/...`).
- **Tags** select tests on the command line: `./tests "[tag]"`, `"~[slow]"`, `--list-tests`, `-s` (show successes), `-r junit|xml` (CI report), `--order rand` (find hidden dependencies between tests).
- **Compared with GoogleTest**: no mocking built in (use GoogleMock, Trompeloeil, or fakes), expression decomposition instead of `EXPECT_EQ(a, b)`, names are strings, sections instead of fixtures. **doctest** has almost the same syntax and the lowest compile cost; **Boost.Test** fits a Boost-heavy code base.
- **Whatever you pick**: tests are independent, named after the behavior, one reason to fail each, and run in CI on every change.
