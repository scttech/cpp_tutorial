#define ZTH_TRACK_ALLOCATIONS
#include <climits>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "text_tools.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using Fields = std::vector<std::string_view>;

// the value of parse_int as a string, "none" when there is none (so a failing test shows what came back)
S show(const std::optional<int>& v) { return v ? std::to_string(*v) : S("none"); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "parse_int reads decimal integers") {
    ZTH_EXPECT_EQ(show(parse_int("42")), S("42"));
    ZTH_EXPECT_EQ(show(parse_int("0")), S("0"));
    ZTH_EXPECT_EQ(show(parse_int("-7")), S("-7"));
    ZTH_EXPECT_EQ(show(parse_int("007")), S("7"));
    ZTH_EXPECT_EQ(show(parse_int("2147483647")), S("2147483647"));
}
ZTH_TEST("works", "parse_int rejects text that is not exactly an integer") {
    for (const char* bad : {"", "abc", "12abc", "1 2", " 5", "5 ", "+5", "-", "--5", "1.5", "0x10", "1e3"}) {
        if (parse_int(bad)) {
            ::zth::fail(S("parse_int(\"") + bad + "\") should be nothing, it gave " + show(parse_int(bad)));
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("works", "parse_double reads floating-point numbers") {
    const auto a = parse_double("3.5");
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_NEAR(*a, 3.5, 1e-12);
    const auto b = parse_double("-0.25");
    if (b) ZTH_EXPECT_NEAR(*b, -0.25, 1e-12);
    const auto c = parse_double("1e3");
    if (c) ZTH_EXPECT_NEAR(*c, 1000.0, 1e-9);
    const auto d = parse_double("42");
    if (d) ZTH_EXPECT_NEAR(*d, 42.0, 1e-12);
}
ZTH_TEST("works", "parse_double rejects text that is not exactly a number") {
    for (const char* bad : {"", "abc", "2.5x", " 2.5", "2.5 ", "1.2.3", "--1", "+1.5"}) {
        if (parse_double(bad)) {
            ::zth::fail(S("parse_double(\"") + bad + "\") should be nothing");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("works", "to_hex writes lower-case hexadecimal without a prefix") {
    ZTH_EXPECT_EQ(to_hex(255), S("ff"));
    ZTH_EXPECT_EQ(to_hex(0), S("0"));
    ZTH_EXPECT_EQ(to_hex(4096), S("1000"));
    ZTH_EXPECT_EQ(to_hex(0xDEADBEEFu), S("deadbeef"));
}
ZTH_TEST("works", "format_row aligns the columns") {
    ZTH_EXPECT_EQ(format_row("ab", 42, 3.14159), S("ab        |   42|    3.14"));
    ZTH_EXPECT_EQ(format_row("widget", 7, 0.5), S("widget    |    7|    0.50"));
}
ZTH_TEST("works", "trim removes whitespace at both ends") {
    ZTH_EXPECT_EQ(S(trim("  hello  ")), S("hello"));
    ZTH_EXPECT_EQ(S(trim("\t x y \r\n")), S("x y"));
    ZTH_EXPECT_EQ(S(trim("none")), S("none"));
}
ZTH_TEST("works", "split_fields splits at the separator") {
    ZTH_EXPECT_TRUE((split_fields("a,b,c", ',') == Fields{"a", "b", "c"}));
    ZTH_EXPECT_TRUE((split_fields("one two", ' ') == Fields{"one", "two"}));
    ZTH_EXPECT_TRUE((split_fields("nosep", ',') == Fields{"nosep"}));
}
ZTH_TEST("works", "parse_log_line: level, code and message") {
    const auto e = parse_log_line("ERROR 503 Service unavailable");
    ZTH_EXPECT_TRUE(e.has_value());
    if (e) {
        ZTH_EXPECT_EQ(e->level, S("ERROR"));
        ZTH_EXPECT_EQ(e->code, 503);
        ZTH_EXPECT_EQ(e->message, S("Service unavailable"));
    }
}
ZTH_TEST("works", "parse_log_line accepts every level") {
    for (const char* level : {"DEBUG", "INFO", "WARN", "ERROR"}) {
        const auto e = parse_log_line(S(level) + " 1 text");
        if (!e || e->level != level) {
            ::zth::fail(S("level ") + level + " was not accepted");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "parse_int at the limits of int, and one beyond") {
    ZTH_EXPECT_EQ(show(parse_int("2147483647")), S("2147483647"));
    ZTH_EXPECT_EQ(show(parse_int("-2147483648")), S("-2147483648"));
    ZTH_EXPECT_EQ(show(parse_int("2147483648")), S("none"));
    ZTH_EXPECT_EQ(show(parse_int("-2147483649")), S("none"));
    ZTH_EXPECT_EQ(show(parse_int("99999999999999999999")), S("none"));
}
ZTH_TEST("robust", "parse_int works on a view into a longer text (it must not read past the end of the view)") {
    const S text = "1234567";
    ZTH_EXPECT_EQ(show(parse_int(std::string_view(text).substr(0, 3))), S("123"));
    ZTH_EXPECT_EQ(show(parse_int(std::string_view(text).substr(2, 2))), S("34"));
    ZTH_EXPECT_EQ(show(parse_int(std::string_view(text).substr(7))), S("none"));
}
ZTH_TEST("robust", "parse_int of text with an embedded zero byte or a newline is nothing") {
    ZTH_EXPECT_EQ(show(parse_int(std::string_view("12\0" "3", 4))), S("none"));
    ZTH_EXPECT_EQ(show(parse_int("12\n")), S("none"));
}
ZTH_TEST("robust", "parse_double: nan and inf are not numbers; very large and very small values") {
    ZTH_EXPECT_FALSE(parse_double("nan").has_value());
    ZTH_EXPECT_FALSE(parse_double("inf").has_value());
    ZTH_EXPECT_FALSE(parse_double("-inf").has_value());
    ZTH_EXPECT_FALSE(parse_double("infinity").has_value());
    ZTH_EXPECT_FALSE(parse_double("1e999").has_value());  // out of range
    const auto tiny = parse_double("1e-5");
    if (tiny) ZTH_EXPECT_NEAR(*tiny, 1e-5, 1e-15);
}
ZTH_TEST("robust", "parse_double: forms of the same number") {
    for (const char* text : {".5", "0.5", "5e-1", "0.50", "-0.0"}) {
        const auto v = parse_double(text);
        if (!v) {
            ::zth::fail(S("parse_double(\"") + text + "\") gave nothing");
            return;
        }
    }
    ZTH_EXPECT_NEAR(*parse_double(".5"), 0.5, 1e-12);
    ZTH_EXPECT_NEAR(*parse_double("5e-1"), 0.5, 1e-12);
}
ZTH_TEST("robust", "to_hex for one-digit values, powers of 16 and the largest unsigned") {
    ZTH_EXPECT_EQ(to_hex(9), S("9"));
    ZTH_EXPECT_EQ(to_hex(10), S("a"));
    ZTH_EXPECT_EQ(to_hex(16), S("10"));
    ZTH_EXPECT_EQ(to_hex(std::numeric_limits<unsigned>::max()), S("ffffffff"));
}
ZTH_TEST("robust", "format_row: a long name is not cut, negative and large numbers, rounding") {
    ZTH_EXPECT_EQ(format_row("a-very-long-name", 1, 1.0), S("a-very-long-name|    1|    1.00"));
    ZTH_EXPECT_EQ(format_row("x", -5, -2.5), S("x         |   -5|   -2.50"));
    ZTH_EXPECT_EQ(format_row("y", 123456, 99999.999), S("y         |123456|100000.00"));
    ZTH_EXPECT_EQ(format_row("z", 0, 0.005), S("z         |    0|    0.01"));
}
ZTH_TEST("robust", "format_row with an empty name") {
    ZTH_EXPECT_EQ(format_row("", 1, 2.0), S("          |    1|    2.00"));
}
ZTH_TEST("robust", "trim: only whitespace, empty, interior whitespace and all the whitespace characters") {
    ZTH_EXPECT_EQ(S(trim("   ")), S(""));
    ZTH_EXPECT_EQ(S(trim("")), S(""));
    ZTH_EXPECT_EQ(S(trim("a b\tc")), S("a b\tc"));
    ZTH_EXPECT_EQ(S(trim(" \t\r\n\v\fx\f\v\n\r\t ")), S("x"));
}
ZTH_TEST("robust", "trim returns a VIEW into the original text (no copy)") {
    const S text = "  hello  ";
    const std::string_view t = trim(text);
    ZTH_EXPECT_TRUE(t.data() == text.data() + 2);
    ZTH_EXPECT_EQ(t.size(), std::size_t{5});
}
ZTH_TEST("robust", "split_fields keeps empty fields at the start, in the middle and at the end") {
    ZTH_EXPECT_TRUE((split_fields("a,,b", ',') == Fields{"a", "", "b"}));
    ZTH_EXPECT_TRUE((split_fields(",a", ',') == Fields{"", "a"}));
    ZTH_EXPECT_TRUE((split_fields("a,", ',') == Fields{"a", ""}));
    ZTH_EXPECT_TRUE((split_fields(",,", ',') == Fields{"", "", ""}));
}
ZTH_TEST("robust", "split_fields of an empty line is ONE empty field") {
    ZTH_EXPECT_TRUE((split_fields("", ',') == Fields{""}));
}
ZTH_TEST("robust", "split_fields gives views into the line, not copies") {
    const S line = "alpha,beta";
    const Fields f = split_fields(line, ',');
    ZTH_EXPECT_EQ(f.size(), std::size_t{2});
    if (f.size() == 2) {
        ZTH_EXPECT_TRUE(f[0].data() == line.data());
        ZTH_EXPECT_TRUE(f[1].data() == line.data() + 6);
    }
}
ZTH_TEST("robust", "parse_log_line: an empty message, a message with spaces, and the code is the number right after the level") {
    const auto a = parse_log_line("INFO 200");
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) {
        ZTH_EXPECT_EQ(a->code, 200);
        ZTH_EXPECT_EQ(a->message, S(""));
    }
    const auto b = parse_log_line("WARN 7 disk  at   91 percent");
    ZTH_EXPECT_TRUE(b.has_value());
    if (b) ZTH_EXPECT_EQ(b->message, S("disk  at   91 percent"));
}
ZTH_TEST("robust", "parse_log_line ignores whitespace around the line (a Windows line ending, for example)") {
    const auto e = parse_log_line("  ERROR 500 boom \r\n");
    ZTH_EXPECT_TRUE(e.has_value());
    if (e) {
        ZTH_EXPECT_EQ(e->level, S("ERROR"));
        ZTH_EXPECT_EQ(e->message, S("boom"));
    }
}
ZTH_TEST("robust", "parse_log_line: negative codes are fine, other malformed lines are nothing") {
    ZTH_EXPECT_TRUE(parse_log_line("INFO -1 negative").has_value());
    for (const char* bad : {"", "ERROR", "ERROR ", "ERROR abc message", "FATAL 1 message", "error 1 message", "ERROR1 5 x", "ERROR 5x message", "ERROR  5 two spaces", "   ", "5 ERROR message"}) {
        if (parse_log_line(bad)) {
            ::zth::fail(S("parse_log_line(\"") + bad + "\") should be nothing");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "parse_log_line: a code that does not fit in an int is not a log line") {
    ZTH_EXPECT_FALSE(parse_log_line("ERROR 99999999999 message").has_value());
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "parse_int and parse_double allocate nothing (from_chars, not stoi / istringstream)") {
    long sum = 0;
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) {
        const auto v = parse_int("12345");
        const auto d = parse_double("3.25");
        if (v) sum += *v;
        if (d) sum += static_cast<long>(*d);
    }
    ZTH_EXPECT_EQ(sum, 1000L * (12345 + 3));
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "trim allocates nothing") {
    const S text = "     a long enough piece of text to need the heap if it were copied      ";
    zth::mem::Scope scope;
    std::size_t total = 0;
    for (int i = 0; i < 100; ++i) total += trim(text).size();
    ZTH_EXPECT_TRUE(total > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "split_fields allocates only its result vector (the fields are views)") {
    const S line = "a-long-field-number-one,a-long-field-number-two,a-long-field-number-three,four";
    zth::mem::Scope scope;
    const Fields f = split_fields(line, ',');
    ZTH_EXPECT_EQ(f.size(), std::size_t{4});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 4);  // a vector that grows 1, 2, 4: a few blocks at most
}
ZTH_TEST("fast", "parse_log_line allocates only the two strings of its result") {
    const S line = "ERROR 503 A message that is long enough to need a heap block of its own";
    zth::mem::Scope scope;
    const auto e = parse_log_line(line);
    ZTH_EXPECT_TRUE(e.has_value());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2);
}
ZTH_TEST("fast", "parsing 50000 log lines is quick") {
    long total = 0;
    for (int i = 0; i < 50000; ++i) {
        const auto e = parse_log_line("WARN 404 Page not found: /some/path/to/a/resource");
        if (e) total += e->code;
    }
    ZTH_EXPECT_EQ(total, 50000L * 404);
}
