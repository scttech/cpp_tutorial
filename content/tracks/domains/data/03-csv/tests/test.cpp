#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "csv.hpp"
#include "zth_test.hpp"

using csv::ErrorKind;
using csv::Options;
using csv::Table;

namespace {

// a readable one-line form: {a|b}{c} with \n, \r, \t and the brace/bar/backslash characters escaped
std::string show(const Table& t) {
    std::string s;
    for (const auto& row : t) {
        s += '{';
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i) s += '|';
            for (char c : row[i]) {
                switch (c) {
                    case '\n': s += "\\n"; break;
                    case '\r': s += "\\r"; break;
                    case '\t': s += "\\t"; break;
                    case '|': case '{': case '}': case '\\': s += '\\'; s += c; break;
                    default: s += c;
                }
            }
        }
        s += '}';
    }
    return s;
}
std::string P(const std::string& text, Options o = {}) {
    const csv::ParseResult r = csv::parse(text, o);
    return r.error ? "ERROR " + show(r.table) : show(r.table);
}
Options with_delim(char d) {
    Options o;
    o.delimiter = d;
    return o;
}
Options lf_only() {
    Options o;
    o.crlf = false;
    return o;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "plain records, with and without a final line terminator") {
    ZTH_EXPECT_EQ(P("a,b,c\n1,2,3\n"), std::string("{a|b|c}{1|2|3}"));
    ZTH_EXPECT_EQ(P("a,b,c\n1,2,3"), std::string("{a|b|c}{1|2|3}"));
    ZTH_EXPECT_EQ(P("a,b,c\r\n1,2,3\r\n"), std::string("{a|b|c}{1|2|3}"));
    ZTH_EXPECT_EQ(P("a,b\r\n1,2\n3,4\r\n"), std::string("{a|b}{1|2}{3|4}"));   // mixed endings
    ZTH_EXPECT_EQ(P("x"), std::string("{x}"));
    ZTH_EXPECT_EQ(P(""), std::string(""));   // no records at all
}
ZTH_TEST("works", "quoted fields: delimiters, doubled quotes, and line breaks are data") {
    ZTH_EXPECT_EQ(P("\"a,b\",c\n"), std::string("{a,b|c}"));
    ZTH_EXPECT_EQ(P("\"say \"\"hi\"\"\",x\n"), std::string("{say \"hi\"|x}"));
    ZTH_EXPECT_EQ(P("\"\"\"\"\n"), std::string("{\"}"));   // a field of one quote character
    ZTH_EXPECT_EQ(P("\"two\nlines\",b\n"), std::string("{two\\nlines|b}"));
    ZTH_EXPECT_EQ(P("\"crlf\r\ninside\",b\r\n"), std::string("{crlf\\r\\ninside|b}"));   // kept exactly
    ZTH_EXPECT_EQ(P("a,\"\",c\n"), std::string("{a||c}"));
    ZTH_EXPECT_EQ(P("\"a\"\n"), std::string("{a}"));
    ZTH_EXPECT_EQ(P("\"a\"\"\",\"\"\"b\"\n"), std::string("{a\"|\"b}"));
}
ZTH_TEST("works", "empty fields: leading, trailing and adjacent delimiters") {
    ZTH_EXPECT_EQ(P(",,\n"), std::string("{||}"));
    ZTH_EXPECT_EQ(P("a,\n"), std::string("{a|}"));
    ZTH_EXPECT_EQ(P("a,"), std::string("{a|}"));   // the text ends right after a delimiter: one more (empty) field
    ZTH_EXPECT_EQ(P(",a\n"), std::string("{|a}"));
    ZTH_EXPECT_EQ(P(","), std::string("{|}"));
    ZTH_EXPECT_EQ(P("a,,b\n"), std::string("{a||b}"));
}
ZTH_TEST("works", "an empty line is a record with one empty field; skip_empty_lines drops it; a quoted \"\" line stays") {
    ZTH_EXPECT_EQ(P("a\n\nb\n"), std::string("{a}{}{b}"));
    ZTH_EXPECT_EQ(P("\n"), std::string("{}"));
    ZTH_EXPECT_EQ(P("\n\n"), std::string("{}{}"));
    ZTH_EXPECT_EQ(P("a\r\n\r\nb"), std::string("{a}{}{b}"));
    Options skip;
    skip.skip_empty_lines = true;
    ZTH_EXPECT_EQ(P("a\n\nb\n\n", skip), std::string("{a}{b}"));
    ZTH_EXPECT_EQ(P("\n\n", skip), std::string(""));
    ZTH_EXPECT_EQ(P("a\n\"\"\nb\n", skip), std::string("{a}{}{b}"));
    ZTH_EXPECT_EQ(P("a,\nb\n", skip), std::string("{a|}{b}"));   // a record with two fields is not an empty line
}
ZTH_TEST("works", "a byte order mark at the very start is skipped, anywhere else it is data") {
    ZTH_EXPECT_EQ(P("\xEF\xBB\xBF" "a,b\n"), std::string("{a|b}"));
    ZTH_EXPECT_EQ(P("\xEF\xBB\xBF"), std::string(""));
    ZTH_EXPECT_EQ(P("a\n\xEF\xBB\xBF" "b\n"), std::string("{a}{\xEF\xBB\xBF" "b}"));
    ZTH_EXPECT_EQ(P("\xEF\xBB\xBF\"q\"\n"), std::string("{q}"));
    ZTH_EXPECT_EQ(P("\xEF\xBB\n"), std::string("{\xEF\xBB}"));   // two of the three bytes: not a BOM
    ZTH_EXPECT_EQ(P("x\xEF\xBB\xBF" "y,z\n"), std::string("{x\xEF\xBB\xBF" "y|z}"));   // inside a field
}
ZTH_TEST("works", "other delimiters; the comma is then an ordinary character; bad delimiters are reported") {
    ZTH_EXPECT_EQ(P("a;b,c;d\n", with_delim(';')), std::string("{a|b,c|d}"));
    ZTH_EXPECT_EQ(P("a\tb\t\"c\td\"\n", with_delim('\t')), std::string("{a|b|c\\td}"));
    ZTH_EXPECT_EQ(P("a|b\n", with_delim('|')), std::string("{a|b}"));
    for (char bad : {'"', '\n', '\r'}) {
        const csv::ParseResult r = csv::parse("a,b\n", with_delim(bad));
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::BadOptions && r.error->line == 0 && r.table.empty());
    }
}
ZTH_TEST("works", "a lone carriage return is data; only \\n and \\r\\n end a record") {
    ZTH_EXPECT_EQ(P("a\rb\n"), std::string("{a\\rb}"));
    ZTH_EXPECT_EQ(P("a\r"), std::string("{a\\r}"));
    ZTH_EXPECT_EQ(P("a\r,b\n"), std::string("{a\\r|b}"));
    ZTH_EXPECT_EQ(P("a\r\r\nb"), std::string("{a\\r}{b}"));
}
ZTH_TEST("works", "errors: kind, line, record, and the records read before them") {
    {
        const csv::ParseResult r = csv::parse("a,b\n\"never closed,c\nmore\n");
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::UnterminatedQuote);
        ZTH_EXPECT_EQ(r.error->line, std::size_t{2});   // where the quote opened
        ZTH_EXPECT_EQ(r.error->record, std::size_t{2});
        ZTH_EXPECT_EQ(show(r.table), std::string("{a|b}"));
    }
    {
        const csv::ParseResult r = csv::parse("a,b\nc,d\nx\"y,z\n");
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::QuoteInUnquotedField);
        ZTH_EXPECT_EQ(r.error->line, std::size_t{3});
        ZTH_EXPECT_EQ(r.error->record, std::size_t{3});
        ZTH_EXPECT_EQ(show(r.table), std::string("{a|b}{c|d}"));
    }
    {
        const csv::ParseResult r = csv::parse("a\n\"x\"y,z\n");
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::TextAfterClosingQuote);
        ZTH_EXPECT_EQ(r.error->line, std::size_t{2});
    }
    {
        const csv::ParseResult r = csv::parse("\"line1\nline2\"x\n");   // the line number counts the line breaks inside the quotes
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::TextAfterClosingQuote);
        ZTH_EXPECT_EQ(r.error->line, std::size_t{2});
    }
    {
        const csv::ParseResult r = csv::parse("\"a\"\"");   // a doubled quote and then the text ends: still open
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::UnterminatedQuote);
        ZTH_EXPECT_EQ(r.error->line, std::size_t{1});
    }
    {
        const csv::ParseResult r = csv::parse("\"");
        ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::UnterminatedQuote);
    }
}
ZTH_TEST("works", "rectangular: every record must be as wide as the first one") {
    Options rect;
    rect.rectangular = true;
    ZTH_EXPECT_EQ(P("a,b\nc,d\n", rect), std::string("{a|b}{c|d}"));
    const csv::ParseResult r = csv::parse("a,b\nc,d\n\"x\ny\"\ne,f\n", rect);   // the record on lines 3-4 has one field
    ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::RaggedRecord);
    ZTH_EXPECT_EQ(r.error->line, std::size_t{3});
    ZTH_EXPECT_EQ(r.error->record, std::size_t{3});
    ZTH_EXPECT_EQ(show(r.table), std::string("{a|b}{c|d}"));
    ZTH_EXPECT_EQ(P("a,b\nc,d,e\n", rect), std::string("ERROR {a|b}"));
    ZTH_EXPECT_EQ(P("a,b\nc,d\n", Options{}), std::string("{a|b}{c|d}"));
    ZTH_EXPECT_EQ(P("a,b\nc\n"), std::string("{a|b}{c}"));   // without the option, ragged is fine
    Options rs;
    rs.rectangular = true;
    rs.skip_empty_lines = true;
    ZTH_EXPECT_EQ(P("a,b\n\nc,d\n", rs), std::string("{a|b}{c|d}"));
}
ZTH_TEST("works", "write: quotes exactly the fields that need it, doubles quotes, ends every record") {
    const Table t = {{"plain", "has,comma", "has \"quote\"", "line\nbreak", ""}, {"x", "y", "z", "", "cr\rhere"}};
    ZTH_EXPECT_EQ(csv::write(t), std::string("plain,\"has,comma\",\"has \"\"quote\"\"\",\"line\nbreak\",\r\nx,y,z,,\"cr\rhere\"\r\n"));
    ZTH_EXPECT_EQ(csv::write(t, lf_only()), std::string("plain,\"has,comma\",\"has \"\"quote\"\"\",\"line\nbreak\",\nx,y,z,,\"cr\rhere\"\n"));
    ZTH_EXPECT_EQ(csv::write(Table{}), std::string(""));
    ZTH_EXPECT_EQ(csv::write(Table{{"a"}}), std::string("a\r\n"));
}
ZTH_TEST("works", "write: the delimiter decides what needs quoting; an empty lone field and an empty row") {
    const Table t = {{"a,b", "c;d"}};
    ZTH_EXPECT_EQ(csv::write(t, with_delim(';')), std::string("a,b;\"c;d\"\r\n"));
    ZTH_EXPECT_EQ(csv::write(t), std::string("\"a,b\",c;d\r\n"));
    ZTH_EXPECT_EQ(csv::write(Table{{""}}), std::string("\"\"\r\n"));   // not an empty line
    ZTH_EXPECT_EQ(csv::write(Table{{"", ""}}), std::string(",\r\n"));
    ZTH_EXPECT_EQ(csv::write(Table{{"a"}, {}, {"b"}}), std::string("a\r\nb\r\n"));   // a row with no fields is skipped
    ZTH_EXPECT_EQ(csv::write(Table{{"\xEF\xBB\xBF" "x", "y"}, {"\xEF\xBB\xBF" "z"}}), std::string("\"\xEF\xBB\xBF" "x\",y\r\n\xEF\xBB\xBF" "z\r\n"));   // only the very first field of the output
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "round trip: write then parse gives the table back, for nasty fields, any delimiter, both line endings") {
    zth::Lcg rng(1234);
    const char alphabet[] = {'a', 'b', ' ', ',', ';', '\t', '"', '\n', '\r', '|', 'x', '"', ',', '\n'};
    for (int round = 0; round < 1500; ++round) {
        Table t;
        const int rows = 1 + rng.below(6);
        for (int r = 0; r < rows; ++r) {
            std::vector<std::string> row;
            const int width = 1 + rng.below(5);
            for (int c = 0; c < width; ++c) {
                std::string f;
                const int len = rng.below(7);
                for (int i = 0; i < len; ++i) f.push_back(alphabet[rng.below(static_cast<int>(sizeof(alphabet)))]);
                if (r == 0 && c == 0 && rng.below(8) == 0) f = "\xEF\xBB\xBF" + f;
                row.push_back(f);
            }
            t.push_back(row);
        }
        Options o;
        o.delimiter = rng.below(3) == 0 ? ';' : rng.below(2) == 0 ? '\t' : ',';
        o.crlf = rng.below(2) == 0;
        const std::string text = csv::write(t, o);
        const csv::ParseResult back = csv::parse(text, o);
        if (back.error || back.table != t) {
            zth::fail("round trip failed for the written text: " + show(t) + " -> " + (back.error ? std::string("ERROR ") : std::string()) + show(back.table));
            return;
        }
    }
}
ZTH_TEST("robust", "garbage in never crashes; whatever parses is stable under write -> parse") {
    zth::Lcg rng(99);
    const char alphabet[] = {'a', ',', '"', '"', '\n', '\r', ' ', ';', 'b', '\xEF', '\xBB', '\xBF'};
    for (int round = 0; round < 6000; ++round) {
        std::string s;
        const int len = rng.below(14);
        for (int i = 0; i < len; ++i) s.push_back(alphabet[rng.below(static_cast<int>(sizeof(alphabet)))]);
        const csv::ParseResult r = csv::parse(s);
        if (!r.error) {
            Table nonempty;
            for (const auto& row : r.table) nonempty.push_back(row);
            const csv::ParseResult again = csv::parse(csv::write(nonempty));
            if (again.error || again.table != nonempty) {
                zth::fail("not stable: " + show(nonempty));
                return;
            }
        }
        Options o;
        o.rectangular = true;
        o.skip_empty_lines = true;
        (void)csv::parse(s, o);
    }
}
ZTH_TEST("robust", "the Reader gives the same records as parse(), reuses the vector it is given, and stays at the end") {
    const std::string text = "a,b,c\n1,\"x\ny\",3\nlong long long field,,\n";
    csv::Reader reader(text);
    std::vector<std::string> fields = {"stale", "values", "from", "before", "and", "more"};
    Table got;
    while (reader.next(fields)) got.push_back(fields);
    ZTH_EXPECT_EQ(show(got), show(csv::parse(text).table));
    ZTH_EXPECT_FALSE(reader.error().has_value());
    ZTH_EXPECT_FALSE(reader.next(fields));
    ZTH_EXPECT_FALSE(reader.next(fields));
    csv::Reader bad("a\n\"oops\n");
    ZTH_EXPECT_TRUE(bad.next(fields));
    ZTH_EXPECT_FALSE(bad.next(fields));
    ZTH_EXPECT_TRUE(bad.error().has_value());
    ZTH_EXPECT_FALSE(bad.next(fields));   // stays failed
    ZTH_EXPECT_TRUE(bad.error().has_value() && bad.error()->kind == ErrorKind::UnterminatedQuote);
}
ZTH_TEST("robust", "records of different widths in a row: the reused vector is shrunk and grown correctly") {
    csv::Reader reader("a,b,c,d,e\nx\ny,z\n,,,\nw\n");
    std::vector<std::string> f;
    std::vector<std::size_t> widths;
    std::vector<std::string> firsts;
    while (reader.next(f)) {
        widths.push_back(f.size());
        firsts.push_back(f.empty() ? "?" : f[0]);
    }
    ZTH_EXPECT_TRUE((widths == std::vector<std::size_t>{5, 1, 2, 4, 1}));
    ZTH_EXPECT_TRUE((firsts == std::vector<std::string>{"a", "x", "y", "", "w"}));
}
ZTH_TEST("robust", "big fields: a one-megabyte quoted field with quotes in it, and 100000 empty fields in one record") {
    std::string field(1 << 20, 'z');
    for (std::size_t i = 0; i < field.size(); i += 1000) field[i] = '"';
    std::string quoted = "\"";
    for (char c : field) {
        if (c == '"') quoted += '"';
        quoted += c;
    }
    quoted += "\",tail\n";
    const csv::ParseResult r = csv::parse(quoted);
    ZTH_EXPECT_TRUE(!r.error && r.table.size() == 1 && r.table[0].size() == 2 && r.table[0][0] == field && r.table[0][1] == "tail");
    const csv::ParseResult wide = csv::parse(std::string(99999, ','));
    ZTH_EXPECT_TRUE(!wide.error && wide.table.size() == 1 && wide.table[0].size() == 100000);
    const csv::ParseResult unterminated = csv::parse("\"" + std::string(2000000, 'q'));
    ZTH_EXPECT_TRUE(unterminated.error.has_value() && unterminated.error->kind == ErrorKind::UnterminatedQuote && unterminated.table.empty());
}
ZTH_TEST("robust", "line numbers stay right across quoted line breaks, CRLF and skipped lines") {
    Options skip;
    skip.skip_empty_lines = true;
    const csv::ParseResult r = csv::parse("a\r\n\r\n\"p\r\nq\"\r\n\r\nx\"y\r\n", skip);
    ZTH_EXPECT_TRUE(r.error.has_value() && r.error->kind == ErrorKind::QuoteInUnquotedField);
    ZTH_EXPECT_EQ(r.error->line, std::size_t{6});
    ZTH_EXPECT_EQ(r.error->record, std::size_t{3});   // two records were delivered
    ZTH_EXPECT_EQ(show(r.table), std::string("{a}{p\\r\\nq}"));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "reading 200000 same-width records through one reused vector allocates almost nothing") {
    std::string text;
    for (int i = 0; i < 200000; ++i) text += "alpha,\"b, " + std::to_string(i) + "\",gamma delta epsilon zeta eta theta iota kappa lambda,4\n";
    csv::Reader reader(text);
    std::vector<std::string> fields;
    ZTH_EXPECT_TRUE(reader.next(fields));   // the first records may allocate the field strings
    ZTH_EXPECT_TRUE(reader.next(fields));
    zth::mem::Scope scope;
    std::size_t n = 2;
    while (reader.next(fields)) ++n;
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 8);
    ZTH_EXPECT_EQ(n, std::size_t{200000});
    ZTH_EXPECT_FALSE(reader.error().has_value());
}
ZTH_TEST("fast", "write builds its output without reallocating per field") {
    Table t;
    for (int i = 0; i < 20000; ++i) t.push_back({"id" + std::to_string(i), "name, with comma", "plain", "say \"hi\""});
    zth::mem::Scope scope;
    const std::string out = csv::write(t);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);   // geometric growth of one string (or one reserve), never one allocation per field
    ZTH_EXPECT_TRUE(out.size() > 20000 * 30);
}
ZTH_TEST_TIMEOUT("fast", "a 10 MB file parses and round-trips in one go", 30) {
    Table t;
    for (int i = 0; i < 150000; ++i) t.push_back({std::to_string(i), "name " + std::to_string(i * 7), "a,b", "multi\nline", "9.75"});
    const std::string text = csv::write(t);
    const csv::ParseResult r = csv::parse(text);
    ZTH_EXPECT_TRUE(!r.error && r.table.size() == t.size() && r.table.back() == t.back() && r.table[12345] == t[12345]);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("csv.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("csv.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "no regex, string streams, getline, strtok or sscanf: a state machine over the characters") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("csv.hpp"));
    for (const char* word : {"regex", "stringstream", "istringstream", "ostringstream", "getline", "strtok", "sscanf", "split", "istream"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
