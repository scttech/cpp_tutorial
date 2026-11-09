// A demo of your test framework (read-only). Press Run to see what it prints.
#include <iostream>

#include "minitest.hpp"

namespace {

bool is_leap_year(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

}  // namespace

// Each MT_TEST registers itself before main() starts. One test is wrong ON PURPOSE, so that you can see what a failure looks like.
MT_TEST(years_divisible_by_four_are_leap_years) {
    MT_CHECK(is_leap_year(2024));
    MT_CHECK(!is_leap_year(2023));
}

MT_TEST(century_years_are_not_leap_years) {
    MT_CHECK_EQ(is_leap_year(1900), false);
    MT_CHECK_EQ(is_leap_year(2100), false);
}

MT_TEST(every_400_years_is_a_leap_year) {
    MT_CHECK_EQ(is_leap_year(2000), true);
}

MT_TEST(a_test_that_fails_on_purpose) {
    MT_CHECK_EQ(is_leap_year(2100), true);  // wrong: 2100 is not a leap year
    MT_CHECK(2 + 2 == 5);                   // also wrong; both failures are reported
}

int main() {
    const int failed = mt::run_all(std::cout);
    std::cout << (failed == 0 ? "all tests passed" : "some tests failed") << "\n";
    return 0;
}
