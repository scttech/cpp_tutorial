#include <gtest/gtest.h>

#include "stack.hpp"

// TEST OUT: write unit tests for BoundedStack (see stack.hpp for what it promises).
//
// Press "Run Tests". Your tests are graded in two ways:
//   1. they must PASS on the real BoundedStack, and
//   2. they must FAIL on every one of several broken copies of it (each has one realistic bug).
// A broken copy that your tests do not notice is listed by name, so you know what to test next.
//
// Tiers: Works = the everyday behavior. Robust = the edges (empty, full, bad input). Clean = no compiler warnings.

TEST(BoundedStack, StartsEmpty) {
    BoundedStack s(3);
    EXPECT_EQ(s.size(), 0u);
}
