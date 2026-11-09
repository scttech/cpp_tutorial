#include <string>

#include "zth_test.hpp"

namespace {
std::string code() { return ::zth::strip_comments_and_literals(::zth::read_file("text_utils_test.cpp")); }
std::size_t count_of(const std::string& text, const std::string& what) {
    std::size_t n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++n;
    return n;
}
}  // namespace

ZTH_TEST("clean", "your tests use at least two SECTIONs") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "SECTION").size() >= 2);
}

ZTH_TEST("clean", "your tests use GENERATE to run a check for several values") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "GENERATE").size() >= 1);
}

ZTH_TEST("clean", "your tests use a matcher: REQUIRE_THAT with a Catch::Matchers matcher") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "REQUIRE_THAT").size() >= 1 || ::zth::find_word(code(), "CHECK_THAT").size() >= 1);
}

ZTH_TEST("clean", "your test cases carry tags such as \"[trim]\" (at least three of them)") {
    // The tags are string literals, which strip_comments_and_literals removes: look at the raw text.
    const std::string raw = ::zth::read_file("text_utils_test.cpp");
    ZTH_EXPECT_TRUE(count_of(raw, ", \"[") >= 3);
}
