#include <string>
#include <vector>

#include "zth_test.hpp"

// Provided by the learner's main.cpp
std::string fizzbuzz(int);
std::string day_type(int);
std::vector<int> primes_up_to(int);

// ---- Works -------------------------------------------------------------------------------------
ZTH_TEST("works", "fizzbuzz(3) is Fizz") {
    ZTH_EXPECT_EQ(fizzbuzz(3), "Fizz");
}
ZTH_TEST("works", "fizzbuzz(5) is Buzz") {
    ZTH_EXPECT_EQ(fizzbuzz(5), "Buzz");
}
ZTH_TEST("works", "fizzbuzz(15) is FizzBuzz (checked before Fizz and Buzz)") {
    ZTH_EXPECT_EQ(fizzbuzz(15), "FizzBuzz");
    ZTH_EXPECT_EQ(fizzbuzz(30), "FizzBuzz");
}
ZTH_TEST("works", "fizzbuzz(7) is the number as text") {
    ZTH_EXPECT_EQ(fizzbuzz(7), "7");
    ZTH_EXPECT_EQ(fizzbuzz(1), "1");
}
ZTH_TEST("works", "day_type: 1-5 weekday, 6-7 weekend") {
    ZTH_EXPECT_EQ(day_type(1), "weekday");
    ZTH_EXPECT_EQ(day_type(3), "weekday");
    ZTH_EXPECT_EQ(day_type(5), "weekday");
    ZTH_EXPECT_EQ(day_type(6), "weekend");
    ZTH_EXPECT_EQ(day_type(7), "weekend");
}
ZTH_TEST("works", "primes_up_to(20) is 2 3 5 7 11 13 17 19") {
    ZTH_EXPECT_EQ(primes_up_to(20), (std::vector<int>{2, 3, 5, 7, 11, 13, 17, 19}));
}
ZTH_TEST("works", "primes_up_to includes n itself when n is prime") {
    ZTH_EXPECT_EQ(primes_up_to(13), (std::vector<int>{2, 3, 5, 7, 11, 13}));
}

// ---- Robust ------------------------------------------------------------------------------------
ZTH_TEST("robust", "day_type rejects 0, 8 and negative days") {
    ZTH_EXPECT_EQ(day_type(0), "invalid");
    ZTH_EXPECT_EQ(day_type(8), "invalid");
    ZTH_EXPECT_EQ(day_type(-1), "invalid");
}
ZTH_TEST("robust", "fizzbuzz handles negatives") {
    ZTH_EXPECT_EQ(fizzbuzz(-3), "Fizz");
    ZTH_EXPECT_EQ(fizzbuzz(-7), "-7");
}
ZTH_TEST("robust", "fizzbuzz(0) is FizzBuzz (0 is divisible by everything)") {
    ZTH_EXPECT_EQ(fizzbuzz(0), "FizzBuzz");
}
ZTH_TEST("robust", "primes_up_to(2) is just 2") {
    ZTH_EXPECT_EQ(primes_up_to(2), (std::vector<int>{2}));
}
ZTH_TEST("robust", "primes_up_to(1), (0) and negatives are empty") {
    ZTH_EXPECT_TRUE(primes_up_to(1).empty());
    ZTH_EXPECT_TRUE(primes_up_to(0).empty());
    ZTH_EXPECT_TRUE(primes_up_to(-10).empty());
}

// ---- Fast: a trial-division version cannot finish in time ----------------------------------------
ZTH_TEST("fast", "primes_up_to(5,000,000) finishes in time and finds 348513 primes") {
    auto p = primes_up_to(5'000'000);
    ZTH_EXPECT_EQ(p.size(), std::size_t{348513});
    ZTH_EXPECT_EQ(p.back(), 4999999);
}
