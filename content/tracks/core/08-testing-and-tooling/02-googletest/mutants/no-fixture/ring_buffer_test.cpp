// MUTANT: plain TESTs only (Clean tier asks for a fixture).
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

TEST(RingBuffer, FrontIsTheOldestItem) {
    RingBuffer b(3);
    b.push(1);
    b.push(2);
    EXPECT_EQ(b.front(), 1);
}

TEST(RingBuffer, BackIsTheNewestItem) {
    RingBuffer b(3);
    b.push(1);
    b.push(2);
    EXPECT_EQ(b.back(), 2);
}

TEST(RingBuffer, PopRemovesAndReturnsTheOldestItem) {
    RingBuffer b(3);
    b.push(1);
    b.push(2);
    EXPECT_EQ(b.pop(), 1);
    EXPECT_EQ(b.size(), 1u);
}

TEST(RingBuffer, PushingOntoAFullBufferOverwritesTheOldestItem) {
    RingBuffer b(3);
    for (int v : {1, 2, 3, 4}) b.push(v);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b.at(0), 2);
    EXPECT_EQ(b.at(1), 3);
    EXPECT_EQ(b.at(2), 4);
}

// ---- the edges -----------------------------------------------------------------------------------------------------

TEST(RingBuffer, PopOnAnEmptyBufferThrows) {
    RingBuffer b(3);
    EXPECT_THROW(b.pop(), std::out_of_range);
}

TEST(RingBuffer, AtBeyondTheSizeThrows) {
    RingBuffer b(3);
    b.push(1);
    b.push(2);
    EXPECT_NO_THROW(b.at(1));
    EXPECT_THROW(b.at(2), std::out_of_range);
}

TEST(RingBuffer, ACapacityOfZeroIsRejected) {
    EXPECT_THROW(RingBuffer b(0), std::invalid_argument);
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
