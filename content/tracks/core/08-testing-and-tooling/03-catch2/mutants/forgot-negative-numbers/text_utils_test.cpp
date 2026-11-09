// MUTANT: parse-int-ignores-the-sign: no negative number is parsed.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include "text_utils.hpp"

using Catch::Matchers::Equals;
using Strings = std::vector<std::string>;

TEST_CASE("trim removes spaces at both ends", "[trim]") {
    REQUIRE(trim("  hello  ") == "hello");
    REQUIRE(trim("hello") == "hello");
    REQUIRE(trim(" a b ") == "a b");
}

TEST_CASE("trim also removes tabs and newlines, and empties a blank string", "[trim]") {
    SECTION("leading tabs and newlines") {
        REQUIRE(trim("\t\nhi") == "hi");
    }
    SECTION("a string of only blanks becomes empty") {
        REQUIRE(trim("   ").empty());
    }
}

TEST_CASE("split cuts at every separator and keeps the empty pieces", "[split]") {
    REQUIRE_THAT(split("a,b,c", ','), Equals(Strings{"a", "b", "c"}));
    SECTION("an empty piece in the middle") {
        REQUIRE_THAT(split("a,,b", ','), Equals(Strings{"a", "", "b"}));
    }
    SECTION("an empty piece at the end") {
        REQUIRE_THAT(split("a,", ','), Equals(Strings{"a", ""}));
    }
}

TEST_CASE("splitting the empty string gives no pieces at all", "[split]") {
    REQUIRE(split("", ',').empty());
}

TEST_CASE("join puts the separator between the pieces only", "[join]") {
    SECTION("several pieces") {
        REQUIRE(join({"a", "b", "c"}, ", ") == "a, b, c");
    }
    SECTION("one piece gets no separator") {
        REQUIRE(join({"a"}, ", ") == "a");
    }
    SECTION("no pieces give an empty string") {
        REQUIRE(join({}, ", ").empty());
    }
}

TEST_CASE("parse_int reads non-negative numbers", "[parse_int]") {
    const int n = GENERATE(0, 7, 42, 2147483647);
    REQUIRE(parse_int(std::to_string(n)) == n);
}


TEST_CASE("parse_int rejects text that is not a number", "[parse_int]") {
    const std::string text = GENERATE(as<std::string>{}, "", "-", "abc", "12abc", " 12");
    REQUIRE_THROWS_AS(parse_int(text), std::invalid_argument);
}

TEST_CASE("parse_int reports numbers that do not fit in an int", "[parse_int]") {
    REQUIRE_THROWS_AS(parse_int("2147483648"), std::out_of_range);
    REQUIRE_THROWS_AS(parse_int("-2147483649"), std::out_of_range);
    REQUIRE_THROWS_AS(parse_int("99999999999999999999"), std::out_of_range);
}
