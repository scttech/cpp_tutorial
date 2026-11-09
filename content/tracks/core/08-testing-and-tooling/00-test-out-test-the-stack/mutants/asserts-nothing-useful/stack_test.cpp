// MUTANT: tests that run the code but check only that it does not crash: every bug survives (only the stack-is-fine test can pass).
#include <gtest/gtest.h>

#include <stdexcept>

#include "stack.hpp"

// ---- the everyday behavior ----------------------------------------------------------------------------------------

TEST(BoundedStack, JustRuns) {
    BoundedStack s(3);
    s.push(1);
    s.pop();
}
