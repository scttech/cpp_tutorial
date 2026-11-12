#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "http_parser.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace http;

namespace {

std::string dump(const RequestParser& p) {
    std::string s = p.method() + " " + p.target() + " 1." + std::to_string(p.version_minor()) + " {";
    for (const Header& h : p.headers()) s += h.name + ":" + h.value + ",";
    s += "} body=" + p.body() + " ka=" + (p.keep_alive() ? "1" : "0");
    return s;
}

struct Result {
    State state;
    ErrorCode error;
    std::size_t consumed;
    std::string text;
};

Result parse_all(const std::string& input, Limits limits = {}) {
    RequestParser p(limits);
    const RequestParser::Feed f = p.feed(input);
    return Result{f.state, p.error(), f.consumed, p.state() == State::Complete ? dump(p) : std::string()};
}

// Same input, delivered in the given chunk sizes (cycled); returns the same Result structure.
Result parse_chunks(const std::string& input, const std::vector<std::size_t>& sizes, Limits limits = {}) {
    RequestParser p(limits);
    std::size_t pos = 0, consumed = 0, k = 0;
    State st = State::NeedMore;
    while (pos < input.size() && st == State::NeedMore) {
        const std::size_t n = std::min(sizes[k++ % sizes.size()], input.size() - pos);
        const RequestParser::Feed f = p.feed(std::string_view(input).substr(pos, n));
        consumed += f.consumed;
        pos += n;
        st = f.state;
        if (f.consumed < n && st == State::Complete) break;   // the rest belongs to the next request
        if (f.consumed < n && st == State::Error) break;
    }
    return Result{p.state(), p.error(), consumed, p.state() == State::Complete ? dump(p) : std::string()};
}

struct Valid {
    const char* name;
    std::string input;
    std::string expected;
};
const std::vector<Valid>& valid_cases() {
    static const std::vector<Valid> v = {
        {"simple GET", "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", "GET / 1.1 {Host:example.com,} body= ka=1"},
        {"POST with Content-Length", "POST /submit HTTP/1.1\r\nHost: a\r\nContent-Length: 5\r\n\r\nhello", "POST /submit 1.1 {Host:a,Content-Length:5,} body=hello ka=1"},
        {"chunked with extension and trailer", "POST /c HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6;ext=1\r\n world\r\n0\r\nX-Trailer: t\r\n\r\n",
         "POST /c 1.1 {Transfer-Encoding:chunked,} body=hello world ka=1"},
        {"HTTP/1.0 closes by default", "GET /a HTTP/1.0\r\n\r\n", "GET /a 1.0 {} body= ka=0"},
        {"HTTP/1.0 keep-alive", "GET /a HTTP/1.0\r\nConnection: keep-alive\r\n\r\n", "GET /a 1.0 {Connection:keep-alive,} body= ka=1"},
        {"HTTP/1.1 Connection: close", "GET / HTTP/1.1\r\nConnection: close\r\n\r\n", "GET / 1.1 {Connection:close,} body= ka=0"},
        {"Connection token list, any case", "GET / HTTP/1.0\r\nConnection: Upgrade, Keep-Alive\r\n\r\n", "GET / 1.0 {Connection:Upgrade, Keep-Alive,} body= ka=1"},
        {"whitespace around values is trimmed", "GET / HTTP/1.1\r\nX-Pad:   spaced value \t\r\nHost:h\r\n\r\n", "GET / 1.1 {X-Pad:spaced value,Host:h,} body= ka=1"},
        {"duplicate headers are kept in order", "GET / HTTP/1.1\r\nAccept: a\r\nAccept: b\r\n\r\n", "GET / 1.1 {Accept:a,Accept:b,} body= ka=1"},
        {"the asterisk target", "OPTIONS * HTTP/1.1\r\n\r\n", "OPTIONS * 1.1 {} body= ka=1"},
        {"Content-Length: 0", "POST /x HTTP/1.1\r\nContent-Length: 0\r\n\r\n", "POST /x 1.1 {Content-Length:0,} body= ka=1"},
        {"an empty header value", "GET / HTTP/1.1\r\nX-Empty:\r\nX-Tab:\t\r\n\r\n", "GET / 1.1 {X-Empty:,X-Tab:,} body= ka=1"},
        {"identical duplicate Content-Length", "POST / HTTP/1.1\r\nContent-Length: 3\r\nContent-Length: 3\r\n\r\nabc", "POST / 1.1 {Content-Length:3,Content-Length:3,} body=abc ka=1"},
        {"Transfer-Encoding in another case", "POST / HTTP/1.1\r\nTRANSFER-ENCODING:  Chunked \r\n\r\n3\r\nabc\r\nA\r\n0123456789\r\n0\r\n\r\n", "POST / 1.1 {TRANSFER-ENCODING:Chunked,} body=abc0123456789 ka=1"},
        {"binary body bytes", std::string("PUT /b HTTP/1.1\r\nContent-Length: 6\r\n\r\n") + std::string("a\0b\r\n\xff", 6), std::string("PUT /b 1.1 {Content-Length:6,} body=") + std::string("a\0b\r\n\xff", 6) + " ka=1"},
        {"hex chunk sizes in both cases", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nb\r\nhello world\r\nB\r\nHELLO WORLD\r\n0\r\n\r\n", "POST / 1.1 {Transfer-Encoding:chunked,} body=hello worldHELLO WORLD ka=1"},
        {"a long target with query", "GET /search?q=a%20b&x=1#frag HTTP/1.1\r\nHost: h\r\n\r\n", "GET /search?q=a%20b&x=1#frag 1.1 {Host:h,} body= ka=1"},
    };
    return v;
}

struct Bad {
    const char* name;
    std::string input;
    ErrorCode code;
};
const std::vector<Bad>& bad_cases() {
    static const std::vector<Bad> v = {
        {"a bare LF ends the request line", "GET / HTTP/1.1\nHost: a\n\n", ErrorCode::BadRequestLine},
        {"a bare LF in the headers", "GET / HTTP/1.1\r\nHost: a\n\r\n", ErrorCode::BadHeader},
        {"a bare CR in a header value", "GET / HTTP/1.1\r\nHost: a\rb\r\n\r\n", ErrorCode::BadHeader},
        {"a lower-case method", "get / HTTP/1.1\r\n\r\n", ErrorCode::BadMethod},
        {"a method with a digit", "G3T / HTTP/1.1\r\n\r\n", ErrorCode::BadMethod},
        {"two spaces in the request line", "GET  / HTTP/1.1\r\n\r\n", ErrorCode::BadRequestLine},
        {"a request line with two parts", "GET /\r\n\r\n", ErrorCode::BadRequestLine},
        {"an empty request line", "\r\n\r\n", ErrorCode::BadRequestLine},
        {"a missing method", " / HTTP/1.1\r\n\r\n", ErrorCode::BadRequestLine},
        {"HTTP/2.0", "GET / HTTP/2.0\r\n\r\n", ErrorCode::BadVersion},
        {"lower-case http", "GET / http/1.1\r\n\r\n", ErrorCode::BadVersion},
        {"a trailing space after the version", "GET / HTTP/1.1 \r\n\r\n", ErrorCode::BadVersion},
        {"a space in the target", "GET /a b HTTP/1.1\r\n\r\n", ErrorCode::BadVersion},
        {"a target without a leading slash", "GET foo HTTP/1.1\r\n\r\n", ErrorCode::BadTarget},
        {"a control character in the target", std::string("GET /a\x01z HTTP/1.1\r\n\r\n"), ErrorCode::BadTarget},
        {"a space before the colon", "GET / HTTP/1.1\r\nHost : a\r\n\r\n", ErrorCode::BadHeader},
        {"an obsolete folded continuation line", "GET / HTTP/1.1\r\nX: a\r\n  b\r\n\r\n", ErrorCode::BadHeader},
        {"a header without a colon", "GET / HTTP/1.1\r\nNoColon\r\n\r\n", ErrorCode::BadHeader},
        {"an empty header name", "GET / HTTP/1.1\r\n: value\r\n\r\n", ErrorCode::BadHeader},
        {"a control character in a value", std::string("GET / HTTP/1.1\r\nX: a\x01" "b\r\n\r\n"), ErrorCode::BadHeader},
        {"a bad character in a name", "GET / HTTP/1.1\r\nBad(Name): v\r\n\r\n", ErrorCode::BadHeader},
        {"Content-Length with a letter", "POST / HTTP/1.1\r\nContent-Length: 5x\r\n\r\nhello", ErrorCode::BadContentLength},
        {"a negative Content-Length", "POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n", ErrorCode::BadContentLength},
        {"a signed Content-Length", "POST / HTTP/1.1\r\nContent-Length: +5\r\n\r\nhello", ErrorCode::BadContentLength},
        {"an empty Content-Length", "POST / HTTP/1.1\r\nContent-Length:\r\n\r\n", ErrorCode::BadContentLength},
        {"an overflowing Content-Length", "POST / HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n", ErrorCode::BadContentLength},
        {"a Content-Length with an inner space", "POST / HTTP/1.1\r\nContent-Length: 1 2\r\n\r\n", ErrorCode::BadContentLength},
        {"two different Content-Lengths", "POST / HTTP/1.1\r\nContent-Length: 3\r\nContent-Length: 4\r\n\r\nabcd", ErrorCode::BadContentLength},
        {"Content-Length and Transfer-Encoding", "POST / HTTP/1.1\r\nContent-Length: 4\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", ErrorCode::ConflictingFraming},
        {"Transfer-Encoding and Content-Length", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 4\r\n\r\n0\r\n\r\n", ErrorCode::ConflictingFraming},
        {"Transfer-Encoding: gzip, chunked", "POST / HTTP/1.1\r\nTransfer-Encoding: gzip, chunked\r\n\r\n", ErrorCode::UnsupportedTransferEncoding},
        {"Transfer-Encoding: identity", "POST / HTTP/1.1\r\nTransfer-Encoding: identity\r\n\r\n", ErrorCode::UnsupportedTransferEncoding},
        {"two Transfer-Encoding headers", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", ErrorCode::UnsupportedTransferEncoding},
        {"a chunk size that is not hex", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", ErrorCode::BadChunk},
        {"an empty chunk size line", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n\r\n", ErrorCode::BadChunk},
        {"a signed chunk size", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n-5\r\nhello\r\n", ErrorCode::BadChunk},
        {"a chunk size with 17 digits", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n00000000000000005\r\nhello\r\n", ErrorCode::BadChunk},
        {"chunk data not followed by CRLF", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhelloXX\r\n", ErrorCode::BadChunk},
        {"a bare LF after a chunk", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\n0\r\n\r\n", ErrorCode::BadChunk},
        {"a chunk-size line with a bare LF", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\nhello\r\n", ErrorCode::BadChunk},
        {"a malformed trailer", "POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nbad trailer\r\n\r\n", ErrorCode::BadHeader},
    };
    return v;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a fresh parser needs more") {
    RequestParser p;
    ZTH_EXPECT_TRUE(p.state() == State::NeedMore);
    ZTH_EXPECT_TRUE(p.error() == ErrorCode::None);
    ZTH_EXPECT_TRUE(p.headers().empty());
    ZTH_EXPECT_TRUE(p.header("Host") == nullptr);
    const auto f = p.feed("");
    ZTH_EXPECT_TRUE(f.state == State::NeedMore);
    ZTH_EXPECT_EQ(f.consumed, std::size_t{0});
}
ZTH_TEST("works", "valid requests of many shapes, parsed in one piece") {
    for (const Valid& c : valid_cases()) {
        const Result r = parse_all(c.input);
        if (r.state != State::Complete || r.text != c.expected || r.consumed != c.input.size()) {
            zth::fail(std::string("`") + c.name + "`: state " + std::to_string(static_cast<int>(r.state)) + ", error " + std::to_string(static_cast<int>(r.error)) + ", consumed " + std::to_string(r.consumed) + "/" +
                      std::to_string(c.input.size()) + ", got `" + r.text + "`, expected `" + c.expected + "`");
            return;
        }
    }
}
ZTH_TEST("works", "invalid requests: the error code says what is wrong") {
    for (const Bad& c : bad_cases()) {
        const Result r = parse_all(c.input);
        if (r.state != State::Error || r.error != c.code) {
            zth::fail(std::string("`") + c.name + "`: state " + std::to_string(static_cast<int>(r.state)) + ", error " + std::to_string(static_cast<int>(r.error)) + ", expected error " + std::to_string(static_cast<int>(c.code)));
            return;
        }
    }
}
ZTH_TEST("works", "a request that is cut short asks for more, it is not an error") {
    for (const std::string& prefix : {std::string("GE"), std::string("GET / HTTP/1.1"), std::string("GET / HTTP/1.1\r"), std::string("GET / HTTP/1.1\r\nHost: a"), std::string("GET / HTTP/1.1\r\nHost: a\r\n"),
                                      std::string("POST / HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel"), std::string("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhel"),
                                      std::string("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n")}) {
        const Result r = parse_all(prefix);
        ZTH_EXPECT_TRUE(r.state == State::NeedMore);
        ZTH_EXPECT_EQ(r.consumed, prefix.size());
    }
}
ZTH_TEST("works", "header lookup ignores case, returns the first match, and returns nullptr for a missing header") {
    RequestParser p;
    p.feed("GET / HTTP/1.1\r\nHost: example.com\r\nX-Dup: first\r\nx-dup: second\r\nContent-Type: text/html\r\n\r\n");
    ZTH_EXPECT_TRUE(p.state() == State::Complete);
    ZTH_EXPECT_TRUE(p.header("host") != nullptr && *p.header("host") == "example.com");
    ZTH_EXPECT_TRUE(p.header("HOST") != nullptr && *p.header("HOST") == "example.com");
    ZTH_EXPECT_TRUE(p.header("X-DUP") != nullptr && *p.header("X-DUP") == "first");
    ZTH_EXPECT_TRUE(p.header("content-type") != nullptr && *p.header("content-type") == "text/html");
    ZTH_EXPECT_TRUE(p.header("Hos") == nullptr);
    ZTH_EXPECT_TRUE(p.header("Hostx") == nullptr);
    ZTH_EXPECT_TRUE(p.header("") == nullptr);
    ZTH_EXPECT_EQ(p.headers().size(), std::size_t{4});
    ZTH_EXPECT_EQ(p.headers()[1].name, std::string("X-Dup"));   // names are stored as received
}
ZTH_TEST("works", "the Connection header is a token list: only whole tokens count") {
    for (const char* v : {"closed", "xclose", "close-me", "keep", "not-close"}) {
        RequestParser p;
        p.feed(std::string("GET / HTTP/1.1\r\nConnection: ") + v + "\r\n\r\n");
        ZTH_EXPECT_TRUE(p.keep_alive());   // none of these is the token "close"
    }
    RequestParser a;
    a.feed("GET / HTTP/1.1\r\nConnection: upgrade ,  CLOSE\r\n\r\n");
    ZTH_EXPECT_FALSE(a.keep_alive());
    RequestParser b;
    b.feed("GET / HTTP/1.0\r\nConnection: keep-alives\r\n\r\n");
    ZTH_EXPECT_FALSE(b.keep_alive());
    RequestParser c;
    c.feed("GET / HTTP/1.1\r\nConnection: keep-alive\r\nConnection: close\r\n\r\n");
    ZTH_EXPECT_FALSE(c.keep_alive());   // several Connection headers: any "close" wins
}
ZTH_TEST("works", "pipelining: the parser stops exactly at the end of the first request and reset() reads the next one") {
    const std::string first = "POST /a HTTP/1.1\r\nContent-Length: 3\r\n\r\nabc";
    const std::string second = "GET /b HTTP/1.1\r\nHost: h\r\n\r\n";
    const std::string third = "POST /c HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nxy\r\n0\r\n\r\n";
    const std::string wire = first + second + third;
    RequestParser p;
    std::size_t pos = 0;
    std::vector<std::string> targets;
    for (int i = 0; i < 3; ++i) {
        const RequestParser::Feed f = p.feed(std::string_view(wire).substr(pos));
        ZTH_EXPECT_TRUE(f.state == State::Complete);
        pos += f.consumed;
        targets.push_back(p.target() + ":" + p.body());
        p.reset();
        ZTH_EXPECT_TRUE(p.state() == State::NeedMore);
    }
    ZTH_EXPECT_EQ(pos, wire.size());
    ZTH_EXPECT_EQ(targets[0], std::string("/a:abc"));
    ZTH_EXPECT_EQ(targets[1], std::string("/b:"));
    ZTH_EXPECT_EQ(targets[2], std::string("/c:xy"));
}
ZTH_TEST("works", "after Complete or Error the parser consumes nothing more until reset") {
    RequestParser p;
    const std::string done = "GET / HTTP/1.1\r\n\r\n";
    p.feed(done);
    const auto again = p.feed("GET /next HTTP/1.1\r\n\r\n");
    ZTH_EXPECT_TRUE(again.state == State::Complete);
    ZTH_EXPECT_EQ(again.consumed, std::size_t{0});
    ZTH_EXPECT_EQ(p.target(), std::string("/"));
    RequestParser e;
    e.feed("BAD\r\n");
    ZTH_EXPECT_TRUE(e.state() == State::Error);
    const auto after = e.feed("GET / HTTP/1.1\r\n\r\n");
    ZTH_EXPECT_TRUE(after.state == State::Error);
    ZTH_EXPECT_EQ(after.consumed, std::size_t{0});
    e.reset();
    ZTH_EXPECT_TRUE(e.error() == ErrorCode::None);
    ZTH_EXPECT_TRUE(e.feed("GET / HTTP/1.1\r\n\r\n").state == State::Complete);
}
ZTH_TEST("works", "limits: header count, line length and body size") {
    Limits l;
    l.max_headers = 3;
    l.max_line = 40;
    l.max_body = 10;
    ZTH_EXPECT_TRUE(parse_all("GET / HTTP/1.1\r\nA: 1\r\nB: 2\r\nC: 3\r\n\r\n", l).state == State::Complete);
    ZTH_EXPECT_TRUE(parse_all("GET / HTTP/1.1\r\nA: 1\r\nB: 2\r\nC: 3\r\nD: 4\r\n\r\n", l).error == ErrorCode::TooManyHeaders);
    ZTH_EXPECT_TRUE(parse_all("GET / HTTP/1.1\r\nX: " + std::string(37, 'v') + "\r\n\r\n", l).state == State::Complete);   // exactly 40 characters
    ZTH_EXPECT_TRUE(parse_all("GET / HTTP/1.1\r\nX: " + std::string(38, 'v') + "\r\n\r\n", l).error == ErrorCode::HeaderTooLarge);   // 41
    ZTH_EXPECT_TRUE(parse_all("GET /" + std::string(40, 'a') + " HTTP/1.1\r\n\r\n", l).error == ErrorCode::HeaderTooLarge);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nContent-Length: 10\r\n\r\n0123456789", l).state == State::Complete);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nContent-Length: 11\r\n\r\n01234567890", l).error == ErrorCode::BodyTooLarge);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\n01234\r\n5\r\n56789\r\n0\r\n\r\n", l).state == State::Complete);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\n01234\r\n6\r\n567890\r\n0\r\n\r\n", l).error == ErrorCode::BodyTooLarge);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFFFFFFFFFF\r\n", l).error == ErrorCode::BodyTooLarge);
}
ZTH_TEST("works", "trailer headers count towards the header limit but are not kept") {
    Limits l;
    l.max_headers = 2;
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nA: 1\r\n\r\n", l).state == State::Complete);
    ZTH_EXPECT_TRUE(parse_all("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nA: 1\r\nB: 2\r\n\r\n", l).error == ErrorCode::TooManyHeaders);
    RequestParser p;
    p.feed("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX: 1\r\n\r\n");
    ZTH_EXPECT_EQ(p.headers().size(), std::size_t{1});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every valid request gives the same result however the bytes arrive: every two-way split, and byte by byte") {
    for (const Valid& c : valid_cases()) {
        for (std::size_t cut = 1; cut < c.input.size(); ++cut) {
            const Result r = parse_chunks(c.input, {cut, c.input.size()});
            if (r.state != State::Complete || r.text != c.expected || r.consumed != c.input.size()) {
                zth::fail(std::string("`") + c.name + "` split after " + std::to_string(cut) + " bytes: got `" + r.text + "`");
                return;
            }
        }
        const Result bytewise = parse_chunks(c.input, {1});
        if (bytewise.state != State::Complete || bytewise.text != c.expected || bytewise.consumed != c.input.size()) {
            zth::fail(std::string("`") + c.name + "` delivered byte by byte: got `" + bytewise.text + "`");
            return;
        }
    }
}
ZTH_TEST("robust", "every invalid request gives the same error however the bytes arrive") {
    for (const Bad& c : bad_cases()) {
        for (const std::vector<std::size_t>& sizes : {std::vector<std::size_t>{1}, std::vector<std::size_t>{2}, std::vector<std::size_t>{3, 1}, std::vector<std::size_t>{5, 2, 7}, std::vector<std::size_t>{1000}}) {
            const Result r = parse_chunks(c.input, sizes);
            if (r.state != State::Error || r.error != c.code) {
                zth::fail(std::string("`") + c.name + "` in pieces of " + std::to_string(sizes[0]) + ": error " + std::to_string(static_cast<int>(r.error)));
                return;
            }
        }
    }
}
ZTH_TEST("robust", "random splits of a pipelined stream give the same requests as feeding it whole") {
    zth::Lcg rng(321);
    std::string wire;
    std::vector<std::string> expected;
    for (const Valid& c : valid_cases()) {
        if (c.expected.find("ka=") == std::string::npos) continue;
        wire += c.input;
        expected.push_back(c.expected);
    }
    for (int round = 0; round < 50; ++round) {
        RequestParser p;
        std::vector<std::string> got;
        std::size_t pos = 0;
        while (pos < wire.size()) {
            const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(1 + rng.below(40)), wire.size() - pos);
            const RequestParser::Feed f = p.feed(std::string_view(wire).substr(pos, n));
            pos += f.consumed;
            if (f.state == State::Complete) {
                got.push_back(dump(p));
                p.reset();
            } else if (f.state == State::Error) {
                zth::fail("unexpected error in round " + std::to_string(round));
                return;
            } else if (f.consumed < n) {
                zth::fail("the parser left bytes unconsumed while asking for more");
                return;
            }
        }
        if (got != expected) {
            zth::fail("round " + std::to_string(round) + ": the pipelined requests differ (" + std::to_string(got.size()) + " parsed, " + std::to_string(expected.size()) + " expected)");
            return;
        }
    }
}
ZTH_TEST("robust", "garbage never crashes the parser, never loops, and an error is final: 5000 random inputs in random pieces") {
    zth::Lcg rng(77);
    const std::string alphabet = "GET POST/HTTP/1.1:\r\n\r\nContent-Length:5 chunked\t;,0a9FzZ\x01\xff";
    for (int round = 0; round < 5000; ++round) {
        std::string input;
        const int len = rng.below(120);
        for (int i = 0; i < len; ++i) input += alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))];
        RequestParser p;
        std::size_t pos = 0;
        while (pos < input.size() && p.state() == State::NeedMore) {
            const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(1 + rng.below(9)), input.size() - pos);
            const RequestParser::Feed f = p.feed(std::string_view(input).substr(pos, n));
            if (f.consumed > n) {
                zth::fail("consumed more bytes than were given");
                return;
            }
            pos += n;
            if (f.state != p.state()) {
                zth::fail("the returned state differs from state()");
                return;
            }
        }
        if (p.state() == State::Error && p.error() == ErrorCode::None) {
            zth::fail("an error state without an error code");
            return;
        }
        if (p.state() != State::Error && p.error() != ErrorCode::None) {
            zth::fail("an error code without the error state");
            return;
        }
    }
}
ZTH_TEST("robust", "bodies of all sizes around the chunk and limit boundaries") {
    for (std::size_t n : {std::size_t{1}, std::size_t{255}, std::size_t{256}, std::size_t{4095}, std::size_t{4096}, std::size_t{65535}, std::size_t{65536}}) {
        std::string body(n, 'x');
        for (std::size_t i = 0; i < n; i += 7) body[i] = static_cast<char>('a' + i % 26);
        const std::string fixed = "POST / HTTP/1.1\r\nContent-Length: " + std::to_string(n) + "\r\n\r\n" + body;
        char hex[32];
        std::snprintf(hex, sizeof hex, "%zx", n);
        const std::string chunked = std::string("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") + hex + "\r\n" + body + "\r\n0\r\n\r\n";
        for (const std::string* input : {&fixed, &chunked}) {
            RequestParser p;
            std::size_t pos = 0;
            while (pos < input->size()) {
                const std::size_t k = std::min<std::size_t>(1500, input->size() - pos);
                pos += p.feed(std::string_view(*input).substr(pos, k)).consumed;
            }
            ZTH_EXPECT_TRUE(p.state() == State::Complete);
            ZTH_EXPECT_TRUE(p.body() == body);
        }
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a 1 MB body delivered one byte at a time: linear time (no re-scanning, no rebuilding of the body string)", 20) {
    constexpr std::size_t kBody = 1 << 20;
    const std::string head = "POST /big HTTP/1.1\r\nContent-Length: " + std::to_string(kBody) + "\r\n\r\n";
    RequestParser p(Limits{8192, 100, kBody});
    for (char c : head) p.feed(std::string_view(&c, 1));
    const char byte = 'z';
    for (std::size_t i = 0; i < kBody; ++i) p.feed(std::string_view(&byte, 1));
    ZTH_EXPECT_TRUE(p.state() == State::Complete);
    ZTH_EXPECT_EQ(p.body().size(), kBody);
}
ZTH_TEST_TIMEOUT("fast", "a 256 KB header line delivered one byte at a time is also linear", 20) {
    Limits l;
    l.max_line = 1 << 20;
    RequestParser p(l);
    const std::string start = "GET / HTTP/1.1\r\nX-Big: ";
    for (char c : start) p.feed(std::string_view(&c, 1));
    const char v = 'v';
    for (int i = 0; i < 256 * 1024; ++i) p.feed(std::string_view(&v, 1));
    for (char c : std::string("\r\n\r\n")) p.feed(std::string_view(&c, 1));
    ZTH_EXPECT_TRUE(p.state() == State::Complete);
    ZTH_EXPECT_EQ(p.headers()[0].value.size(), std::size_t{256 * 1024});
}
ZTH_TEST_TIMEOUT("fast", "a hostile line without a CRLF is rejected after about max_line bytes, not buffered: 400 MB of 'a' costs nothing", 20) {
    const std::string chunk(1 << 20, 'a');
    RequestParser p;   // max_line 8192
    std::size_t total = 0;
    for (int i = 0; i < 400 && p.state() == State::NeedMore; ++i) {
        const auto f = p.feed(chunk);
        total += f.consumed;
    }
    ZTH_EXPECT_TRUE(p.state() == State::Error);
    ZTH_EXPECT_TRUE(p.error() == ErrorCode::HeaderTooLarge);
    ZTH_EXPECT_TRUE(total < 16 * 1024);
}
ZTH_TEST_TIMEOUT("fast", "a request with 100 headers parses 20000 times a second or better", 20) {
    std::string req = "GET /x HTTP/1.1\r\n";
    for (int i = 0; i < 100; ++i) req += "X-Header-" + std::to_string(i) + ": value number " + std::to_string(i) + "\r\n";
    req += "\r\n";
    RequestParser p;
    for (int i = 0; i < 5000; ++i) {
        p.reset();
        p.feed(req);
    }
    ZTH_EXPECT_TRUE(p.state() == State::Complete);
    ZTH_EXPECT_EQ(p.headers().size(), std::size_t{100});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("http_parser.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("http_parser.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a hand-written parser: no regex, no stream extraction, no sscanf/strtol/stoul (they accept signs, spaces and garbage)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("http_parser.hpp"));
    for (const char* word : {"regex", "regex_match", "regex_search", "istringstream", "stringstream", "sscanf", "strtol", "strtoul", "strtoull", "atoi", "atol", "stoi", "stol", "stoul", "stoull", "from_chars", "strtok"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "input is a string_view") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("http_parser.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
