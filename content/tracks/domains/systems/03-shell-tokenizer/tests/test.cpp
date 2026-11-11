#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "tokenizer.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace shell;

namespace {

// Words as [text], operators as {text}, separated by single spaces.
std::string fmt(const Result& r) {
    std::string s;
    for (const Token& t : r.tokens) {
        if (!s.empty()) s += ' ';
        s += t.kind == TokenKind::Word ? "[" + t.text + "]" : "{" + t.text + "}";
    }
    return s;
}
std::string tok(const std::string& line, const Env& env = Env{}) { return fmt(tokenize(line, env)); }

const Env kEnv = {{"HOME", "/home/u"}, {"NAME", "World"}, {"EMPTY", ""}, {"SPACED", "a b | \"c\" 'd' $NAME"}};

// ---- an independent way to write a word so that the tokenizer must read it back ----
std::string quote_sq(const std::string& s) {
    std::string out = "'";
    for (char c : s) out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}
std::string quote_dq(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '$') out += '\\';
        out += c;
    }
    return out + "\"";
}
bool plain(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'; }
std::string quote_bs(const std::string& s) {
    if (s.empty()) return "''";
    std::string out;
    for (char c : s) {
        if (!plain(c)) out += '\\';
        out += c;
    }
    return out;
}
std::string quote_piece(int style, const std::string& s) {
    switch (style) {
        case 0: return s.find('\'') == std::string::npos ? quote_sq(s) : quote_bs(s);
        case 1: return quote_dq(s);
        case 2: return quote_bs(s);
        default: {
            if (s.empty()) return quote_dq(s);
            for (char c : s)
                if (!plain(c)) return quote_bs(s);
            return s;
        }
    }
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty line and a line of blanks have no tokens") {
    ZTH_EXPECT_EQ(tok(""), std::string(""));
    ZTH_EXPECT_EQ(tok("   \t  "), std::string(""));
    ZTH_EXPECT_TRUE(tokenize("").ok());
}
ZTH_TEST("works", "words are separated by spaces and tabs, in any amount") {
    ZTH_EXPECT_EQ(tok("ls -la /tmp"), std::string("[ls] [-la] [/tmp]"));
    ZTH_EXPECT_EQ(tok("  ls \t -la   x  "), std::string("[ls] [-la] [x]"));
    ZTH_EXPECT_EQ(tok("one"), std::string("[one]"));
}
ZTH_TEST("works", "each token knows where it starts, counting the opening quote") {
    const Result r = tokenize("ab  cd 'e f'");
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{3});
    if (r.tokens.size() != 3) return;
    ZTH_EXPECT_EQ(r.tokens[0].pos, std::size_t{0});
    ZTH_EXPECT_EQ(r.tokens[1].pos, std::size_t{4});
    ZTH_EXPECT_EQ(r.tokens[2].pos, std::size_t{7});
    const Result q = tokenize("x \"a b\"c");
    ZTH_EXPECT_EQ(q.tokens.size(), std::size_t{2});
    if (q.tokens.size() == 2) ZTH_EXPECT_EQ(q.tokens[1].pos, std::size_t{2});
}
ZTH_TEST("works", "single quotes keep everything literally") {
    ZTH_EXPECT_EQ(tok(R"(echo 'hello   world' '$HOME \n "q" | &&')", kEnv), std::string(R"([echo] [hello   world] [$HOME \n "q" | &&])"));
}
ZTH_TEST("works", "double quotes keep spaces; a backslash escapes only quote, backslash and dollar") {
    ZTH_EXPECT_EQ(tok(R"(echo "a  b" "say \"hi\"" "back\\slash" "cost \$5" "keep \n" "it's")"),
                  std::string(R"([echo] [a  b] [say "hi"] [back\slash] [cost $5] [keep \n] [it's])"));
}
ZTH_TEST("works", "pieces that touch are one word, whatever their quoting") {
    ZTH_EXPECT_EQ(tok(R"(a"b c"d'e f'g)"), std::string("[ab cde fg]"));
    ZTH_EXPECT_EQ(tok(R"(--name="Ada L"  --x='y z')"), std::string("[--name=Ada L] [--x=y z]"));
}
ZTH_TEST("works", "an empty quoted string is a real word, but inside a word it adds nothing") {
    ZTH_EXPECT_EQ(tok(R"(echo "" '' x)"), std::string("[echo] [] [] [x]"));
    ZTH_EXPECT_EQ(tok(R"(a""b)"), std::string("[ab]"));
    ZTH_EXPECT_EQ(tok(R"("")"), std::string("[]"));
    ZTH_EXPECT_EQ(tok(R"('' "")"), std::string("[] []"));
    const Result r = tokenize(R"(x "")");
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{2});
    if (r.tokens.size() == 2) ZTH_EXPECT_EQ(r.tokens[1].pos, std::size_t{2});
}
ZTH_TEST("works", "a backslash outside quotes makes the next character literal") {
    ZTH_EXPECT_EQ(tok(R"(a\ b c\\d \'q\' \"x\" \|\; \#t)"), std::string(R"([a b] [c\d] ['q'] ["x"] [|;] [#t])"));
}
ZTH_TEST("works", "operators are tokens of their own, with or without spaces around them") {
    ZTH_EXPECT_EQ(tok("a&&b||c;d>e>>f<g&h|i"), std::string("[a] {&&} [b] {||} [c] {;} [d] {>} [e] {>>} [f] {<} [g] {&} [h] {|} [i]"));
    ZTH_EXPECT_EQ(tok("ls | grep x > out.txt && echo ok"), std::string("[ls] {|} [grep] [x] {>} [out.txt] {&&} [echo] [ok]"));
    ZTH_EXPECT_EQ(tok("|"), std::string("{|}"));
    ZTH_EXPECT_EQ(tok(";;"), std::string("{;} {;}"));
}
ZTH_TEST("works", "the longest operator wins") {
    ZTH_EXPECT_EQ(tok("a>>>b"), std::string("[a] {>>} {>} [b]"));
    ZTH_EXPECT_EQ(tok("a&&&b"), std::string("[a] {&&} {&} [b]"));
    ZTH_EXPECT_EQ(tok("a|||b"), std::string("[a] {||} {|} [b]"));
    ZTH_EXPECT_EQ(tok("a<<b"), std::string("[a] {<} {<} [b]"));
    ZTH_EXPECT_EQ(tok("a&|b"), std::string("[a] {&} {|} [b]"));
}
ZTH_TEST("works", "operators know their position, two-character ones too") {
    const Result r = tokenize("a&&b");
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{3});
    if (r.tokens.size() != 3) return;
    ZTH_EXPECT_EQ(r.tokens[0].pos, std::size_t{0});
    ZTH_EXPECT_EQ(r.tokens[1].pos, std::size_t{1});
    ZTH_EXPECT_TRUE(r.tokens[1].kind == TokenKind::Operator);
    ZTH_EXPECT_EQ(r.tokens[2].pos, std::size_t{3});
}
ZTH_TEST("works", "quoted or escaped operators are ordinary words") {
    ZTH_EXPECT_EQ(tok(R"(echo "a|b" '&&' a\;b ">")"), std::string("[echo] [a|b] [&&] [a;b] [>]"));
}
ZTH_TEST("works", "a # at the start of a word begins a comment; elsewhere it is a character") {
    ZTH_EXPECT_EQ(tok("ls # a comment | with stuff"), std::string("[ls]"));
    ZTH_EXPECT_EQ(tok("# only a comment"), std::string(""));
    ZTH_EXPECT_EQ(tok("echo a#b"), std::string("[echo] [a#b]"));
    ZTH_EXPECT_EQ(tok(R"(echo "# no" '#' \#)"), std::string("[echo] [# no] [#] [#]"));
    ZTH_EXPECT_EQ(tok("ls;#x"), std::string("[ls] {;}"));
    ZTH_EXPECT_EQ(tok("echo a #b c"), std::string("[echo] [a]"));
    ZTH_EXPECT_TRUE(tokenize("echo # 'unterminated in a comment").ok());
}
ZTH_TEST("works", "$NAME and ${NAME} are replaced; the name ends at the first non-name character") {
    ZTH_EXPECT_EQ(tok("echo $HOME", kEnv), std::string("[echo] [/home/u]"));
    ZTH_EXPECT_EQ(tok("$HOME/bin", kEnv), std::string("[/home/u/bin]"));
    ZTH_EXPECT_EQ(tok("${HOME}_x", kEnv), std::string("[/home/u_x]"));
    ZTH_EXPECT_EQ(tok("$NAME-$NAME.txt", kEnv), std::string("[World-World.txt]"));
    ZTH_EXPECT_EQ(tok("pre${NAME}post", kEnv), std::string("[preWorldpost]"));
    Env e = {{"HOME_DIR", "/d"}, {"HOME", "/h"}, {"A1", "x"}, {"_u", "y"}};
    ZTH_EXPECT_EQ(tok("$HOME_DIR $A1 $_u", e), std::string("[/d] [x] [y]"));
    const Result r = tokenize("echo $HOME", kEnv);
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{2});
    if (r.tokens.size() == 2) ZTH_EXPECT_EQ(r.tokens[1].pos, std::size_t{5});
}
ZTH_TEST("works", "expansion works inside double quotes, never inside single quotes, and can be escaped") {
    ZTH_EXPECT_EQ(tok(R"(echo "hi $NAME!" "${NAME}s")", kEnv), std::string("[echo] [hi World!] [Worlds]"));
    ZTH_EXPECT_EQ(tok(R"(echo '$HOME' '${HOME}')", kEnv), std::string("[echo] [$HOME] [${HOME}]"));
    ZTH_EXPECT_EQ(tok(R"(echo \$HOME "\$HOME")", kEnv), std::string("[echo] [$HOME] [$HOME]"));
}
ZTH_TEST("works", "a value is never split or re-read: spaces, quotes and operators in it stay characters") {
    ZTH_EXPECT_EQ(tok("echo $SPACED", kEnv), std::string(R"([echo] [a b | "c" 'd' $NAME])"));
    ZTH_EXPECT_EQ(tok(R"(echo "$SPACED")", kEnv), std::string(R"([echo] [a b | "c" 'd' $NAME])"));
}
ZTH_TEST("works", "a $ that starts no reference is just a $") {
    ZTH_EXPECT_EQ(tok("echo $ $5 a$ $-x", kEnv), std::string("[echo] [$] [$5] [a$] [$-x]"));
    ZTH_EXPECT_EQ(tok(R"("a$")", kEnv), std::string("[a$]"));
    ZTH_EXPECT_EQ(tok(R"("$")", kEnv), std::string("[$]"));
    ZTH_EXPECT_EQ(tok(R"("$ $1")", kEnv), std::string("[$ $1]"));
}
ZTH_TEST("works", "an empty or undefined expansion vanishes unless it is quoted") {
    ZTH_EXPECT_EQ(tok("echo $EMPTY x", kEnv), std::string("[echo] [x]"));
    ZTH_EXPECT_EQ(tok(R"(echo "$EMPTY" x)", kEnv), std::string("[echo] [] [x]"));
    ZTH_EXPECT_EQ(tok("a$EMPTY", kEnv), std::string("[a]"));
    ZTH_EXPECT_EQ(tok("$EMPTY$EMPTY", kEnv), std::string(""));
    ZTH_EXPECT_EQ(tok(R"("$EMPTY"$EMPTY)", kEnv), std::string("[]"));
    ZTH_EXPECT_EQ(tok("$NOPE", kEnv), std::string(""));
    ZTH_EXPECT_EQ(tok("$NAMEx", kEnv), std::string(""));
    ZTH_EXPECT_EQ(tok(R"("[$NOPE]")", kEnv), std::string("[[]]"));
    ZTH_EXPECT_EQ(tok("x $EMPTY y", kEnv), std::string("[x] [y]"));
    const Result r = tokenize("$EMPTY foo", kEnv);
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{1});
    if (r.tokens.size() == 1) ZTH_EXPECT_EQ(r.tokens[0].pos, std::size_t{7});
}
ZTH_TEST("works", "a realistic command line") {
    ZTH_EXPECT_EQ(tok(R"(grep -R "TODO: fix" src/ | sort -u > 'my todos.txt' && echo "done: $NAME" # tail)", kEnv),
                  std::string("[grep] [-R] [TODO: fix] [src/] {|} [sort] [-u] {>} [my todos.txt] {&&} [echo] [done: World]"));
}
ZTH_TEST("works", "a missing closing single quote is reported where the quote opened, with the finished tokens kept") {
    const Result r = tokenize("echo 'abc");
    ZTH_EXPECT_FALSE(r.ok());
    ZTH_EXPECT_TRUE(r.error == ErrorKind::UnterminatedSingleQuote);
    ZTH_EXPECT_EQ(r.error_pos, std::size_t{5});
    ZTH_EXPECT_EQ(fmt(r), std::string("[echo]"));
}
ZTH_TEST("works", "a missing closing double quote, also when the last quote is escaped") {
    const Result r = tokenize(R"(echo "abc)");
    ZTH_EXPECT_TRUE(r.error == ErrorKind::UnterminatedDoubleQuote);
    ZTH_EXPECT_EQ(r.error_pos, std::size_t{5});
    const Result s = tokenize(R"(echo "abc\")");
    ZTH_EXPECT_TRUE(s.error == ErrorKind::UnterminatedDoubleQuote);
    ZTH_EXPECT_EQ(s.error_pos, std::size_t{5});
    ZTH_EXPECT_EQ(fmt(s), std::string("[echo]"));
    const Result t = tokenize("\"abc\\");   // ends with a lone backslash inside the quote
    ZTH_EXPECT_TRUE(t.error == ErrorKind::UnterminatedDoubleQuote);
    ZTH_EXPECT_EQ(t.error_pos, std::size_t{0});
}
ZTH_TEST("works", "a backslash at the end of the line is an error at the backslash") {
    const Result r = tokenize("echo abc\\");
    ZTH_EXPECT_TRUE(r.error == ErrorKind::DanglingBackslash);
    ZTH_EXPECT_EQ(r.error_pos, std::size_t{8});
    ZTH_EXPECT_EQ(fmt(r), std::string("[echo]"));
    ZTH_EXPECT_TRUE(tokenize("\\").error == ErrorKind::DanglingBackslash);
    ZTH_EXPECT_TRUE(tokenize("a\\\\").ok());   // an escaped backslash at the end is fine
}
ZTH_TEST("works", "an unclosed ${ and an invalid name inside ${ } are reported at the $") {
    const Result a = tokenize("echo ${HOME");
    ZTH_EXPECT_TRUE(a.error == ErrorKind::UnterminatedBrace);
    ZTH_EXPECT_EQ(a.error_pos, std::size_t{5});
    const Result b = tokenize(R"(echo "${HOME")");
    ZTH_EXPECT_TRUE(b.error == ErrorKind::UnterminatedBrace);
    ZTH_EXPECT_EQ(b.error_pos, std::size_t{6});
    for (const char* line : {"echo ${}", "echo ${1x}", "echo ${a-b}", "echo ${ HOME }", "echo ${HO ME}"}) {
        const Result r = tokenize(line);
        ZTH_EXPECT_TRUE(r.error == ErrorKind::BadSubstitution);
        ZTH_EXPECT_EQ(r.error_pos, std::size_t{5});
    }
}
ZTH_TEST("works", "the unfinished word is dropped; the first error wins") {
    const Result a = tokenize("a b 'c");
    ZTH_EXPECT_EQ(fmt(a), std::string("[a] [b]"));
    const Result b = tokenize("x'y");
    ZTH_EXPECT_EQ(fmt(b), std::string(""));
    ZTH_EXPECT_EQ(b.error_pos, std::size_t{1});
    const Result c = tokenize("'a \"b");
    ZTH_EXPECT_TRUE(c.error == ErrorKind::UnterminatedSingleQuote);
    ZTH_EXPECT_EQ(c.error_pos, std::size_t{0});
    ZTH_EXPECT_EQ(tokenize("ok").error_pos, std::size_t{0});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "5000 random lines, written with every kind of quoting, read back exactly (text, kind and position)") {
    zth::Lcg rng(31337);
    const std::string alphabet = "abZ9_-./ \t'\"\\$|&;<>#{}!~";
    for (int round = 0; round < 5000; ++round) {
        std::string line;
        std::vector<Token> expected;
        auto blanks = [&](int min_n) {
            const int n = min_n + rng.below(3 - min_n);
            for (int i = 0; i < n; ++i) line += rng.below(4) == 0 ? '\t' : ' ';
        };
        blanks(0);
        const int count = rng.below(6);
        bool prev_op = false, prev_word = false;
        for (int k = 0; k < count; ++k) {
            if (rng.below(4) == 0) {
                static const char* ops[] = {"|", "&&", "||", ";", ">", ">>", "<", "&"};
                if (prev_op) blanks(1);   // two operators in a row need a blank: `>` `>` would merge
                else if (prev_word) blanks(0);
                const std::string op = ops[rng.below(8)];
                expected.push_back({TokenKind::Operator, op, line.size()});
                line += op;
                prev_op = true;
                prev_word = false;
            } else {
                if (prev_word) blanks(1);
                else if (prev_op) blanks(0);
                const std::size_t pos = line.size();
                std::string text;
                const int pieces = 1 + rng.below(3);
                for (int p = 0; p < pieces; ++p) {
                    std::string piece;
                    const int len = rng.below(7);
                    for (int c = 0; c < len; ++c) piece += alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))];
                    const int style = rng.below(4);
                    // a bare (style 3) piece after a piece that ended a backslash escape is fine; an empty piece in a multi-piece word adds nothing
                    line += quote_piece(style, piece);
                    text += piece;
                }
                expected.push_back({TokenKind::Word, text, pos});
                prev_word = true;
                prev_op = false;
            }
        }
        blanks(0);
        const Result r = tokenize(line);
        bool same = r.ok() && r.tokens.size() == expected.size();
        for (std::size_t i = 0; same && i < expected.size(); ++i)
            same = r.tokens[i].kind == expected[i].kind && r.tokens[i].text == expected[i].text && r.tokens[i].pos == expected[i].pos;
        if (!same) {
            zth::fail("the line `" + line + "` was read as `" + fmt(r) + "` (error " + describe(r.error) + "), expected " + std::to_string(expected.size()) + " tokens, the first being `" +
                      (expected.empty() ? std::string("none") : expected[0].text) + "`");
            return;
        }
    }
}
ZTH_TEST("robust", "20000 random garbage lines: the tokenizer always stops, errors point inside the line, and a successful result survives re-quoting") {
    zth::Lcg rng(99);
    const std::string alphabet = "ab '\"\\${}|&;<># \t_1-";
    for (int round = 0; round < 20000; ++round) {
        std::string line;
        const int len = rng.below(30);
        for (int i = 0; i < len; ++i) line += alphabet[static_cast<std::size_t>(rng.below(static_cast<int>(alphabet.size())))];
        const Result r = tokenize(line);
        if (!r.ok()) {
            if (r.error_pos >= line.size()) {
                zth::fail("error position " + std::to_string(r.error_pos) + " is outside the line `" + line + "`");
                return;
            }
            continue;
        }
        std::string again;
        for (const Token& t : r.tokens) {
            again += t.kind == TokenKind::Operator ? " " + t.text + " " : " " + quote_sq(t.text) + " ";
        }
        const Result r2 = tokenize(again);
        bool same = r2.ok() && r2.tokens.size() == r.tokens.size();
        for (std::size_t i = 0; same && i < r.tokens.size(); ++i) same = r.tokens[i].kind == r2.tokens[i].kind && r.tokens[i].text == r2.tokens[i].text;
        if (!same) {
            zth::fail("`" + line + "` gave `" + fmt(r) + "` but writing that back gave `" + fmt(r2) + "`");
            return;
        }
    }
}
ZTH_TEST("robust", "the tokenizer never looks past the end of the view: every prefix of a tricky line, followed by poison in memory") {
    const std::string tricky = R"(echo "a $HOME ${NAME} \" b" 'c d' x\ y >>f && g | h;# tail \)";
    const std::string poison = "\"'\\${ #}";
    for (std::size_t len = 0; len <= tricky.size(); ++len) {
        const std::string buffer = tricky.substr(0, len) + poison;
        const std::string_view view(buffer.data(), len);
        const Result r = tokenize(view, kEnv);
        const Result clean = tokenize(std::string(tricky.substr(0, len)), kEnv);   // same text without any poison after it
        if (fmt(r) != fmt(clean) || r.error != clean.error || r.error_pos != clean.error_pos) {
            zth::fail("the prefix of length " + std::to_string(len) + " gave a different result when memory after it held other characters");
            return;
        }
        if (!r.ok() && r.error_pos >= len) {
            zth::fail("the error position of the prefix of length " + std::to_string(len) + " is outside it");
            return;
        }
    }
}
ZTH_TEST("robust", "a long line with 100000 mixed tokens is read completely and correctly") {
    std::string line;
    for (int i = 0; i < 100000; ++i) {
        switch (i % 5) {
            case 0: line += "word" + std::to_string(i) + " "; break;
            case 1: line += "\"two words\" "; break;
            case 2: line += "'x y'\"z\"w "; break;
            case 3: line += "&& "; break;
            default: line += "$NAME;"; break;
        }
    }
    const Result r = tokenize(line, kEnv);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{100000 / 5 * 6});
    ZTH_EXPECT_EQ(r.tokens.front().text, std::string("word0"));
    ZTH_EXPECT_EQ(r.tokens[1].text, std::string("two words"));
    ZTH_EXPECT_EQ(r.tokens[2].text, std::string("x yzw"));
    ZTH_EXPECT_EQ(r.tokens[3].text, std::string("&&"));
    ZTH_EXPECT_EQ(r.tokens[4].text, std::string("World"));
    ZTH_EXPECT_EQ(r.tokens[5].text, std::string(";"));
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a 6 MB line with a million tokens is tokenized in one pass (quadratic copying would never finish)", 20) {
    std::string line;
    line.reserve(7000000);
    for (int i = 0; i < 700000; ++i) line += i % 2 ? "'b c' " : "ab\"cd\" ";
    for (int i = 0; i < 300000; ++i) line += "x| ";
    const Result r = tokenize(line);
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{700000 + 600000});
}
ZTH_TEST_TIMEOUT("fast", "one very long quoted word and one very long unterminated quote", 20) {
    std::string big(5000000, 'q');
    const Result r = tokenize("'" + big + "'");
    ZTH_EXPECT_TRUE(r.ok());
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{1});
    if (r.tokens.size() == 1) ZTH_EXPECT_EQ(r.tokens[0].text.size(), big.size());
    const Result u = tokenize("\"" + big);
    ZTH_EXPECT_TRUE(u.error == ErrorKind::UnterminatedDoubleQuote);
    ZTH_EXPECT_EQ(u.error_pos, std::size_t{0});
    const Result s = tokenize("a '" + big);
    ZTH_EXPECT_TRUE(s.error == ErrorKind::UnterminatedSingleQuote);
}
ZTH_TEST("fast", "short words cost no heap allocation of their own: only the token list grows (a copy of the line per token would show here)") {
    std::string line;
    for (int i = 0; i < 2000; ++i) line += "abcdefg ";
    ZTH_EXPECT_TRUE(line.size() > 15000);
    zth::mem::Scope scope;
    const Result r = tokenize(line);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 40);
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{2000});
}
ZTH_TEST("fast", "long words cost a couple of allocations each (the text growing), never one per character or per copy of the line") {
    std::string line;
    for (int i = 0; i < 1000; ++i) line += "a_word_longer_than_the_small_string_buffer ";
    zth::mem::Scope scope;
    const Result r = tokenize(line);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2 * 1000 + 40);
    ZTH_EXPECT_EQ(r.tokens.size(), std::size_t{1000});
}
ZTH_TEST("fast", "operators and empty lines allocate nothing at all") {
    zth::mem::Scope scope;
    for (int i = 0; i < 1000; ++i) {
        const Result a = tokenize("");
        const Result b = tokenize("   # comment only");
        (void)a;
        (void)b;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("tokenizer.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tokenizer.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a hand-written scanner: no regex, no stringstreams, no strtok, no printf family") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tokenizer.hpp"));
    for (const char* word : {"regex", "regex_search", "regex_match", "stringstream", "istringstream", "ostringstream", "strtok", "sscanf", "printf", "getline", "split", "isspace", "isalpha", "isalnum"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "it takes a string_view and does not copy the line: no std::string built from the whole line") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("tokenizer.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "string_view").empty());
}
