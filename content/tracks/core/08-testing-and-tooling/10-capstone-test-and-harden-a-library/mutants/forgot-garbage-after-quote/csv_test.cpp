// MUTANT: text-after-closing-quote-accepted: no test for text after the closing quote.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "csv.hpp"

using csv::ParseError;
using csv::Row;

// ================================================ parse_line ===========================================================

TEST(ParseLine, SplitsAtTheDelimiter) {
    EXPECT_EQ(csv::parse_line("a,b,c"), (Row{"a", "b", "c"}));
}

TEST(ParseLine, KeepsEmptyFieldsInTheMiddle) {
    EXPECT_EQ(csv::parse_line("a,,c"), (Row{"a", "", "c"}));
}

TEST(ParseLine, ATrailingDelimiterMeansAnEmptyLastField) {
    EXPECT_EQ(csv::parse_line("a,b,"), (Row{"a", "b", ""}));
}

TEST(ParseLine, TheEmptyLineIsOneEmptyField) {
    EXPECT_EQ(csv::parse_line(""), (Row{""}));
}

TEST(ParseLine, ADelimiterInsideQuotesIsData) {
    EXPECT_EQ(csv::parse_line("\"a,b\",c"), (Row{"a,b", "c"}));
}

TEST(ParseLine, TwoQuotesInsideQuotesAreOneQuote) {
    EXPECT_EQ(csv::parse_line("\"say \"\"hi\"\"\",x"), (Row{"say \"hi\"", "x"}));
}

TEST(ParseLine, ACustomDelimiterWorks) {
    EXPECT_EQ(csv::parse_line("a;b,c;d", ';'), (Row{"a", "b,c", "d"}));
}

TEST(ParseLine, TheLineEndingIsNotPartOfTheLastField) {
    EXPECT_EQ(csv::parse_line("a,b\r\n"), (Row{"a", "b"}));
    EXPECT_EQ(csv::parse_line("a,b\n"), (Row{"a", "b"}));
}

TEST(ParseLine, AQuoteThatIsNeverClosedIsAnError) {
    EXPECT_THROW(csv::parse_line("\"abc"), ParseError);
}


// ================================================ format_line ==========================================================

TEST(FormatLine, JoinsPlainFields) {
    EXPECT_EQ(csv::format_line(Row{"a", "b", "c"}), "a,b,c");
}

TEST(FormatLine, QuotesAFieldThatContainsTheDelimiter) {
    EXPECT_EQ(csv::format_line(Row{"a,b", "c"}), "\"a,b\",c");
}

TEST(FormatLine, DoublesTheQuotesInsideAQuotedField) {
    EXPECT_EQ(csv::format_line(Row{"say \"hi\""}), "\"say \"\"hi\"\"\"");
}

TEST(FormatLine, WhatIsFormattedCanBeParsedBack) {
    const std::vector<Row> rows = {Row{"a"}, Row{"", ""}, Row{"x,y", "\"q\"", "multi\nline"}, Row{"", "last"}};
    for (const Row& row : rows) {
        EXPECT_EQ(csv::parse_line(csv::format_line(row)), row);
    }
}

// ================================================ parse_document =======================================================

TEST(ParseDocument, OneRecordPerLine) {
    const auto rows = csv::parse_document("a,b\nc,d\n");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[1], (Row{"c", "d"}));
}

TEST(ParseDocument, ALineBreakInsideQuotesBelongsToTheField) {
    const auto rows = csv::parse_document("a,\"x\ny\"\nc,d\n");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], (Row{"a", "x\ny"}));
}

TEST(ParseDocument, BlankLinesAreSkippedAndAMissingFinalBreakIsFine) {
    const auto rows = csv::parse_document("a\n\n\nb");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[1], (Row{"b"}));
}

TEST(ParseDocument, WindowsLineEndingsWork) {
    const auto rows = csv::parse_document("a,b\r\nc,d\r\n");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], (Row{"a", "b"}));
}

TEST(ParseDocument, AnUnclosedQuoteIsAnError) {
    EXPECT_THROW(csv::parse_document("a,\"b\nc"), ParseError);
}
