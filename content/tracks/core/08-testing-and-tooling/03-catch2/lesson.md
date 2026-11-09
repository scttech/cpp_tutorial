# Catch2, and choosing a framework

**Catch2** is the other widely used free C++ test framework (BSL-1.0 license). Where GoogleTest feels like JUnit, Catch2 feels lighter and more C++-like: test names are plain strings, there are no classes to derive from, and the assertions are a few macros whose failure messages **decompose the expression** for you.

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("trim removes spaces at both ends", "[trim]") {
    REQUIRE(trim("  hello  ") == "hello");
}
```

If that fails, Catch2 prints the expression and both sides, although you wrote one plain `==`:

```
text_utils_test.cpp:7: FAILED:
  REQUIRE( trim("  hello  ") == "hello" )
with expansion:
  "hello  " == "hello"
```

The first argument of `TEST_CASE` is the name (any string, it should read like a sentence), the optional second is a list of **tags**.

## Assertions

| | meaning |
|---|---|
| `REQUIRE(expr)` | must hold; on failure the **test case stops** (like `ASSERT_` in GoogleTest) |
| `CHECK(expr)` | must hold; on failure the test case **carries on** (like `EXPECT_`) |
| `REQUIRE_THROWS_AS(expr, Type)`, `REQUIRE_THROWS(expr)`, `REQUIRE_NOTHROW(expr)` | exceptions |
| `REQUIRE_THAT(value, matcher)` | a **matcher**: `Equals(vec)`, `StartsWith("x")`, `ContainsSubstring("x")`, `Contains(x)`, `WithinAbs(x, eps)` ... and they combine: `StartsWith("a") && !EndsWith("z")` |
| `REQUIRE(x == Catch::Approx(0.3))` | floating point numbers (`0.1 + 0.2 != 0.3` exactly) |
| `INFO("while testing " << x)`, `CAPTURE(x)` | extra context that is printed only if the test fails |

Do not write `REQUIRE(a == 1 && b == 2)`: the message cannot tell you which half failed. One condition per `REQUIRE`.

## Sections: shared setup without a fixture class

A `SECTION` is a sub-test inside a test case. Catch2 runs the test case **once per leaf section**, from the top each time, so the code *before* a section is the setup, fresh for each section:

```cpp
TEST_CASE("a stack", "[stack]") {
    BoundedStack s(2);              // runs again for every section: a new stack each time
    s.push(1);

    SECTION("pop returns it") { REQUIRE(s.pop() == 1); }
    SECTION("it is not empty")   { REQUIRE_FALSE(s.empty()); }
}
```

Sections nest, and each leaf is a separate path through the test. This replaces the fixture class of GoogleTest for most uses (for a real fixture class, `TEST_CASE_METHOD(Fixture, "name")` exists).

## Generators: one check, many values

```cpp
TEST_CASE("parse_int reads non-negative numbers", "[parse_int]") {
    const int n = GENERATE(0, 7, 42, 2147483647);       // the whole test case runs once per value
    REQUIRE(parse_int(std::to_string(n)) == n);
}
```

`GENERATE(values...)`, `GENERATE(range(1, 10))`, `GENERATE(as<std::string>{}, "a", "b")` (to say which type the values have) and `GENERATE(take(5, random(0, 100)))` all work. A failure says which generated value it happened with. This is Catch2's answer to parameterized tests, and it is much shorter than `TEST_P`.

## Tags, running, and styles

- **Tags**: `"[trim][fast]"`. Run a subset with the program's arguments: `./tests "[trim]"`, `./tests "~[slow]"`, `./tests "split*"`; `--list-tests` lists them; `-s` shows passing assertions too; `-r junit` writes a report for CI.
- **BDD style**: `SCENARIO("a full stack") { GIVEN("a full stack") { WHEN("pushing") { THEN("it throws") ... } } }` is the same machinery with other names.
- **Benchmarks**: `BENCHMARK("sort 1000") { return sort(data); };` measures code (Catch2 v3 includes `catch2/benchmark/...`).
- **v3 vs v2**: Catch2 v3 (used here) is a compiled library plus many small headers; v2 was a single giant header. Include only what you use (`catch_test_macros.hpp` is the cheap one).

## Choosing a framework

| | GoogleTest | Catch2 v3 | doctest | Boost.Test |
|---|---|---|---|---|
| License | BSD-3 | BSL-1.0 | MIT | BSL-1.0 |
| Style | macros + classes (`TEST_F`) | test cases + sections | like Catch2, lighter | macros, many options |
| Mocking | **GoogleMock** (same project) | none (use a mock library) | none | none |
| Failure message | shows both sides of the macro | decomposes the whole expression | decomposes | shows both sides |
| Compile cost | moderate | moderate (v3: light headers) | the lightest | heavier |
| Where you meet it | Google, LLVM, Chromium, a lot of industry code | many open-source libraries | game and embedded code, header-only libraries | the Boost ecosystem |

None is wrong. Use what the **project already uses**; for a new project, GoogleTest if you want mocking in the same box, Catch2 or doctest if you want the lightest syntax. The *ideas* (arrange/act/assert, one behavior per test, edge cases, independence) are identical, and so is what you do with the failure: read it from the top.

## Your task

`text_utils.hpp` (read-only) holds four helpers (`trim`, `split`, `join`, `parse_int`). Their comment says what each one promises. Write the tests with Catch2 in `text_utils_test.cpp`. As before, **Run Tests** runs them against the real helpers, then against broken copies; a bug that your tests miss is listed by name with a hint.

- **Works**: the everyday behavior.
- **Robust**: the edges: blank strings, the empty string, bad text, numbers that do not fit.
- **Clean**: at least **two `SECTION`s**, a **`GENERATE`**, a **matcher** (`REQUIRE_THAT`), **tags** on at least three test cases, and no compiler warnings.
