#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "parser.hpp"
#include "writer.hpp"
#include "zth_test.hpp"

using namespace json;

// shortens a text for a failure message (a local helper: zth::shorten is not available in every build)
inline std::string clip(std::string s) {
    for (char& c : s) {
        if (c == '\n') c = ' ';
    }
    return s.size() > 90 ? s.substr(0, 87) + "..." : s;
}

namespace {

std::string show(const std::string& t) {
    std::string s;
    for (char c : t) {
        if (c == '\n') s += "\\n";
        else if (c == '\t') s += "\\t";
        else if (c == '\r') s += "\\r";
        else if (static_cast<unsigned char>(c) < 0x20) s += "?";
        else s += c;
    }
    return clip(s);
}

Value must_parse(const std::string& text) {
    ParseResult r = parse(text);
    if (!r.ok()) {
        ::zth::fail("parse(" + show(text) + ") reported " + name(r.error) + " at " + std::to_string(r.line) + ":" + std::to_string(r.column) + " (" + r.message + ")");
        return Value();
    }
    return std::move(r.value);
}

void expect_error(const std::string& text, Error e, std::size_t line, std::size_t col) {
    const ParseResult r = parse(text);
    const std::string want = std::string(name(e)) + " at " + std::to_string(line) + ":" + std::to_string(col);
    const std::string got = r.ok() ? std::string("success") : std::string(name(r.error)) + " at " + std::to_string(r.line) + ":" + std::to_string(r.column);
    if (want != got) ::zth::fail("parse(" + show(text) + "): expected " + want + ", got " + got);
    else if (r.message.empty()) ::zth::fail("parse(" + show(text) + "): the error has no message");
}

void expect_text(const Value& v, const std::string& want, int indent = -1) {
    const std::string got = to_string(v, indent);
    if (got != want) ::zth::fail("to_string gave " + show(got) + ", expected " + show(want));
}

// a random value for the round-trip test
Value random_value(::zth::Lcg& rng, int depth) {
    switch (rng.below(depth > 3 ? 4 : 6)) {
        case 0: return Value(nullptr);
        case 1: return Value(rng.below(2) == 0);
        case 2: {
            switch (rng.below(4)) {
                case 0: return Value(rng.below(2000) - 1000);
                case 1: return Value(static_cast<double>(rng.next()) / 1024.0);
                case 2: return Value(std::ldexp(static_cast<double>(rng.next()) + 0.5, rng.below(120) - 60));
                default: return Value(-static_cast<double>(rng.next()) / 7.0);
            }
        }
        case 3: {
            std::string s;
            const int n = rng.below(12);
            for (int i = 0; i < n; ++i) {
                switch (rng.below(5)) {
                    case 0: s += static_cast<char>(rng.below(32)); break;       // control characters
                    case 1: s += "\"\\/"[rng.below(3)]; break;                 // the characters that need escaping
                    case 2: s += "\xC3\xA9"; break;                            // a UTF-8 e-acute
                    case 3: s += "\xF0\x9F\x98\x80"; break;                    // a UTF-8 emoji
                    default: s += static_cast<char>('a' + rng.below(26));
                }
            }
            return Value(std::move(s));
        }
        case 4: {
            Array a;
            const int n = rng.below(5);
            for (int i = 0; i < n; ++i) a.push_back(random_value(rng, depth + 1));
            return Value(std::move(a));
        }
        default: {
            Object o;
            const int n = rng.below(5);
            for (int i = 0; i < n; ++i) o.insert_or_assign("k" + std::to_string(rng.below(8)), random_value(rng, depth + 1));
            return Value(std::move(o));
        }
    }
}

// a document of n records, as text
std::string make_document(int n) {
    std::string t = "[";
    for (int i = 0; i < n; ++i) {
        if (i) t += ",\n";
        t += "  {\"id\": " + std::to_string(i) + ", \"name\": \"user number " + std::to_string(i) + "\", \"tags\": [\"alpha\", \"beta\", \"gamma\"], ";
        t += "\"score\": " + std::to_string(i) + ".25, \"active\": " + (i % 2 ? "true" : "false") + ", \"note\": null}";
    }
    return t + "]";
}

Value make_value(int n) { return must_parse(make_document(n)); }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "literals and whitespace around the document") {
    ZTH_EXPECT_TRUE(must_parse("null") == Value(nullptr));
    ZTH_EXPECT_TRUE(must_parse("true") == Value(true));
    ZTH_EXPECT_TRUE(must_parse("false") == Value(false));
    ZTH_EXPECT_TRUE(must_parse(" \t\r\n true \n") == Value(true));
}
ZTH_TEST("works", "numbers: integers, negatives, fractions and exponents") {
    ZTH_EXPECT_TRUE(must_parse("0") == Value(0));
    ZTH_EXPECT_TRUE(must_parse("42") == Value(42));
    ZTH_EXPECT_TRUE(must_parse("-17") == Value(-17));
    ZTH_EXPECT_TRUE(must_parse("3.5") == Value(3.5));
    ZTH_EXPECT_TRUE(must_parse("-0.25") == Value(-0.25));
    ZTH_EXPECT_TRUE(must_parse("1e3") == Value(1000));
    ZTH_EXPECT_TRUE(must_parse("2E-2") == Value(0.02));
    ZTH_EXPECT_TRUE(must_parse("2.5e+2") == Value(250));
    ZTH_EXPECT_TRUE(must_parse("123456789") == Value(123456789));
}
ZTH_TEST("works", "strings and the simple escapes") {
    ZTH_EXPECT_TRUE(must_parse("\"\"") == Value(""));
    ZTH_EXPECT_TRUE(must_parse("\"hello world\"") == Value("hello world"));
    ZTH_EXPECT_TRUE(must_parse("\"a\\nb\\tc\"") == Value("a\nb\tc"));
    ZTH_EXPECT_TRUE(must_parse("\"q\\\"q\\\\q\\/q\"") == Value("q\"q\\q/q"));
    ZTH_EXPECT_TRUE(must_parse("\"\\b\\f\\r\"") == Value("\b\f\r"));
}
ZTH_TEST("works", "\\u escapes become UTF-8, also surrogate pairs; UTF-8 text passes through") {
    ZTH_EXPECT_TRUE(must_parse("\"\\u0041\"") == Value("A"));
    ZTH_EXPECT_TRUE(must_parse("\"\\u00e9\"") == Value("\xC3\xA9"));
    ZTH_EXPECT_TRUE(must_parse("\"\\u00E9\"") == Value("\xC3\xA9"));
    ZTH_EXPECT_TRUE(must_parse("\"\\u20ac\"") == Value("\xE2\x82\xAC"));
    ZTH_EXPECT_TRUE(must_parse("\"\\ud83d\\ude00\"") == Value("\xF0\x9F\x98\x80"));
    ZTH_EXPECT_TRUE(must_parse("\"\xC3\xA9\xF0\x9F\x98\x80\"") == Value("\xC3\xA9\xF0\x9F\x98\x80"));
    ZTH_EXPECT_TRUE(must_parse("\"\\u0000\"") == Value(std::string(1, '\0')));
}
ZTH_TEST("works", "arrays") {
    ZTH_EXPECT_TRUE(must_parse("[]") == Value(Array{}));
    ZTH_EXPECT_TRUE(must_parse("[1, 2, 3]") == Value(Array{1, 2, 3}));
    ZTH_EXPECT_TRUE(must_parse(" [ 1 , \"x\" ,true,null, [ ] ] ") == Value(Array{1, "x", true, nullptr, Array{}}));
    ZTH_EXPECT_TRUE(must_parse("[[1],[2,[3]]]") == Value(Array{Array{1}, Array{2, Array{3}}}));
}
ZTH_TEST("works", "objects") {
    ZTH_EXPECT_TRUE(must_parse("{}") == Value(Object{}));
    ZTH_EXPECT_TRUE(must_parse("{\"a\": 1, \"b\": [true, null], \"c\": {\"d\": \"e\"}}") ==
                    Value(Object{{"a", 1}, {"b", Array{true, nullptr}}, {"c", Object{{"d", "e"}}}}));
    ZTH_EXPECT_TRUE(must_parse("{\"a\\nb\": 1}") == Value(Object{{"a\nb", 1}}));
}
ZTH_TEST("works", "a repeated key: the last one wins") {
    ZTH_EXPECT_TRUE(must_parse("{\"a\": 1, \"b\": 2, \"a\": 3}") == Value(Object{{"a", 3}, {"b", 2}}));
}
ZTH_TEST("works", "writer: compact text, object keys sorted") {
    expect_text(Value(nullptr), "null");
    expect_text(Value(true), "true");
    expect_text(Value(Array{1, 2, 3}), "[1,2,3]");
    expect_text(Value(Object{{"b", Array{true, nullptr}}, {"a", 1}}), "{\"a\":1,\"b\":[true,null]}");
    expect_text(Value(Array{}), "[]");
    expect_text(Value(Object{}), "{}");
}
ZTH_TEST("works", "writer: numbers (whole numbers without a fraction, others read back exactly)") {
    expect_text(Value(0), "0");
    expect_text(Value(42), "42");
    expect_text(Value(-7), "-7");
    expect_text(Value(1.5), "1.5");
    expect_text(Value(-0.25), "-0.25");
    expect_text(Value(0.1), "0.1");
    expect_text(Value(100000.0), "100000");
    expect_text(Value(3.14159), "3.14159");
}
ZTH_TEST("works", "writer: strings are quoted and escaped") {
    expect_text(Value("hi"), "\"hi\"");
    expect_text(Value("a\"b\\c"), "\"a\\\"b\\\\c\"");
    expect_text(Value("l1\nl2\ttab\r"), "\"l1\\nl2\\ttab\\r\"");
    expect_text(Value("\b\f"), "\"\\b\\f\"");
    expect_text(Value("a/b"), "\"a/b\"");
}
ZTH_TEST("works", "writer: pretty printing with an indent") {
    const Value v(Object{{"name", "Ada"}, {"langs", Array{"C++", "Rust"}}, {"empty", Array{}}, {"none", Object{}}});
    expect_text(v, "{\n  \"empty\": [],\n  \"langs\": [\n    \"C++\",\n    \"Rust\"\n  ],\n  \"name\": \"Ada\",\n  \"none\": {}\n}", 2);
    expect_text(Value(Array{1, Array{2}}), "[\n    1,\n    [\n        2\n    ]\n]", 4);
}
ZTH_TEST("works", "writing and parsing again gives the same value") {
    const Value v(Object{{"a", Array{1, 2.5, "x", nullptr, true}}, {"b", Object{{"c", "d\n"}}}});
    ZTH_EXPECT_TRUE(must_parse(to_string(v)) == v);
    ZTH_EXPECT_TRUE(must_parse(to_string(v, 2)) == v);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "errors: the text ends too early (UnexpectedEnd, position just after the last character) (off-by-one)") {
    expect_error("", Error::UnexpectedEnd, 1, 1);
    expect_error("   \n ", Error::UnexpectedEnd, 2, 2);
    expect_error("[1,2", Error::UnexpectedEnd, 1, 5);
    expect_error("[", Error::UnexpectedEnd, 1, 2);
    expect_error("{", Error::UnexpectedEnd, 1, 2);
    expect_error("{\"a\"", Error::UnexpectedEnd, 1, 5);
    expect_error("{\"a\":", Error::UnexpectedEnd, 1, 6);
    expect_error("{\"a\":1", Error::UnexpectedEnd, 1, 7);
    expect_error("tru", Error::UnexpectedEnd, 1, 4);
    expect_error("\"abc", Error::UnexpectedEnd, 1, 5);
    expect_error("\"\\", Error::UnexpectedEnd, 1, 3);
}
ZTH_TEST("robust", "errors: a character that cannot be here (UnexpectedCharacter)") {
    expect_error("[1,]", Error::UnexpectedCharacter, 1, 4);
    expect_error("[,1]", Error::UnexpectedCharacter, 1, 2);
    expect_error("{\"a\":1,}", Error::UnexpectedCharacter, 1, 8);
    expect_error("{\"a\" 1}", Error::UnexpectedCharacter, 1, 6);
    expect_error("{a:1}", Error::UnexpectedCharacter, 1, 2);
    expect_error("{\"a\":}", Error::UnexpectedCharacter, 1, 6);
    expect_error("[1 2]", Error::UnexpectedCharacter, 1, 4);
    expect_error("trux", Error::UnexpectedCharacter, 1, 4);
    expect_error("nul1", Error::UnexpectedCharacter, 1, 4);
    expect_error(".5", Error::UnexpectedCharacter, 1, 1);
    expect_error("+1", Error::UnexpectedCharacter, 1, 1);
    expect_error("'a'", Error::UnexpectedCharacter, 1, 1);
    expect_error("[01]", Error::UnexpectedCharacter, 1, 3);
    expect_error("[1}", Error::UnexpectedCharacter, 1, 3);
    expect_error("{\"a\":1]", Error::UnexpectedCharacter, 1, 7);
}
ZTH_TEST("robust", "errors: anything after the document (TrailingContent)") {
    expect_error("1 2", Error::TrailingContent, 1, 3);
    expect_error("[] x", Error::TrailingContent, 1, 4);
    expect_error("{}}", Error::TrailingContent, 1, 3);
    expect_error("01", Error::TrailingContent, 1, 2);
    expect_error("-01", Error::TrailingContent, 1, 3);
    expect_error("0x10", Error::TrailingContent, 1, 2);
    expect_error("nullx", Error::TrailingContent, 1, 5);
    expect_error("\"a\" \"b\"", Error::TrailingContent, 1, 5);
}
ZTH_TEST("robust", "errors: the number grammar (BadNumber, at the place where a digit was needed)") {
    expect_error("-", Error::BadNumber, 1, 2);
    expect_error("-x", Error::BadNumber, 1, 2);
    expect_error("1.", Error::BadNumber, 1, 3);
    expect_error("1.e3", Error::BadNumber, 1, 3);
    expect_error("1e", Error::BadNumber, 1, 3);
    expect_error("1e+", Error::BadNumber, 1, 4);
    expect_error("[2.]", Error::BadNumber, 1, 4);
    expect_error("-.5", Error::BadNumber, 1, 2);
}
ZTH_TEST("robust", "errors: a number too big for a double (NumberOutOfRange, at its first character)") {
    expect_error("1e999", Error::NumberOutOfRange, 1, 1);
    expect_error("-1e999", Error::NumberOutOfRange, 1, 1);
    expect_error("[1, 1e400]", Error::NumberOutOfRange, 1, 5);
    ZTH_EXPECT_TRUE(must_parse("1e-999") == Value(0));  // too small is not an error: it rounds to zero
    ZTH_EXPECT_TRUE(must_parse("1.7976931348623157e308") == Value(std::numeric_limits<double>::max()));
}
ZTH_TEST("robust", "errors: raw control characters in a string (BadString)") {
    expect_error("\"a\tb\"", Error::BadString, 1, 3);
    expect_error("\"a\nb\"", Error::BadString, 1, 3);
    expect_error(std::string("\"a") + '\x01' + "\"", Error::BadString, 1, 3);
    ZTH_EXPECT_TRUE(must_parse("\"a\\tb\"") == Value("a\tb"));
}
ZTH_TEST("robust", "errors: broken escapes (BadEscape, at the backslash)") {
    expect_error("\"\\x\"", Error::BadEscape, 1, 2);
    expect_error("\"abc\\q\"", Error::BadEscape, 1, 5);
    expect_error("\"\\u12\"", Error::BadEscape, 1, 2);
    expect_error("\"\\u12G4\"", Error::BadEscape, 1, 2);
    expect_error("\"\\ud83d\"", Error::BadEscape, 1, 2);
    expect_error("\"\\ud83dx\"", Error::BadEscape, 1, 2);
    expect_error("\"\\ud83d\\u0041\"", Error::BadEscape, 1, 2);
    expect_error("\"\\ude00\"", Error::BadEscape, 1, 2);
    expect_error("\"\\ud83d\\ude0g\"", Error::BadEscape, 1, 2);
    expect_error("{\n  \"a\": \"\\q\"\n}", Error::BadEscape, 2, 9);
}
ZTH_TEST("robust", "errors: lines and columns over several lines, also with \\r\\n") {
    expect_error("{\n  \"a\": [1, 2,\n  ]\n}", Error::UnexpectedCharacter, 3, 3);
    expect_error("[1,\r\n 2,\r\n x]", Error::UnexpectedCharacter, 3, 2);
    expect_error("[\n\n\n", Error::UnexpectedEnd, 4, 1);
    expect_error("{\"a\":\n1\n}\n}", Error::TrailingContent, 4, 1);
}
ZTH_TEST("robust", "a failed parse has a null value, an error and a message; a good one has no error") {
    const ParseResult bad = parse("[1,");
    ZTH_EXPECT_FALSE(bad.ok());
    ZTH_EXPECT_TRUE(bad.value == Value());
    ZTH_EXPECT_FALSE(bad.message.empty());
    const ParseResult good = parse("[1]");
    ZTH_EXPECT_TRUE(good.ok());
    ZTH_EXPECT_TRUE(good.error == Error::None);
}
ZTH_TEST("robust", "nesting: 128 levels are fine, the 129th is TooDeep (also for 100000 brackets, without a stack overflow)") {
    const std::string ok_text = std::string(128, '[') + std::string(128, ']');
    ZTH_EXPECT_TRUE(parse(ok_text).ok());
    expect_error(std::string(129, '[') + std::string(129, ']'), Error::TooDeep, 1, 129);
    expect_error(std::string(100000, '['), Error::TooDeep, 1, 129);
    std::string objects;
    for (int i = 0; i < 129; ++i) objects += "{\"k\":";
    expect_error(objects, Error::TooDeep, 1, 128 * 5 + 1);
    std::string mixed;
    for (int i = 0; i < 70; ++i) mixed += "[{\"k\":";
    expect_error(mixed, Error::TooDeep, 1, 385);
}
ZTH_TEST("robust", "writer: NaN and infinity become null; negative zero and big numbers are valid JSON") {
    expect_text(Value(std::numeric_limits<double>::quiet_NaN()), "null");
    expect_text(Value(std::numeric_limits<double>::infinity()), "null");
    expect_text(Value(-std::numeric_limits<double>::infinity()), "null");
    expect_text(Value(-0.0), "-0");
    expect_text(Value(123456789012345.0), "123456789012345");
    expect_text(Value(1e21), "1e+21");
    expect_text(Value(0.5), "0.5");
    for (const double d : {1e15, 1.5e300, 1e-7, 123456789.123456789, std::numeric_limits<double>::max(), std::numeric_limits<double>::min(), 0.1 + 0.2, 5e-324}) {
        const Value back = must_parse(to_string(Value(d)));
        ZTH_EXPECT_TRUE(back == Value(d));
    }
    expect_text(Value(0.1 + 0.2), "0.30000000000000004");
}
ZTH_TEST("robust", "writer: control characters become \\u00xx, other bytes pass through unchanged") {
    expect_text(Value(std::string("\x01\x1f", 2)), "\"\\u0001\\u001f\"");
    expect_text(Value(std::string(1, '\0')), "\"\\u0000\"");
    expect_text(Value("\xC3\xA9\xF0\x9F\x98\x80"), "\"\xC3\xA9\xF0\x9F\x98\x80\"");
    expect_text(Value("\x7f"), "\"\x7f\"");
    expect_text(Value(Object{{"k\"ey", 1}}), "{\"k\\\"ey\":1}");
}
ZTH_TEST("robust", "writer: pretty printing with indent 0, nested empty containers, deep indentation") {
    expect_text(Value(Array{1, 2}), "[\n1,\n2\n]", 0);
    expect_text(Value(Array{Array{}, Object{}}), "[\n  [],\n  {}\n]", 2);
    expect_text(Value(Object{{"a", Object{{"b", Object{{"c", 1}}}}}}), "{\n \"a\": {\n  \"b\": {\n   \"c\": 1\n  }\n }\n}", 1);
}
ZTH_TEST_TIMEOUT("robust", "round trip: 400 random values, compact and pretty, parse(to_string(v)) == v and the text is stable", 10) {
    ::zth::Lcg rng(20240607);
    for (int i = 0; i < 400; ++i) {
        const Value v = random_value(rng, 0);
        const std::string compact = to_string(v);
        const ParseResult a = parse(compact);
        if (!a.ok() || !(a.value == v)) {
            ::zth::fail("round trip failed for " + show(compact) + (a.ok() ? "" : std::string(" (") + name(a.error) + ")"));
            return;
        }
        ZTH_EXPECT_EQ(to_string(a.value), compact);
        const ParseResult b = parse(to_string(v, 2));
        if (!b.ok() || !(b.value == v)) {
            ::zth::fail("pretty round trip failed for " + show(compact));
            return;
        }
    }
}
ZTH_TEST("robust", "a long string and a long array are fine") {
    std::string big(100000, 'x');
    ZTH_EXPECT_TRUE(must_parse("\"" + big + "\"") == Value(big));
    std::string arr = "[";
    for (int i = 0; i < 20000; ++i) arr += (i ? ",1" : "1");
    arr += "]";
    ZTH_EXPECT_EQ(must_parse(arr).as_array().size(), std::size_t{20000});
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "values are moved into their array or object, not copied (at most 50 copies for a 3000-record document)", 10) {
    const std::string text = make_document(3000);
    stats::reset();
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.value.as_array().size(), std::size_t{3000});
    ZTH_EXPECT_TRUE(stats::copies <= 50);
}
ZTH_TEST_TIMEOUT("fast", "parse allocations: at most 11 per record (3000 records; the minimum is about 10)", 10) {
    const std::string text = make_document(3000);
    zth::mem::Scope scope;
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 3000 * 11);
}
ZTH_TEST_TIMEOUT("fast", "scaling: a document twice as big costs about twice the allocations", 10) {
    const std::string small = make_document(1500), large = make_document(3000);
    std::size_t a = 0, b = 0;
    {
        zth::mem::Scope scope;
        (void)parse(small);
        a = scope.allocs();
    }
    {
        zth::mem::Scope scope;
        (void)parse(large);
        b = scope.allocs();
    }
    ZTH_EXPECT_TRUE(a > 1000);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}
ZTH_TEST_TIMEOUT("fast", "a 1 MB string (with escapes) is built without an allocation per character (at most 60)", 10) {
    std::string body;
    for (int i = 0; i < 100000; ++i) body += "abcdefg\\n\\u00e9x";
    const std::string text = "\"" + body + "\"";
    zth::mem::Scope scope;
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 60);
}
ZTH_TEST_TIMEOUT("fast", "writing builds one output string: at most 150 allocations for a 3000-record value, compact or pretty", 10) {
    const Value v = make_value(3000);
    stats::reset();
    {
        zth::mem::Scope scope;
        const std::string s = to_string(v);
        ZTH_EXPECT_TRUE(s.size() > 100000);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 150);
    }
    {
        zth::mem::Scope scope;
        const std::string s = to_string(v, 2);
        ZTH_EXPECT_TRUE(s.size() > 100000);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 150);
    }
    ZTH_EXPECT_EQ(stats::copies, 0ULL);  // looking at a value is not copying it
}
ZTH_TEST_TIMEOUT("fast", "a 20 MB document parses and writes within the time limit (no quadratic copying of the input or the output)", 10) {
    std::string text = "[";
    for (int i = 0; i < 400000; ++i) text += (i ? ", \"abcdefghijklmnopqrstuvwxyz\"" : "\"abcdefghijklmnopqrstuvwxyz\"");
    text += "]";
    ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.value.as_array().size(), std::size_t{400000});
    ZTH_EXPECT_TRUE(to_string(r.value).size() > 10000000);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "parser.hpp and writer.hpp have no raw new/delete/malloc/free") {
    ZTH_EXPECT_NO_RAW_MEMORY("parser.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("writer.hpp");
}
ZTH_TEST("clean", "errors are values: no throw, try, catch or goto in the parser and the writer") {
    for (const char* file : {"parser.hpp", "writer.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"throw", "try", "catch", "goto"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`: report errors with the Error value");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "no throwing conversions and no streams: std::stod / stoi / atof / sscanf / stringstream are out") {
    for (const char* file : {"parser.hpp", "writer.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"stod", "stof", "stoi", "stol", "stoll", "atof", "atoi", "sscanf", "stringstream", "istringstream", "ostringstream"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the parser reads through std::string_view and no input is copied up front") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("parser.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
