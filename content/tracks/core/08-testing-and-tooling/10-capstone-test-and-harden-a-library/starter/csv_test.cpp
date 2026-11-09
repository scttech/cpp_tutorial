#include <gtest/gtest.h>

#include "csv.hpp"

// TWO jobs in this capstone, and they help each other.
//
//   1. TEST the library: write GoogleTest tests for csv.hpp in this file (the rules are in the comment at the top of csv.hpp).
//      Run Tests runs your tests against your csv.hpp, and against several BROKEN copies of a correct library: your tests must
//      pass on the correct code and fail on every broken copy.
//   2. HARDEN the library: csv.hpp has bugs of its own (your tests will show you), a performance problem, and findings of
//      clang-tidy and cppcheck. Fix them. Hidden checks look at the behavior of YOUR csv.hpp.
//
// The build uses -Wall -Wextra -Wpedantic -Wconversion -Wshadow and the sanitizers; the Clean tier needs no warning at all, no
// finding from either analyzer, and no NOLINT or suppression comment.

TEST(ParseLine, SplitsAtTheDelimiter) {
    const csv::Row row = csv::parse_line("a,b,c");
    ASSERT_EQ(row.size(), 3u);
    EXPECT_EQ(row[0], "a");
    EXPECT_EQ(row[2], "c");
}
