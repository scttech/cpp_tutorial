#include <catch2/catch_test_macros.hpp>

#include "text_utils.hpp"

// Write the tests for the four helpers in text_utils.hpp (read its comment: it says what each one promises). Press "Run Tests":
// your tests must pass on the real helpers and fail on every broken copy of them.
//
// The Clean tier asks you to practice the Catch2 features from the lesson:
//   * SECTIONs inside a TEST_CASE (at least two), and tags on your test cases ("[trim]", "[split]", ...);
//   * GENERATE, to run one check for several values;
//   * a matcher: REQUIRE_THAT(value, SomeMatcher)
// and your file must compile without warnings.
//
// Catch2 headers you may need:
//   <catch2/catch_test_macros.hpp>              TEST_CASE, SECTION, REQUIRE, CHECK, REQUIRE_THROWS_AS
//   <catch2/generators/catch_generators.hpp>    GENERATE, values, range, as<T>
//   <catch2/matchers/catch_matchers_string.hpp> StartsWith, EndsWith, ContainsSubstring
//   <catch2/matchers/catch_matchers_vector.hpp> Equals, Contains, UnorderedEquals   (for std::vector)

TEST_CASE("trim leaves a string without blanks alone", "[trim]") {
    REQUIRE(trim("hello") == "hello");
}
