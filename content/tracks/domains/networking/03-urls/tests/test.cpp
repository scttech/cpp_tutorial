#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <utility>
#include <vector>

#include "types.hpp"
#include "urlkit.hpp"
#include "zth_test.hpp"

using namespace urlkit;

namespace {

std::string norm(const char* text) {
    const auto u = parse(text);
    return u ? to_string(*u) : std::string("(invalid)");
}
std::string res(const Url& base, const char* ref) {
    const auto u = resolve(base, ref);
    return u ? to_string(*u) : std::string("(invalid)");
}

struct Pair {
    const char* in;
    const char* out;
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "percent_decode: valid escapes of either case, and every kind of invalid escape") {
    ZTH_EXPECT_EQ(percent_decode("").value_or("!"), std::string(""));
    ZTH_EXPECT_EQ(percent_decode("plain-text_1.2~3").value_or("!"), std::string("plain-text_1.2~3"));
    ZTH_EXPECT_EQ(percent_decode("%41%42%43").value_or("!"), std::string("ABC"));
    ZTH_EXPECT_EQ(percent_decode("caf%C3%A9").value_or("!"), std::string("caf\xC3\xA9"));
    ZTH_EXPECT_EQ(percent_decode("caf%c3%a9").value_or("!"), std::string("caf\xC3\xA9"));
    ZTH_EXPECT_EQ(percent_decode("a%20b").value_or("!"), std::string("a b"));
    ZTH_EXPECT_EQ(percent_decode("a+b").value_or("!"), std::string("a+b"));   // '+' is a space only in query strings
    ZTH_EXPECT_EQ(percent_decode("%25").value_or("!"), std::string("%"));
    ZTH_EXPECT_EQ(percent_decode("%2541").value_or("!"), std::string("%41"));   // decoded once, not twice
    ZTH_EXPECT_EQ(percent_decode(std::string_view("a%00b", 5)).value_or("!"), std::string("a\0b", 3));
    ZTH_EXPECT_EQ(percent_decode("%7e%7E").value_or("!"), std::string("~~"));
    ZTH_EXPECT_EQ(percent_decode("x%41").value_or("!"), std::string("xA"));   // an escape at the very end
    for (const char* bad : {"%", "%4", "%zz", "%4z", "%z4", "a%", "a%4", "%%41", "100%", "%g0", "% 41", "%-1"}) {
        if (percent_decode(bad).has_value()) {
            zth::fail(std::string("`") + bad + "` should be invalid");
            return;
        }
    }
}
ZTH_TEST("works", "percent_encode: unreserved characters stay, everything else becomes %XX in upper case, `keep` extends the safe set") {
    ZTH_EXPECT_EQ(percent_encode(""), std::string(""));
    ZTH_EXPECT_EQ(percent_encode("AZaz09-._~"), std::string("AZaz09-._~"));
    ZTH_EXPECT_EQ(percent_encode("a b"), std::string("a%20b"));
    ZTH_EXPECT_EQ(percent_encode("caf\xC3\xA9"), std::string("caf%C3%A9"));
    ZTH_EXPECT_EQ(percent_encode("a/b?c=d&e#f"), std::string("a%2Fb%3Fc%3Dd%26e%23f"));
    ZTH_EXPECT_EQ(percent_encode("a/b?c", "/"), std::string("a/b%3Fc"));
    ZTH_EXPECT_EQ(percent_encode("a/b?c", "/?"), std::string("a/b?c"));
    ZTH_EXPECT_EQ(percent_encode("*!'()"), std::string("%2A%21%27%28%29"));
    ZTH_EXPECT_EQ(percent_encode("%"), std::string("%25"));
    ZTH_EXPECT_EQ(percent_encode(std::string_view("\0\x7F\xFF", 3)), std::string("%00%7F%FF"));
    ZTH_EXPECT_EQ(percent_encode("+"), std::string("%2B"));
}
ZTH_TEST("works", "remove_dot_segments follows RFC 3986 section 5.2.4") {
    const Pair cases[] = {
        {"/a/b/c/./../../g", "/a/g"}, {"/a/b/../c/./d", "/a/c/d"}, {"/a/b/..", "/a/"}, {"/a/.", "/a/"}, {"/..", "/"}, {"/../a", "/a"}, {"/a//b", "/a//b"},
        {"", ""}, {"/", "/"}, {"/a/b/c/", "/a/b/c/"}, {"/a/./b/./c", "/a/b/c"}, {"/a/b/../../..", "/"}, {"/a/../../b", "/b"}, {"///", "///"},
        {"../a", "a"}, {"./a", "a"}, {".", ""}, {"..", ""}, {"mid/content=5/../6", "mid/6"}, {"/a/b/c/../../../../d", "/d"}, {"/a/..b/c", "/a/..b/c"}, {"/a/.b/./c.", "/a/.b/c."},
    };
    for (const Pair& c : cases) {
        if (remove_dot_segments(c.in) != c.out) {
            zth::fail(std::string("remove_dot_segments(\"") + c.in + "\") = \"" + remove_dot_segments(c.in) + "\", expected \"" + c.out + "\"");
            return;
        }
    }
}
ZTH_TEST("works", "parse: the parts of a URL, with scheme and host in lower case and escapes in upper case") {
    const auto u = parse("HTTP://User:Pw@Example.COM:8080/a/b%2fc?x=1&y=%7e#Frag%7e");
    ZTH_EXPECT_TRUE(u.has_value());
    if (!u) return;
    ZTH_EXPECT_EQ(u->scheme, std::string("http"));
    ZTH_EXPECT_EQ(u->userinfo, std::string("User:Pw"));
    ZTH_EXPECT_EQ(u->host, std::string("example.com"));
    ZTH_EXPECT_TRUE(u->port.has_value() && *u->port == 8080);
    ZTH_EXPECT_EQ(u->path, std::string("/a/b%2Fc"));
    ZTH_EXPECT_TRUE(u->query.has_value() && *u->query == "x=1&y=%7E");
    ZTH_EXPECT_TRUE(u->fragment.has_value() && *u->fragment == "Frag%7E");
}
ZTH_TEST("works", "parse: optional parts are really optional, and empty is not the same as absent") {
    const auto a = parse("http://host");
    ZTH_EXPECT_TRUE(a.has_value() && a->path.empty() && !a->port && !a->query && !a->fragment && a->userinfo.empty());
    const auto b = parse("http://host/p?#");
    ZTH_EXPECT_TRUE(b.has_value() && b->query.has_value() && b->query->empty() && b->fragment.has_value() && b->fragment->empty());
    const auto c = parse("http://host/p#f?not-a-query");
    ZTH_EXPECT_TRUE(c.has_value() && !c->query && c->fragment && *c->fragment == "f?not-a-query");
    const auto d = parse("http://host?q#f");
    ZTH_EXPECT_TRUE(d.has_value() && d->path.empty() && d->query && *d->query == "q" && d->fragment && *d->fragment == "f");
    const auto e = parse("http://host:/p");
    ZTH_EXPECT_TRUE(e.has_value() && !e->port && e->host == "host");
    const auto f = parse("a+b-c.d://h/");
    ZTH_EXPECT_TRUE(f.has_value() && f->scheme == "a+b-c.d");
}
ZTH_TEST("works", "parse: authority details: userinfo before the last @, IPv6 literals, ports") {
    const auto a = parse("http://us@er:pw@host/");
    ZTH_EXPECT_TRUE(a.has_value() && a->userinfo == "us@er:pw" && a->host == "host");
    const auto b = parse("https://[2001:DB8::1]:8443/x");
    ZTH_EXPECT_TRUE(b.has_value() && b->host == "[2001:db8::1]" && b->port && *b->port == 8443);
    const auto c = parse("http://[::1]/");
    ZTH_EXPECT_TRUE(c.has_value() && c->host == "[::1]" && !c->port);
    const auto d = parse("http://h:65535/");
    ZTH_EXPECT_TRUE(d.has_value() && d->port && *d->port == 65535);
    const auto e = parse("http://h:0080/");
    ZTH_EXPECT_TRUE(e.has_value() && e->port && *e->port == 80);
}
ZTH_TEST("works", "parse rejects what is not a hierarchical URL") {
    for (const char* bad : {"", "example.com", "//example.com/", "http:/example.com", "http:example.com", "1http://x/", "ht tp://x/", "http://", "http:///path", "http://?q", "http://exa mple.com/", "http://a:99999/",
                            "http://a:65536/", "http://a:12x/", "http://a:-1/", "http://a:1:2/", "http://[::1/", "http://[::1]x/", "http://a/%zz", "http://a/%4", "http://a/p ath", "http://a/p?q r", "http://a/p#f g",
                            "http://a%zz/", "http://us er@host/", "http://host]/", "http://[host/", "http://a:99999999999999/"}) {
        if (parse(bad).has_value()) {
            zth::fail(std::string("`") + bad + "` should be rejected");
            return;
        }
    }
    ZTH_EXPECT_FALSE(parse(std::string("http://a/p\x01q")).has_value());
    ZTH_EXPECT_FALSE(parse(std::string("http://a/p\x7Fq")).has_value());
    ZTH_EXPECT_FALSE(parse(std::string("http://a\x01/")).has_value());
}
ZTH_TEST("works", "to_string: default ports are dropped, an empty http path becomes /, everything else is kept") {
    const Pair cases[] = {
        {"HTTP://User@Example.COM:80/a/b/../c/./d?x=1&y=%7e#Frag", "http://User@example.com/a/b/../c/./d?x=1&y=%7E#Frag"},
        {"https://[::1]:8443/", "https://[::1]:8443/"}, {"https://h:443/x", "https://h/x"}, {"http://h:443/x", "http://h:443/x"}, {"https://h:80/x", "https://h:80/x"},
        {"http://example.com", "http://example.com/"}, {"ws://h", "ws://h/"}, {"wss://h:443", "wss://h/"}, {"ftp://host", "ftp://host"}, {"ftp://host:21/f", "ftp://host/f"}, {"ftp://host:22/f", "ftp://host:22/f"},
        {"gopher://h:70", "gopher://h:70"}, {"http://example.com:/p", "http://example.com/p"}, {"http://a:0/", "http://a:0/"}, {"http://a?q#f", "http://a/?q#f"}, {"http://u:p@h:81/?", "http://u:p@h:81/?"},
        {"http://a/b#", "http://a/b#"}, {"http://a/%2f%2F", "http://a/%2F%2F"}, {"http://a/p?a=%e2%82%ac", "http://a/p?a=%E2%82%AC"},
    };
    for (const Pair& c : cases) {
        if (norm(c.in) != c.out) {
            zth::fail(std::string("`") + c.in + "` becomes `" + norm(c.in) + "`, expected `" + c.out + "`");
            return;
        }
    }
}
ZTH_TEST("works", "resolve: the normal examples of RFC 3986 section 5.4.1") {
    const auto base = parse("http://a/b/c/d;p?q");
    ZTH_EXPECT_TRUE(base.has_value());
    if (!base) return;
    const Pair cases[] = {
        {"g", "http://a/b/c/g"}, {"./g", "http://a/b/c/g"}, {"g/", "http://a/b/c/g/"}, {"/g", "http://a/g"}, {"//g", "http://g/"}, {"?y", "http://a/b/c/d;p?y"}, {"g?y", "http://a/b/c/g?y"},
        {"#s", "http://a/b/c/d;p?q#s"}, {"g#s", "http://a/b/c/g#s"}, {"g?y#s", "http://a/b/c/g?y#s"}, {";x", "http://a/b/c/;x"}, {"g;x", "http://a/b/c/g;x"}, {"g;x?y#s", "http://a/b/c/g;x?y#s"},
        {"", "http://a/b/c/d;p?q"}, {".", "http://a/b/c/"}, {"./", "http://a/b/c/"}, {"..", "http://a/b/"}, {"../", "http://a/b/"}, {"../g", "http://a/b/g"}, {"../..", "http://a/"}, {"../../", "http://a/"},
        {"../../g", "http://a/g"},
    };
    for (const Pair& c : cases) {
        if (res(*base, c.in) != c.out) {
            zth::fail(std::string("resolving `") + c.in + "` gives `" + res(*base, c.in) + "`, expected `" + c.out + "`");
            return;
        }
    }
}
ZTH_TEST("works", "resolve: the abnormal examples of RFC 3986 section 5.4.2 (dots above the root, dots inside segments, dots in the query)") {
    const auto base = parse("http://a/b/c/d;p?q");
    if (!base) return;
    const Pair cases[] = {
        {"../../../g", "http://a/g"}, {"../../../../g", "http://a/g"}, {"/./g", "http://a/g"}, {"/../g", "http://a/g"}, {"g.", "http://a/b/c/g."}, {".g", "http://a/b/c/.g"}, {"g..", "http://a/b/c/g.."},
        {"..g", "http://a/b/c/..g"}, {"./../g", "http://a/b/g"}, {"./g/.", "http://a/b/c/g/"}, {"g/./h", "http://a/b/c/g/h"}, {"g/../h", "http://a/b/c/h"}, {"g;x=1/./y", "http://a/b/c/g;x=1/y"},
        {"g;x=1/../y", "http://a/b/c/y"}, {"g?y/./x", "http://a/b/c/g?y/./x"}, {"g?y/../x", "http://a/b/c/g?y/../x"}, {"g#s/./x", "http://a/b/c/g#s/./x"}, {"g#s/../x", "http://a/b/c/g#s/../x"},
    };
    for (const Pair& c : cases) {
        if (res(*base, c.in) != c.out) {
            zth::fail(std::string("resolving `") + c.in + "` gives `" + res(*base, c.in) + "`, expected `" + c.out + "`");
            return;
        }
    }
}
ZTH_TEST("works", "resolve: absolute references, other bases, and references that cannot be resolved") {
    const auto base = parse("https://u@host:8443/dir/file?old#frag");
    if (!base) return;
    ZTH_EXPECT_EQ(res(*base, "http://x/y/../z"), std::string("http://x/z"));
    ZTH_EXPECT_EQ(res(*base, "HTTP://X.COM:80/a/./b"), std::string("http://x.com/a/b"));
    ZTH_EXPECT_EQ(res(*base, "other"), std::string("https://u@host:8443/dir/other"));
    ZTH_EXPECT_EQ(res(*base, "?new"), std::string("https://u@host:8443/dir/file?new"));
    ZTH_EXPECT_EQ(res(*base, ""), std::string("https://u@host:8443/dir/file?old"));   // the base fragment is NOT inherited
    ZTH_EXPECT_EQ(res(*base, "//other.org/p"), std::string("https://other.org/p"));
    ZTH_EXPECT_EQ(res(*base, "mailto:me@example.com"), std::string("(invalid)"));
    ZTH_EXPECT_EQ(res(*base, "g:h"), std::string("(invalid)"));
    ZTH_EXPECT_EQ(res(*base, "p%zz"), std::string("(invalid)"));
    ZTH_EXPECT_EQ(res(*base, "p q"), std::string("(invalid)"));
    ZTH_EXPECT_EQ(res(*base, "http://bad host/"), std::string("(invalid)"));
    const auto root = parse("http://host");   // an empty base path
    ZTH_EXPECT_EQ(res(*root, "a/b"), std::string("http://host/a/b"));
    ZTH_EXPECT_EQ(res(*root, "../a"), std::string("http://host/a"));
    ZTH_EXPECT_EQ(res(*root, ""), std::string("http://host/"));
}
ZTH_TEST("works", "parse_query decodes pairs, plus signs and escapes, and keeps repeated keys in order") {
    using Q = std::vector<std::pair<std::string, std::string>>;
    ZTH_EXPECT_TRUE(parse_query("a=1&b=%20x&c&a=2") == (Q{{"a", "1"}, {"b", " x"}, {"c", ""}, {"a", "2"}}));
    ZTH_EXPECT_TRUE(parse_query("x=a+b%2Bc") == (Q{{"x", "a b+c"}}));
    ZTH_EXPECT_TRUE(parse_query("&&a=1&&") == (Q{{"a", "1"}}));
    ZTH_EXPECT_TRUE(parse_query("a=b=c") == (Q{{"a", "b=c"}}));
    ZTH_EXPECT_TRUE(parse_query("=v") == (Q{{"", "v"}}));
    ZTH_EXPECT_TRUE(parse_query("k=") == (Q{{"k", ""}}));
    ZTH_EXPECT_TRUE(parse_query("%41=%42") == (Q{{"A", "B"}}));
    ZTH_EXPECT_TRUE(parse_query("bad=%zz%4&ok=1") == (Q{{"bad", "%zz%4"}, {"ok", "1"}}));
    ZTH_EXPECT_TRUE(parse_query("n=J%C3%B6rg+M") == (Q{{"n", "J\xC3\xB6rg M"}}));
    ZTH_EXPECT_TRUE(parse_query("").empty());
    ZTH_EXPECT_TRUE(parse_query("a") == (Q{{"a", ""}}));
    ZTH_EXPECT_TRUE(parse_query("k=%") == (Q{{"k", "%"}}));
}
ZTH_TEST("works", "build_query percent-encodes keys and values and writes empty values as key=") {
    using Q = std::vector<std::pair<std::string, std::string>>;
    ZTH_EXPECT_EQ(build_query(Q{{"a", "1"}, {"b c", "x&y"}, {"\xC3\xA9", ""}}), std::string("a=1&b%20c=x%26y&%C3%A9="));
    ZTH_EXPECT_EQ(build_query(Q{}), std::string(""));
    ZTH_EXPECT_EQ(build_query(Q{{"k", "a+b=c"}}), std::string("k=a%2Bb%3Dc"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "percent_decode(percent_encode(x)) == x for random bytes and every `keep` set") {
    zth::Lcg rng(11);
    for (int i = 0; i < 3000; ++i) {
        std::string x(static_cast<std::size_t>(rng.below(60)), '\0');
        for (char& c : x) c = static_cast<char>(rng.below(256));
        const std::string keep = rng.below(2) ? std::string("/:@") : std::string();
        const std::string e = percent_encode(x, keep);
        for (char c : e) {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~' || c == '%' || keep.find(c) != std::string::npos;
            if (!ok) {
                zth::fail("the encoding contains an unsafe character");
                return;
            }
        }
        const auto back = percent_decode(e);
        if (!back || *back != x) {
            zth::fail("a random string did not round-trip (round " + std::to_string(i) + ")");
            return;
        }
    }
}
ZTH_TEST("robust", "to_string(parse(x)) is stable: parsing the output gives the same output") {
    for (const char* in : {"HTTP://User@Example.COM:80/a/b/../c/./d?x=1&y=%7e#Frag", "https://[::1]:8443/", "http://example.com", "ftp://host:21/f", "http://a?q#f", "http://a:0/", "ws://h:80",
                           "http://h/%7e%7E?%aa#%bb", "gopher://h:70/1"}) {
        const std::string once = norm(in);
        if (norm(once.c_str()) != once) {
            zth::fail(std::string("normalising `") + in + "` twice gives a different result: `" + once + "` then `" + norm(once.c_str()) + "`");
            return;
        }
    }
}
ZTH_TEST("robust", "resolving against a resolved URL is consistent: resolve(resolve(base, a), b) with dot segments equals resolve(base, a/b)") {
    const auto base = parse("http://h/x/y/z");
    if (!base) return;
    const auto mid = resolve(*base, "../k/");
    ZTH_EXPECT_TRUE(mid.has_value());
    if (!mid) return;
    ZTH_EXPECT_EQ(res(*mid, "m/n"), res(*base, "../k/m/n"));
    ZTH_EXPECT_EQ(res(*mid, "../m"), res(*base, "../k/../m"));
    ZTH_EXPECT_EQ(res(*mid, "../../../../m"), res(*base, "/m"));
}
ZTH_TEST("robust", "garbage never crashes any function: 20000 random strings through everything") {
    zth::Lcg rng(5);
    const std::string alphabet = "hHtTpP:/?#[]@%aA0 9fF.-_~&=+\x01\xFF";
    const auto base = parse("http://a/b/c/d;p?q");
    for (int round = 0; round < 20000; ++round) {
        std::string s(static_cast<std::size_t>(rng.below(40)), 'x');
        for (char& c : s) c = alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))];
        (void)percent_decode(s);
        (void)percent_encode(s, "/");
        const std::string d = remove_dot_segments(s);
        if (d.size() > s.size()) {
            zth::fail("remove_dot_segments made the path longer");
            return;
        }
        if (const auto u = parse(s)) {
            const std::string out = to_string(*u);
            if (!parse(out)) {
                zth::fail("`" + s + "` parsed, but its serialisation `" + out + "` does not");
                return;
            }
        }
        if (base) (void)resolve(*base, s);
        (void)parse_query(s);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "remove_dot_segments is linear: 300000 segments and 300000 `..` in a row", 20) {
    std::string path;
    for (int i = 0; i < 300000; ++i) path += "/seg";
    const std::string deep = path;
    for (int i = 0; i < 300000; ++i) path += "/..";
    path += "/end";
    ZTH_EXPECT_EQ(remove_dot_segments(path), std::string("/end"));
    ZTH_EXPECT_EQ(remove_dot_segments(deep).size(), deep.size());
}
ZTH_TEST_TIMEOUT("fast", "percent encoding and decoding of 8 MB", 20) {
    std::string raw(8 * 1024 * 1024, '\0');
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<char>(i * 131 + 7);
    const std::string e = percent_encode(raw);
    const auto back = percent_decode(e);
    ZTH_EXPECT_TRUE(back.has_value() && *back == raw);
}
ZTH_TEST_TIMEOUT("fast", "parsing a URL with a 2 MB path and a 1 MB query is linear", 20) {
    const std::string url = "http://example.com/" + std::string(2 * 1024 * 1024, 'p') + "?" + std::string(1024 * 1024, 'q') + "#frag";
    const auto u = parse(url);
    ZTH_EXPECT_TRUE(u.has_value() && u->path.size() == 2 * 1024 * 1024 + 1 && u->query->size() == 1024 * 1024);
    ZTH_EXPECT_EQ(to_string(*u).size(), url.size());
}
ZTH_TEST("fast", "percent_decode of a string without escapes allocates one result and nothing else") {
    const std::string text(1000, 'a');
    zth::mem::Scope scope;
    const auto r = percent_decode(text);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 1);
    ZTH_EXPECT_TRUE(r.has_value());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("urlkit.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("urlkit.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a hand-written parser: no regex, no number-parsing library calls, no streams") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("urlkit.hpp"));
    for (const char* word : {"regex", "regex_match", "regex_search", "sscanf", "strtol", "strtoul", "atoi", "stoi", "stoul", "from_chars", "istringstream", "stringstream", "strtok", "iostream"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the inputs are string_views") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("urlkit.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
