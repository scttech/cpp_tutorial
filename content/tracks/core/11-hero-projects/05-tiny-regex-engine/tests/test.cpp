#define ZTH_TRACK_ALLOCATIONS
#include <set>
#include <string>
#include <vector>

#include "regex.hpp"
#include "zth_test.hpp"

using namespace rx;

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

std::string shape(const std::string& pattern) {
    ParseResult r = parse(pattern);
    if (!r.ok()) {
        ::zth::fail("parse(" + show(pattern) + ") reported " + name(r.error) + " at " + std::to_string(r.pos) + " (" + r.message + ")");
        return "";
    }
    return describe(*r.ast);
}

void expect_shape(const std::string& pattern, const std::string& want) {
    const std::string got = shape(pattern);
    if (got != want) ::zth::fail("the tree of " + show(pattern) + " is " + got + ", expected " + want);
}

void expect_error(const std::string& pattern, CompileError e, std::size_t pos) {
    const ParseResult r = parse(pattern);
    const std::string want = std::string(name(e)) + " at " + std::to_string(pos);
    const std::string got = r.ok() ? "success (" + describe(*r.ast) + ")" : std::string(name(r.error)) + " at " + std::to_string(r.pos);
    if (want != got) ::zth::fail("parse(" + show(pattern) + "): expected " + want + ", got " + got);
    else if (r.message.empty()) ::zth::fail("parse(" + show(pattern) + "): the error has no message");
    const CompileResult c = compile(pattern);
    if (c.ok() || c.regex.has_value()) ::zth::fail("compile(" + show(pattern) + ") should fail too");
}

Regex must_compile(const std::string& pattern) {
    CompileResult c = compile(pattern);
    if (!c.ok()) {
        ::zth::fail("compile(" + show(pattern) + ") reported " + name(c.error) + " at " + std::to_string(c.pos) + " (" + c.message + ")");
        return Regex(Nfa());
    }
    return std::move(*c.regex);
}

void expect_full(const std::string& pattern, const std::string& text, bool want) {
    const Regex re = must_compile(pattern);
    if (re.full_match(text) != want) ::zth::fail("full_match(" + show(pattern) + ", \"" + show(text) + "\") should be " + (want ? "true" : "false"));
}

void expect_search(const std::string& pattern, const std::string& text, long begin, long end, std::size_t from = 0) {
    const Regex re = must_compile(pattern);
    const auto m = re.search(text, from);
    const std::string want = begin < 0 ? "none" : "[" + std::to_string(begin) + "," + std::to_string(end) + ")";
    const std::string got = m ? "[" + std::to_string(m->begin) + "," + std::to_string(m->end) + ")" : "none";
    if (want != got) ::zth::fail("search(" + show(pattern) + ", \"" + show(text) + "\"" + (from ? ", " + std::to_string(from) : "") + ") gave " + got + ", expected " + want);
}

std::string all_matches(const std::string& pattern, const std::string& text) {
    const Regex re = must_compile(pattern);
    std::string s;
    for (const Match& m : re.find_all(text)) s += "[" + std::to_string(m.begin) + "," + std::to_string(m.end) + ")";
    return s;
}

// ---- an independent meaning of a syntax tree: the set of offsets where a match of the node starting at `pos` can end ----
using Ends = std::set<std::size_t>;
Ends ends(const Node& n, std::string_view text, std::size_t pos) {
    const std::size_t len = text.size();
    if (std::holds_alternative<Empty>(n.v)) return {pos};
    if (const auto* lit = std::get_if<Literal>(&n.v)) return pos < len && static_cast<unsigned char>(text[pos]) == lit->ch ? Ends{pos + 1} : Ends{};
    if (std::holds_alternative<AnyChar>(n.v)) return pos < len && text[pos] != '\n' ? Ends{pos + 1} : Ends{};
    if (const auto* set = std::get_if<CharSet>(&n.v)) return pos < len && set->bits.test(static_cast<unsigned char>(text[pos])) ? Ends{pos + 1} : Ends{};
    if (std::holds_alternative<LineStart>(n.v)) return pos == 0 ? Ends{pos} : Ends{};
    if (std::holds_alternative<LineEnd>(n.v)) return pos == len ? Ends{pos} : Ends{};
    if (const auto* cat = std::get_if<Concat>(&n.v)) {
        Ends cur{pos};
        for (const NodePtr& part : cat->parts) {
            Ends next;
            for (std::size_t p : cur) {
                const Ends e = ends(*part, text, p);
                next.insert(e.begin(), e.end());
            }
            cur = std::move(next);
        }
        return cur;
    }
    if (const auto* alt = std::get_if<Alt>(&n.v)) {
        Ends all;
        for (const NodePtr& o : alt->options) {
            const Ends e = ends(*o, text, pos);
            all.insert(e.begin(), e.end());
        }
        return all;
    }
    const auto& rep = std::get<Repeat>(n.v);
    Ends first = ends(*rep.child, text, pos);
    if (rep.kind == '?') {
        first.insert(pos);
        return first;
    }
    Ends result = first;
    if (rep.kind == '*') result.insert(pos);
    std::vector<std::size_t> frontier(first.begin(), first.end());
    while (!frontier.empty()) {
        const std::size_t p = frontier.back();
        frontier.pop_back();
        for (std::size_t e : ends(*rep.child, text, p)) {
            if (result.insert(e).second) frontier.push_back(e);
        }
    }
    return result;
}

std::optional<Match> reference_search(const Node& ast, std::string_view text, std::size_t from) {
    for (std::size_t s = from; s <= text.size(); ++s) {
        const Ends e = ends(ast, text, s);
        if (!e.empty()) return Match{s, *e.rbegin()};
    }
    return std::nullopt;
}

std::string random_pattern(::zth::Lcg& rng, int depth) {
    switch (rng.below(depth > 3 ? 4 : 10)) {
        case 0: case 1: return std::string(1, "ab"[rng.below(2)]);
        case 2: return rng.below(3) == 0 ? "." : std::string(1, "abc"[rng.below(3)]);
        case 3: {
            static const char* classes[] = {"[ab]", "[^a]", "[a-b]", "[^ab]", "\\w", "\\D", "[\\n]", "^", "$"};
            return classes[rng.below(9)];
        }
        case 4: case 5: return random_pattern(rng, depth + 1) + random_pattern(rng, depth + 1);
        case 6: return "(" + random_pattern(rng, depth + 1) + "|" + random_pattern(rng, depth + 1) + ")";
        case 7: return "(" + random_pattern(rng, depth + 1) + ")*";
        case 8: return "(" + random_pattern(rng, depth + 1) + ")+";
        default: return "(" + random_pattern(rng, depth + 1) + ")?";
    }
}

std::string random_text(::zth::Lcg& rng) {
    std::string t;
    const int n = rng.below(9);
    for (int i = 0; i < n; ++i) t += "aabbc\n"[rng.below(6)];
    return t;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "parse: concatenation, alternation, quantifiers and their precedence") {
    expect_shape("abc", "(cat a b c)");
    expect_shape("a", "a");
    expect_shape("a|b", "(alt a b)");
    expect_shape("a|bc|d", "(alt a (cat b c) d)");
    expect_shape("a*", "(* a)");
    expect_shape("a+", "(+ a)");
    expect_shape("a?", "(? a)");
    expect_shape("ab*", "(cat a (* b))");
    expect_shape("a*b", "(cat (* a) b)");
    expect_shape("ab|cd*", "(alt (cat a b) (cat c (* d)))");
    expect_shape("a.b", "(cat a . b)");
}
ZTH_TEST("works", "parse: groups make no node of their own") {
    expect_shape("(a)", "a");
    expect_shape("((a))", "a");
    expect_shape("(ab)*", "(* (cat a b))");
    expect_shape("(ab)c", "(cat (cat a b) c)");
    expect_shape("a(b|c)d", "(cat a (alt b c) d)");
    expect_shape("(a|b)*", "(* (alt a b))");
    expect_shape("((ab)|c)+", "(+ (alt (cat a b) c))");
}
ZTH_TEST("works", "parse: empty regex and empty alternatives; stacked quantifiers; anchors; classes and escapes") {
    expect_shape("", "()");
    expect_shape("()", "()");
    expect_shape("a|", "(alt a ())");
    expect_shape("|a", "(alt () a)");
    expect_shape("(|a)b", "(cat (alt () a) b)");
    expect_shape("a**", "(* (* a))");
    expect_shape("a+?", "(? (+ a))");
    expect_shape("^a$", "(cat ^ a $)");
    expect_shape("[abc]", "set(3)");
    expect_shape("[a-z]", "set(26)");
    expect_shape("\\d", "set(10)");
    expect_shape("\\.", "\\.");
    expect_shape("\\n", "\\x0a");
    expect_shape("a\\*b", "(cat a \\* b)");
}
ZTH_TEST("works", "full_match: literals, the dot and the three quantifiers") {
    expect_full("abc", "abc", true);
    expect_full("abc", "ab", false);
    expect_full("abc", "abcd", false);
    expect_full("a.c", "abc", true);
    expect_full("a.c", "a\nc", false);
    expect_full("a*", "", true);
    expect_full("a*", "aaa", true);
    expect_full("a*", "aab", false);
    expect_full("a+", "", false);
    expect_full("a+", "a", true);
    expect_full("a+", "aaaa", true);
    expect_full("a?", "", true);
    expect_full("a?", "a", true);
    expect_full("a?", "aa", false);
    expect_full("x*y+z?", "xxyy", true);
    expect_full("x*y+z?", "xxz", false);
    expect_full("colou?r", "color", true);
    expect_full("colou?r", "colour", true);
    expect_full("colou?r", "colouur", false);
}
ZTH_TEST("works", "full_match: alternation and groups") {
    expect_full("a|b", "a", true);
    expect_full("a|b", "b", true);
    expect_full("a|b", "c", false);
    expect_full("a|b", "ab", false);
    expect_full("ab|cd", "cd", true);
    expect_full("ab|cd", "ad", false);
    expect_full("(ab)*", "ababab", true);
    expect_full("(ab)*", "aba", false);
    expect_full("(a|b)*c", "abbac", true);
    expect_full("(a|b)*c", "abbad", false);
    expect_full("a(b|c)d", "acd", true);
    expect_full("(a|b)+", "", false);
    expect_full("((a|b)c)+", "acbcac", true);
    expect_full("(a*)*", "aaa", true);
    expect_full("(a+)+", "aaaa", true);
    expect_full("(a|ab)(c|bcd)", "abcd", true);
}
ZTH_TEST("works", "full_match: the empty regex and empty alternatives") {
    expect_full("", "", true);
    expect_full("", "a", false);
    expect_full("a|", "", true);
    expect_full("a|", "a", true);
    expect_full("a|", "b", false);
    expect_full("(|a)b", "b", true);
    expect_full("(|a)b", "ab", true);
    expect_full("()", "", true);
    expect_full("a()b", "ab", true);
}
ZTH_TEST("works", "classes and escapes") {
    expect_full("[abc]", "b", true);
    expect_full("[abc]", "d", false);
    expect_full("[a-z]+", "hello", true);
    expect_full("[a-z]+", "Hello", false);
    expect_full("[0-9]+", "2024", true);
    expect_full("[^0-9]+", "abc", true);
    expect_full("[^0-9]+", "a1", false);
    expect_full("[a-cx-z]+", "abxyz", true);
    expect_full("[a-cx-z]+", "d", false);
    expect_full("\\d+", "123", true);
    expect_full("\\d+", "12a", false);
    expect_full("\\w+", "ab_9", true);
    expect_full("\\w+", "ab-9", false);
    expect_full("\\s+", " \t\n", true);
    expect_full("\\D", "a", true);
    expect_full("\\D", "5", false);
    expect_full("\\W", "-", true);
    expect_full("\\S+", "ab", true);
    expect_full("\\S", "\n", false);
    expect_full("\\.", ".", true);
    expect_full("\\.", "a", false);
    expect_full("a\\*b", "a*b", true);
    expect_full("\\(x\\)", "(x)", true);
    expect_full("\\[x\\]", "[x]", true);
    expect_full("\\\\", "\\", true);
    expect_full("\\n", "\n", true);
    expect_full("\\t\\r", "\t\r", true);
    expect_full("\\|\\?\\+\\^\\$", "|?+^$", true);
}
ZTH_TEST("works", "search: the leftmost match, and among those the longest") {
    expect_search("b+", "abbbc", 1, 4);
    expect_search("a|ab|abc", "xabcd", 1, 4);
    expect_search("(a|ab)(c|bcd)", "abcd", 0, 4);
    expect_search("abc", "xxabcxx", 2, 5);
    expect_search("abc", "xxabxx", -1, 0);
    expect_search("a*", "baaac", 0, 0);
    expect_search("a+", "baaac", 1, 4);
    expect_search("x*", "", 0, 0);
    expect_search("[0-9]+", "room 101, floor 7", 5, 8);
    expect_search("colou?r", "the colour red", 4, 10);
    expect_search("(ab|a)(bc|c)?", "abc", 0, 3);
    expect_search("a.*c", "xabcabcx", 1, 7);
}
ZTH_TEST("works", "search: from, and anchors ^ and $ (absolute positions)") {
    expect_search("^ab", "abab", 0, 2);
    expect_search("^ab", "abab", -1, 0, 1);
    expect_search("ab$", "abab", 2, 4);
    expect_search("ab$", "abab", 2, 4, 2);
    expect_search("^$", "", 0, 0);
    expect_search("^$", "a", -1, 0);
    expect_search("^a*$", "aaa", 0, 3);
    expect_search("^a*$", "aab", -1, 0);
    expect_search("a", "banana", 1, 2);
    expect_search("a", "banana", 3, 4, 2);
    expect_search("a", "banana", 5, 6, 4);
    expect_search("a", "banana", -1, 0, 6);
    expect_search("x*", "ab", 2, 2, 2);
    expect_search("$", "abc", 3, 3);
    expect_search("^", "abc", 0, 0);
}
ZTH_TEST("works", "find_all: non-overlapping matches, with empty matches the way JavaScript does") {
    ZTH_EXPECT_EQ(all_matches("a", "banana"), std::string("[1,2)[3,4)[5,6)"));
    ZTH_EXPECT_EQ(all_matches("an", "banana"), std::string("[1,3)[3,5)"));
    ZTH_EXPECT_EQ(all_matches("[0-9]+", "a1 22 333"), std::string("[1,2)[3,5)[6,9)"));
    ZTH_EXPECT_EQ(all_matches("a*", "baaac"), std::string("[0,0)[1,4)[4,4)[5,5)"));
    ZTH_EXPECT_EQ(all_matches("x", "abc"), std::string(""));
    ZTH_EXPECT_EQ(all_matches("", "ab"), std::string("[0,0)[1,1)[2,2)"));
    ZTH_EXPECT_EQ(all_matches("^a", "aaa"), std::string("[0,1)"));
    ZTH_EXPECT_EQ(all_matches("a$", "aaa"), std::string("[2,3)"));
    ZTH_EXPECT_EQ(all_matches("aa", "aaaaa"), std::string("[0,2)[2,4)"));
}
ZTH_TEST("works", "compile: the NFA has a start state and is small") {
    const Regex re = must_compile("(ab|c)*d");
    ZTH_EXPECT_TRUE(re.nfa().size() >= 5);
    ZTH_EXPECT_TRUE(re.nfa().size() <= 12);
    ZTH_EXPECT_TRUE(re.nfa().start() < re.nfa().size());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "errors: parentheses (UnmatchedParen: a lone ) at the ), an unclosed ( at the innermost one)") {
    expect_error(")", CompileError::UnmatchedParen, 0);
    expect_error("a)", CompileError::UnmatchedParen, 1);
    expect_error("(a))", CompileError::UnmatchedParen, 3);
    expect_error("a|b)c", CompileError::UnmatchedParen, 3);
    expect_error("(", CompileError::UnmatchedParen, 0);
    expect_error("(a", CompileError::UnmatchedParen, 0);
    expect_error("((a)", CompileError::UnmatchedParen, 0);
    expect_error("(a(b", CompileError::UnmatchedParen, 2);
    expect_error("a(b|c", CompileError::UnmatchedParen, 1);
    expect_error("(a|(b)", CompileError::UnmatchedParen, 0);
}
ZTH_TEST("robust", "errors: a quantifier with nothing to repeat (NothingToRepeat, at the quantifier; also after ^ and $)") {
    expect_error("*", CompileError::NothingToRepeat, 0);
    expect_error("+a", CompileError::NothingToRepeat, 0);
    expect_error("a|*", CompileError::NothingToRepeat, 2);
    expect_error("(*a)", CompileError::NothingToRepeat, 1);
    expect_error("(a|?)", CompileError::NothingToRepeat, 3);
    expect_error("^*", CompileError::NothingToRepeat, 1);
    expect_error("a$+", CompileError::NothingToRepeat, 2);
    expect_error("(^)*", CompileError::NothingToRepeat, 3);
}
ZTH_TEST("robust", "errors: escapes (BadEscape, at the backslash)") {
    expect_error("\\", CompileError::BadEscape, 0);
    expect_error("ab\\", CompileError::BadEscape, 2);
    expect_error("\\a", CompileError::BadEscape, 0);
    expect_error("x\\-", CompileError::BadEscape, 1);
    expect_error("\\{", CompileError::BadEscape, 0);
    expect_error("a\\xb", CompileError::BadEscape, 1);
    expect_error("(\\q)", CompileError::BadEscape, 1);
}
ZTH_TEST("robust", "errors: classes (BadClass, at the [ or at the start of a bad range; escapes inside a class: BadEscape)") {
    expect_error("[", CompileError::BadClass, 0);
    expect_error("[abc", CompileError::BadClass, 0);
    expect_error("x[a-", CompileError::BadClass, 1);
    expect_error("[]", CompileError::BadClass, 0);
    expect_error("[^]", CompileError::BadClass, 0);
    expect_error("a[]b]", CompileError::BadClass, 1);
    expect_error("[z-a]", CompileError::BadClass, 1);
    expect_error("x[a-cz-m]", CompileError::BadClass, 5);
    expect_error("[a-\\d]", CompileError::BadClass, 1);
    expect_error("[\\q]", CompileError::BadEscape, 1);
    expect_error("[abc\\", CompileError::BadEscape, 4);
}
ZTH_TEST("robust", "classes: dash rules, escapes inside, negation matches newline") {
    expect_full("[a-]", "-", true);
    expect_full("[a-]", "a", true);
    expect_full("[a-]", "b", false);
    expect_full("[-a]", "-", true);
    expect_full("[a-c-e]", "-", true);
    expect_full("[a-c-e]", "e", true);
    expect_full("[a-c-e]", "d", false);
    expect_full("[--/]", ".", true);
    expect_full("[\\]]", "]", true);
    expect_full("[\\\\]", "\\", true);
    expect_full("[\\-]", "-", true);
    expect_full("[\\^a]", "^", true);
    expect_full("[a^]", "^", true);
    expect_full("[\\d_]+", "12_3", true);
    expect_full("[\\d_]+", "12a", false);
    expect_full("[^\\d]", "x", true);
    expect_full("[^\\d]", "7", false);
    expect_full("[\\n\\t]+", "\n\t", true);
    expect_full("[.*+?]+", ".*+?", true);
    expect_full("[()|]+", "(|)", true);
    expect_full("[^a]", "\n", true);
    expect_full("[\\w.]+", "a.b_c", true);
    expect_full("[a-zA-Z0-9_]+", "Hello_World9", true);
    expect_full("[\\D]", "x", true);
    expect_full("[\\S ]+", "a b", true);
}
ZTH_TEST("robust", "ordinary characters that are not special: ] { } - and the like are literals") {
    expect_full("]", "]", true);
    expect_full("a{2}", "a{2}", true);
    expect_full("}", "}", true);
    expect_full("a-b", "a-b", true);
    expect_full("a,b", "a,b", true);
    expect_full("\"quoted\"", "\"quoted\"", true);
    expect_full("a b", "a b", true);
}
ZTH_TEST("robust", "the dot does not match a newline, a negated class and \\S \\W \\D do or do not as their sets say") {
    expect_full(".", "\n", false);
    expect_full(".*", "a\nb", false);
    expect_full("[^x]*", "a\nb", true);
    expect_full("\\W", "\n", true);
    expect_full("\\D", "\n", true);
    expect_search("a.*b", "a\nb a b", 4, 7);
}
ZTH_TEST("robust", "bytes: values above 127 and NUL bytes are ordinary bytes") {
    expect_full("\xC3\xA9", "\xC3\xA9", true);
    expect_full(".", "\xFF", true);
    expect_full("[^a]", "\xFF", true);
    expect_full("\\W", "\xC3", true);
    expect_full("a.b", std::string("a\0b", 3), true);
    expect_full("[\\w]*", std::string("ab\0c", 4), false);
    expect_search("b", std::string("a\0b", 3), 2, 3);
    ZTH_EXPECT_EQ(all_matches("\xFF+", "\xFF\xFF.\xFF"), std::string("[0,2)[3,4)"));
}
ZTH_TEST("robust", "leftmost beats longest; longest among equal starts; shorter alternatives later in the pattern still lose to longer ones") {
    expect_search("b|abcd", "abcd", 0, 4);
    expect_search("bcd|abc", "abcd", 0, 3);
    expect_search("x*|y", "y", 0, 1);
    expect_search("a*?", "aaa", 0, 3);
    expect_search("(a|ab)(bc|c)", "abc", 0, 3);
    expect_search("(a*)(ab)*b", "aabab", 0, 3);
    expect_search("a+b|a", "aaab aaa", 0, 4);
    expect_search("ab|abab|ababab", "abababab", 0, 6);
}
ZTH_TEST("robust", "empty matches: at the start, between, at the end, and from past the end") {
    expect_search("a*", "", 0, 0);
    expect_search("a*", "b", 0, 0);
    expect_search("a*", "b", 1, 1, 1);
    expect_search("a*", "b", -1, 0, 2);
    expect_search("", "abc", 0, 0);
    expect_search("", "abc", 3, 3, 3);
    expect_search("$", "", 0, 0);
    expect_search("b*$", "aab", 2, 3);
    expect_search("b*$", "aa", 2, 2);
    expect_search("(a|)", "ba", 0, 0);
    expect_search("a?", "ba", 0, 0);
}
ZTH_TEST("robust", "anchors inside the pattern: in the middle of a concatenation and in alternatives") {
    expect_full("^a", "a", true);
    expect_full("a$", "a", true);
    expect_full("^a$", "a", true);
    expect_full("^a$", "ab", false);
    expect_full("a^b", "ab", false);
    expect_full("a$b", "ab", false);
    expect_full("^^a$$", "a", true);
    expect_full("(^|x)a", "a", true);
    expect_full("(^|x)a", "xa", true);
    expect_full("a(x|$)", "a", true);
    expect_full("a(x|$)", "ax", true);
    expect_full("a(x|$)", "ay", false);
    expect_search("(^|,)b", "a,b", 1, 3);
    expect_search("(^|,)a", "a,a", 0, 1);
    expect_search("a($|,)", "a,a", 0, 2);
    expect_search("(^|,)a", "a,a", 1, 3, 1);
    ZTH_EXPECT_EQ(all_matches("(^|,)[a-z]", "a,b,c"), std::string("[0,1)[1,3)[3,5)"));
}
ZTH_TEST("robust", "find_all in detail: adjacent matches, empty matches around non-empty ones, the empty text") {
    ZTH_EXPECT_EQ(all_matches("a|b", "abba"), std::string("[0,1)[1,2)[2,3)[3,4)"));
    ZTH_EXPECT_EQ(all_matches("b*", "abba"), std::string("[0,0)[1,3)[3,3)[4,4)"));
    ZTH_EXPECT_EQ(all_matches("a*", ""), std::string("[0,0)"));
    ZTH_EXPECT_EQ(all_matches("a", ""), std::string(""));
    ZTH_EXPECT_EQ(all_matches("x*", "xx"), std::string("[0,2)[2,2)"));
    ZTH_EXPECT_EQ(all_matches("\\d+", "12,345;6"), std::string("[0,2)[3,6)[7,8)"));
    ZTH_EXPECT_EQ(all_matches("(ab)+", "ababab ab"), std::string("[0,6)[7,9)"));
}
ZTH_TEST_TIMEOUT("robust", "no catastrophic backtracking: (a*)*b, (a|a)*b and (a?)^n a^n on long texts", 10) {
    const std::string a(5000, 'a');
    expect_full("(a*)*b", a, false);
    expect_full("(a*)*b", a + "b", true);
    expect_full("(a|a)*b", a, false);
    expect_full("(a|aa)+$", a, true);
    expect_search("(a*)*b", a, -1, 0);
    expect_search("(x+x+)+y", std::string(5000, 'x'), -1, 0);
    std::string optional_a, plain_a;
    for (int i = 0; i < 30; ++i) {
        optional_a += "a?";
        plain_a += "a";
    }
    expect_full(optional_a + plain_a, plain_a, true);
    expect_full(optional_a + plain_a, std::string(29, 'a'), false);
}
ZTH_TEST("robust", "nesting: 100 levels of parentheses are fine, the 101st is TooDeep (also 100000, without a crash)") {
    ZTH_EXPECT_TRUE(compile(std::string(100, '(') + "a" + std::string(100, ')')).ok());
    expect_error(std::string(101, '(') + "a" + std::string(101, ')'), CompileError::TooDeep, 100);
    expect_error(std::string(100000, '('), CompileError::TooDeep, 100);
    ZTH_EXPECT_TRUE(compile("a" + std::string(100, '*')).ok());
    expect_error("a" + std::string(101, '*'), CompileError::TooDeep, 101);
    expect_error("a" + std::string(100000, '+'), CompileError::TooDeep, 101);
    expect_full(std::string(100, '(') + "a" + std::string(100, ')'), "a", true);
    expect_full("a" + std::string(100, '*'), "aaa", true);
}
ZTH_TEST_TIMEOUT("robust", "long patterns: 5000 optional pieces, 5000 alternatives, a 20000-character literal", 10) {
    std::string opt, alt, lit;
    for (int i = 0; i < 5000; ++i) opt += "a?";
    for (int i = 0; i < 5000; ++i) alt += (i ? "|x" : "x") + std::to_string(i);
    for (int i = 0; i < 20000; ++i) lit += static_cast<char>('a' + i % 26);
    expect_full(opt, "aaa", true);
    expect_full(opt, std::string(5001, 'a'), false);
    expect_full(alt, "x4999", true);
    expect_full(alt, "x5000", false);
    expect_full(lit, lit, true);
    expect_full(lit, lit + "a", false);
}
ZTH_TEST_TIMEOUT("robust", "differential test: 600 random patterns on random texts agree with an independent evaluation of the syntax tree (full_match, search, search from k)", 20) {
    ::zth::Lcg rng(1234567);
    int checked = 0, skipped = 0;
    for (int i = 0; i < 600; ++i) {
        const std::string pattern = random_pattern(rng, 0);
        ParseResult parsed = parse(pattern);
        if (!parsed.ok()) {
            if (parsed.error != CompileError::NothingToRepeat) {
                ::zth::fail("the generator made a pattern that fails to parse: " + show(pattern) + " (" + name(parsed.error) + ")");
                return;
            }
            ++skipped;
            continue;
        }
        const CompileResult c = compile(pattern);
        if (!c.ok()) {
            ::zth::fail("compile failed for " + show(pattern));
            return;
        }
        for (int t = 0; t < 8; ++t) {
            const std::string text = random_text(rng);
            const bool full = ends(*parsed.ast, text, 0).count(text.size()) > 0;
            if (c.regex->full_match(text) != full) {
                ::zth::fail("full_match(" + show(pattern) + ", \"" + show(text) + "\") should be " + (full ? "true" : "false"));
                return;
            }
            const std::size_t from = text.empty() ? 0 : static_cast<std::size_t>(rng.below(static_cast<int>(text.size()) + 1));
            for (const std::size_t f : {static_cast<std::size_t>(0), from}) {
                const auto want = reference_search(*parsed.ast, text, f);
                const auto got = c.regex->search(text, f);
                if (want.has_value() != got.has_value() || (want && !(*want == *got))) {
                    ::zth::fail("search(" + show(pattern) + ", \"" + show(text) + "\", " + std::to_string(f) + ") gave " +
                                (got ? "[" + std::to_string(got->begin) + "," + std::to_string(got->end) + ")" : std::string("none")) + ", expected " +
                                (want ? "[" + std::to_string(want->begin) + "," + std::to_string(want->end) + ")" : std::string("none")));
                    return;
                }
            }
            ++checked;
        }
    }
    ZTH_EXPECT_TRUE(checked > 3000);
    ZTH_EXPECT_TRUE(skipped < 200);
}

// ================================================ Fast ========================================================
namespace {
// visits per (text length x NFA size)
double visits_per_cell(const std::string& pattern, const std::string& text, bool search) {
    const Regex re = must_compile(pattern);
    stats::reset();
    if (search) (void)re.search(text);
    else (void)re.full_match(text);
    return static_cast<double>(stats::state_visits) / (static_cast<double>(text.size() + 1) * static_cast<double>(re.nfa().size()));
}
}  // namespace

ZTH_TEST_TIMEOUT("fast", "matching is linear in the text: the work for (a*)*b, a*b, (a|aa)*c on 20000 bytes is at most 2.5 state visits per (byte x state)", 10) {
    const std::string a(20000, 'a');
    ZTH_EXPECT_TRUE(visits_per_cell("(a*)*b", a, false) <= 2.5);
    ZTH_EXPECT_TRUE(visits_per_cell("(a*)*b", a, true) <= 2.5);
    ZTH_EXPECT_TRUE(visits_per_cell("a*b", a, true) <= 2.5);
    ZTH_EXPECT_TRUE(visits_per_cell("(a|aa)*c", a, true) <= 2.5);
    ZTH_EXPECT_TRUE(visits_per_cell("[a-c]+x|ab*c", a, true) <= 2.5);
}
ZTH_TEST_TIMEOUT("fast", "search does not restart at every offset: a text where a long almost-match begins at every position (a*b on aaaa...a)", 10) {
    const Regex re = must_compile("a*b");
    stats::reset();
    ZTH_EXPECT_FALSE(re.search(std::string(20000, 'a')).has_value());
    ZTH_EXPECT_TRUE(stats::state_visits <= 20000ULL * 3 * re.nfa().size());
}
ZTH_TEST_TIMEOUT("fast", "search stops as soon as the match is known: a match at the start of a 20000-byte text costs a handful of visits", 10) {
    const Regex re = must_compile("ab");
    std::string text = "ab" + std::string(20000, 'x');
    stats::reset();
    const auto m = re.search(text);
    ZTH_EXPECT_TRUE(m.has_value());
    ZTH_EXPECT_TRUE(stats::state_visits <= 50);
    const Regex longer = must_compile("a+");
    text = std::string(100, 'a') + std::string(20000, 'x');
    stats::reset();
    ZTH_EXPECT_TRUE(longer.search(text).has_value());
    ZTH_EXPECT_TRUE(stats::state_visits <= 100 * 12 + 100);
}
ZTH_TEST_TIMEOUT("fast", "twice the text costs twice the work (visits), for full_match and for search", 10) {
    for (const bool search : {false, true}) {
        const Regex re = must_compile("(ab|a|b)*c?d");
        auto work = [&](std::size_t n) {
            std::string t;
            for (std::size_t i = 0; i < n; ++i) t += "ab"[i % 2];
            stats::reset();
            if (search) (void)re.search(t);
            else (void)re.full_match(t);
            return stats::state_visits;
        };
        const auto a = work(10000), b = work(20000);
        ZTH_EXPECT_TRUE(a > 10000);
        ZTH_EXPECT_TRUE(b <= a * 5 / 2);
    }
}
ZTH_TEST_TIMEOUT("fast", "the NFA grows linearly with the pattern: at most one state per pattern character plus two (nested groups and quantifiers do not copy)", 10) {
    for (const char* pattern : {"((((a*)*)*)*)*", "(a|b|c|d|e|f)+(g|h)*i?", "((a?)?)?((b+)+)+", "(|||||||)", "a|||b", "[a-z]+@[a-z]+\\.(com|org|net)"}) {
        const Regex re = must_compile(pattern);
        ZTH_EXPECT_TRUE(re.nfa().size() <= std::string(pattern).size() + 2);
    }
    std::string deep = "a";
    for (int i = 0; i < 40; ++i) deep = "(" + deep + "|b)*";
    const Regex re = must_compile(deep);
    ZTH_EXPECT_TRUE(re.nfa().size() <= deep.size() + 2);
}
ZTH_TEST_TIMEOUT("fast", "matching a long text allocates a bounded amount: at most 40 allocations for 200000 bytes, for full_match and for search", 10) {
    const Regex re = must_compile("(x|ab|c+)*d");
    std::string text;
    for (int i = 0; i < 50000; ++i) text += "abcx"[i % 4];
    {
        zth::mem::Scope scope;
        (void)re.full_match(text);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);
    }
    {
        zth::mem::Scope scope;
        (void)re.search(text);
        ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);
    }
}
ZTH_TEST_TIMEOUT("fast", "compiling a pattern of 100000 characters is fast and does little allocation (at most 4 per character)", 10) {
    std::string pattern;
    for (int i = 0; i < 20000; ++i) pattern += "ab*(c|d)";
    zth::mem::Scope scope;
    const CompileResult c = compile(pattern);
    ZTH_EXPECT_TRUE(c.ok());
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, pattern.size() * 4);
}
ZTH_TEST_TIMEOUT("fast", "find_all on a large text: 20000 matches in time", 10) {
    std::string text;
    for (int i = 0; i < 20000; ++i) text += "word" + std::to_string(i % 10) + " ";
    const Regex re = must_compile("[a-z]+[0-9]");
    ZTH_EXPECT_EQ(re.find_all(text).size(), std::size_t{20000});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw new/delete/malloc/free in the three files") {
    ZTH_EXPECT_NO_RAW_MEMORY("parser.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("nfa.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("matcher.hpp");
}
ZTH_TEST("clean", "you wrote it yourself: no <regex>, no std::regex, no std::function") {
    for (const char* file : {"parser.hpp", "nfa.hpp", "matcher.hpp"}) {
        const std::string text = ::zth::read_file(file);
        if (text.find("#include <regex>") != std::string::npos || text.find("#include <functional>") != std::string::npos) {
            ::zth::fail(std::string(file) + " includes <regex> or <functional>");
            return;
        }
        const std::string code = ::zth::strip_comments_and_literals(text);
        for (const char* word : {"regex", "function", "throw", "try", "catch", "goto", "dynamic_cast", "typeid", "cout", "printf"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the matcher looks at states only through nfa.at() and builds nothing: no copy of the NFA, no std::set / std::map / std::unordered_set") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("matcher.hpp"));
    for (const char* word : {"set", "map", "unordered_set", "unordered_map", "deque"}) {
        // `set` is allowed as the name of the member nfa.set(i); containers are not
        if (std::string(word) == "set") continue;
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("matcher.hpp uses `") + word + "`: a simulation needs only vectors");
            return;
        }
    }
    ZTH_EXPECT_TRUE(code.find("std::set") == std::string::npos);
}
ZTH_TEST("clean", "the tree and the NFA are read through the variant: std::get_if / holds_alternative / visit in nfa.hpp and parser.hpp") {
    const std::string nfa = ::zth::strip_comments_and_literals(::zth::read_file("nfa.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(nfa, "get_if").empty() || !::zth::find_word(nfa, "visit").empty() || !::zth::find_word(nfa, "holds_alternative").empty());
}
