#define ZTH_TRACK_ALLOCATIONS
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "logger.hpp"
#include "zth_test.hpp"

using namespace slog;

namespace {

struct Capture : Sink {
    std::vector<std::string> lines;
    void write(std::string_view line) override { lines.emplace_back(line); }
};
struct Fake : Clock {
    std::int64_t now = 1714564800123;   // 2024-05-01T12:00:00.123Z
    std::int64_t now_ms() override { return now; }
};
struct Env {
    Capture sink;
    Fake clock;
    Logger make(Options o = {}) { return Logger(sink, clock, o); }
};
const std::string kT0 = "2024-05-01T12:00:00.123Z";
std::string line(const std::string& level, const std::string& msg, const std::string& rest = "", const std::string& ts = "2024-05-01T12:00:00.123Z") {
    return "{\"ts\":\"" + ts + "\",\"level\":\"" + level + "\",\"msg\":\"" + msg + "\"" + rest + "}";
}

std::string J(const Record& r) { return to_json(r); }
Record R(const std::string& msg, std::vector<Field> f = {}, std::int64_t ts = 1714564800123, Level l = Level::Info) {
    Record r;
    r.level = l;
    r.timestamp_ms = ts;
    r.message = msg;
    r.fields = std::move(f);
    return r;
}
std::string esc(const std::string& s) {
    std::string out;
    escape_json(out, s);
    return out;
}

// ---- a strict JSON parser, written independently of the library, for the round-trip tests ---------------------------------------------
struct Json {
    enum Kind { Str, Num, True, False, Null } kind = Null;
    std::string text;   // decoded string, or the number's text
};
struct Parsed {
    bool ok = false;
    std::string why;
    std::vector<std::pair<std::string, Json>> members;
};
struct JsonParser {
    explicit JsonParser(std::string_view text) : s(text) {}
    std::string_view s;
    std::size_t i = 0;
    std::string why;
    bool fail(const std::string& w) {
        if (why.empty()) why = w + " at " + std::to_string(i);
        return false;
    }
    void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i; }
    static void put_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) { out.push_back(static_cast<char>(0xC0 | (cp >> 6))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back(static_cast<char>(0xE0 | (cp >> 12))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
        else { out.push_back(static_cast<char>(0xF0 | (cp >> 18))); out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (cp & 0x3F))); }
    }
    // strictly valid UTF-8 sequence starting at i (advances), or false
    bool utf8(std::string& out) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t n = c < 0x80 ? 1 : (c >= 0xC2 && c <= 0xDF) ? 2 : (c >= 0xE0 && c <= 0xEF) ? 3 : (c >= 0xF0 && c <= 0xF4) ? 4 : 0;
        if (n == 0 || i + n > s.size()) return fail("bad UTF-8 lead");
        unsigned cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
        for (std::size_t k = 1; k < n; ++k) {
            const unsigned char d = static_cast<unsigned char>(s[i + k]);
            if ((d & 0xC0) != 0x80) return fail("bad UTF-8 continuation");
            cp = (cp << 6) | (d & 0x3F);
        }
        if ((n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return fail("overlong, surrogate or out of range");
        out.append(s.substr(i, n));
        i += n;
        return true;
    }
    bool string(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("string expected");
        ++i;
        while (i < s.size() && s[i] != '"') {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x20) return fail("raw control character in string");
            if (c == '\\') {
                if (++i >= s.size()) return fail("dangling backslash");
                const char e = s[i++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        if (i + 4 > s.size()) return fail("short \\u");
                        unsigned cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            const char h = s[i + static_cast<std::size_t>(k)];
                            const int v = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
                            if (v < 0) return fail("bad hex");
                            cp = cp * 16 + static_cast<unsigned>(v);
                        }
                        i += 4;
                        put_utf8(out, cp);
                        break;
                    }
                    default: return fail("bad escape");
                }
                continue;
            }
            if (!utf8(out)) return false;
        }
        if (i >= s.size()) return fail("unterminated string");
        ++i;
        return true;
    }
    bool number(Json& j) {
        const std::size_t start = i;
        if (i < s.size() && s[i] == '-') ++i;
        if (i >= s.size() || s[i] < '0' || s[i] > '9') return fail("digit expected");
        if (s[i] == '0') ++i;
        else while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i < s.size() && s[i] == '.') {
            ++i;
            if (i >= s.size() || s[i] < '0' || s[i] > '9') return fail("digit after '.'");
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            if (i >= s.size() || s[i] < '0' || s[i] > '9') return fail("digit in exponent");
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }
        j.kind = Json::Num;
        j.text = std::string(s.substr(start, i - start));
        return true;
    }
    bool literal(const char* w, Json::Kind k, Json& j) {
        const std::string_view word(w);
        if (s.substr(i, word.size()) != word) return fail("bad literal");
        i += word.size();
        j.kind = k;
        return true;
    }
    bool value(Json& j) {
        ws();
        if (i >= s.size()) return fail("value expected");
        if (s[i] == '"') { j.kind = Json::Str; return string(j.text); }
        if (s[i] == 't') return literal("true", Json::True, j);
        if (s[i] == 'f') return literal("false", Json::False, j);
        if (s[i] == 'n') return literal("null", Json::Null, j);
        return number(j);
    }
    Parsed object() {
        Parsed p;
        ws();
        if (i >= s.size() || s[i] != '{') { fail("object expected"); p.why = why; return p; }
        ++i;
        ws();
        if (i < s.size() && s[i] == '}') { ++i; p.ok = true; return p; }
        for (;;) {
            ws();
            std::string key;
            if (!string(key)) { p.why = why; return p; }
            ws();
            if (i >= s.size() || s[i] != ':') { fail("':' expected"); p.why = why; return p; }
            ++i;
            Json j;
            if (!value(j)) { p.why = why; return p; }
            p.members.push_back({key, j});
            ws();
            if (i < s.size() && s[i] == ',') { ++i; continue; }
            if (i < s.size() && s[i] == '}') { ++i; break; }
            fail("',' or '}' expected");
            p.why = why;
            return p;
        }
        if (i != s.size()) { fail("trailing text"); p.why = why; return p; }
        p.ok = true;
        return p;
    }
};
Parsed parse(const std::string& line) { return JsonParser{line}.object(); }
const Json* get(const Parsed& p, const std::string& key) {
    for (const auto& m : p.members)
        if (m.first == key) return &m.second;
    return nullptr;
}

// the WHATWG decoder again, written differently (a table of valid byte ranges per length), as the oracle for sanitize_utf8
std::string oracle_sanitize(const std::string& in) {
    std::string out;
    std::size_t i = 0;
    while (i < in.size()) {
        const unsigned char b0 = static_cast<unsigned char>(in[i]);
        if (b0 < 0x80) { out.push_back(static_cast<char>(b0)); ++i; continue; }
        struct Range { unsigned char lo, hi; };
        Range r[3];
        std::size_t len = 0;
        if (b0 >= 0xC2 && b0 <= 0xDF) { len = 2; r[0] = {0x80, 0xBF}; }
        else if (b0 == 0xE0) { len = 3; r[0] = {0xA0, 0xBF}; r[1] = {0x80, 0xBF}; }
        else if (b0 == 0xED) { len = 3; r[0] = {0x80, 0x9F}; r[1] = {0x80, 0xBF}; }
        else if (b0 >= 0xE1 && b0 <= 0xEF) { len = 3; r[0] = {0x80, 0xBF}; r[1] = {0x80, 0xBF}; }
        else if (b0 == 0xF0) { len = 4; r[0] = {0x90, 0xBF}; r[1] = {0x80, 0xBF}; r[2] = {0x80, 0xBF}; }
        else if (b0 == 0xF4) { len = 4; r[0] = {0x80, 0x8F}; r[1] = {0x80, 0xBF}; r[2] = {0x80, 0xBF}; }
        else if (b0 >= 0xF1 && b0 <= 0xF3) { len = 4; r[0] = {0x80, 0xBF}; r[1] = {0x80, 0xBF}; r[2] = {0x80, 0xBF}; }
        else { out += "\xEF\xBF\xBD"; ++i; continue; }
        std::size_t k = 1;
        for (; k < len; ++k) {
            if (i + k >= in.size()) break;
            const unsigned char b = static_cast<unsigned char>(in[i + k]);
            if (b < r[k - 1].lo || b > r[k - 1].hi) break;
        }
        if (k == len) { out.append(in, i, len); i += len; }
        else { out += "\xEF\xBF\xBD"; i += k; }
    }
    return out;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "level names and parsing") {
    ZTH_EXPECT_EQ(std::string(level_name(Level::Trace)), std::string("trace"));
    ZTH_EXPECT_EQ(std::string(level_name(Level::Debug)), std::string("debug"));
    ZTH_EXPECT_EQ(std::string(level_name(Level::Info)), std::string("info"));
    ZTH_EXPECT_EQ(std::string(level_name(Level::Warn)), std::string("warn"));
    ZTH_EXPECT_EQ(std::string(level_name(Level::Error)), std::string("error"));
    ZTH_EXPECT_EQ(std::string(level_name(Level::Fatal)), std::string("fatal"));
    ZTH_EXPECT_TRUE(parse_level("info") == Level::Info && parse_level("INFO") == Level::Info && parse_level("WaRn") == Level::Warn && parse_level("warning") == Level::Warn && parse_level("WARNING") == Level::Warn);
    ZTH_EXPECT_TRUE(parse_level("trace") == Level::Trace && parse_level("DEBUG") == Level::Debug && parse_level("error") == Level::Error && parse_level("Fatal") == Level::Fatal);
    for (const char* bad : {"", " info", "info ", "warnings", "err", "critical", "i", "infoo", "ınfo"}) ZTH_EXPECT_FALSE(parse_level(bad).has_value());
    ZTH_EXPECT_TRUE(Level::Trace < Level::Debug && Level::Debug < Level::Info && Level::Info < Level::Warn && Level::Warn < Level::Error && Level::Error < Level::Fatal);
}
ZTH_TEST("works", "iso8601: known instants, leap days, the year 2100, and times before 1970") {
    ZTH_EXPECT_EQ(iso8601(0), std::string("1970-01-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(1714564800123), std::string("2024-05-01T12:00:00.123Z"));
    ZTH_EXPECT_EQ(iso8601(951782400000), std::string("2000-02-29T00:00:00.000Z"));       // 2000 is a leap year
    ZTH_EXPECT_EQ(iso8601(951782400000 + 86400000), std::string("2000-03-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(4107542400000), std::string("2100-03-01T00:00:00.000Z"));     // 2100 is not
    ZTH_EXPECT_EQ(iso8601(1709164800000), std::string("2024-02-29T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(86399999), std::string("1970-01-01T23:59:59.999Z"));
    ZTH_EXPECT_EQ(iso8601(86400000), std::string("1970-01-02T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(1000000000000), std::string("2001-09-09T01:46:40.000Z"));
    ZTH_EXPECT_EQ(iso8601(1700000000999), std::string("2023-11-14T22:13:20.999Z"));
    ZTH_EXPECT_EQ(iso8601(-1), std::string("1969-12-31T23:59:59.999Z"));
    ZTH_EXPECT_EQ(iso8601(-1000), std::string("1969-12-31T23:59:59.000Z"));
    ZTH_EXPECT_EQ(iso8601(-86400000), std::string("1969-12-31T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(-86400001), std::string("1969-12-30T23:59:59.999Z"));
    ZTH_EXPECT_EQ(iso8601(-2208988800000), std::string("1900-01-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(-2203891200000), std::string("1900-03-01T00:00:00.000Z"));    // 1900 is not a leap year
}
ZTH_TEST("works", "iso8601: the first and last representable years, and everything beyond is empty") {
    ZTH_EXPECT_EQ(iso8601(253402300799999), std::string("9999-12-31T23:59:59.999Z"));
    ZTH_EXPECT_EQ(iso8601(253402300800000), std::string(""));
    ZTH_EXPECT_EQ(iso8601(-62167219200000), std::string("0000-01-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(-62167219200001), std::string(""));
    ZTH_EXPECT_EQ(iso8601(std::numeric_limits<std::int64_t>::max()), std::string(""));
    ZTH_EXPECT_EQ(iso8601(std::numeric_limits<std::int64_t>::min()), std::string(""));
    ZTH_EXPECT_EQ(iso8601(-62135596800000), std::string("0001-01-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(iso8601(-62162035200000), std::string("0000-03-01T00:00:00.000Z"));   // year 0 is a leap year: Feb 29 0000 exists
    ZTH_EXPECT_EQ(iso8601(-62162121600000), std::string("0000-02-29T00:00:00.000Z"));
}
ZTH_TEST("works", "escape_json: the quote, the backslash, the named controls and the \\u00xx ones; everything else is untouched") {
    ZTH_EXPECT_EQ(esc("plain text 123"), std::string("plain text 123"));
    ZTH_EXPECT_EQ(esc("say \"hi\""), std::string("say \\\"hi\\\""));
    ZTH_EXPECT_EQ(esc("C:\\dir\\file"), std::string("C:\\\\dir\\\\file"));
    ZTH_EXPECT_EQ(esc("a\nb\rc\td\be\ff"), std::string("a\\nb\\rc\\td\\be\\ff"));
    ZTH_EXPECT_EQ(esc(std::string("a\0b", 3)), std::string("a\\u0000b"));
    ZTH_EXPECT_EQ(esc("\x01\x02\x1F"), std::string("\\u0001\\u0002\\u001f"));
    ZTH_EXPECT_EQ(esc("\x0B\x0E"), std::string("\\u000b\\u000e"));
    ZTH_EXPECT_EQ(esc("/slash/ and DEL\x7F"), std::string("/slash/ and DEL\x7F"));
    ZTH_EXPECT_EQ(esc("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80"), std::string("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80"));
    ZTH_EXPECT_EQ(esc("\xE2\x80\xA8\xE2\x80\xA9"), std::string("\xE2\x80\xA8\xE2\x80\xA9"));   // U+2028 and U+2029 are legal in JSON
    ZTH_EXPECT_EQ(esc(""), std::string(""));
    std::string appended = "prefix:";
    escape_json(appended, "a\"b");
    ZTH_EXPECT_EQ(appended, std::string("prefix:a\\\"b"));   // it appends, it does not replace
}
ZTH_TEST("works", "invalid UTF-8 is replaced by U+FFFD, following the WHATWG rule") {
    const std::string F = "\xEF\xBF\xBD";
    ZTH_EXPECT_EQ(sanitize_utf8("ok"), std::string("ok"));
    ZTH_EXPECT_EQ(sanitize_utf8("\xFF"), F);
    ZTH_EXPECT_EQ(sanitize_utf8("a\xFF" "b"), "a" + F + "b");
    ZTH_EXPECT_EQ(sanitize_utf8("\xC3"), F);                       // a cut-off sequence at the end
    ZTH_EXPECT_EQ(sanitize_utf8("\xE2\x82"), F);                   // cut off after two bytes: still ONE replacement
    ZTH_EXPECT_EQ(sanitize_utf8("\xE2\x82" "A"), F + "A");         // the byte that broke it is kept
    ZTH_EXPECT_EQ(sanitize_utf8("\xF0\x9F\x98"), F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xF0\x9F" "AB"), F + "AB");
    ZTH_EXPECT_EQ(sanitize_utf8("\x80"), F);                       // a stray continuation byte
    ZTH_EXPECT_EQ(sanitize_utf8("\x80\x80"), F + F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xC0\x80"), F + F);               // overlong NUL: C0 is never valid
    ZTH_EXPECT_EQ(sanitize_utf8("\xC1\xBF"), F + F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xE0\x80\x80"), F + F + F);       // overlong: after E0 the next byte must be A0..BF
    ZTH_EXPECT_EQ(sanitize_utf8("\xE0\xA0\x80"), std::string("\xE0\xA0\x80"));
    ZTH_EXPECT_EQ(sanitize_utf8("\xED\xA0\x80"), F + F + F);       // a UTF-16 surrogate
    ZTH_EXPECT_EQ(sanitize_utf8("\xED\x9F\xBF"), std::string("\xED\x9F\xBF"));
    ZTH_EXPECT_EQ(sanitize_utf8("\xF0\x80\x80\x80"), F + F + F + F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xF0\x90\x80\x80"), std::string("\xF0\x90\x80\x80"));   // U+10000, the first valid 4-byte
    ZTH_EXPECT_EQ(sanitize_utf8("\xF4\x8F\xBF\xBF"), std::string("\xF4\x8F\xBF\xBF"));   // U+10FFFF
    ZTH_EXPECT_EQ(sanitize_utf8("\xF4\x90\x80\x80"), F + F + F + F);                       // above U+10FFFF
    ZTH_EXPECT_EQ(sanitize_utf8("\xF5\x80\x80\x80"), F + F + F + F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xF8\xFE\xFF"), F + F + F);
    ZTH_EXPECT_EQ(sanitize_utf8("\xC2\x80"), std::string("\xC2\x80"));
    ZTH_EXPECT_EQ(sanitize_utf8("\xDF\xBF"), std::string("\xDF\xBF"));
    ZTH_EXPECT_EQ(esc("x\xFF\xFE y"), "x" + F + F + " y");
}
ZTH_TEST("works", "to_json: the basic layout, level names, no spaces, no newline") {
    ZTH_EXPECT_EQ(J(R("hello")), line("info", "hello"));
    for (Level l : {Level::Trace, Level::Debug, Level::Info, Level::Warn, Level::Error, Level::Fatal}) ZTH_EXPECT_EQ(J(R("m", {}, 1714564800123, l)), line(std::string(level_name(l)), "m"));
    ZTH_EXPECT_EQ(J(R("")), line("info", ""));
    ZTH_EXPECT_EQ(J(R("tab\there", {}, 0)), line("info", "tab\\there", "", "1970-01-01T00:00:00.000Z"));
    ZTH_EXPECT_EQ(J(R("late", {}, 253402300800000)), std::string("{\"ts\":null,\"level\":\"info\",\"msg\":\"late\"}"));
    ZTH_EXPECT_EQ(J(R("early", {}, -62167219200001)), std::string("{\"ts\":null,\"level\":\"info\",\"msg\":\"early\"}"));
}
ZTH_TEST("works", "to_json: field values of every type, in order") {
    const Record r = R("m", {kv("name", "ann"), kv("n", 42), kv("big", std::int64_t{9223372036854775807LL}), kv("small", std::int64_t{-9223372036854775807LL - 1}), kv("ok", true), kv("bad", false), kv_null("nothing"), kv("ratio", 0.25)});
    ZTH_EXPECT_EQ(J(r), line("info", "m", ",\"name\":\"ann\",\"n\":42,\"big\":9223372036854775807,\"small\":-9223372036854775808,\"ok\":true,\"bad\":false,\"nothing\":null,\"ratio\":0.25"));
    ZTH_EXPECT_EQ(J(R("m", {kv("z", 1), kv("a", 2), kv("m", 3)})), line("info", "m", ",\"z\":1,\"a\":2,\"m\":3"));   // given order, not sorted
    ZTH_EXPECT_EQ(J(R("m", {kv("s", "line\nbreak \"q\" \\ \xC3\xA9")})), line("info", "m", ",\"s\":\"line\\nbreak \\\"q\\\" \\\\ \xC3\xA9\""));
    ZTH_EXPECT_EQ(J(R("m", {kv("", 1)})), line("info", "m", ",\"\":1"));   // an empty key is a key
    ZTH_EXPECT_EQ(J(R("m", {kv("k\"ey\n", 1)})), line("info", "m", ",\"k\\\"ey\\n\":1"));   // keys are escaped too
}
ZTH_TEST("works", "doubles: the shortest text that reads back exactly, a decimal point for whole numbers, null for NaN and infinity") {
    const auto d = [](double v) { return J(R("m", {kv("d", v)})); };
    const auto field = [](const std::string& v) { return line("info", "m", ",\"d\":" + v); };
    ZTH_EXPECT_EQ(d(0.1), field("0.1"));
    ZTH_EXPECT_EQ(d(0.5), field("0.5"));
    ZTH_EXPECT_EQ(d(-2.75), field("-2.75"));
    ZTH_EXPECT_EQ(d(1.0), field("1.0"));
    ZTH_EXPECT_EQ(d(100.0), field("100.0"));
    ZTH_EXPECT_EQ(d(-3.0), field("-3.0"));
    ZTH_EXPECT_EQ(d(0.0), field("0.0"));
    ZTH_EXPECT_EQ(d(-0.0), field("-0.0"));
    ZTH_EXPECT_EQ(d(1e21), field("1e+21"));
    ZTH_EXPECT_EQ(d(1e-9), field("1e-09"));
    ZTH_EXPECT_EQ(d(1.5e300), field("1.5e+300"));
    ZTH_EXPECT_EQ(d(0.30000000000000004), field("0.30000000000000004"));   // 0.1 + 0.2, exactly as the double needs
    ZTH_EXPECT_EQ(d(123456789.125), field("123456789.125"));
    ZTH_EXPECT_EQ(d(std::numeric_limits<double>::quiet_NaN()), field("null"));
    ZTH_EXPECT_EQ(d(std::numeric_limits<double>::infinity()), field("null"));
    ZTH_EXPECT_EQ(d(-std::numeric_limits<double>::infinity()), field("null"));
    ZTH_EXPECT_EQ(d(1.7976931348623157e308), field("1.7976931348623157e+308"));
    ZTH_EXPECT_EQ(d(5e-324), field("5e-324"));
}
ZTH_TEST("works", "reserved keys are renamed; duplicate keys collapse to the first position with the last value") {
    ZTH_EXPECT_EQ(J(R("m", {kv("ts", "mine"), kv("level", "x"), kv("msg", "y")})), line("info", "m", ",\"_ts\":\"mine\",\"_level\":\"x\",\"_msg\":\"y\""));
    ZTH_EXPECT_EQ(J(R("m", {kv("a", 1), kv("b", 2), kv("a", 3)})), line("info", "m", ",\"a\":3,\"b\":2"));
    ZTH_EXPECT_EQ(J(R("m", {kv("a", 1), kv("a", 2), kv("a", 3)})), line("info", "m", ",\"a\":3"));
    ZTH_EXPECT_EQ(J(R("m", {kv("a", 1), kv("_ts", 2), kv("ts", 3)})), line("info", "m", ",\"a\":1,\"_ts\":3"));   // "ts" becomes "_ts", which already exists
    ZTH_EXPECT_EQ(J(R("m", {kv("msg", 1), kv("msg", 2)})), line("info", "m", ",\"_msg\":2"));
    ZTH_EXPECT_EQ(J(R("m", {kv("Ts", 1), kv("TS", 2)})), line("info", "m", ",\"Ts\":1,\"TS\":2"));   // case matters
    ZTH_EXPECT_EQ(J(R("m", {kv("a", "x"), kv("a", true), kv_null("b"), kv("b", 1.5)})), line("info", "m", ",\"a\":true,\"b\":1.5"));
}
ZTH_TEST("works", "the logger writes JSON lines with the clock's time and drops records below the minimum level") {
    Env e;
    Options o;
    o.min_level = Level::Info;
    Logger log = e.make(o);
    log.trace("t");
    log.debug("d");
    ZTH_EXPECT_TRUE(e.sink.lines.empty());
    log.info("i", {kv("a", 1)});
    log.warn("w");
    log.error("e");
    log.fatal("f");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{4});
    ZTH_EXPECT_EQ(e.sink.lines[0], line("info", "i", ",\"a\":1"));
    ZTH_EXPECT_EQ(e.sink.lines[1], line("warn", "w"));
    ZTH_EXPECT_EQ(e.sink.lines[2], line("error", "e"));
    ZTH_EXPECT_EQ(e.sink.lines[3], line("fatal", "f"));
    e.clock.now += 1000;
    log.log(Level::Info, "later");
    ZTH_EXPECT_EQ(e.sink.lines[4], line("info", "later", "", "2024-05-01T12:00:01.123Z"));
    Env all;
    Options everything;
    everything.min_level = Level::Trace;
    Logger loud = all.make(everything);
    loud.trace("t");
    ZTH_EXPECT_EQ(all.sink.lines.size(), std::size_t{1});
    Env quiet;
    Options only_fatal;
    only_fatal.min_level = Level::Fatal;
    Logger q = quiet.make(only_fatal);
    q.error("e");
    q.fatal("f");
    ZTH_EXPECT_EQ(quiet.sink.lines.size(), std::size_t{1});
}
ZTH_TEST("works", "records below the minimum level do not read the clock or disturb suppression") {
    struct Counting : Clock {
        int reads = 0;
        std::int64_t now_ms() override { ++reads; return 5; }
    } clock;
    Capture sink;
    Options o;
    o.min_level = Level::Warn;
    Logger log(sink, clock, o);
    log.info("quiet");
    log.debug("quiet");
    ZTH_EXPECT_EQ(clock.reads, 0);
    log.warn("loud");
    ZTH_EXPECT_EQ(clock.reads, 1);   // once per record that is written
}
ZTH_TEST("works", "context fields: with() adds fields to every record, outermost first, and children can nest") {
    Env e;
    Logger root = e.make();
    Logger req = root.with({kv("request", "r1")});
    Logger user = req.with({kv("user", "ann"), kv("n", 2)});
    root.info("a");
    req.info("b", {kv("x", 1)});
    user.info("c", {kv("x", 2)});
    ZTH_EXPECT_EQ(e.sink.lines[0], line("info", "a"));
    ZTH_EXPECT_EQ(e.sink.lines[1], line("info", "b", ",\"request\":\"r1\",\"x\":1"));
    ZTH_EXPECT_EQ(e.sink.lines[2], line("info", "c", ",\"request\":\"r1\",\"user\":\"ann\",\"n\":2,\"x\":2"));
    user.info("d", {kv("request", "override")});   // a duplicate key: the call's value wins, at the context field's position
    ZTH_EXPECT_EQ(e.sink.lines[3], line("info", "d", ",\"request\":\"override\",\"user\":\"ann\",\"n\":2"));
    root.info("e");   // the root is unchanged by its children
    ZTH_EXPECT_EQ(e.sink.lines[4], line("info", "e"));
}
ZTH_TEST("works", "repeated records inside the window are suppressed and reported with a count") {
    Env e;
    Options o;
    o.dedupe_window_ms = 1000;
    Logger log = e.make(o);
    log.error("down", {kv("host", "a")});
    for (int i = 0; i < 4; ++i) {
        e.clock.now += 100;
        log.error("down", {kv("host", "a")});
    }
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{1});   // only the first was written
    e.clock.now += 100;   // 500 ms after the first line
    log.error("other");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{3});
    ZTH_EXPECT_EQ(e.sink.lines[1], line("error", "down", ",\"host\":\"a\",\"repeated\":4", "2024-05-01T12:00:00.623Z"));   // the summary, at the time of the record that ended the run
    ZTH_EXPECT_EQ(e.sink.lines[2], line("error", "other", "", "2024-05-01T12:00:00.623Z"));
}
ZTH_TEST("works", "the suppression window: strictly less than the window suppresses; the same message after the window is written again, after its summary") {
    Env e;
    Options o;
    o.dedupe_window_ms = 1000;
    Logger log = e.make(o);
    log.warn("x");
    e.clock.now += 999;
    log.warn("x");   // 999 < 1000: suppressed
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{1});
    e.clock.now += 1;   // exactly 1000 after the first line: not suppressed
    log.warn("x");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{3});
    ZTH_EXPECT_EQ(e.sink.lines[1], line("warn", "x", ",\"repeated\":1", "2024-05-01T12:00:01.123Z"));
    ZTH_EXPECT_EQ(e.sink.lines[2], line("warn", "x", "", "2024-05-01T12:00:01.123Z"));
    e.clock.now += 500;   // the window now runs from the line just written
    log.warn("x");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{3});
}
ZTH_TEST("works", "what makes two records 'the same': level, message and all fields (context included)") {
    Env e;
    Options o;
    o.dedupe_window_ms = 1000;
    Logger log = e.make(o);
    log.info("m", {kv("a", 1)});
    log.info("m", {kv("a", 2)});     // different field value
    log.info("m", {kv("b", 1)});     // different field
    log.warn("m", {kv("b", 1)});     // different level
    log.warn("n", {kv("b", 1)});     // different message
    Logger child = log.with({kv("c", 1)});
    child.warn("n", {kv("b", 1)});   // different context
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{6});
    ZTH_EXPECT_EQ(e.sink.lines[3], line("warn", "m", ",\"b\":1"));   // the level is part of the key: this one was written, not counted
    ZTH_EXPECT_EQ(e.sink.lines[4], line("warn", "n", ",\"b\":1"));
    log.info("z", {kv("a", 1.0)});
    log.info("z", {kv("a", 1)});     // 1.0 and 1 are different values
    log.info("z", {kv("a", "1")});
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{9});
    log.info("z", {kv("a", "1")});   // now an exact repeat
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{9});
}
ZTH_TEST("works", "children and parents share the suppression state; flush() writes a pending summary and only that") {
    Env e;
    Options o;
    o.dedupe_window_ms = 5000;
    Logger parent = e.make(o);
    Logger child = parent.with({kv("c", 1)});
    child.error("boom");
    child.error("boom");     // suppressed
    child.error("boom");     // suppressed
    parent.flush();          // a child's pending summary is written by the parent
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{2});
    ZTH_EXPECT_EQ(e.sink.lines[1], line("error", "boom", ",\"c\":1,\"repeated\":2"));
    parent.flush();
    child.flush();
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{2});   // nothing pending: nothing written
    child.error("boom");     // inside the window of the first line: counted again
    child.error("boom");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{2});
    parent.error("boom");    // the parent's record has no context fields: a different key, so the pending summary is written first
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{4});
    ZTH_EXPECT_EQ(e.sink.lines[2], line("error", "boom", ",\"c\":1,\"repeated\":2"));
    ZTH_EXPECT_EQ(e.sink.lines[3], line("error", "boom"));
}
ZTH_TEST("works", "without a dedupe window every record is written; records below the level are not counted as repeats") {
    Env e;
    Logger log = e.make();
    for (int i = 0; i < 5; ++i) log.info("same");
    ZTH_EXPECT_EQ(e.sink.lines.size(), std::size_t{5});
    Env d;
    Options o;
    o.dedupe_window_ms = 1000;
    Logger logd = d.make(o);
    logd.info("a");
    logd.debug("b");   // below the level: invisible to the suppression logic
    logd.info("a");
    logd.flush();
    ZTH_EXPECT_EQ(d.sink.lines.size(), std::size_t{2});
    ZTH_EXPECT_EQ(d.sink.lines[1], line("info", "a", ",\"repeated\":1"));
}
ZTH_TEST("works", "line length limit: the message is cut at a code point with an ellipsis; then fields are dropped from the end") {
    Env e;
    Options o;
    o.max_line_bytes = 80;
    Logger log = e.make(o);
    log.info("short", {kv("a", 1)});
    ZTH_EXPECT_EQ(e.sink.lines[0], line("info", "short", ",\"a\":1"));   // fits: unchanged
    const std::string long_msg(200, 'x');
    log.info(long_msg);
    ZTH_EXPECT_EQ(e.sink.lines[1].size(), std::size_t{80});
    const std::string overhead = line("info", "\xE2\x80\xA6");   // the line with just the ellipsis as message
    const std::size_t room = 80 - overhead.size();
    ZTH_EXPECT_EQ(e.sink.lines[1], line("info", std::string(room, 'x') + "\xE2\x80\xA6"));
    log.info(long_msg, {kv("keep", 1), kv("drop", "this field does not fit anywhere near")});
    // 'keep' fits with a shortened message; the message gets what is left
    const auto parsed = parse(e.sink.lines[2]);
    ZTH_EXPECT_TRUE(parsed.ok);
    ZTH_EXPECT_TRUE(e.sink.lines[2].size() <= 80);
}
ZTH_TEST("works", "line length limit: a line of exactly the limit is not shortened, one byte more is") {
    const std::string exact = line("info", "hello", ",\"k\":1");
    Env e;
    Options o;
    o.max_line_bytes = exact.size();
    Logger log = e.make(o);
    log.info("hello", {kv("k", 1)});
    ZTH_EXPECT_EQ(e.sink.lines[0], exact);
    Env f;
    Options tight;
    tight.max_line_bytes = exact.size() - 1;
    Logger shorter = f.make(tight);
    shorter.info("hello", {kv("k", 1)});
    ZTH_EXPECT_TRUE(f.sink.lines[0].size() <= exact.size() - 1);
    ZTH_EXPECT_TRUE(f.sink.lines[0] != exact && f.sink.lines[0].find("\xE2\x80\xA6") != std::string::npos);
}
ZTH_TEST("works", "line length limit: whole code points only, never a split character, and exactly the longest prefix that fits") {
    Env e;
    Options o;
    o.max_line_bytes = 70;
    Logger log = e.make(o);
    std::string msg;
    for (int i = 0; i < 100; ++i) msg += "\xC3\xA9";   // 100 two-byte characters
    log.info(msg);
    const Parsed p = parse(e.sink.lines[0]);
    ZTH_EXPECT_TRUE(p.ok);
    const std::string cut = get(p, "msg")->text;
    ZTH_EXPECT_TRUE(cut.size() >= 3 && cut.substr(cut.size() - 3) == "\xE2\x80\xA6");
    ZTH_EXPECT_TRUE(e.sink.lines[0].size() <= 70 && e.sink.lines[0].size() >= 70 - 1);   // 2-byte characters: at most one byte is lost
    ZTH_EXPECT_EQ(oracle_sanitize(cut), cut);
    // a message of 3-byte characters: the slack can be up to 2 bytes
    std::string euro;
    for (int i = 0; i < 100; ++i) euro += "\xE2\x82\xAC";
    log.info(euro);
    const Parsed q = parse(e.sink.lines[1]);
    ZTH_EXPECT_TRUE(q.ok && e.sink.lines[1].size() <= 70 && e.sink.lines[1].size() >= 68);
    // one more character would not have fitted
    const std::string cut_euro = get(q, "msg")->text;
    const std::size_t chars = (cut_euro.size() - 3) / 3;
    const std::string with_one_more = "{\"ts\":\"2024-05-01T12:00:00.123Z\",\"level\":\"info\",\"msg\":\"" + euro.substr(0, (chars + 1) * 3) + "\xE2\x80\xA6\"}";
    ZTH_EXPECT_TRUE(with_one_more.size() > 70);
}
ZTH_TEST("works", "line length limit: fields are dropped from the end, one at a time, until the minimal line fits; if nothing fits the minimal line is written anyway") {
    Env e;
    Options o;
    o.max_line_bytes = 75;
    Logger log = e.make(o);
    log.info("hello", {kv("first", 1), kv("second", "a value that is long enough to push the line over the limit")});
    ZTH_EXPECT_EQ(e.sink.lines[0], line("info", "hello", ",\"first\":1"));   // 'second' had to go, then everything fits without touching the message
    log.info("hello", {kv("first", "x"), kv("second", std::string(100, 'y'))});
    ZTH_EXPECT_TRUE(e.sink.lines[1].size() <= 75);
    Options tiny;
    tiny.max_line_bytes = 10;
    Env t;
    Logger tl = t.make(tiny);
    tl.info("message", {kv("a", 1)});
    ZTH_EXPECT_EQ(t.sink.lines[0], line("info", "\xE2\x80\xA6"));   // the shortest possible line, even though it exceeds the limit
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every line the formatter produces for random messages and fields is strict, valid JSON that decodes to the (sanitised) original") {
    zth::Lcg rng(2024);
    const std::string alphabet = "ab \"\\/\n\t\r\x01\x1F\x7F{}[]:,\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xFF\xC0\xED\xA0\x80\x80\xE2\x82";
    auto random_text = [&](int max_len) {
        std::string s;
        for (int i = 0, n = rng.below(max_len); i < n; ++i) s.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
        return s;
    };
    for (int round = 0; round < 3000; ++round) {
        Record r;
        r.level = static_cast<Level>(rng.below(6));
        r.timestamp_ms = static_cast<std::int64_t>(rng.next()) * (rng.below(2) ? 1000 : -1000) + rng.below(1000);
        r.message = random_text(30);
        std::map<std::string, Value> last;   // the expected final value per (renamed) key
        std::vector<std::string> order;
        for (int k = 0, n = rng.below(6); k < n; ++k) {
            Field f;
            f.key = rng.below(5) == 0 ? (rng.below(2) ? "ts" : "msg") : random_text(5);
            switch (rng.below(5)) {
                case 0: f = kv(f.key, random_text(20)); break;
                case 1: f = kv(f.key, static_cast<std::int64_t>(static_cast<int>(rng.next()) - 1000000000)); break;
                case 2: f = kv(f.key, static_cast<double>(static_cast<int>(rng.next() % 2000001) - 1000000) / 64.0); break;
                case 3: f = kv(f.key, rng.below(2) == 0); break;
                default: f = kv_null(f.key); break;
            }
            r.fields.push_back(f);
        }
        const std::string text = to_json(r);
        const Parsed p = parse(text);
        if (!p.ok) {
            zth::fail("not valid JSON (" + p.why + "): " + text);
            return;
        }
        ZTH_EXPECT_TRUE(text.find('\n') == std::string::npos && text.find('\r') == std::string::npos);
        ZTH_EXPECT_TRUE(p.members.size() >= 3 && p.members[0].first == "ts" && p.members[1].first == "level" && p.members[2].first == "msg");
        ZTH_EXPECT_EQ(p.members[2].second.text, oracle_sanitize(r.message));
        ZTH_EXPECT_EQ(p.members[1].second.text, std::string(level_name(r.level)));
        // the model: rename, then keep the first position and the last value
        std::vector<std::pair<std::string, const Value*>> model;
        for (const Field& f : r.fields) {
            const std::string key = (f.key == "ts" || f.key == "level" || f.key == "msg") ? "_" + f.key : f.key;
            bool found = false;
            for (auto& m : model)
                if (m.first == key) { m.second = &f.value; found = true; }
            if (!found) model.push_back({key, &f.value});
        }
        ZTH_EXPECT_EQ(p.members.size(), 3 + model.size());
        for (std::size_t i = 0; i < model.size() && 3 + i < p.members.size(); ++i) {
            const auto& m = p.members[3 + i];
            ZTH_EXPECT_EQ(m.first, oracle_sanitize(model[i].first));
            const Value& v = *model[i].second;
            switch (v.type) {
                case Value::Type::String: ZTH_EXPECT_TRUE(m.second.kind == Json::Str && m.second.text == oracle_sanitize(v.s)); break;
                case Value::Type::Int: ZTH_EXPECT_TRUE(m.second.kind == Json::Num && m.second.text == std::to_string(v.i)); break;
                case Value::Type::Double: ZTH_EXPECT_TRUE(m.second.kind == Json::Num && std::strtod(m.second.text.c_str(), nullptr) == v.d && m.second.text.find_first_of(".eE") != std::string::npos); break;
                case Value::Type::Bool: ZTH_EXPECT_TRUE(m.second.kind == (v.b ? Json::True : Json::False)); break;
                case Value::Type::Null: ZTH_EXPECT_TRUE(m.second.kind == Json::Null); break;
            }
        }
    }
}
ZTH_TEST("robust", "sanitize_utf8 agrees with an independently written decoder on 100000 random byte strings, and its output is always valid") {
    zth::Lcg rng(9);
    const unsigned char interesting[] = {0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xFF};
    for (int round = 0; round < 100000; ++round) {
        std::string s;
        for (int i = 0, n = rng.below(8); i < n; ++i) s.push_back(rng.below(4) == 0 ? static_cast<char>(rng.below(256)) : static_cast<char>(interesting[rng.below(25)]));
        const std::string got = sanitize_utf8(s), want = oracle_sanitize(s);
        if (got != want) {
            std::string hex;
            for (unsigned char c : s) hex += " " + std::to_string(c);
            zth::fail("sanitize_utf8 differs from the oracle for bytes:" + hex);
            return;
        }
        std::string sink;
        JsonParser jp{got};
        std::size_t pos = 0;
        while (pos < got.size()) {
            jp.i = pos;
            if (!jp.utf8(sink)) {
                zth::fail("output is not valid UTF-8");
                return;
            }
            pos = jp.i;
        }
    }
    // a valid string is never changed, whatever its characters
    const std::string valid = "ascii \xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80 \xEF\xBF\xBD \xF4\x8F\xBF\xBF \xED\x9F\xBF \xEE\x80\x80";
    ZTH_EXPECT_EQ(sanitize_utf8(valid), valid);
}
ZTH_TEST("robust", "iso8601 agrees with <chrono> for 200000 random instants between the years 0 and 9999, and its edges are exact") {
    using namespace std::chrono;
    zth::Lcg rng(31);
    for (int round = 0; round < 200000; ++round) {
        const std::int64_t span = 253402300800000LL + 62167219200000LL;   // years 0000..9999 in ms
        std::int64_t ms = -62167219200000LL + static_cast<std::int64_t>((static_cast<unsigned long long>(rng.next()) << 31 ^ rng.next()) % static_cast<unsigned long long>(span));
        if (round % 4 == 0) ms = (static_cast<std::int64_t>(rng.below(400000)) - 200000) * 86400000LL + rng.below(5) - 2;   // around midnights
        const sys_time<milliseconds> tp{milliseconds{ms}};
        const auto day = floor<days>(tp);
        const year_month_day ymd{day};
        const auto rest = tp - day;
        const hh_mm_ss<milliseconds> hms{rest};
        char buf[40];
        std::snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02d:%02d:%02d.%03dZ", static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()), static_cast<int>(hms.hours().count()),
                      static_cast<int>(hms.minutes().count()), static_cast<int>(hms.seconds().count()), static_cast<int>(hms.subseconds().count()));
        const std::string want = (static_cast<int>(ymd.year()) < 0 || static_cast<int>(ymd.year()) > 9999) ? "" : buf;
        if (iso8601(ms) != want) {
            zth::fail("iso8601(" + std::to_string(ms) + ") = \"" + iso8601(ms) + "\", expected \"" + want + "\"");
            return;
        }
    }
}
ZTH_TEST("robust", "doubles: 20000 random values round-trip exactly through the text the logger writes") {
    zth::Lcg rng(5);
    for (int i = 0; i < 20000; ++i) {
        double v;
        const unsigned kind = rng.next() % 4;
        if (kind == 0) v = static_cast<double>(static_cast<int>(rng.next())) / 1024.0;
        else if (kind == 1) v = std::ldexp(static_cast<double>(rng.next()) / 2147483648.0 + 0.5, rng.below(2000) - 1000);
        else if (kind == 2) v = static_cast<double>(rng.below(1000)) * (rng.below(2) ? 0.1 : 1e-7);
        else v = std::ldexp(1.0, rng.below(2090) - 1070);
        if (rng.below(2)) v = -v;
        if (!std::isfinite(v)) continue;
        const Parsed p = parse(to_json(R("m", {kv("d", v)})));
        ZTH_EXPECT_TRUE(p.ok && get(p, "d") != nullptr);
        if (!p.ok || !get(p, "d")) return;
        ZTH_EXPECT_TRUE(std::strtod(get(p, "d")->text.c_str(), nullptr) == v);
        ZTH_EXPECT_TRUE(get(p, "d")->text.find_first_of(".eE") != std::string::npos);
    }
}
ZTH_TEST("robust", "every line the logger writes is valid JSON within the limit, over random messages, limits and suppression windows") {
    zth::Lcg rng(77);
    const std::string alphabet = "ab \"\n\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xFF";
    for (int round = 0; round < 1500; ++round) {
        Env e;
        Options o;
        o.min_level = static_cast<Level>(rng.below(3));
        o.dedupe_window_ms = rng.below(2) ? 0 : 1 + rng.below(500);
        o.max_line_bytes = rng.below(3) == 0 ? 0 : static_cast<std::size_t>(60 + rng.below(100));
        Logger log = e.make(o);
        Logger child = log.with({kv("c", rng.below(3))});
        for (int k = 0, n = 1 + rng.below(25); k < n; ++k) {
            e.clock.now += rng.below(300);
            std::string msg;
            for (int i = 0, m = rng.below(4) == 0 ? rng.below(150) : rng.below(3); i < m; ++i) msg.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
            std::vector<Field> f;
            if (rng.below(2)) f.push_back(kv("a", rng.below(3)));
            if (rng.below(3) == 0) f.push_back(kv("s", std::string(static_cast<std::size_t>(rng.below(80)), 'z')));
            (rng.below(2) ? log : child).log(static_cast<Level>(rng.below(6)), msg, f);
        }
        log.flush();
        for (const std::string& l : e.sink.lines) {
            const Parsed p = parse(l);
            if (!p.ok) {
                zth::fail("invalid JSON from the logger (" + p.why + "): " + l);
                return;
            }
            const std::size_t minimal = std::string("{\"ts\":\"2024-05-01T12:00:00.123Z\",\"level\":\"error\",\"msg\":\"\xE2\x80\xA6\"}").size();
            if (o.max_line_bytes >= minimal + 10) ZTH_EXPECT_TRUE(l.size() <= o.max_line_bytes);
        }
    }
}
ZTH_TEST("robust", "a random run of identical messages: the counted repeats plus the written lines account for every record") {
    zth::Lcg rng(3);
    for (int round = 0; round < 300; ++round) {
        Env e;
        Options o;
        o.dedupe_window_ms = 1 + rng.below(1000);
        Logger log = e.make(o);
        int records = 0;
        for (int k = 0, n = 1 + rng.below(60); k < n; ++k) {
            e.clock.now += rng.below(400);
            log.warn(rng.below(4) == 0 ? "other" : "same");
            ++records;
        }
        log.flush();
        int accounted = 0;
        for (const std::string& l : e.sink.lines) {
            const Parsed p = parse(l);
            ZTH_EXPECT_TRUE(p.ok);
            const Json* rep = get(p, "repeated");
            accounted += rep ? std::atoi(rep->text.c_str()) : 1;
        }
        ZTH_EXPECT_EQ(accounted, records);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "200000 lines formatted, and a 4 MB message escaped", 20) {
    std::size_t total = 0;
    for (int i = 0; i < 200000; ++i) total += to_json(R("request handled", {kv("id", i), kv("path", "/api/v1/items"), kv("ms", i * 0.25), kv("ok", i % 3 == 0)}, 1714564800000LL + i)).size();
    ZTH_EXPECT_TRUE(total > 200000 * 100);
    std::string big;
    for (int i = 0; i < 4 * 1024 * 1024 / 8; ++i) big += "ab\"\n\xC3\xA9\xFF!";
    std::string out;
    escape_json(out, big);
    ZTH_EXPECT_TRUE(out.size() > big.size());
    ZTH_EXPECT_TRUE(parse("{\"a\":\"" + out + "\"}").ok);
}
ZTH_TEST("fast", "a log call makes a bounded number of allocations; a line of 10000 fields does not take quadratic time") {
    Capture sink;
    Fake clock;
    Logger log(sink, clock);
    log.info("warm up", {kv("a", 1)});
    sink.lines.reserve(100);
    zth::mem::Scope scope;
    log.info("one call", {kv("user", "ann"), kv("n", 3), kv("ok", true)});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);
    Record big;
    big.message = "wide";
    for (int i = 0; i < 10000; ++i) big.fields.push_back(kv("key" + std::to_string(i % 5000), i));   // half of them are duplicates
    const std::string text = to_json(big);
    const Parsed p = parse(text);
    ZTH_EXPECT_TRUE(p.ok && p.members.size() == 3 + 5000);
}
ZTH_TEST_TIMEOUT("fast", "a million identical records inside the window cost almost nothing", 20) {
    Capture sink;
    Fake clock;
    Options o;
    o.dedupe_window_ms = 1000000;
    Logger log(sink, clock, o);
    for (int i = 0; i < 300000; ++i) log.error("same thing again", {kv("host", "a"), kv("code", 7)});
    ZTH_EXPECT_EQ(sink.lines.size(), std::size_t{1});
    log.flush();
    ZTH_EXPECT_EQ(sink.lines.size(), std::size_t{2});
    ZTH_EXPECT_TRUE(sink.lines[1].find("\"repeated\":299999") != std::string::npos);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("logger.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("logger.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "formatting by hand and with to_chars: no printf family, no streams, no std::format, no JSON or regex library, no strftime/gmtime") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("logger.hpp"));
    for (const char* word : {"snprintf", "sprintf", "printf", "ostringstream", "stringstream", "ostream", "iostream", "format", "to_string", "regex", "nlohmann", "json", "gmtime", "localtime", "strftime", "mktime", "chrono", "year_month_day", "sys_days", "stod", "strtod"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "to_chars").empty());
}
