// MUTANT: the tests only call the functions: no bug is noticed.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "csv.hpp"

using csv::ParseError;
using csv::Row;

TEST(Csv, JustRuns) {
    csv::parse_line("a,b");
    csv::format_line(csv::Row{"a"});
    csv::parse_document("a\n");
}
