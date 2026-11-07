#include <climits>
#include <iomanip>
#include <sstream>
#include <string>

#include "zth_test.hpp"

static std::string run(const std::string& input) {
    return ZTH_RUN_USER_MAIN(input).out;
}

static const char* kHeader = " Celsius  Fahrenheit\n";

// Independent reference formatting for larger tables (small cases below are written out by hand).
static std::string expected_table(long long start, long long end, long long step) {
    std::ostringstream os;
    os << std::setw(8) << "Celsius" << std::setw(12) << "Fahrenheit" << '\n';
    os << std::fixed << std::setprecision(1);
    for (long long c = start; c <= end; c += step) {
        os << std::setw(8) << c << std::setw(12) << (static_cast<double>(c) * 9.0 / 5.0 + 32.0) << '\n';
    }
    return os.str();
}

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "the example from the lesson: 0 30 10") {
    ZTH_EXPECT_EQ(run("0 30 10\n"),
                  std::string(kHeader) +
                      "       0        32.0\n"
                      "      10        50.0\n"
                      "      20        68.0\n"
                      "      30        86.0\n");
}
ZTH_TEST("works", "fractional Fahrenheit values: 1 3 1 gives 33.8, 35.6, 37.4 (not integer division)") {
    ZTH_EXPECT_EQ(run("1 3 1\n"),
                  std::string(kHeader) +
                      "       1        33.8\n"
                      "       2        35.6\n"
                      "       3        37.4\n");
}
ZTH_TEST("works", "negative temperatures: -40 -30 5") {
    ZTH_EXPECT_EQ(run("-40 -30 5\n"),
                  std::string(kHeader) +
                      "     -40       -40.0\n"
                      "     -35       -31.0\n"
                      "     -30       -22.0\n");
}
ZTH_TEST("works", "input may be spread over several lines") {
    ZTH_EXPECT_EQ(run("0\n20\n10\n"), expected_table(0, 20, 10));
}
ZTH_TEST("works", "a longer table: -100 to 100 in steps of 25") {
    ZTH_EXPECT_EQ(run("-100 100 25\n"), expected_table(-100, 100, 25));
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "the end value is included only if the step lands on it: 0 25 10 stops at 20") {
    ZTH_EXPECT_EQ(run("0 25 10\n"), expected_table(0, 20, 10));
}
ZTH_TEST("robust", "start equal to end gives a single row") {
    ZTH_EXPECT_EQ(run("37 37 1\n"), std::string(kHeader) + "      37        98.6\n");
}
ZTH_TEST("robust", "step zero or negative is an error") {
    ZTH_EXPECT_EQ(run("0 10 0\n"), "error: step must be positive\n");
    ZTH_EXPECT_EQ(run("0 10 -5\n"), "error: step must be positive\n");
}
ZTH_TEST("robust", "start greater than end is an error") {
    ZTH_EXPECT_EQ(run("10 0 1\n"), "error: start must not exceed end\n");
}
ZTH_TEST("robust", "errors are checked in order: a bad step wins over start > end") {
    ZTH_EXPECT_EQ(run("10 0 0\n"), "error: step must be positive\n");
}
ZTH_TEST("robust", "missing, non-numeric or empty input is reported") {
    ZTH_EXPECT_EQ(run(""), "error: expected three integers: start end step\n");
    ZTH_EXPECT_EQ(run("1 2\n"), "error: expected three integers: start end step\n");
    ZTH_EXPECT_EQ(run("a b c\n"), "error: expected three integers: start end step\n");
    ZTH_EXPECT_EQ(run("0 10 x\n"), "error: expected three integers: start end step\n");
}
ZTH_TEST("robust", "values near INT_MAX do not overflow the loop counter (undefined behavior)") {
    ZTH_EXPECT_EQ(run("2147483640 2147483647 5\n"), expected_table(2147483640LL, 2147483647LL, 5));
}
ZTH_TEST("robust", "values near INT_MIN work too") {
    ZTH_EXPECT_EQ(run("-2147483648 -2147483640 4\n"), expected_table(-2147483648LL, -2147483640LL, 4));
}
ZTH_TEST("robust", "a huge step does not overflow either") {
    ZTH_EXPECT_EQ(run("0 100 2147483647\n"), expected_table(0, 100, 2147483647LL));
}
ZTH_TEST("robust", "extra tokens after the three numbers are ignored") {
    ZTH_EXPECT_EQ(run("0 10 10 and more words\n"), expected_table(0, 10, 10));
}
