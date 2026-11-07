#define ZTH_TRACK_ALLOCATIONS
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "zth_test.hpp"

std::vector<std::string> tokenize(std::string_view);
std::map<std::string, int> word_frequencies(std::string_view);
std::string_view trim(std::string_view);
std::string_view first_word(std::string_view);
std::string join(const std::vector<std::string>&, std::string_view);
std::size_t utf8_length(std::string_view);

using Words = std::vector<std::string>;
using Freq = std::map<std::string, int>;

namespace {
// Is `part` a view into the characters of `whole` (no copy was made)?
bool points_into(std::string_view part, std::string_view whole) {
    return part.data() >= whole.data() && part.data() + part.size() <= whole.data() + whole.size();
}
}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "tokenize splits on whitespace") {
    ZTH_EXPECT_EQ(tokenize("hello world"), (Words{"hello", "world"}));
}
ZTH_TEST("works", "tokenize ignores repeated, leading and trailing whitespace; tabs and newlines count") {
    ZTH_EXPECT_EQ(tokenize("  a  b\tc\nd \n"), (Words{"a", "b", "c", "d"}));
}
ZTH_TEST("works", "word_frequencies counts words, ignoring case and punctuation") {
    ZTH_EXPECT_EQ(word_frequencies("The cat and the hat. THE END!"),
                  (Freq{{"and", 1}, {"cat", 1}, {"end", 1}, {"hat", 1}, {"the", 3}}));
}
ZTH_TEST("works", "trim removes whitespace at both ends") {
    ZTH_EXPECT_EQ(std::string(trim("  hi there \n")), "hi there");
}
ZTH_TEST("works", "first_word returns the first word") {
    ZTH_EXPECT_EQ(std::string(first_word("  alpha beta")), "alpha");
}
ZTH_TEST("works", "join puts the separator between the parts") {
    ZTH_EXPECT_EQ(join({"a", "b", "c"}, ", "), "a, b, c");
}
ZTH_TEST("works", "utf8_length counts characters, not bytes") {
    ZTH_EXPECT_EQ(utf8_length("caf\xc3\xa9"), std::size_t{4});
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "tokenize: empty and whitespace-only input give no tokens") {
    ZTH_EXPECT_EQ(tokenize(""), Words{});
    ZTH_EXPECT_EQ(tokenize(" \t\r\n \v\f"), Words{});
}
ZTH_TEST("robust", "tokenize: Windows line endings and other ASCII whitespace separate tokens") {
    ZTH_EXPECT_EQ(tokenize("one\r\ntwo\vthree\ffour"), (Words{"one", "two", "three", "four"}));
}
ZTH_TEST("robust", "tokenize: a single word, and a word at the very end") {
    ZTH_EXPECT_EQ(tokenize("solo"), Words{"solo"});
    ZTH_EXPECT_EQ(tokenize("a b"), (Words{"a", "b"}));
}
ZTH_TEST("robust", "tokenize: bytes of non-ASCII characters are not whitespace (no UB for negative chars)") {
    ZTH_EXPECT_EQ(tokenize("caf\xc3\xa9 \xe2\x82\xac" "5"), (Words{"caf\xc3\xa9", "\xe2\x82\xac" "5"}));
}
ZTH_TEST("robust", "tokenize: 200 000 words stay fast") {
    std::string big;
    for (int i = 0; i < 200000; ++i) big += "word ";
    ZTH_EXPECT_EQ(tokenize(big).size(), std::size_t{200000});
}
ZTH_TEST("robust", "word_frequencies: empty text, digits and punctuation only") {
    ZTH_EXPECT_EQ(word_frequencies(""), Freq{});
    ZTH_EXPECT_EQ(word_frequencies("123 456 !!! ..."), Freq{});
}
ZTH_TEST("robust", "word_frequencies: digits and apostrophes separate words; the last word is counted") {
    ZTH_EXPECT_EQ(word_frequencies("abc123def don't"), (Freq{{"abc", 1}, {"def", 1}, {"don", 1}, {"t", 1}}));
}
ZTH_TEST("robust", "word_frequencies: non-ASCII bytes separate words (a string is bytes, not characters)") {
    ZTH_EXPECT_EQ(word_frequencies("caf\xc3\xa9 au lait"), (Freq{{"au", 1}, {"caf", 1}, {"lait", 1}}));
}
ZTH_TEST("robust", "word_frequencies: mixed case counts as the same word") {
    ZTH_EXPECT_EQ(word_frequencies("Go go GO gO"), (Freq{{"go", 4}}));
}
ZTH_TEST("robust", "trim: nothing to trim, all whitespace, empty, inner spaces kept") {
    ZTH_EXPECT_EQ(std::string(trim("abc")), "abc");
    ZTH_EXPECT_EQ(std::string(trim(" \t\n ")), "");
    ZTH_EXPECT_EQ(std::string(trim("")), "");
    ZTH_EXPECT_EQ(std::string(trim("\t a  b \t")), "a  b");
}
ZTH_TEST("robust", "trim returns a view into its argument, not into a temporary copy") {
    const std::string s = "   a long string that does not fit in the small string buffer   ";
    std::string_view t = trim(s);
    ZTH_EXPECT_TRUE(points_into(t, s));
    ZTH_EXPECT_EQ(t.size(), s.size() - 6);
}
ZTH_TEST("robust", "first_word: no words, a single word, leading whitespace of every kind") {
    ZTH_EXPECT_EQ(std::string(first_word("")), "");
    ZTH_EXPECT_EQ(std::string(first_word("   \t\n")), "");
    ZTH_EXPECT_EQ(std::string(first_word("solo")), "solo");
    ZTH_EXPECT_EQ(std::string(first_word("\n\t next word")), "next");
}
ZTH_TEST("robust", "join: empty list, one part, empty separator, empty parts") {
    ZTH_EXPECT_EQ(join({}, ","), "");
    ZTH_EXPECT_EQ(join({"only"}, ","), "only");
    ZTH_EXPECT_EQ(join({"a", "b"}, ""), "ab");
    ZTH_EXPECT_EQ(join({"", "", "x"}, "-"), "--x");
}
ZTH_TEST("robust", "utf8_length: empty, ASCII, 2-, 3- and 4-byte characters") {
    ZTH_EXPECT_EQ(utf8_length(""), std::size_t{0});
    ZTH_EXPECT_EQ(utf8_length("plain"), std::size_t{5});
    ZTH_EXPECT_EQ(utf8_length("\xc3\xa9"), std::size_t{1});                  // e with acute (2 bytes)
    ZTH_EXPECT_EQ(utf8_length("\xe2\x82\xac"), std::size_t{1});              // euro sign (3 bytes)
    ZTH_EXPECT_EQ(utf8_length("\xf0\x9f\x98\x80 ok"), std::size_t{4});       // emoji (4 bytes), space, o, k
}

// ---- Fast: views must not copy; join must not keep regrowing ------------------------------------
ZTH_TEST("fast", "trim makes no heap allocation") {
    const std::string s(500, ' ');
    std::string padded = s + "payload that is long enough to need the heap" + s;
    zth::mem::Scope m;
    std::string_view t = trim(padded);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 0);
    ZTH_EXPECT_EQ(t.size(), std::size_t{44});
}
ZTH_TEST("fast", "first_word makes no heap allocation") {
    std::string s = "   extraordinarily-long-first-word second";
    zth::mem::Scope m;
    std::string_view w = first_word(s);
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 0);
    ZTH_EXPECT_EQ(w.size(), std::size_t{31});
}
ZTH_TEST("fast", "join builds its result with a single allocation") {
    Words parts(300, "0123456789");
    zth::mem::Scope m;
    std::string out = join(parts, ", ");
    ZTH_EXPECT_ALLOCS_AT_MOST(m, 1);
    ZTH_EXPECT_EQ(out.size(), std::size_t{300 * 10 + 299 * 2});
}
