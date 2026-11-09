// MUTANT: zero-capacity-allowed: the constructor check is never tested.
#include <gtest/gtest.h>

#include <stdexcept>

#include "stack.hpp"

// ---- the everyday behavior ----------------------------------------------------------------------------------------

TEST(BoundedStack, PopReturnsTheNewestItem) {
    BoundedStack s(3);
    s.push(1);
    s.push(2);
    EXPECT_EQ(s.pop(), 2);
}

TEST(BoundedStack, TopShowsTheNewestItemWithoutRemovingIt) {
    BoundedStack s(3);
    s.push(1);
    s.push(2);
    EXPECT_EQ(s.top(), 2);
    EXPECT_EQ(s.size(), 2u);
}

TEST(BoundedStack, PopShrinksTheStack) {
    BoundedStack s(3);
    s.push(1);
    s.push(2);
    s.pop();
    EXPECT_EQ(s.size(), 1u);
}

TEST(BoundedStack, EmptyReflectsTheContents) {
    BoundedStack s(3);
    EXPECT_TRUE(s.empty());
    s.push(7);
    EXPECT_FALSE(s.empty());
}

// ---- the edges -----------------------------------------------------------------------------------------------------

TEST(BoundedStack, PushOntoAFullStackThrows) {
    BoundedStack s(2);
    s.push(1);
    s.push(2);
    EXPECT_TRUE(s.full());
    EXPECT_THROW(s.push(3), std::length_error);
    EXPECT_EQ(s.size(), 2u);
}

TEST(BoundedStack, PopOnAnEmptyStackThrows) {
    BoundedStack s(2);
    EXPECT_THROW(s.pop(), std::out_of_range);
}

TEST(BoundedStack, TopOnAnEmptyStackThrows) {
    const BoundedStack s(2);
    EXPECT_THROW(s.top(), std::out_of_range);
}

TEST(BoundedStack, ClearEmptiesTheStack) {
    BoundedStack s(3);
    s.push(1);
    s.push(2);
    s.clear();
    EXPECT_EQ(s.size(), 0u);
}

