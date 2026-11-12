#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "config.hpp"
#include "zth_test.hpp"

using namespace conf;

namespace {

using V = Value;

ParseResult P(const std::string& text) { return parse(text); }

// the error of a parse, as "Kind line:col" for compact comparisons
std::string kind_name(ErrorKind k) {
    switch (k) {
        case ErrorKind::UnexpectedCharacter: return "UnexpectedCharacter";
        case ErrorKind::InvalidKey: return "InvalidKey";
        case ErrorKind::InvalidSection: return "InvalidSection";
        case ErrorKind::ExpectedEquals: return "ExpectedEquals";
        case ErrorKind::ExpectedValue: return "ExpectedValue";
        case ErrorKind::InvalidValue: return "InvalidValue";
        case ErrorKind::InvalidNumber: return "InvalidNumber";
        case ErrorKind::UnterminatedString: return "UnterminatedString";
        case ErrorKind::InvalidEscape: return "InvalidEscape";
        case ErrorKind::ControlCharacter: return "ControlCharacter";
        case ErrorKind::UnterminatedArray: return "UnterminatedArray";
        case ErrorKind::MixedArray: return "MixedArray";
        case ErrorKind::TrailingGarbage: return "TrailingGarbage";
        case ErrorKind::DuplicateKey: return "DuplicateKey";
        case ErrorKind::DuplicateSection: return "DuplicateSection";
    }
    return "?";
}
std::string E(const std::string& text) {
    const ParseResult r = parse(text);
    if (r.config) return "OK";
    if (!r.error) return "NEITHER";
    return kind_name(r.error->kind) + " " + std::to_string(r.error->line) + ":" + std::to_string(r.error->column);
}
// the one value of a document of a single setting, or a marker
Value one(const std::string& text) {
    const ParseResult r = parse(text);
    if (!r.config || r.config->entries().size() != 1) return V::str("<<not a single setting>>");
    return r.config->entries()[0].value;
}
std::string paths(const Config& c) {
    std::string s;
    for (const auto& e : c.entries()) s += (s.empty() ? "" : ",") + e.path;
    return s;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a document with sections, dotted keys, comments and every value type") {
    const ParseResult r = P(
        "# top\n"
        "name = \"demo\"  # trailing comment\n"
        "debug = true\n"
        "\n"
        "[server]\n"
        "port = 8080\n"
        "ratio = 0.75\n"
        "[server.tls]\n"
        "cert = 'a.pem'\n"
        "a.b = 1\n");
    ZTH_EXPECT_TRUE(r.config.has_value() && !r.error);
    ZTH_EXPECT_EQ(paths(*r.config), std::string("name,debug,server.port,server.ratio,server.tls.cert,server.tls.a.b"));
    ZTH_EXPECT_EQ(r.config->get_string("name").value_or("?"), std::string("demo"));
    ZTH_EXPECT_EQ(r.config->get_bool("debug").value_or(false), true);
    ZTH_EXPECT_EQ(r.config->get_int("server.port").value_or(0), 8080LL);
    ZTH_EXPECT_NEAR(r.config->get_double("server.ratio").value_or(0), 0.75, 0);
    ZTH_EXPECT_EQ(r.config->get_string("server.tls.cert").value_or("?"), std::string("a.pem"));
    ZTH_EXPECT_EQ(r.config->get_int("server.tls.a.b").value_or(0), 1LL);
    ZTH_EXPECT_TRUE(r.config->has_section("server") && r.config->has_section("server.tls") && !r.config->has_section("other"));
}
ZTH_TEST("works", "empty and comment-only documents, blank lines, CRLF line endings and a byte order mark") {
    ZTH_EXPECT_TRUE(P("").config.has_value() && P("").config->entries().empty());
    ZTH_EXPECT_TRUE(P("\n\n  \t \n").config.has_value());
    ZTH_EXPECT_TRUE(P("# nothing\n# here").config.has_value() && P("# nothing").config->entries().empty());
    const ParseResult crlf = P("a = 1\r\n\r\n[s]\r\nb = \"x\"\r\n");
    ZTH_EXPECT_TRUE(crlf.config.has_value() && paths(*crlf.config) == "a,s.b" && crlf.config->get_string("s.b").value_or("?") == "x");
    const ParseResult bom = P("\xEF\xBB\xBF" "a = 1\n");
    ZTH_EXPECT_TRUE(bom.config.has_value() && bom.config->get_int("a").value_or(0) == 1);
    ZTH_EXPECT_EQ(E("\xEF\xBB\xBF" "a = 1 2\n"), std::string("TrailingGarbage 1:7"));   // the BOM is not a column
    ZTH_EXPECT_EQ(P("a=1").config->get_int("a").value_or(0), 1LL);                        // no final newline, no spaces
    ZTH_EXPECT_EQ(P("  \t a \t = \t 1 \t \n").config->get_int("a").value_or(0), 1LL);
}
ZTH_TEST("works", "keys: bare, quoted, dotted, with spaces around the dots; sections with spaces inside the brackets") {
    ZTH_EXPECT_EQ(paths(*P("a-b_c1 = 1\n\"my key\" = 2\n'lit key' = 3\nx . y = 4\n\"a.b\".c = 5\n").config), std::string("a-b_c1,my key,lit key,x.y,a.b.c"));
    ZTH_EXPECT_EQ(paths(*P("[ a . b ]\nk = 1\n[\"q r\"]\nk = 2\n").config), std::string("a.b.k,q r.k"));
    ZTH_EXPECT_TRUE(P("[a]\n[b]\n").config.has_value() && P("[a]\n[b]\n").config->has_section("b"));
    ZTH_EXPECT_EQ(paths(*P("1 = 2\n-a- = 3\n").config), std::string("1,-a-"));
}
ZTH_TEST("works", "basic strings: every escape, Unicode escapes written as UTF-8, bytes above 0x7F passed through") {
    ZTH_EXPECT_TRUE(one("s = \"a\\\"b\\\\c\\nd\\te\\rf\\bg\\fh\"\n") == V::str("a\"b\\c\nd\te\rf\bg\fh"));
    ZTH_EXPECT_TRUE(one("s = \"\\u0041\\u00e9\\u20AC\"\n") == V::str("A\xC3\xA9\xE2\x82\xAC"));
    ZTH_EXPECT_TRUE(one("s = \"\\u0080\\u07FF\\u0800\\uFFFD\\uD7FF\\uE000\"\n") == V::str("\xC2\x80\xDF\xBF\xE0\xA0\x80\xEF\xBF\xBD\xED\x9F\xBF\xEE\x80\x80"));
    ZTH_EXPECT_TRUE(one("s = \"caf\xC3\xA9 \xE2\x82\xAC\"\n") == V::str("caf\xC3\xA9 \xE2\x82\xAC"));
    ZTH_EXPECT_TRUE(one("s = \"\"\n") == V::str(""));
    ZTH_EXPECT_TRUE(one("s = \"tab\there\"\n") == V::str("tab\there"));   // a literal tab is allowed
    ZTH_EXPECT_TRUE(one("s = \"#not a comment\"\n") == V::str("#not a comment"));
    ZTH_EXPECT_TRUE(one("s = \"a,b]c\"\n") == V::str("a,b]c"));
    ZTH_EXPECT_TRUE(one("s = \"\\u0000\"\n") == V::str(std::string(1, '\0')));
}
ZTH_TEST("works", "literal strings keep every backslash, and can hold double quotes") {
    ZTH_EXPECT_TRUE(one("s = 'C:\\new\\table'\n") == V::str("C:\\new\\table"));
    ZTH_EXPECT_TRUE(one("s = 'say \"hi\"'\n") == V::str("say \"hi\""));
    ZTH_EXPECT_TRUE(one("s = ''\n") == V::str(""));
    ZTH_EXPECT_TRUE(one("s = 'a # b'\n") == V::str("a # b"));
}
ZTH_TEST("works", "integers: signs, underscores, hex, the extremes") {
    ZTH_EXPECT_TRUE(one("i = 0\n") == V::integer(0));
    ZTH_EXPECT_TRUE(one("i = 42\n") == V::integer(42));
    ZTH_EXPECT_TRUE(one("i = +42\n") == V::integer(42));
    ZTH_EXPECT_TRUE(one("i = -42\n") == V::integer(-42));
    ZTH_EXPECT_TRUE(one("i = -0\n") == V::integer(0));
    ZTH_EXPECT_TRUE(one("i = 1_000_000\n") == V::integer(1000000));
    ZTH_EXPECT_TRUE(one("i = 0xFF\n") == V::integer(255));
    ZTH_EXPECT_TRUE(one("i = 0xdead_BEEF\n") == V::integer(3735928559LL));
    ZTH_EXPECT_TRUE(one("i = 0x7FFFFFFFFFFFFFFF\n") == V::integer(9223372036854775807LL));
    ZTH_EXPECT_TRUE(one("i = 9223372036854775807\n") == V::integer(9223372036854775807LL));
    ZTH_EXPECT_TRUE(one("i = -9223372036854775808\n") == V::integer(-9223372036854775807LL - 1));
    ZTH_EXPECT_TRUE(one("i = 0x0\n") == V::integer(0));
}
ZTH_TEST("works", "floats: fractions, exponents, signs, underscores; a float is not an integer and vice versa") {
    ZTH_EXPECT_TRUE(one("f = 3.14\n") == V::real(3.14));
    ZTH_EXPECT_TRUE(one("f = -0.5\n") == V::real(-0.5));
    ZTH_EXPECT_TRUE(one("f = +1.5\n") == V::real(1.5));
    ZTH_EXPECT_TRUE(one("f = 1e10\n") == V::real(1e10));
    ZTH_EXPECT_TRUE(one("f = 2.5E-3\n") == V::real(2.5e-3));
    ZTH_EXPECT_TRUE(one("f = 6.02e+23\n") == V::real(6.02e23));
    ZTH_EXPECT_TRUE(one("f = 1_0.5_5\n") == V::real(10.55));
    ZTH_EXPECT_TRUE(one("f = 0.0\n") == V::real(0.0));
    ZTH_EXPECT_TRUE(one("f = 0e0\n") == V::real(0.0));
    ZTH_EXPECT_TRUE(one("f = 5e-324\n") == V::real(5e-324));            // the smallest denormal is fine
    ZTH_EXPECT_TRUE(one("f = 1.7976931348623157e308\n") == V::real(1.7976931348623157e308));
    ZTH_EXPECT_FALSE(one("f = 1.0\n") == V::integer(1));
    ZTH_EXPECT_TRUE(P("a = 1\nb = 1.0\n").config->get_double("a").value_or(0) == 1.0 && !P("a = 1\n").config->get_int("a").has_value() == false);
    ZTH_EXPECT_FALSE(P("b = 1.5\n").config->get_int("b").has_value());
    ZTH_EXPECT_FALSE(P("b = \"1\"\n").config->get_double("b").has_value());
}
ZTH_TEST("works", "booleans, and the typed getters return nothing for a missing key or a different type") {
    const ParseResult r = P("t = true\nf = false\ns = \"x\"\n");
    ZTH_EXPECT_TRUE(r.config->get_bool("t").value_or(false) == true && r.config->get_bool("f").value_or(true) == false);
    ZTH_EXPECT_FALSE(r.config->get_bool("s").has_value());
    ZTH_EXPECT_FALSE(r.config->get_string("t").has_value());
    ZTH_EXPECT_FALSE(r.config->get_int("missing").has_value());
    ZTH_EXPECT_TRUE(r.config->find("missing") == nullptr && r.config->find("t") != nullptr);
}
ZTH_TEST("works", "arrays: empty, nested, trailing commas, line breaks and comments between elements") {
    ZTH_EXPECT_TRUE(one("a = []\n") == V::array({}));
    ZTH_EXPECT_TRUE(one("a = [1,2,3]\n") == V::array({V::integer(1), V::integer(2), V::integer(3)}));
    ZTH_EXPECT_TRUE(one("a = [ 1 , 2 , 3 , ]\n") == V::array({V::integer(1), V::integer(2), V::integer(3)}));
    ZTH_EXPECT_TRUE(one("a = [\"x\", 'y']\n") == V::array({V::str("x"), V::str("y")}));
    ZTH_EXPECT_TRUE(one("a = [[1, 2], [\"a\"], []]\n") == V::array({V::array({V::integer(1), V::integer(2)}), V::array({V::str("a")}), V::array({})}));
    ZTH_EXPECT_TRUE(one("a = [\n  1, # one\n  2,\n\n  3 # three\n]\n") == V::array({V::integer(1), V::integer(2), V::integer(3)}));
    ZTH_EXPECT_TRUE(one("a = [\r\n1,\r\n2\r\n]\r\n") == V::array({V::integer(1), V::integer(2)}));
    ZTH_EXPECT_TRUE(one("a = [true,false]\n") == V::array({V::boolean(true), V::boolean(false)}));
    ZTH_EXPECT_TRUE(one("a = [1.5, 2e3]\n") == V::array({V::real(1.5), V::real(2000.0)}));
    ZTH_EXPECT_TRUE(one("a = [\"]\", \"#\", \",\"]\n") == V::array({V::str("]"), V::str("#"), V::str(",")}));
}
ZTH_TEST("works", "line numbers follow arrays that span several lines") {
    const ParseResult r = P("a = [\n1,\n2\n]\nb = 7\nc = oops\n");
    ZTH_EXPECT_TRUE(!r.config && r.error && r.error->kind == ErrorKind::InvalidValue && r.error->line == 6 && r.error->column == 5);
}
ZTH_TEST("works", "errors in keys and headers: where each one points") {
    ZTH_EXPECT_EQ(E("= 1\n"), std::string("InvalidKey 1:1"));
    ZTH_EXPECT_EQ(E("a..b = 1\n"), std::string("InvalidKey 1:3"));
    ZTH_EXPECT_EQ(E(".a = 1\n"), std::string("InvalidKey 1:1"));
    ZTH_EXPECT_EQ(E("a. = 1\n"), std::string("InvalidKey 1:4"));
    ZTH_EXPECT_EQ(E("a b = 1\n"), std::string("ExpectedEquals 1:3"));
    ZTH_EXPECT_EQ(E("abc\n"), std::string("ExpectedEquals 1:4"));
    ZTH_EXPECT_EQ(E("abc"), std::string("ExpectedEquals 1:4"));
    ZTH_EXPECT_EQ(E("x = 1\n  $y = 2\n"), std::string("InvalidKey 2:3"));
    ZTH_EXPECT_EQ(E("a : 1\n"), std::string("ExpectedEquals 1:3"));
    ZTH_EXPECT_EQ(E("[a\nb = 1\n"), std::string("InvalidSection 1:3"));
    ZTH_EXPECT_EQ(E("[a"), std::string("InvalidSection 1:3"));
    ZTH_EXPECT_EQ(E("[]\n"), std::string("InvalidSection 1:2"));
    ZTH_EXPECT_EQ(E("[a..b]\n"), std::string("InvalidSection 1:4"));
    ZTH_EXPECT_EQ(E("[[a]]\n"), std::string("InvalidSection 1:2"));
    ZTH_EXPECT_EQ(E("[a] x\n"), std::string("TrailingGarbage 1:5"));
    ZTH_EXPECT_EQ(E("[a] # fine\n"), std::string("OK"));
    ZTH_EXPECT_EQ(E("[a b]\n"), std::string("InvalidSection 1:4"));
}
ZTH_TEST("works", "errors in values: missing, bare words, trailing text") {
    ZTH_EXPECT_EQ(E("a =\n"), std::string("ExpectedValue 1:4"));
    ZTH_EXPECT_EQ(E("a = \n"), std::string("ExpectedValue 1:5"));
    ZTH_EXPECT_EQ(E("a ="), std::string("ExpectedValue 1:4"));
    ZTH_EXPECT_EQ(E("a = # comment\n"), std::string("ExpectedValue 1:5"));
    ZTH_EXPECT_EQ(E("a = ,\n"), std::string("ExpectedValue 1:5"));
    ZTH_EXPECT_EQ(E("a = hello\n"), std::string("InvalidValue 1:5"));
    ZTH_EXPECT_EQ(E("a = True\n"), std::string("InvalidValue 1:5"));
    ZTH_EXPECT_EQ(E("a = truee\n"), std::string("InvalidValue 1:5"));
    ZTH_EXPECT_EQ(E("a = inf\n"), std::string("InvalidValue 1:5"));
    ZTH_EXPECT_EQ(E("a = _1\n"), std::string("InvalidValue 1:5"));   // an underscore cannot start a number
    ZTH_EXPECT_EQ(E("a = nan\n"), std::string("InvalidValue 1:5"));
    ZTH_EXPECT_EQ(E("a = 1 2\n"), std::string("TrailingGarbage 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\" y\n"), std::string("TrailingGarbage 1:9"));
    ZTH_EXPECT_EQ(E("a = true false\n"), std::string("TrailingGarbage 1:10"));
    ZTH_EXPECT_EQ(E("a = 1]\n"), std::string("TrailingGarbage 1:6"));
    ZTH_EXPECT_EQ(E("a = 1,\n"), std::string("TrailingGarbage 1:6"));
    ZTH_EXPECT_EQ(E("a = \"\"\"x\"\"\"\n"), std::string("TrailingGarbage 1:7"));   // triple quotes are not supported: "" then a stray quote
}
ZTH_TEST("works", "errors in numbers: the grammar is strict and the position is the first character of the number") {
    for (const char* bad : {"007", "00", "01.5", "1_", "1__0", "1_.5", "1._5", "1.", ".5", "-.5", "1.e3", "1e", "1e+", "1e5.5", "0x", "0xG", "0x_1", "0x1_", "-0x1", "+0x1", "0b101", "1.2.3", "--1", "+-1", "1-2", "1e1_", "9223372036854775808", "-9223372036854775809",
                            "0x8000000000000000", "1e999", "-1e999", "12abc", "0.", "5e-", "1__000"})
        ZTH_EXPECT_EQ(E(std::string("n = ") + bad + "\n"), std::string("InvalidNumber 1:5"));
    ZTH_EXPECT_EQ(E("first = 1\nsecond = 2.\n"), std::string("InvalidNumber 2:10"));
    ZTH_EXPECT_EQ(E("a = [1, 08]\n"), std::string("InvalidNumber 1:9"));
}
ZTH_TEST("works", "errors in strings: unterminated (at the opening quote), bad escapes (at the backslash), control characters") {
    ZTH_EXPECT_EQ(E("a = \"abc\n"), std::string("UnterminatedString 1:5"));
    ZTH_EXPECT_EQ(E("a = \"abc"), std::string("UnterminatedString 1:5"));
    ZTH_EXPECT_EQ(E("a = 'abc\n"), std::string("UnterminatedString 1:5"));
    ZTH_EXPECT_EQ(E("a = \"abc\r\n"), std::string("UnterminatedString 1:5"));
    ZTH_EXPECT_EQ(E("a = [\"x\", \"y\n]\n"), std::string("UnterminatedString 1:11"));
    ZTH_EXPECT_EQ(E("a = \"x\\qy\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\\"\n"), std::string("UnterminatedString 1:5"));   // \" is an escaped quote: the string never closes
    ZTH_EXPECT_EQ(E("a = \"x\\u12\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\u12G4\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\uD800\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\uDFFF\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\0\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\'\"\n"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\\"), std::string("InvalidEscape 1:7"));
    ZTH_EXPECT_EQ(E("a = \"x\x01y\"\n"), std::string("ControlCharacter 1:7"));
    ZTH_EXPECT_EQ(E("a = 'x\x7Fy'\n"), std::string("ControlCharacter 1:7"));
    ZTH_EXPECT_EQ(E(std::string("a = \"x") + '\0' + "y\"\n"), std::string("ControlCharacter 1:7"));
    ZTH_EXPECT_EQ(E("\"a\x1F\" = 1\n"), std::string("ControlCharacter 1:3"));
}
ZTH_TEST("works", "a lone carriage return is a control character wherever it appears outside a comment") {
    ZTH_EXPECT_EQ(E("a = 1\rb = 2\n"), std::string("TrailingGarbage 1:6"));
    ZTH_EXPECT_EQ(E("a\r= 1\n"), std::string("ExpectedEquals 1:2"));
    ZTH_EXPECT_EQ(E("a = \"x\ry\"\n"), std::string("ControlCharacter 1:7"));
    ZTH_EXPECT_EQ(E("# fine \r still a comment\na = 1\n"), std::string("OK"));
}
ZTH_TEST("works", "arrays: unterminated, mixed types, bad separators, empty elements") {
    ZTH_EXPECT_EQ(E("a = [1, 2\n"), std::string("UnterminatedArray 1:5"));
    ZTH_EXPECT_EQ(E("a = [\n1,\n2,\n"), std::string("UnterminatedArray 1:5"));
    ZTH_EXPECT_EQ(E("a = ["), std::string("UnterminatedArray 1:5"));
    ZTH_EXPECT_EQ(E("a = [1, [2, 3\n"), std::string("UnterminatedArray 1:9"));   // the innermost unclosed array is reported
    ZTH_EXPECT_EQ(E("a = [1, 2.5]\n"), std::string("MixedArray 1:9"));
    ZTH_EXPECT_EQ(E("a = [\"x\", 1]\n"), std::string("MixedArray 1:11"));
    ZTH_EXPECT_EQ(E("a = [true, \"x\"]\n"), std::string("MixedArray 1:12"));
    ZTH_EXPECT_EQ(E("a = [1,\n  2,\n  'three']\n"), std::string("MixedArray 3:3"));
    ZTH_EXPECT_EQ(E("a = [[1], 2]\n"), std::string("MixedArray 1:11"));
    ZTH_EXPECT_EQ(E("a = [1 2]\n"), std::string("UnexpectedCharacter 1:8"));
    ZTH_EXPECT_EQ(E("a = [1;2]\n"), std::string("InvalidNumber 1:6"));   // "1;2" is one token
    ZTH_EXPECT_EQ(E("a = [1,,2]\n"), std::string("ExpectedValue 1:8"));
    ZTH_EXPECT_EQ(E("a = [,]\n"), std::string("ExpectedValue 1:6"));
    ZTH_EXPECT_EQ(E("a = [1,2]]\n"), std::string("TrailingGarbage 1:10"));
    ZTH_EXPECT_EQ(E("a = [1 # comment\n, 2]\n"), std::string("OK"));
}
ZTH_TEST("works", "duplicates: keys (at the second key), sections (at the second '['), and dotted-path collisions across sections") {
    ZTH_EXPECT_EQ(E("a = 1\nb = 2\na = 3\n"), std::string("DuplicateKey 3:1"));
    ZTH_EXPECT_EQ(E("[s]\nx = 1\n  x = 2\n"), std::string("DuplicateKey 3:3"));
    ZTH_EXPECT_EQ(E("[s]\nx = 1\n[t]\nx = 2\n"), std::string("OK"));
    ZTH_EXPECT_EQ(E("s.x = 1\n[s]\nx = 2\n"), std::string("DuplicateKey 3:1"));   // the same full path "s.x"
    ZTH_EXPECT_EQ(E("[s]\n[s]\n"), std::string("DuplicateSection 2:1"));
    ZTH_EXPECT_EQ(E("[a]\nx=1\n[b]\n  [a]\n"), std::string("DuplicateSection 4:3"));
    ZTH_EXPECT_EQ(E("[a.b]\n[a]\n[a.b]\n"), std::string("DuplicateSection 3:1"));
    ZTH_EXPECT_EQ(E("\"a b\" = 1\n'a b' = 2\n"), std::string("DuplicateKey 2:1"));   // quoting does not make a different key
    ZTH_EXPECT_EQ(E("a.b = 1\na . b = 2\n"), std::string("DuplicateKey 2:1"));
    ZTH_EXPECT_EQ(E("a = 1\nA = 2\n"), std::string("OK"));   // keys are case-sensitive
}
ZTH_TEST("works", "the first error wins, whichever kind it is, and later lines are never looked at") {
    ZTH_EXPECT_EQ(E("a = 1x\nb = \"unterminated\n"), std::string("InvalidNumber 1:5"));
    ZTH_EXPECT_EQ(E("ok = 1\n\n\n   bad line here\n[also bad\n"), std::string("ExpectedEquals 4:8"));
    ZTH_EXPECT_EQ(E("# c\n\n\t\tkey = [1, 2\n"), std::string("UnterminatedArray 3:9"));
}
ZTH_TEST("works", "render() prints a compiler-style diagnostic with a caret under the column") {
    const std::string src = "name = \"x\"\nport = 80x\nlast = 1\n";
    const ParseResult r = P(src);
    ZTH_EXPECT_TRUE(r.error.has_value());
    const std::string out = render(src, *r.error, "app.conf");
    ZTH_EXPECT_EQ(out, "app.conf:2:8: error: " + r.error->message + "\n2 | port = 80x\n  |        ^\n");
    const Error handmade{ErrorKind::InvalidValue, 12, 3, "oops"};
    std::string text;
    for (int i = 1; i <= 12; ++i) text += "\tline " + std::to_string(i) + "\n";
    ZTH_EXPECT_EQ(render(text, handmade, "f"), std::string("f:12:3: error: oops\n12 | \tline 12\n   | \t ^\n"));   // the tab before the column is kept, so the caret lines up
}
ZTH_TEST("works", "render() copes with CRLF files, the last line without a newline, and positions beyond the text") {
    ZTH_EXPECT_EQ(render("a = 1\r\nb = x\r\n", Error{ErrorKind::InvalidValue, 2, 5, "m"}, "f"), std::string("f:2:5: error: m\n2 | b = x\n  |     ^\n"));
    ZTH_EXPECT_EQ(render("a = 1\nb = x", Error{ErrorKind::InvalidValue, 2, 5, "m"}, "f"), std::string("f:2:5: error: m\n2 | b = x\n  |     ^\n"));
    ZTH_EXPECT_EQ(render("a = 1\n", Error{ErrorKind::ExpectedValue, 3, 1, "m"}, "f"), std::string("f:3:1: error: m\n3 | \n  | ^\n"));
    ZTH_EXPECT_EQ(render("", Error{ErrorKind::ExpectedValue, 1, 1, "m"}, "f"), std::string("f:1:1: error: m\n1 | \n  | ^\n"));
    ZTH_EXPECT_EQ(render("x\n", Error{ErrorKind::ExpectedValue, 1, 1, "m"}, ""), std::string(":1:1: error: m\n1 | x\n  | ^\n"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "documents written by a generator parse back to exactly the values that were written (2000 random documents)") {
    zth::Lcg rng(31);
    const char* words[] = {"alpha", "beta", "gamma-1", "delta_2", "x", "y", "Z9"};
    for (int round = 0; round < 2000; ++round) {
        std::string doc;
        std::vector<std::pair<std::string, Value>> want;
        std::string section;
        const int n = 1 + rng.below(12);
        for (int i = 0; i < n; ++i) {
            if (rng.below(5) == 0) {
                section = std::string(words[rng.below(7)]) + std::to_string(round * 100 + i);
                doc += "[" + section + "]\n";
                continue;
            }
            const std::string key = std::string(words[rng.below(7)]) + std::to_string(i);
            Value v;
            std::string text;
            switch (rng.below(5)) {
                case 0: { const long long x = static_cast<long long>(rng.below(2000001)) - 1000000; v = Value::integer(x); text = std::to_string(x); break; }
                case 1: { const double x = (static_cast<int>(rng.below(200001)) - 100000) / 64.0; char b[64]; std::snprintf(b, sizeof b, "%.17g", x); v = Value::real(x); text = b; if (text.find_first_of(".eEn") == std::string::npos) text += ".0"; break; }
                case 2: { v = Value::boolean(rng.below(2) == 0); text = v.b ? "true" : "false"; break; }
                case 3: { std::string s; for (int k = 0, m = rng.below(8); k < m; ++k) s.push_back("ab #,]\"\\\t\xC3\xA9"[rng.below(11)]); v = Value::str(s);
                          text = "\""; for (char c : s) { if (c == '"') text += "\\\""; else if (c == '\\') text += "\\\\"; else if (c == '\t') text += "\\t"; else text.push_back(c); } text += "\""; break; }
                default: { std::vector<Value> items; text = "["; for (int k = 0, m = rng.below(5); k < m; ++k) { items.push_back(Value::integer(k * 7 - 3)); text += (k ? ", " : "") + std::to_string(k * 7 - 3); } text += (rng.below(2) && !items.empty()) ? ",]" : "]"; v = Value::array(items); break; }
            }
            doc += key + " = " + text + (rng.below(3) == 0 ? "   # note" : "") + (rng.below(4) == 0 ? "\r\n" : "\n");
            want.push_back({(section.empty() ? "" : section + ".") + key, v});
        }
        const ParseResult r = P(doc);
        if (!r.config) {
            zth::fail("a generated document was rejected at " + std::to_string(r.error->line) + ":" + std::to_string(r.error->column) + ": " + r.error->message + "\n" + doc);
            return;
        }
        for (const auto& [path, v] : want) {
            const Value* got = r.config->find(path);
            if (!got || !(*got == v)) {
                zth::fail("value of " + path + " is wrong in\n" + doc);
                return;
            }
        }
    }
}
ZTH_TEST("robust", "every one-character mutation of a valid document is either accepted or rejected with a position inside the document, never a crash") {
    const std::string base = "# c\nname = \"a\\n\\u00e9\"\n[s.t]\nlist = [1, 2,\n  3]\nratio = -1.5e-3  # x\nhex = 0xFF\nflag = true\n'q k' = 'lit'\n";
    ZTH_EXPECT_TRUE(P(base).config.has_value());
    const std::string alphabet = "a1\"'\\[]=,.#_ \t\r\n-+e0x{}\x01\x7f\xC3";
    int rejected = 0;
    for (std::size_t pos = 0; pos <= base.size(); ++pos)
        for (char c : alphabet) {
            for (int mode = 0; mode < 3; ++mode) {
                std::string doc = base;
                if (mode == 0 && pos < doc.size()) doc[pos] = c;
                else if (mode == 1) doc.insert(pos, 1, c);
                else if (mode == 2 && pos < doc.size()) doc.erase(pos, 1);
                else continue;
                const ParseResult r = P(doc);
                ZTH_EXPECT_TRUE(r.config.has_value() != r.error.has_value());
                if (r.error) {
                    ++rejected;
                    std::size_t lines = 1;
                    for (char d : doc) lines += d == '\n';
                    ZTH_EXPECT_TRUE(r.error->line >= 1 && r.error->line <= lines);
                    ZTH_EXPECT_TRUE(r.error->column >= 1 && !r.error->message.empty());
                }
            }
        }
    ZTH_EXPECT_TRUE(rejected > 500);
}
ZTH_TEST("robust", "random garbage never crashes or hangs, and errors always point inside the text") {
    zth::Lcg rng(77);
    const std::string alphabet = "ab1 \t\r\n\"'\\[]=,.#_-+eE0xX{}\xEF\xBB\xBF";
    for (int round = 0; round < 5000; ++round) {
        std::string doc;
        for (int i = 0, n = rng.below(40); i < n; ++i) doc.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
        const ParseResult r = P(doc);
        ZTH_EXPECT_TRUE(r.config.has_value() != r.error.has_value());
        if (r.error) {
            std::size_t lines = 1, last_line_start = 0;
            for (std::size_t k = 0; k < doc.size(); ++k)
                if (doc[k] == '\n') {
                    ++lines;
                    last_line_start = k + 1;
                }
            ZTH_EXPECT_TRUE(r.error->line >= 1 && r.error->line <= lines && r.error->column >= 1);
            if (r.error->line == lines) ZTH_EXPECT_TRUE(r.error->column <= doc.size() - last_line_start + 1);
            (void)render(doc, *r.error, "x");
        }
    }
}
ZTH_TEST("robust", "numbers: random integers and doubles written with %.17g parse back exactly; the boundaries of long long are exact") {
    zth::Lcg rng(3);
    for (int i = 0; i < 3000; ++i) {
        const long long v = static_cast<long long>((static_cast<unsigned long long>(rng.next()) << 33) ^ (static_cast<unsigned long long>(rng.next()) << 2) ^ rng.next()) * (rng.below(2) ? 1 : -1);
        ZTH_EXPECT_TRUE(one("i = " + std::to_string(v) + "\n") == V::integer(v));
        const double d = std::ldexp(static_cast<double>(rng.next()) / 2147483648.0 - 0.5, rng.below(200) - 100);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", d);
        std::string t = buf;
        if (t.find_first_of(".eEn") == std::string::npos) t += ".0";
        ZTH_EXPECT_TRUE(one("f = " + t + "\n") == V::real(d));
    }
    ZTH_EXPECT_EQ(E("i = 9223372036854775807\n"), std::string("OK"));
    ZTH_EXPECT_EQ(E("i = 9223372036854775808\n"), std::string("InvalidNumber 1:5"));
    ZTH_EXPECT_EQ(E("i = -9223372036854775808\n"), std::string("OK"));
    ZTH_EXPECT_EQ(E("i = -9223372036854775809\n"), std::string("InvalidNumber 1:5"));
    ZTH_EXPECT_EQ(E("i = 99999999999999999999999999\n"), std::string("InvalidNumber 1:5"));
    ZTH_EXPECT_EQ(E("i = 0xFFFFFFFFFFFFFFFFFF\n"), std::string("InvalidNumber 1:5"));
}
ZTH_TEST("robust", "deep and long: 500 nested arrays, a 100000-element array, a 1 MB string, a 100000-character key, 20000 sections") {
    std::string deep = "a = ";
    deep.append(500, '[');
    deep.append(500, ']');
    deep += "\n";
    ZTH_EXPECT_TRUE(P(deep).config.has_value());
    std::string wide = "a = [";
    for (int i = 0; i < 100000; ++i) wide += (i ? "," : "") + std::to_string(i);
    wide += "]\n";
    const ParseResult w = P(wide);
    ZTH_EXPECT_TRUE(w.config.has_value() && w.config->find("a")->a.size() == 100000 && w.config->find("a")->a[99999] == V::integer(99999));
    const std::string big(1 << 20, 'x');
    ZTH_EXPECT_TRUE(one("s = \"" + big + "\"\n") == V::str(big));
    const std::string key(100000, 'k');
    ZTH_EXPECT_EQ(P(key + " = 1\n").config->get_int(key).value_or(0), 1LL);
    std::string many;
    for (int i = 0; i < 20000; ++i) many += "[sec" + std::to_string(i) + "]\nk = " + std::to_string(i) + "\n";
    const ParseResult m = P(many);
    ZTH_EXPECT_TRUE(m.config.has_value() && m.config->entries().size() == 20000 && m.config->get_int("sec19999.k").value_or(-1) == 19999);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a 3 MB document of 100000 settings parses in linear time (duplicate detection is a hash lookup)", 20) {
    std::string doc;
    for (int i = 0; i < 100000; ++i) doc += "key_number_" + std::to_string(i) + " = " + (i % 3 == 0 ? "\"value " + std::to_string(i) + "\"" : i % 3 == 1 ? std::to_string(i) + ".5" : "[1, 2, 3]") + "  # c\n";
    const ParseResult r = P(doc);
    ZTH_EXPECT_TRUE(r.config.has_value() && r.config->entries().size() == 100000);
    std::string dup = doc + "key_number_99999 = 1\n";
    ZTH_EXPECT_EQ(E(dup), std::string("DuplicateKey 100001:1"));
}
ZTH_TEST("fast", "the tokenizer does not allocate per character: a document of 20000 integers makes a bounded number of allocations per line") {
    std::string doc;
    for (int i = 0; i < 20000; ++i) doc += "k" + std::to_string(i) + " = " + std::to_string(i) + "\n";
    zth::mem::Scope scope;
    const ParseResult r = P(doc);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 20000 * 9 + 100);
    ZTH_EXPECT_TRUE(r.config.has_value());
}
ZTH_TEST_TIMEOUT("fast", "errors deep in a huge document are found quickly, and render() of a late line does not rescan quadratically", 20) {
    std::string doc;
    for (int i = 0; i < 200000; ++i) doc += "a" + std::to_string(i) + " = 1\n";
    doc += "bad = 1x\n";
    const ParseResult r = P(doc);
    ZTH_EXPECT_TRUE(r.error && r.error->line == 200001 && r.error->column == 7);
    std::size_t total = 0;
    for (int i = 0; i < 200; ++i) total += render(doc, *r.error, "f").size();
    ZTH_EXPECT_TRUE(total > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("config.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("config.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a hand-written scanner: no regex, no string streams, no stoi/stod/strtod/sscanf/atoi; numbers go through from_chars") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("config.hpp"));
    for (const char* word : {"regex", "stringstream", "istringstream", "ostringstream", "stoi", "stol", "stoll", "stod", "stof", "strtol", "strtoll", "strtod", "atoi", "atol", "atof", "sscanf", "getline", "boost", "toml"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "from_chars").empty());
}
