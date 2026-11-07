#include "zth_test.hpp"

// Provided by the learner's main.cpp
double celsius_to_fahrenheit(double);
double fahrenheit_to_celsius(double);
double km_to_miles(double);
int minutes_to_seconds(int);
double average_of_two(int, int);

ZTH_TEST("works", "celsius_to_fahrenheit(100) is 212") {
    ZTH_EXPECT_NEAR(celsius_to_fahrenheit(100), 212.0, 1e-9);
}
ZTH_TEST("works", "celsius_to_fahrenheit(0) is 32 and (-40) is -40") {
    ZTH_EXPECT_NEAR(celsius_to_fahrenheit(0), 32.0, 1e-9);
    ZTH_EXPECT_NEAR(celsius_to_fahrenheit(-40), -40.0, 1e-9);
}
ZTH_TEST("works", "fahrenheit_to_celsius(212) is 100 (not 0: beware 5/9)") {
    ZTH_EXPECT_NEAR(fahrenheit_to_celsius(212), 100.0, 1e-9);
}
ZTH_TEST("works", "fahrenheit_to_celsius(98.6) is about 37") {
    ZTH_EXPECT_NEAR(fahrenheit_to_celsius(98.6), 37.0, 1e-9);
}
ZTH_TEST("works", "km_to_miles(10) is about 6.21371") {
    ZTH_EXPECT_NEAR(km_to_miles(10), 6.21371, 1e-6);
}
ZTH_TEST("works", "minutes_to_seconds(2) is 120") {
    ZTH_EXPECT_EQ(minutes_to_seconds(2), 120);
    ZTH_EXPECT_EQ(minutes_to_seconds(0), 0);
}
ZTH_TEST("works", "average_of_two(1, 2) is exactly 1.5") {
    ZTH_EXPECT_NEAR(average_of_two(1, 2), 1.5, 1e-12);
}
ZTH_TEST("works", "average_of_two(-3, 4) is 0.5") {
    ZTH_EXPECT_NEAR(average_of_two(-3, 4), 0.5, 1e-12);
}
