#include <climits>
#include <string>
#include <vector>

#include "metrics.hpp"
#include "zth_test.hpp"

// ================================================ Works =======================================================
ZTH_TEST("works", "Counter starts at the value it is given, and add() adds") {
    Counter c(5);
    ZTH_EXPECT_EQ(c.value(), 5);
    c.add(3);
    ZTH_EXPECT_EQ(c.value(), 8);
}
ZTH_TEST("works", "sum_of_squares adds up every square") {
    ZTH_EXPECT_EQ(sum_of_squares({1, 2, 3}), 14);
}
ZTH_TEST("works", "percent rounds a fraction to the nearest whole percentage") {
    ZTH_EXPECT_EQ(percent(0.739), 74);
    ZTH_EXPECT_EQ(percent(0.5), 50);
}
ZTH_TEST("works", "clamp_to_byte keeps values that fit and clamps the ones that do not") {
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(100)), 100);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(300)), 255);
}
ZTH_TEST("works", "pad_left pads with the fill character") {
    ZTH_EXPECT_EQ(pad_left("7", 3, '0'), std::string("007"));
}
ZTH_TEST("works", "grade_points maps the score to 4, 3 or 2") {
    ZTH_EXPECT_EQ(grade_points(95), 4);
    ZTH_EXPECT_EQ(grade_points(85), 3);
    ZTH_EXPECT_EQ(grade_points(75), 2);
}
ZTH_TEST("works", "label names the levels") {
    ZTH_EXPECT_EQ(std::string(label(Level::Low)), std::string("low"));
    ZTH_EXPECT_EQ(std::string(label(Level::High)), std::string("high"));
}
ZTH_TEST("works", "is_zero tells zero from non-zero") {
    ZTH_EXPECT_TRUE(is_zero(0));
    ZTH_EXPECT_FALSE(is_zero(7));
}
ZTH_TEST("works", "sum_to adds 1 to n") {
    ZTH_EXPECT_EQ(sum_to(5), 15LL);
    ZTH_EXPECT_EQ(sum_to(100), 5050LL);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "Counter works with negative and zero values") {
    Counter z(0);
    z.add(-4);
    ZTH_EXPECT_EQ(z.value(), -4);
    Counter n(-3);
    ZTH_EXPECT_EQ(n.value(), -3);
}
ZTH_TEST("robust", "sum_of_squares of nothing is 0, and negative numbers count positively") {
    ZTH_EXPECT_EQ(sum_of_squares({}), 0);
    ZTH_EXPECT_EQ(sum_of_squares({-2}), 4);
    ZTH_EXPECT_EQ(sum_of_squares({-3, 4}), 25);
}
ZTH_TEST("robust", "percent rounds halves away from zero and handles negatives and the ends") {
    ZTH_EXPECT_EQ(percent(0.125), 13);
    ZTH_EXPECT_EQ(percent(-0.125), -13);
    ZTH_EXPECT_EQ(percent(-0.126), -13);
    ZTH_EXPECT_EQ(percent(0.0), 0);
    ZTH_EXPECT_EQ(percent(1.0), 100);
    ZTH_EXPECT_EQ(percent(0.994), 99);
}
ZTH_TEST("robust", "clamp_to_byte clamps at both ends and does not wrap around") {
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(-5)), 0);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(0)), 0);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(255)), 255);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(256)), 255);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(INT_MIN)), 0);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(INT_MAX)), 255);
    ZTH_EXPECT_EQ(static_cast<int>(clamp_to_byte(1000)), 255);
}
ZTH_TEST("robust", "pad_left leaves long text alone and pads the empty string") {
    ZTH_EXPECT_EQ(pad_left("abc", 2, '*'), std::string("abc"));
    ZTH_EXPECT_EQ(pad_left("abc", 3, '*'), std::string("abc"));
    ZTH_EXPECT_EQ(pad_left("", 3, '-'), std::string("---"));
    ZTH_EXPECT_EQ(pad_left("a", 4, '*'), std::string("***a"));
}
ZTH_TEST("robust", "grade_points is 0 below 70, also for 0 and negative scores, and exact at every boundary") {
    ZTH_EXPECT_EQ(grade_points(69), 0);
    ZTH_EXPECT_EQ(grade_points(0), 0);
    ZTH_EXPECT_EQ(grade_points(-5), 0);
    ZTH_EXPECT_EQ(grade_points(70), 2);
    ZTH_EXPECT_EQ(grade_points(79), 2);
    ZTH_EXPECT_EQ(grade_points(80), 3);
    ZTH_EXPECT_EQ(grade_points(89), 3);
    ZTH_EXPECT_EQ(grade_points(90), 4);
    ZTH_EXPECT_EQ(grade_points(100), 4);
}
ZTH_TEST("robust", "label has a name for every level, including the last one") {
    ZTH_EXPECT_EQ(std::string(label(Level::Low)), std::string("low"));
    ZTH_EXPECT_EQ(std::string(label(Level::Medium)), std::string("medium"));
    ZTH_EXPECT_EQ(std::string(label(Level::High)), std::string("high"));
    ZTH_EXPECT_EQ(std::string(label(Level::Critical)), std::string("critical"));
}
ZTH_TEST("robust", "is_zero is false for negative numbers too") {
    ZTH_EXPECT_FALSE(is_zero(-1));
    ZTH_EXPECT_FALSE(is_zero(1));
    ZTH_EXPECT_TRUE(is_zero(0));
}
ZTH_TEST("robust", "sum_to handles zero, negative and millions of terms without a giant array on the stack") {
    ZTH_EXPECT_EQ(sum_to(0), 0LL);
    ZTH_EXPECT_EQ(sum_to(-7), 0LL);
    ZTH_EXPECT_EQ(sum_to(1), 1LL);
    ZTH_EXPECT_EQ(sum_to(3000000), 4500001500000LL);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "metrics.hpp does not silence warnings with #pragma (fix the cause instead)") {
    const std::string raw = ::zth::read_file("metrics.hpp");
    ZTH_EXPECT_FALSE(raw.find("diagnostic") != std::string::npos);
    ZTH_EXPECT_FALSE(raw.find("-Wno") != std::string::npos);
}
ZTH_TEST("clean", "metrics.hpp does not hide a problem behind [[maybe_unused]], (void) casts or C-style casts") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("metrics.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "maybe_unused").empty());
    ZTH_EXPECT_TRUE(code.find("(void)") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find("(int)") == std::string::npos && code.find("(std::uint8_t)") == std::string::npos && code.find("(unsigned char)") == std::string::npos);
}
ZTH_TEST("clean", "no variable-length arrays or C arrays sized at run time: metrics.hpp has no `int numbers[n]`") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("metrics.hpp"));
    ZTH_EXPECT_TRUE(code.find("numbers[n]") == std::string::npos);
}
