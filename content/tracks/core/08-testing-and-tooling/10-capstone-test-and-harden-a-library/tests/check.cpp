#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "csv.hpp"
#include "zth_test.hpp"

using csv::Row;

namespace {
bool throws_parse_error(const std::string& text, bool document = false) {
    try {
        if (document) csv::parse_document(text);
        else csv::parse_line(text);
    } catch (const csv::ParseError&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}
}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "your csv.hpp: parse_line splits at the delimiter and keeps empty fields") {
    ZTH_EXPECT_EQ(csv::parse_line("a,b,c"), (Row{"a", "b", "c"}));
    ZTH_EXPECT_EQ(csv::parse_line("a,,c"), (Row{"a", "", "c"}));
    ZTH_EXPECT_EQ(csv::parse_line("a;b", ';'), (Row{"a", "b"}));
}
ZTH_TEST("works", "your csv.hpp: quoted fields lose their quotes and keep the delimiter and doubled quotes inside") {
    ZTH_EXPECT_EQ(csv::parse_line("\"a,b\",c"), (Row{"a,b", "c"}));
    ZTH_EXPECT_EQ(csv::parse_line("\"say \"\"hi\"\"\",x"), (Row{"say \"hi\"", "x"}));
}
ZTH_TEST("works", "your csv.hpp: format_line joins, quotes and doubles quotes") {
    ZTH_EXPECT_EQ(csv::format_line(Row{"a", "b"}), std::string("a,b"));
    ZTH_EXPECT_EQ(csv::format_line(Row{"a,b", "c"}), std::string("\"a,b\",c"));
    ZTH_EXPECT_EQ(csv::format_line(Row{"say \"hi\""}), std::string("\"say \"\"hi\"\"\""));
}
ZTH_TEST("works", "your csv.hpp: parse_document gives one record per line") {
    const auto rows = csv::parse_document("a,b\nc,d\n");
    ZTH_EXPECT_EQ(rows.size(), std::size_t{2});
    if (rows.size() == 2) ZTH_EXPECT_EQ(rows[1], (Row{"c", "d"}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "your csv.hpp: a trailing delimiter gives an empty last field; the empty line is one empty field") {
    ZTH_EXPECT_EQ(csv::parse_line("a,b,"), (Row{"a", "b", ""}));
    ZTH_EXPECT_EQ(csv::parse_line("a,"), (Row{"a", ""}));
    ZTH_EXPECT_EQ(csv::parse_line(","), (Row{"", ""}));
    ZTH_EXPECT_EQ(csv::parse_line(""), (Row{""}));
}
ZTH_TEST("robust", "your csv.hpp: an unclosed quote is a csv::ParseError (a std::runtime_error), also at the very end of the line") {
    ZTH_EXPECT_TRUE(throws_parse_error("\"abc"));
    ZTH_EXPECT_TRUE(throws_parse_error("a,\"b"));
    ZTH_EXPECT_TRUE(throws_parse_error("\""));
    bool is_runtime_error = false;
    try {
        csv::parse_line("\"x");
    } catch (const std::runtime_error&) {
        is_runtime_error = true;
    } catch (...) {
    }
    ZTH_EXPECT_TRUE(is_runtime_error);
}
ZTH_TEST("robust", "your csv.hpp: text after a closing quote is a csv::ParseError") {
    ZTH_EXPECT_TRUE(throws_parse_error("\"abc\"def"));
    ZTH_EXPECT_TRUE(throws_parse_error("x,\"abc\"d,y"));
    ZTH_EXPECT_EQ(csv::parse_line("\"abc\",d"), (Row{"abc", "d"}));
}
ZTH_TEST("robust", "your csv.hpp: line endings are not part of the last field") {
    ZTH_EXPECT_EQ(csv::parse_line("a,b\r\n"), (Row{"a", "b"}));
    ZTH_EXPECT_EQ(csv::parse_line("a,b\n"), (Row{"a", "b"}));
    ZTH_EXPECT_EQ(csv::parse_line("a,\"b\"\r\n"), (Row{"a", "b"}));
}
ZTH_TEST("robust", "your csv.hpp: a quote inside an unquoted field is a plain character; spaces are kept") {
    ZTH_EXPECT_EQ(csv::parse_line("ab\"cd, x "), (Row{"ab\"cd", " x "}));
}
ZTH_TEST("robust", "your csv.hpp: format_line quotes fields with a quote, a CR or an LF, honors the delimiter, and round-trips") {
    ZTH_EXPECT_EQ(csv::format_line(Row{"a\nb"}), std::string("\"a\nb\""));
    ZTH_EXPECT_EQ(csv::format_line(Row{"a;b", "c,d"}, ';'), std::string("\"a;b\";c,d"));
    ZTH_EXPECT_EQ(csv::format_line(Row{}), std::string());
    const std::vector<Row> rows = {Row{""}, Row{"", ""}, Row{"x,y", "\"q\"", "multi\nline"}, Row{"\"", "\"\""}, Row{"tab\there", " lead"}};
    for (const Row& row : rows) ZTH_EXPECT_EQ(csv::parse_line(csv::format_line(row)), row);
}
ZTH_TEST("robust", "your csv.hpp: parse_document keeps a line break inside quotes, skips blank lines and copes with CRLF and a missing final break") {
    const auto a = csv::parse_document("a,\"x\ny\"\nc,d\n");
    ZTH_EXPECT_EQ(a.size(), std::size_t{2});
    if (a.size() == 2) ZTH_EXPECT_EQ(a[0], (Row{"a", "x\ny"}));
    const auto b = csv::parse_document("a\n\n\nb");
    ZTH_EXPECT_EQ(b.size(), std::size_t{2});
    const auto c = csv::parse_document("a,b\r\nc,d\r\n");
    ZTH_EXPECT_EQ(c.size(), std::size_t{2});
    if (c.size() == 2) ZTH_EXPECT_EQ(c[1], (Row{"c", "d"}));
    ZTH_EXPECT_EQ(csv::parse_document("").size(), std::size_t{0});
}
ZTH_TEST("robust", "your csv.hpp: an unclosed quote in a document is a csv::ParseError") {
    ZTH_EXPECT_TRUE(throws_parse_error("a,\"b\nc", true));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "format_line builds its result with amortized growth: a row of 3000 fields needs few heap allocations") {
    Row row(3000, std::string(20, 'x'));
    ::zth::mem::Scope scope;
    const std::string line = csv::format_line(row);
    ZTH_EXPECT_EQ(line.size(), std::size_t{3000 * 20 + 2999});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 60);
}
ZTH_TEST("fast", "parse_line of a row of 3000 short fields needs few heap allocations") {
    const std::string line = csv::format_line(Row(3000, "abcd"));
    ::zth::mem::Scope scope;
    const Row row = csv::parse_line(line);
    ZTH_EXPECT_EQ(row.size(), std::size_t{3000});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 60);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "findings are fixed, not hidden: csv.hpp has no NOLINT, cppcheck-suppress or #pragma diagnostic comment") {
    const std::string raw = ::zth::read_file("csv.hpp");
    ZTH_EXPECT_TRUE(raw.find("NOLINT") == std::string::npos);
    ZTH_EXPECT_TRUE(raw.find("cppcheck-suppress") == std::string::npos);
    ZTH_EXPECT_TRUE(raw.find("diagnostic") == std::string::npos);
}
