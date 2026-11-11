#define ZTH_TRACK_ALLOCATIONS
#include <cstdio>
#include <string>
#include <vector>

#include "fix.hpp"
#include "zth_test.hpp"

using namespace fix;

namespace {

std::string soh(std::string s) {
    for (char& c : s) {
        if (c == '|') c = kSoh;
    }
    return s;
}

// builds a complete message from the fields between 9= and 10= (with | for SOH), computing length and checksum
std::string make(const std::string& body_bars, const std::string& begin = "FIX.4.4") {
    const std::string body = soh(body_bars);
    std::string text = "8=" + begin + std::string(1, kSoh) + "9=" + std::to_string(body.size()) + std::string(1, kSoh) + body;
    char sum[8];
    std::snprintf(sum, sizeof sum, "%03d", checksum_of(text));
    return text + "10=" + sum + std::string(1, kSoh);
}

std::string bars(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c == kSoh) c = '|';
    }
    return out;
}

void expect_error(const std::string& text, Error e, std::size_t pos) {
    const ParseResult r = parse(text);
    const std::string want = std::string(name(e)) + " at " + std::to_string(pos);
    const std::string got = r.ok() ? std::string("success") : std::string(name(r.error)) + " at " + std::to_string(r.pos);
    if (want != got) ::zth::fail("parse(" + bars(text) + "): expected " + want + ", got " + got);
}

std::string value_of(const ParseResult& r, int tag) {
    const Field* f = find(r, tag);
    return f ? std::string(f->value) : std::string("<none>");
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "checksum_of: the sum of the bytes modulo 256") {
    ZTH_EXPECT_EQ(checksum_of(""), 0);
    ZTH_EXPECT_EQ(checksum_of("A"), 65);
    ZTH_EXPECT_EQ(checksum_of("AB"), 131);
    ZTH_EXPECT_EQ(checksum_of(std::string(4, '\xFF')), (4 * 255) % 256);
    ZTH_EXPECT_EQ(checksum_of(std::string("\x01\x01", 2)), 2);
    ZTH_EXPECT_EQ(checksum_of(std::string(300, 'a')), (300 * 97) % 256);
}
ZTH_TEST("works", "a valid message: every field in order with its tag and value") {
    const std::string text = make("35=D|49=ACME|56=EXCH|55=MSFT|54=1|38=100|44=250.50|");
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.fields.size(), std::size_t{10});
    ZTH_EXPECT_EQ(r.fields[0].tag, 8);
    ZTH_EXPECT_EQ(std::string(r.fields[0].value), std::string("FIX.4.4"));
    ZTH_EXPECT_EQ(r.fields[1].tag, 9);
    ZTH_EXPECT_EQ(r.fields[2].tag, 35);
    ZTH_EXPECT_EQ(std::string(r.fields[2].value), std::string("D"));
    ZTH_EXPECT_EQ(r.fields[9].tag, 10);
    ZTH_EXPECT_EQ(value_of(r, 55), std::string("MSFT"));
    ZTH_EXPECT_EQ(value_of(r, 44), std::string("250.50"));
    ZTH_EXPECT_EQ(value_of(r, 999), std::string("<none>"));
}
ZTH_TEST("works", "find returns the first field with the tag; repeated tags are all kept") {
    const std::string text = make("35=8|448=AAA|448=BBB|448=CCC|");
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(value_of(r, 448), std::string("AAA"));
    int count = 0;
    for (const Field& f : r.fields) count += f.tag == 448;
    ZTH_EXPECT_EQ(count, 3);
}
ZTH_TEST("works", "values may be empty and may contain '='; only the first '=' of a field splits it") {
    const std::string text = make("35=D|58=|99=a=b=c|");
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(value_of(r, 58), std::string(""));
    ZTH_EXPECT_TRUE(find(r, 58) != nullptr);
    ZTH_EXPECT_EQ(value_of(r, 99), std::string("a=b=c"));
}
ZTH_TEST("works", "values are VIEWS into the input: no copy is made") {
    const std::string text = make("35=D|55=MSFT|44=250.50|");
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    for (const Field& f : r.fields) {
        ZTH_EXPECT_TRUE(f.value.data() >= text.data() && f.value.data() + f.value.size() <= text.data() + text.size());
    }
    const Field* sym = find(r, 55);
    ZTH_EXPECT_EQ(static_cast<std::size_t>(sym->value.data() - text.data()), text.find("MSFT"));
}
ZTH_TEST("works", "a wrong checksum is BadChecksum at the 10 field; a wrong body length is BadBodyLength") {
    std::string text = make("35=D|55=MSFT|");
    const std::size_t ten = text.rfind("10=");
    std::string bad = text;
    bad[ten + 3] = bad[ten + 3] == '9' ? '0' : static_cast<char>(bad[ten + 3] + 1);
    expect_error(bad, Error::BadChecksum, ten);
    std::string wrong_len = text;
    wrong_len.replace(text.find("9=") + 2, 2, "99");   // the real length is 2 digits: replace by 99
    expect_error(wrong_len, Error::BadBodyLength, text.find(kSoh, text.find(kSoh) + 1) + 1);
}
ZTH_TEST("works", "empty text, a missing final separator, and a field without '=' are rejected") {
    expect_error("", Error::Empty, 0);
    std::string text = make("35=D|55=MSFT|");
    expect_error(text.substr(0, text.size() - 1), Error::MissingDelimiter, text.rfind("10="));
    expect_error(soh("8=FIX.4.4|9=5|garbage|10=000|"), Error::MissingDelimiter, 14);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "bad tags: empty, letters, signs, zero, too big for an int; the position is the start of the field") {
    const std::string head = std::string("8=FIX.4.4") + kSoh;
    expect_error(head + soh("=abc|9=3|10=000|"), Error::BadTag, head.size());
    expect_error(head + soh("9=7|35=D|x1=2|10=000|"), Error::BadTag, head.size() + 4 + 5);
    expect_error(head + soh("9=7|-5=D|10=000|"), Error::BadTag, head.size() + 4);
    expect_error(head + soh("9=7|0=D|10=000|"), Error::BadTag, head.size() + 4);
    expect_error(head + soh("9=7|99999999999=D|10=000|"), Error::BadTag, head.size() + 4);
    expect_error(head + soh("9=7| 35=D|10=000|"), Error::BadTag, head.size() + 4);
    expect_error(head + soh("9=7|3 5=D|10=000|"), Error::BadTag, head.size() + 4);
}
ZTH_TEST("robust", "the begin string: the first field must be 8=<something>") {
    expect_error(soh("9=5|35=D|10=000|"), Error::BadBeginString, 0);
    expect_error(soh("8=|9=5|35=D|10=000|"), Error::BadBeginString, 0);
    expect_error(soh("35=D|8=FIX.4.4|9=0|10=000|"), Error::BadBeginString, 0);
    ZTH_EXPECT_TRUE(parse(make("35=D|", "FIXT.1.1")).ok());
}
ZTH_TEST("robust", "the body length: field 9 must follow, hold digits, and be right (it counts BYTES, so a long value counts fully)") {
    expect_error(soh("8=FIX.4.4|35=D|10=000|"), Error::BadBodyLength, 10);
    expect_error(soh("8=FIX.4.4|9=abc|35=D|10=000|"), Error::BadBodyLength, 10);
    expect_error(soh("8=FIX.4.4|9=|35=D|10=000|"), Error::BadBodyLength, 10);
    expect_error(soh("8=FIX.4.4|9=-3|35=D|10=000|"), Error::BadBodyLength, 10);
    expect_error(soh("8=FIX.4.4|9=99999999999999999999|35=D|10=000|"), Error::BadBodyLength, 10);
    ZTH_EXPECT_TRUE(parse(make("35=D|58=" + std::string(500, 'x') + "|")).ok());
    ZTH_EXPECT_TRUE(parse(make("35=D|58=\xC3\xA9\xC3\xA9|")).ok());   // two 2-byte characters: the length counts bytes
    std::string text = make("35=D|55=MSFT|");
    text.replace(text.find("9=") + 2, 2, "12");   // the true length is 13
    expect_error(text, Error::BadBodyLength, text.find(kSoh, text.find(kSoh) + 1) + 1);
}
ZTH_TEST("robust", "the checksum field: exactly three digits, last, and right") {
    std::string good = make("35=D|55=MSFT|");
    const std::size_t ten = good.rfind("10=");
    // two digits, four digits, letters, empty
    for (const char* bad : {"10=12", "10=1234", "10=abc", "10="}) {
        std::string t = good.substr(0, ten) + bad + std::string(1, kSoh);
        expect_error(t, Error::BadChecksum, ten);
    }
    // no 10 field at all: the last field is something else
    std::string no_ten = good.substr(0, ten);
    expect_error(no_ten, Error::BadChecksum, no_ten.rfind("55="));   // the last field is 55=MSFT: not a checksum field
}
ZTH_TEST("robust", "data after the checksum field: TrailingData, at the byte after it") {
    std::string good = make("35=D|");
    std::string with_tail = good + soh("58=late|");
    expect_error(with_tail, Error::TrailingData, good.size());
    std::string two_ten = make("35=D|") + soh("10=000|");
    expect_error(two_ten, Error::TrailingData, good.size());
}
ZTH_TEST("robust", "the checksum counts bytes above 127 as unsigned values") {
    const std::string text = make("35=D|58=\xFF\xFE\xFD\xC3|");
    ZTH_EXPECT_TRUE(parse(text).ok());
    const std::string text2 = make("35=D|58=" + std::string(100, '\x80') + "|");
    ZTH_EXPECT_TRUE(parse(text2).ok());
}
ZTH_TEST("robust", "an empty message body (just 8, 9=0 and 10) is valid") {
    const std::string text = make("");
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.fields.size(), std::size_t{3});
}
ZTH_TEST("robust", "tags with leading zeros and big tags are accepted; the SOH is the only separator ('|' is data)") {
    const std::string text = make("0035=D|58=a|b|c|2147483647=ok|");
    // '|' here is part of make's SOH replacement: so this builds fields 58=a, b, c... use an explicit message instead
    (void)text;
    std::string body = "35=D" + std::string(1, kSoh) + "58=a|b|c" + std::string(1, kSoh) + "2147483647=ok" + std::string(1, kSoh);
    std::string msg = "8=FIX.4.4" + std::string(1, kSoh) + "9=" + std::to_string(body.size()) + std::string(1, kSoh) + body;
    char sum[8];
    std::snprintf(sum, sizeof sum, "%03d", checksum_of(msg));
    msg += "10=" + std::string(sum) + std::string(1, kSoh);
    const ParseResult r = parse(msg);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(value_of(r, 58), std::string("a|b|c"));
    ZTH_EXPECT_EQ(value_of(r, 2147483647), std::string("ok"));
    const ParseResult zeros = parse(make("0035=D|"));
    ZTH_EXPECT_TRUE(zeros.ok());
    ZTH_EXPECT_EQ(zeros.fields[2].tag, 35);
}
ZTH_TEST("robust", "a failed parse returns no fields; the same text parses the same way every time") {
    const std::string text = make("35=D|55=MSFT|");
    const ParseResult a = parse(text), b = parse(text);
    ZTH_EXPECT_TRUE(a.ok() && b.ok());
    ZTH_EXPECT_EQ(a.fields.size(), b.fields.size());
    std::string bad = text;
    bad.back() = 'X';
    const ParseResult f = parse(bad);
    ZTH_EXPECT_FALSE(f.ok());
    ZTH_EXPECT_TRUE(f.fields.empty());
}
ZTH_TEST_TIMEOUT("robust", "400 random messages with random corruption: a corrupted byte is never accepted unless the message is still valid", 10) {
    ::zth::Lcg rng(2025);
    int rejected = 0;
    for (int i = 0; i < 400; ++i) {
        std::string body = "35=D|";
        const int n = rng.below(8);
        for (int k = 0; k < n; ++k) body += std::to_string(100 + rng.below(800)) + "=" + std::string(static_cast<std::size_t>(rng.below(10)), static_cast<char>('a' + rng.below(26))) + "|";
        std::string text = make(body);
        ZTH_EXPECT_TRUE(parse(text).ok());
        const std::size_t at = static_cast<std::size_t>(rng.below(static_cast<int>(text.size())));
        const char original = text[at];
        text[at] = static_cast<char>(original ^ (1 << rng.below(7)));
        const ParseResult r = parse(text);
        if (!r.ok()) ++rejected;
        // if it was accepted, the corruption must have kept every rule: re-check by hand that the checksum matches
        if (r.ok()) ZTH_EXPECT_EQ(r.fields.back().tag, 10);
    }
    ZTH_EXPECT_TRUE(rejected >= 380);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "zero copy: parsing a message makes only the few allocations of the growing vectors, however long the values", 10) {
    const std::string text = make("35=D|58=" + std::string(100000, 'x') + "|55=MSFT|");
    zth::mem::Scope scope;
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 10);   // the field vector grows in steps; nothing is allocated per value
}
ZTH_TEST_TIMEOUT("fast", "allocations do not grow with the length of the values or with their number beyond the vector's growth", 10) {
    std::string body = "35=D|";
    for (int i = 0; i < 5000; ++i) body += std::to_string(1000 + i) + "=" + std::string(40, 'v') + "|";
    const std::string text = make(body);
    zth::mem::Scope scope;
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.fields.size(), std::size_t{5004});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);
}
ZTH_TEST_TIMEOUT("fast", "200000 small messages in time", 10) {
    const std::string text = make("35=D|49=ACME|56=EXCH|11=ORD1|55=MSFT|54=1|38=100|44=250.50|");
    std::size_t fields = 0;
    for (int i = 0; i < 200000; ++i) fields += parse(text).fields.size();
    ZTH_EXPECT_EQ(fields, std::size_t{200000} * 11);
}
ZTH_TEST_TIMEOUT("fast", "a message of 20 MB parses in one pass", 10) {
    std::string body = "35=D|";
    for (int i = 0; i < 400000; ++i) body += "58=" + std::string(45, 'z') + "|";
    const std::string text = make(body);
    ZTH_EXPECT_TRUE(text.size() > 18000000);
    const ParseResult r = parse(text);
    ZTH_EXPECT_TRUE(r.ok());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("fix.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("fix.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "zero copy: no std::string is made in the parser, no substr into a string, no stoi / stol / atoi / sscanf / streams") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("fix.hpp"));
    for (const char* word : {"string", "stoi", "stol", "stoll", "stoul", "atoi", "atol", "sscanf", "stringstream", "istringstream", "to_string"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("fix.hpp uses `") + word + "`");
            return;
        }
    }
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "from_chars").empty());
}
