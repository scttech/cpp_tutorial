#include <climits>
#include <cstdint>

#include "zth_test.hpp"

// Provided by the learner's main.cpp
bool safe_add(int, int, int&);
int floor_mod(int, int);
bool is_power_of_two(unsigned int);
int count_set_bits(std::uint32_t);

// ---- Works: ordinary values -------------------------------------------------------------------
ZTH_TEST("works", "safe_add(2, 3) stores 5 and returns true") {
    int out = -1;
    ZTH_EXPECT_TRUE(safe_add(2, 3, out));
    ZTH_EXPECT_EQ(out, 5);
}
ZTH_TEST("works", "safe_add detects INT_MAX + 1") {
    int out = 123;
    ZTH_EXPECT_FALSE(safe_add(INT_MAX, 1, out));
}
ZTH_TEST("works", "safe_add handles negative operands") {
    int out = 0;
    ZTH_EXPECT_TRUE(safe_add(-10, -20, out));
    ZTH_EXPECT_EQ(out, -30);
}
ZTH_TEST("works", "floor_mod(7, 5) is 2") {
    ZTH_EXPECT_EQ(floor_mod(7, 5), 2);
}
ZTH_TEST("works", "floor_mod(-1, 5) is 4 (not -1)") {
    ZTH_EXPECT_EQ(floor_mod(-1, 5), 4);
}
ZTH_TEST("works", "is_power_of_two: 8 yes, 6 no") {
    ZTH_EXPECT_TRUE(is_power_of_two(8));
    ZTH_EXPECT_FALSE(is_power_of_two(6));
}
ZTH_TEST("works", "count_set_bits(0xFF) is 8") {
    ZTH_EXPECT_EQ(count_set_bits(0xFFu), 8);
}

// ---- Robust: the edges -------------------------------------------------------------------------
ZTH_TEST("robust", "safe_add leaves out unchanged when it overflows") {
    int out = -7;
    ZTH_EXPECT_FALSE(safe_add(INT_MAX, 1, out));
    ZTH_EXPECT_EQ(out, -7);
}
ZTH_TEST("robust", "safe_add detects INT_MIN + -1 (underflow) and keeps out") {
    int out = 42;
    ZTH_EXPECT_FALSE(safe_add(INT_MIN, -1, out));
    ZTH_EXPECT_EQ(out, 42);
}
ZTH_TEST("robust", "safe_add(INT_MAX, INT_MIN) is -1, which fits") {
    int out = 0;
    ZTH_EXPECT_TRUE(safe_add(INT_MAX, INT_MIN, out));
    ZTH_EXPECT_EQ(out, -1);
}
ZTH_TEST("robust", "safe_add(INT_MIN, INT_MAX) is -1, which fits") {
    int out = 0;
    ZTH_EXPECT_TRUE(safe_add(INT_MIN, INT_MAX, out));
    ZTH_EXPECT_EQ(out, -1);
}
ZTH_TEST("robust", "safe_add accepts results exactly at the limits") {
    int out = 0;
    ZTH_EXPECT_TRUE(safe_add(INT_MAX - 1, 1, out));
    ZTH_EXPECT_EQ(out, INT_MAX);
    ZTH_EXPECT_TRUE(safe_add(INT_MIN + 1, -1, out));
    ZTH_EXPECT_EQ(out, INT_MIN);
}
ZTH_TEST("robust", "floor_mod of an exact negative multiple is 0") {
    ZTH_EXPECT_EQ(floor_mod(-10, 5), 0);
}
ZTH_TEST("robust", "floor_mod(INT_MIN, 3) is 1") {
    ZTH_EXPECT_EQ(floor_mod(INT_MIN, 3), 1);
}
ZTH_TEST("robust", "is_power_of_two(0) is false") {
    ZTH_EXPECT_FALSE(is_power_of_two(0));
}
ZTH_TEST("robust", "is_power_of_two handles the top bit (1u << 31)") {
    ZTH_EXPECT_TRUE(is_power_of_two(1u << 31));
    ZTH_EXPECT_FALSE(is_power_of_two(UINT_MAX));
}
ZTH_TEST("robust", "count_set_bits handles 0 and all-ones") {
    ZTH_EXPECT_EQ(count_set_bits(0u), 0);
    ZTH_EXPECT_EQ(count_set_bits(0xFFFFFFFFu), 32);
}
