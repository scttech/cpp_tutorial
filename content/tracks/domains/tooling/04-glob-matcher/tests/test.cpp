#define ZTH_TRACK_ALLOCATIONS
#include <chrono>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "glob.hpp"
#include "zth_test.hpp"

using glob::match;
using glob::Options;

namespace {

Options dot() {
    Options o;
    o.dot = true;
    return o;
}
Options nocase() {
    Options o;
    o.ignore_case = true;
    return o;
}

// A second, independent matcher for the RANDOM tests: the pattern is built as an AST and evaluated by computing, for each node, the set of path positions reachable from a set of start positions.
struct Node {
    enum Kind { Lit, Any, Set, Star, Alt, SegLoop, PathStar, PathPlus } kind;
    unsigned char c = 0;
    std::string set;                       // members of a (positive) set
    bool negate = false;
    std::vector<std::vector<Node>> alts;   // for Alt
    Node(Kind k = Lit) : kind(k) {}
};
using Seq = std::vector<Node>;

std::string render(const Seq& s);
std::string render_node(const Node& n) {
    switch (n.kind) {
        case Node::Lit: return std::string(1, static_cast<char>(n.c));
        case Node::Any: return "?";
        case Node::Set: return "[" + std::string(n.negate ? "!" : "") + n.set + "]";
        case Node::Star: return "*";
        case Node::Alt: {
            std::string out = "{";
            for (std::size_t i = 0; i < n.alts.size(); ++i) out += (i ? "," : "") + render(n.alts[i]);
            return out + "}";
        }
        case Node::SegLoop: return "**/";
        case Node::PathStar: return "**";
        case Node::PathPlus: return "**";
    }
    return "";
}
std::string render(const Seq& s) {
    std::string out;
    for (const Node& n : s) out += render_node(n);
    return out;
}

struct Eval {
    std::string_view s;
    bool dot;
    bool seg_start(std::size_t i) const { return i == 0 || s[i - 1] == '/'; }
    bool blocked(std::size_t i) const { return !dot && i < s.size() && s[i] == '.' && seg_start(i); }
    std::set<std::size_t> seq(const Seq& q, std::set<std::size_t> from) const {
        for (const Node& n : q) from = node(n, from);
        return from;
    }
    std::set<std::size_t> node(const Node& n, const std::set<std::size_t>& from) const {
        std::set<std::size_t> out;
        switch (n.kind) {
            case Node::Lit:
                for (std::size_t i : from)
                    if (i < s.size() && static_cast<unsigned char>(s[i]) == n.c) out.insert(i + 1);
                break;
            case Node::Any:
                for (std::size_t i : from)
                    if (i < s.size() && s[i] != '/' && !blocked(i)) out.insert(i + 1);
                break;
            case Node::Set:
                for (std::size_t i : from) {
                    if (i >= s.size() || s[i] == '/' || blocked(i)) continue;
                    const bool in = n.set.find(s[i]) != std::string::npos;
                    if (in != n.negate) out.insert(i + 1);
                }
                break;
            case Node::Star:
                for (std::size_t i : from) {
                    if (blocked(i)) continue;
                    out.insert(i);
                    for (std::size_t j = i; j < s.size() && s[j] != '/'; ++j) out.insert(j + 1);
                }
                break;
            case Node::Alt:
                for (const Seq& a : n.alts)
                    for (std::size_t p : seq(a, from)) out.insert(p);
                break;
            case Node::SegLoop: {   // zero or more "segment/" (a segment may not start with '.' unless dot)
                std::set<std::size_t> frontier = from;
                out = from;
                while (!frontier.empty()) {
                    std::set<std::size_t> fresh;
                    for (std::size_t i : frontier) {
                        if (blocked(i)) continue;
                        for (std::size_t j = i; j <= s.size(); ++j) {
                            if (j < s.size() && s[j] == '/') {
                                if (out.insert(j + 1).second) fresh.insert(j + 1);
                                break;
                            }
                            if (j == s.size()) break;
                        }
                        // segments may be empty or contain no '/': only the NEXT '/' ends the segment, but a loop iteration may also stop at any later '/' (a longer segment is not allowed: it has no '/')
                    }
                    frontier = fresh;
                }
                break;
            }
            case Node::PathStar:   // everything: zero or more characters of any kind
                for (std::size_t i : from) {
                    out.insert(i);
                    for (std::size_t j = i; j < s.size() && !blocked(j); ++j) out.insert(j + 1);
                }
                break;
            case Node::PathPlus:   // one or more characters of any kind
                for (std::size_t i : from)
                    for (std::size_t j = i; j < s.size() && !blocked(j); ++j) out.insert(j + 1);
                break;
        }
        return out;
    }
    bool matches(const Seq& q) const {
        const auto ends = seq(q, {0});
        return ends.count(s.size()) != 0;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "literals, '?', '*': the whole path must match; wildcards stop at '/'") {
    ZTH_EXPECT_TRUE(match("abc", "abc") && !match("abc", "abcd") && !match("abc", "ab") && !match("abc", "xabc"));
    ZTH_EXPECT_TRUE(match("", "") && !match("", "a") && !match("a", ""));
    ZTH_EXPECT_TRUE(match("a?c", "abc") && match("a?c", "a.c") && !match("a?c", "ac") && !match("a?c", "abbc") && !match("a?c", "a/c"));
    ZTH_EXPECT_TRUE(match("*", "") && match("*", "anything") && !match("*", "a/b"));
    ZTH_EXPECT_TRUE(match("*.cpp", "main.cpp") && match("*.cpp", ".cpp") == false && match("*.cpp", "a.b.cpp") && !match("*.cpp", "main.cpp.bak") && !match("*.cpp", "src/main.cpp"));
    ZTH_EXPECT_TRUE(match("a*c", "ac") && match("a*c", "abbbc") && match("a*c", "acac") && !match("a*c", "acb"));
    ZTH_EXPECT_TRUE(match("*a*b*", "xaxbx") && match("*a*b*", "ab") && !match("*a*b*", "ba"));
    ZTH_EXPECT_TRUE(match("src/*/x.h", "src/a/x.h") && !match("src/*/x.h", "src/a/b/x.h") && match("src/*/x.h", "src//x.h"));   // an empty segment is a segment
    ZTH_EXPECT_TRUE(match("***", "abc") && match("a***b", "axxb") && !match("a***b", "a/b"));
    ZTH_EXPECT_TRUE(match("/*", "/etc") && !match("/*", "etc") && match("*/", "dir/") && !match("*/", "dir"));
    ZTH_EXPECT_TRUE(match("?", "x") && !match("?", "") && !match("?", "xy") && !match("?", "/"));
}
ZTH_TEST("works", "'?' and sets match single bytes: a two-byte UTF-8 character needs two question marks") {
    ZTH_EXPECT_TRUE(match("caf??", "caf\xC3\xA9") && !match("caf?", "caf\xC3\xA9"));
    ZTH_EXPECT_TRUE(match("caf\xC3\xA9", "caf\xC3\xA9") && match("*\xC3\xA9", "caf\xC3\xA9"));
    ZTH_EXPECT_TRUE(match("[\x80-\xFF]", "\xE9") && !match("[\x80-\xFF]", "e"));
}
ZTH_TEST("works", "character sets: members, ranges, negation, ']' first, '-' at the ends, never '/'") {
    ZTH_EXPECT_TRUE(match("[abc]", "b") && !match("[abc]", "d") && !match("[abc]", "") && !match("[abc]", "bb"));
    ZTH_EXPECT_TRUE(match("[a-f]", "c") && match("[a-f]", "a") && match("[a-f]", "f") && !match("[a-f]", "g") && !match("[a-f]", "`"));
    ZTH_EXPECT_TRUE(match("[0-9a-fA-F]*", "DeadBeef") && !match("[0-9a-fA-F]", "g"));
    ZTH_EXPECT_TRUE(match("[!abc]", "d") && !match("[!abc]", "a") && match("[^abc]", "d") && !match("[^abc]", "c"));
    ZTH_EXPECT_TRUE(match("[!a-c]x", "dx") && !match("[!a-c]x", "bx"));
    ZTH_EXPECT_TRUE(match("[]a]", "]") && match("[]a]", "a") && !match("[]a]", "b"));
    ZTH_EXPECT_TRUE(match("[!]a]", "b") && !match("[!]a]", "]") && !match("[!]a]", "a"));
    ZTH_EXPECT_TRUE(match("[-a]", "-") && match("[a-]", "-") && match("[-a]", "a") && !match("[-a]", "b"));
    ZTH_EXPECT_TRUE(match("[a\\]b]", "]") && match("[a\\-z]", "-") && !match("[a\\-z]", "m"));   // escapes inside a set
    ZTH_EXPECT_TRUE(match("[*?]", "*") && match("[*?]", "?") && !match("[*?]", "a") && match("[{,}]", ","));   // metacharacters are plain inside a set
    ZTH_EXPECT_FALSE(match("[/]", "/"));
    ZTH_EXPECT_FALSE(match("[!a]", "/"));
    ZTH_EXPECT_FALSE(match("a[/]b", "a/b"));
    ZTH_EXPECT_FALSE(match("a[!x]b", "a/b"));
    ZTH_EXPECT_TRUE(match("[a-a]", "a") && !match("[a-a]", "b"));
    ZTH_EXPECT_TRUE(match("[\\]]", "]") && match("[\\\\]", "\\"));
}
ZTH_TEST("works", "escapes make metacharacters literal") {
    ZTH_EXPECT_TRUE(match("\\*", "*") && !match("\\*", "a") && match("a\\*b", "a*b") && !match("a\\*b", "axb"));
    ZTH_EXPECT_TRUE(match("\\?", "?") && match("\\[x\\]", "[x]") && match("\\{a,b\\}", "{a,b}") && match("\\\\", "\\"));
    ZTH_EXPECT_TRUE(match("a\\bc", "abc") && match("\\/", "/"));   // an escaped ordinary character is itself
    ZTH_EXPECT_TRUE(match("\\**", "*abc") && !match("\\**", "abc"));
}
ZTH_TEST("works", "braces: alternatives, nesting, empty alternatives, wildcards inside, and ordinary commas and braces outside") {
    ZTH_EXPECT_TRUE(match("*.{cpp,hpp}", "a.cpp") && match("*.{cpp,hpp}", "a.hpp") && !match("*.{cpp,hpp}", "a.h") && !match("*.{cpp,hpp}", "a."));
    ZTH_EXPECT_TRUE(match("{a,b}{c,d}", "ad") && match("{a,b}{c,d}", "bc") && !match("{a,b}{c,d}", "ab") && !match("{a,b}{c,d}", "a"));
    ZTH_EXPECT_TRUE(match("x{a,b{c,d},e}y", "xay") && match("x{a,b{c,d},e}y", "xbcy") && match("x{a,b{c,d},e}y", "xbdy") && match("x{a,b{c,d},e}y", "xey") && !match("x{a,b{c,d},e}y", "xby"));
    ZTH_EXPECT_TRUE(match("a{,b}", "a") && match("a{,b}", "ab") && !match("a{,b}", "abb") && match("{,}", "") && match("{}", ""));
    ZTH_EXPECT_TRUE(match("{*.c,*.h}", "x.h") && match("{src,lib}/**/*.cpp", "lib/a/b.cpp") && !match("{src,lib}/**/*.cpp", "bin/a/b.cpp"));
    ZTH_EXPECT_TRUE(match("{a*,b?}", "abcd") && match("{a*,b?}", "bx") && !match("{a*,b?}", "bxy"));
    ZTH_EXPECT_TRUE(match("a,b", "a,b") && match("a}b", "a}b") && !match("a}b", "ab"));
    ZTH_EXPECT_TRUE(match("{[ab],[cd]x}", "a") && match("{[ab],[cd]x}", "dx") && !match("{[ab],[cd]x}", "c"));
    ZTH_EXPECT_TRUE(match("{a/b,c}", "a/b") && match("{a/b,c}", "c") && !match("{a/b,c}", "a"));
    ZTH_EXPECT_TRUE(match("{a\\,b,c}", "a,b") && match("{a\\,b,c}", "c") && !match("{a\\,b,c}", "a"));
}
ZTH_TEST("works", "'**' as a whole segment: at the start, in the middle, at the end, alone") {
    ZTH_EXPECT_TRUE(match("**", "") && match("**", "a") && match("**", "a/b/c") && match("**", "/") );
    ZTH_EXPECT_TRUE(match("**/x", "x") && match("**/x", "d/x") && match("**/x", "d/e/x") && !match("**/x", "dx") && !match("**/x", "d/xy") && !match("**/x", "x/y"));
    ZTH_EXPECT_TRUE(match("**/*.cpp", "main.cpp") && match("**/*.cpp", "src/main.cpp") && match("**/*.cpp", "a/b/c/main.cpp") && !match("**/*.cpp", "main.cpp/x"));
    ZTH_EXPECT_TRUE(match("a/**/x", "a/x") && match("a/**/x", "a/d/x") && match("a/**/x", "a/d/e/x") && !match("a/**/x", "ax") && !match("a/**/x", "b/x") && !match("a/**/x", "a/d/y"));
    ZTH_EXPECT_TRUE(match("a/**/b/**/c", "a/b/c") && match("a/**/b/**/c", "a/1/b/2/3/c") && !match("a/**/b/**/c", "a/b"));
    ZTH_EXPECT_TRUE(match("a/**", "a/x") && match("a/**", "a/d/x") && match("a/**", "a/d/e/") && !match("a/**", "a") && !match("a/**", "a/") && !match("a/**", "b/x"));
    ZTH_EXPECT_TRUE(match("**/", "d/") && match("**/", "") && match("**/", "a/b/") && !match("**/", "a"));
    ZTH_EXPECT_TRUE(match("**/**", "a") && match("**/**", "a/b/c"));
}
ZTH_TEST("works", "'**' that is not a whole segment behaves like '*'") {
    ZTH_EXPECT_TRUE(match("a**b", "axxb") && !match("a**b", "a/b") && !match("a**b", "ax/xb"));
    ZTH_EXPECT_TRUE(match("**x", "abx") && !match("**x", "a/x"));
    ZTH_EXPECT_TRUE(match("x**", "xyz") && !match("x**", "x/y"));
    ZTH_EXPECT_TRUE(match("a/**b", "a/xb") && !match("a/**b", "a/x/b"));
    ZTH_EXPECT_TRUE(match("a/b**/c", "a/bxx/c") && !match("a/b**/c", "a/bx/y/c"));
}
ZTH_TEST("works", "dotfiles: wildcards do not match a leading '.' of a path segment unless the pattern says so or dot is on") {
    ZTH_EXPECT_FALSE(match("*", ".git"));
    ZTH_EXPECT_FALSE(match("?git", ".git"));
    ZTH_EXPECT_FALSE(match("[.]git", ".git"));
    ZTH_EXPECT_FALSE(match("[!x]git", ".git"));
    ZTH_EXPECT_TRUE(match(".*", ".git") && match(".git", ".git") && match(".?it", ".git") && match("\\.git", ".git"));
    ZTH_EXPECT_FALSE(match("*.cpp", ".hidden.cpp"));
    ZTH_EXPECT_TRUE(match("*", "a.b") && match("a*", "a.b") && match("*.*", "a.b") && match("a?b", "a.b") && match("*.cpp", "x.cpp"));
    ZTH_EXPECT_FALSE(match("src/*", "src/.hidden"));
    ZTH_EXPECT_TRUE(match("src/.*", "src/.hidden") && match("src/.hidden", "src/.hidden"));
    ZTH_EXPECT_FALSE(match("*/x", ".d/x"));
    ZTH_EXPECT_FALSE(match("**/x", ".d/x"));
    ZTH_EXPECT_FALSE(match("**/x", "a/.d/x"));
    ZTH_EXPECT_TRUE(match("**/x", "a/b/x") && match(".d/**/x", ".d/a/x") && match("**/.git", ".git") && match("**/.git", "a/b/.git"));
    ZTH_EXPECT_FALSE(match("a/**", "a/.hidden"));
    ZTH_EXPECT_FALSE(match("**", ".hidden"));
    ZTH_EXPECT_FALSE(match("**", "a/.hidden/b"));
    ZTH_EXPECT_TRUE(match("a/**", "a/b.c") && match("**", "a.b/c.d"));
    ZTH_EXPECT_FALSE(match("{*,b}", ".a"));
    ZTH_EXPECT_TRUE(match("{.*,b}", ".a"));
    ZTH_EXPECT_TRUE(match("*", ".git", dot()) && match("?git", ".git", dot()) && match("[!x]git", ".git", dot()) && match("**/x", ".d/a/.e/x", dot()) && match("**", ".a/.b", dot()) && match("a/**", "a/.h", dot()));
    ZTH_EXPECT_FALSE(match("*", "a/.git", dot()));   // dot never lets a wildcard cross a '/'
    ZTH_EXPECT_TRUE(match("*", ".", dot()) && !match("*", "."));
}
ZTH_TEST("works", "ignore_case: literals and sets, ASCII only") {
    ZTH_EXPECT_TRUE(match("readme.MD", "README.md", nocase()) && match("*.CPP", "Main.cpp", nocase()) && match("[a-c]", "B", nocase()) && match("[A-C]", "b", nocase()) && !match("[a-c]", "D", nocase()));
    ZTH_EXPECT_TRUE(match("[!a]", "b", nocase()) && !match("[!a]", "A", nocase()) && !match("[!a-c]", "B", nocase()));
    ZTH_EXPECT_FALSE(match("README", "readme"));
    ZTH_EXPECT_FALSE(match("[a-c]", "B"));
    ZTH_EXPECT_TRUE(match("\xC3\xA9", "\xC3\xA9", nocase()) && !match("\xC3\xA9", "\xC3\x89", nocase()));   // only ASCII letters fold
    ZTH_EXPECT_FALSE(match("\xC3", "\xE3", nocase()));   // bytes above 0x7F never fold, even when they differ only in the bit that folds ASCII letters
    ZTH_EXPECT_FALSE(match("\xE3", "\xC3", nocase()));
    ZTH_EXPECT_FALSE(match("[\xC3]", "\xE3", nocase()));
    ZTH_EXPECT_TRUE(match("A{b,C}d", "aBD", nocase()) && match("**/X", "a/b/x", nocase()));
    ZTH_EXPECT_TRUE(match("[_]", "_", nocase()) && match("[@-Z]", "A", nocase()));   // '@'..'Z' also contains 'A'..'Z'
}
ZTH_TEST("works", "check() accepts valid patterns and reports the position of the first error") {
    for (const char* ok : {"", "*", "a?b[c-d]{e,f}**", "[]a]", "[!]a]", "\\*", "{,}", "}", ",", "a,b}", "[a-]", "[-a]", "x/**/y", "{a,{b,{c,d}}}"}) ZTH_EXPECT_FALSE(glob::check(ok).has_value());
    struct Bad { const char* pattern; std::size_t at; };
    const Bad bad[] = {{"[abc", 0}, {"ab[cd", 2}, {"[", 0}, {"[]", 0}, {"[!]", 0}, {"[a-", 0}, {"x[\\", 1}, {"{a,b", 0}, {"a{b", 1}, {"{", 0}, {"{a,{b,c}", 0}, {"a\\", 1}, {"\\", 0}, {"[z-a]", 1}, {"x[9-0]", 2}, {"[a-c-b]x[z-a]", 9}, {"[a\\", 0}};
    for (const Bad& b : bad) {
        const auto e = glob::check(b.pattern);
        ZTH_EXPECT_TRUE(e.has_value());
        if (e) {
            ZTH_EXPECT_EQ(e->position, b.at);
            ZTH_EXPECT_FALSE(e->message.empty());
        }
        ZTH_EXPECT_FALSE(glob::Glob::compile(b.pattern).has_value());
        ZTH_EXPECT_FALSE(match(b.pattern, ""));
        ZTH_EXPECT_FALSE(match(b.pattern, b.pattern));
    }
}
ZTH_TEST("works", "brace nesting is limited to 32 levels; deeper patterns are errors, not stack overflows") {
    std::string ok, deep;
    for (int i = 0; i < 32; ++i) ok += "{a,";
    ok += "x";
    for (int i = 0; i < 32; ++i) ok += "}";
    ZTH_EXPECT_FALSE(glob::check(ok).has_value());
    ZTH_EXPECT_TRUE(match(ok, "x") && match(ok, "a") && !match(ok, "b"));
    for (int i = 0; i < 33; ++i) deep += "{a,";
    deep += "x";
    for (int i = 0; i < 33; ++i) deep += "}";
    const auto e = glob::check(deep);
    ZTH_EXPECT_TRUE(e.has_value() && e->position == 96);   // the 33rd '{'
    std::string huge(1000000, '{');
    const auto e2 = glob::check(huge);
    ZTH_EXPECT_TRUE(e2.has_value());
    ZTH_EXPECT_FALSE(match(huge, "x"));
}
ZTH_TEST("works", "compile once, match many; the Glob object can be reused and copied") {
    const auto g = glob::Glob::compile("src/**/*.{cpp,hpp}");
    ZTH_EXPECT_TRUE(g.has_value());
    int hits = 0;
    for (const char* p : {"src/a.cpp", "src/x/y/z.hpp", "src/a.h", "lib/a.cpp", "src/.hid/a.cpp", "src/a.cpp/b"}) hits += g->matches(p);
    ZTH_EXPECT_EQ(hits, 2);
    const glob::Glob copy = *g;
    ZTH_EXPECT_TRUE(copy.matches("src/q.hpp") && g->matches("src/q.hpp"));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "catastrophic patterns: nested stars, alternations and sets against long near-matches finish at once") {
    const auto start = std::chrono::steady_clock::now();
    std::string stars;
    for (int i = 0; i < 40; ++i) stars += "*a";
    ZTH_EXPECT_FALSE(match(stars + "b", std::string(300, 'a')));
    ZTH_EXPECT_TRUE(match(stars, std::string(300, 'a')));
    std::string alts;
    for (int i = 0; i < 30; ++i) alts += "{a,aa,aaa}";
    ZTH_EXPECT_FALSE(match(alts + "b", std::string(200, 'a')));
    ZTH_EXPECT_TRUE(match(alts, std::string(60, 'a')));
    ZTH_EXPECT_FALSE(match(alts, std::string(91, 'a')));
    std::string mixed;
    for (int i = 0; i < 25; ++i) mixed += "*?";
    ZTH_EXPECT_FALSE(match(mixed + "z", std::string(400, 'a')));
    std::string segs;
    for (int i = 0; i < 20; ++i) segs += "**/";
    std::string path;
    for (int i = 0; i < 300; ++i) path += "d/";
    ZTH_EXPECT_FALSE(match(segs + "x", path + "y"));
    ZTH_EXPECT_TRUE(match(segs + "x", path + "x"));
    std::string classes;
    for (int i = 0; i < 40; ++i) classes += "[ab]*";
    ZTH_EXPECT_FALSE(match(classes + "c", std::string(500, 'a')));
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    ZTH_EXPECT_TRUE(ms < 3000);
}
ZTH_TEST_TIMEOUT("robust", "a million-character pattern and a million-character path: no recursion on either, bounded time", 10) {
    ZTH_EXPECT_TRUE(match("*a*b", std::string(1000000, 'a') + "b"));
    ZTH_EXPECT_FALSE(match(std::string(1000000, 'a'), std::string(999999, 'a')));
    ZTH_EXPECT_TRUE(match(std::string(1000000, 'a'), std::string(1000000, 'a')));
    ZTH_EXPECT_TRUE(match("a*b", "a" + std::string(1000000, 'x') + "b"));
    ZTH_EXPECT_FALSE(match(std::string(200000, '*') + "x", std::string(5000, 'y')));   // a run of stars is one star, not 200000 nested loops
    ZTH_EXPECT_TRUE(match(std::string(200000, '*') + "x", std::string(5000, 'y') + "x"));
    std::string pattern;
    for (int i = 0; i < 3000; ++i) pattern += "a*";
    pattern += "b";
    const auto g = glob::Glob::compile(pattern);
    ZTH_EXPECT_TRUE(g.has_value());
    ZTH_EXPECT_FALSE(g->matches(std::string(1000, 'a') + "b"));   // 3000 'a's are needed
    ZTH_EXPECT_TRUE(g->matches(std::string(3000, 'a') + "b"));
    ZTH_EXPECT_TRUE(match("**/" + std::string(1000, 'x'), std::string(500000, 'd') + "/" + std::string(1000, 'x')));
    ZTH_EXPECT_FALSE(match("**/" + std::string(1000, 'x'), std::string(500000, 'd') + "/" + std::string(999, 'x')));
}
ZTH_TEST("robust", "random patterns (built as syntax trees) against random paths, evaluated by an independent set-of-positions interpreter, with and without the dot option") {
    zth::Lcg rng(2024);
    const std::string letters = "ab.";
    auto lit = [&]() { Node n{Node::Lit}; n.c = static_cast<unsigned char>(letters[static_cast<std::size_t>(rng.below(3))]); return n; };
    std::function<Seq(int, bool)> gen_seq;   // `inner`: inside braces (no ** forms, no '/')
    std::function<Node(int)> gen_item = [&](int depth) -> Node {
        switch (rng.below(depth > 0 ? 8 : 7)) {
            case 0: case 1: return lit();
            case 2: return Node{Node::Any};
            case 3: case 4: return Node{Node::Star};
            case 5: { Node n{Node::Set}; n.negate = rng.below(3) == 0; n.set = rng.below(2) ? "ab" : "a."; return n; }
            case 6: { Node n{Node::Lit}; n.c = '/'; return n; }
            default: { Node n{Node::Alt}; for (int k = 0, m = 1 + rng.below(3); k < m; ++k) n.alts.push_back(gen_seq(depth - 1, true)); return n; }
        }
    };
    gen_seq = [&](int depth, bool inner) {
        Seq s;
        for (int k = 0, m = rng.below(4); k < m; ++k) {
            Node n = gen_item(inner ? depth : 1);
            // keep '*' runs and '**' ambiguities out: never put two stars side by side, never a star next to a '/' that could form **/
            if (n.kind == Node::Star && !s.empty() && s.back().kind == Node::Star) continue;
            s.push_back(n);
        }
        return s;
    };
    int matched = 0, total = 0;
    for (int round = 0; round < 4000; ++round) {
        Seq pattern;
        // optional double-star segments at the front, in the middle (after a '/'), or at the end
        const int form = rng.below(5);
        if (form == 1) pattern.push_back(Node{Node::SegLoop});
        pattern = [&] { Seq p = pattern; for (const Node& n : gen_seq(2, false)) p.push_back(n); return p; }();
        if (form == 2) { Node slash{Node::Lit}; slash.c = '/'; pattern.push_back(slash); pattern.push_back(Node{Node::SegLoop}); for (const Node& n : gen_seq(1, false)) pattern.push_back(n); }
        if (form == 3) { Node slash{Node::Lit}; slash.c = '/'; pattern.push_back(slash); pattern.push_back(Node{Node::PathPlus}); }
        if (form == 4) pattern = Seq{Node{Node::PathStar}};
        std::string text = render(pattern);
        // a pattern may contain accidental syntax; skip those that the library rejects on purpose (a '**' next to a star, say)
        if (glob::check(text).has_value()) continue;
        // a '*' directly before "**/" or after "/**" changes the meaning of '**': regenerate
        if (text.find("***") != std::string::npos) continue;
        // a "**" that was rendered from a star next to a PathPlus etc. is fine; but "*" followed directly by a SegLoop makes "***/" -> skip handled above
        for (int p = 0; p < 6; ++p) {
            std::string path;
            for (int k = 0, n = rng.below(9); k < n; ++k) path.push_back("ab./"[rng.below(4)]);
            for (bool d : {false, true}) {
                Options o;
                o.dot = d;
                Eval ev{path, d};
                const bool want = ev.matches(pattern);
                const bool got = match(text, path, o);
                ++total;
                matched += want;
                if (want != got) {
                    zth::fail("pattern \"" + text + "\" vs path \"" + path + "\" with dot=" + (d ? "true" : "false") + ": the interpreter says " + (want ? "match" : "no match") + ", the library says " + (got ? "match" : "no match"));
                    return;
                }
            }
        }
    }
    ZTH_EXPECT_TRUE(total > 10000 && matched > 500);
}
ZTH_TEST("robust", "garbage patterns and garbage paths never crash or hang; invalid ones match nothing") {
    zth::Lcg rng(9);
    const std::string alphabet = "ab*?[]{}\\,!^-/.\xC3\xA9";
    for (int round = 0; round < 20000; ++round) {
        std::string pattern, path;
        for (int i = 0, n = rng.below(14); i < n; ++i) pattern.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
        for (int i = 0, n = rng.below(14); i < n; ++i) path.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
        const bool valid = !glob::check(pattern).has_value();
        const auto g = glob::Glob::compile(pattern);
        ZTH_EXPECT_EQ(g.has_value(), valid);
        if (!valid) ZTH_EXPECT_FALSE(match(pattern, path));
        else ZTH_EXPECT_EQ(match(pattern, path), g->matches(path));
        if (valid) (void)match(pattern, path, dot());
    }
}
ZTH_TEST("robust", "a literal pattern matches exactly itself; escaping every character makes any string a literal pattern") {
    zth::Lcg rng(5);
    const std::string alphabet = "ab*?[]{}\\,!^-/.";
    for (int round = 0; round < 2000; ++round) {
        std::string text;
        for (int i = 0, n = rng.below(12); i < n; ++i) text.push_back(alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))]);
        std::string escaped;
        for (char c : text) {
            escaped.push_back('\\');
            escaped.push_back(c);
        }
        ZTH_EXPECT_TRUE(match(escaped, text, dot()));
        std::string other = text;
        if (!other.empty()) other.back() = other.back() == 'x' ? 'y' : 'x';
        else other = "x";
        ZTH_EXPECT_FALSE(match(escaped, other, dot()));
        ZTH_EXPECT_FALSE(match(escaped, text + "x", dot()));
    }
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "matches() allocates a bounded number of times, whatever the length of the path") {
    const auto g = glob::Glob::compile("src/**/{test,spec}_[a-z]*.{cpp,hpp}");
    ZTH_EXPECT_TRUE(g.has_value());
    const std::string short_path = "src/a/test_x.cpp", long_path = "src/" + std::string(5000, 'd') + "/" + std::string(3000, 'e') + "/test_" + std::string(20000, 'x') + ".hpp";
    zth::mem::Scope s1;
    ZTH_EXPECT_TRUE(g->matches(short_path));
    ZTH_EXPECT_ALLOCS_AT_MOST(s1, 16);
    zth::mem::Scope s2;
    ZTH_EXPECT_TRUE(g->matches(long_path));
    ZTH_EXPECT_ALLOCS_AT_MOST(s2, 16);
}
ZTH_TEST_TIMEOUT("fast", "40000 paths against 12 compiled patterns", 20) {
    const char* patterns[] = {"*.cpp", "**/*.hpp", "src/**/test_?.cpp", "{a,b,c}/**", "**/.git/**", "[a-m]*/*.{h,hpp,cpp}", "*", "**", "docs/**/*.md", "a/b/c", "x*y*z", "**/build/**/*.o"};
    std::vector<glob::Glob> globs;
    for (const char* p : patterns) globs.push_back(*glob::Glob::compile(p, dot()));
    std::size_t hits = 0;
    zth::Lcg rng(1);
    for (int i = 0; i < 40000; ++i) {
        std::string path;
        for (int d = 0, n = 1 + rng.below(5); d < n; ++d) {
            if (d) path += '/';
            path += "abcdms.xyz"[rng.below(10)];
            path += std::to_string(rng.below(100));
            if (d == n - 1) path += rng.below(2) ? ".cpp" : ".hpp";
        }
        for (const glob::Glob& g : globs) hits += g.matches(path);
    }
    ZTH_EXPECT_TRUE(hits > 20000);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("glob.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("glob.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a matcher of your own: no regex, fnmatch, glob(), wordexp, std::filesystem, no std::function") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("glob.hpp"));
    for (const char* word : {"regex", "regex_match", "fnmatch", "wordexp", "filesystem", "function", "glob_t", "boost", "wildcard"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
