// MUTANT: zero-capacity-allowed: the constructor check is never tested.
#include <gtest/gtest.h>

#include <stdexcept>

#include "ring_buffer.hpp"

// A FIXTURE: every TEST_F gets its own brand-new RingBuffer (a new fixture object is built for each test), so tests cannot affect each other.
class RingBufferTest : public ::testing::Test {
protected:
    RingBuffer buffer{3};

    void fill(std::initializer_list<int> values) {
        for (int v : values) buffer.push(v);
    }
};

// ---- the everyday behavior ----------------------------------------------------------------------------------------

TEST_F(RingBufferTest, FrontIsTheOldestItem) {
    fill({1, 2});
    EXPECT_EQ(buffer.front(), 1);
}

TEST_F(RingBufferTest, BackIsTheNewestItem) {
    fill({1, 2});
    EXPECT_EQ(buffer.back(), 2);
}

TEST_F(RingBufferTest, PopRemovesAndReturnsTheOldestItem) {
    fill({1, 2});
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_EQ(buffer.size(), 1u);
}

TEST_F(RingBufferTest, PushingOntoAFullBufferOverwritesTheOldestItem) {
    fill({1, 2, 3, 4});
    ASSERT_EQ(buffer.size(), 3u);  // no point looking at the items if the size is wrong
    EXPECT_EQ(buffer.at(0), 2);
    EXPECT_EQ(buffer.at(1), 3);
    EXPECT_EQ(buffer.at(2), 4);
}

// ---- the edges -----------------------------------------------------------------------------------------------------

TEST_F(RingBufferTest, PopOnAnEmptyBufferThrows) {
    EXPECT_THROW(buffer.pop(), std::out_of_range);
}

TEST_F(RingBufferTest, AtBeyondTheSizeThrows) {
    fill({1, 2});
    EXPECT_NO_THROW(buffer.at(1));
    EXPECT_THROW(buffer.at(2), std::out_of_range);
}


// A PARAMETERIZED test: the same body for several capacities. The capacity is the parameter (GetParam()).
class RingBufferCapacity : public ::testing::TestWithParam<std::size_t> {};

TEST_P(RingBufferCapacity, IsFullExactlyWhenItHoldsItsCapacity) {
    const std::size_t capacity = GetParam();
    RingBuffer b(capacity);
    for (std::size_t i = 0; i + 1 < capacity; ++i) b.push(static_cast<int>(i));
    EXPECT_FALSE(b.full());
    b.push(99);
    EXPECT_TRUE(b.full());
}

INSTANTIATE_TEST_SUITE_P(Capacities, RingBufferCapacity, ::testing::Values(1u, 2u, 3u, 8u));
