#include <string>
#include <vector>

#include "zth_test.hpp"

// Provided by the learner's main.cpp. Note: default arguments are NOT declared here on purpose;
// they are checked through the program's own output (the call site lives in the learner's file).
double mean(const std::vector<double>&);
double median(const std::vector<double>&);
int mode(const std::vector<int>&);
std::string repeat(const std::string&, int, const std::string&);
std::string kind_of(int);
std::string kind_of(double);

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "mean of {1, 2, 3, 4} is 2.5") {
    ZTH_EXPECT_NEAR(mean({1, 2, 3, 4}), 2.5, 1e-12);
}
ZTH_TEST("works", "median of an odd-sized set is the middle value (unsorted input)") {
    ZTH_EXPECT_NEAR(median({9, 1, 5}), 5.0, 1e-12);
}
ZTH_TEST("works", "median of an even-sized set averages the two middle values") {
    ZTH_EXPECT_NEAR(median({4, 1, 3, 2}), 2.5, 1e-12);
}
ZTH_TEST("works", "mode of {1, 2, 2, 3} is 2") {
    ZTH_EXPECT_EQ(mode({1, 2, 2, 3}), 2);
}
ZTH_TEST("works", "repeat with explicit arguments: repeat(\"ab\", 3, \"-\") is ab-ab-ab") {
    ZTH_EXPECT_EQ(repeat("ab", 3, "-"), "ab-ab-ab");
}
ZTH_TEST("works", "kind_of picks the int and double overloads") {
    ZTH_EXPECT_EQ(kind_of(3), "int");
    ZTH_EXPECT_EQ(kind_of(3.5), "double");
}
ZTH_TEST("works", "default arguments: program prints abab, ababab and ab-ab-ab") {
    auto r = ZTH_RUN_USER_MAIN();
    ZTH_EXPECT_CONTAINS(r.out, "repeat(\"ab\") = abab\n");
    ZTH_EXPECT_CONTAINS(r.out, "repeat(\"ab\", 3) = ababab\n");
    ZTH_EXPECT_CONTAINS(r.out, "repeat(\"ab\", 3, \"-\") = ab-ab-ab\n");
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "mean and median of an empty vector are 0.0 (no NaN, no crash)") {
    ZTH_EXPECT_EQ(mean({}), 0.0);
    ZTH_EXPECT_EQ(median({}), 0.0);
}
ZTH_TEST("robust", "median of a single element is that element") {
    ZTH_EXPECT_NEAR(median({7}), 7.0, 1e-12);
}
ZTH_TEST("robust", "median does not modify the caller's vector") {
    const std::vector<double> data{5, 3, 1, 4, 2};
    std::vector<double> copy = data;
    (void)median(copy);
    ZTH_EXPECT_EQ(copy, data);
}
ZTH_TEST("robust", "mean and median work with negative numbers") {
    ZTH_EXPECT_NEAR(mean({-4, -2, 0}), -2.0, 1e-12);
    ZTH_EXPECT_NEAR(median({-4, -2, 0, 2}), -1.0, 1e-12);
}
ZTH_TEST("robust", "mode of an empty vector is 0") {
    ZTH_EXPECT_EQ(mode({}), 0);
}
ZTH_TEST("robust", "mode tie-break: smallest of the most frequent values") {
    ZTH_EXPECT_EQ(mode({5, 3, 5, 3, 9}), 3);
    ZTH_EXPECT_EQ(mode({7, 7, 2, 2, 4, 4}), 2);
}
ZTH_TEST("robust", "mode works with negative values") {
    ZTH_EXPECT_EQ(mode({-1, -1, 2}), -1);
}
ZTH_TEST("robust", "repeat: zero or negative times gives an empty string; one copy has no separator") {
    ZTH_EXPECT_EQ(repeat("x", 0, ","), "");
    ZTH_EXPECT_EQ(repeat("x", -3, ","), "");
    ZTH_EXPECT_EQ(repeat("x", 1, ","), "x");
}
ZTH_TEST("robust", "kind_of: char promotes to int, float promotes to double") {
    ZTH_EXPECT_EQ(kind_of('a'), "int");
    ZTH_EXPECT_EQ(kind_of(2.5f), "double");
}
