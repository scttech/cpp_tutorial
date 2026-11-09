#include <gtest/gtest.h>

#include "ring_buffer.hpp"

// Write the tests for RingBuffer (see ring_buffer.hpp for what it promises). Press "Run Tests" to have them graded:
// they must pass on the real RingBuffer and fail on every broken copy of it.
//
// The Clean tier also asks you to practice the three big GoogleTest features:
//   * a FIXTURE: a class derived from ::testing::Test, used with TEST_F, so that tests share their setup;
//   * a PARAMETERIZED test: TEST_P + INSTANTIATE_TEST_SUITE_P, one test body run for many inputs;
//   * ASSERT_*, for a check after which the test cannot sensibly go on.
// and your file must compile without warnings.

TEST(RingBuffer, StartsEmpty) {
    RingBuffer buffer(3);
    EXPECT_TRUE(buffer.empty());
}
