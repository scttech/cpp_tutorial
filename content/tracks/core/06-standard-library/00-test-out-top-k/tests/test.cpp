#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <string>
#include <vector>

#include "top_k.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using W = std::vector<std::string>;

// a letters-only word that is long enough (24 characters) to need its own heap block if it is ever copied into a std::string
S long_word(int i) {
    S w(24, 'a');
    for (int pos = 23; pos >= 20; --pos) {
        w[static_cast<std::size_t>(pos)] = static_cast<char>('a' + i % 26);
        i /= 26;
    }
    return w;
}

}  // namespace

// ---- Works --------------------------------------------------------------------------------------------
ZTH_TEST("works", "the most frequent words, most frequent first") {
    ZTH_EXPECT_TRUE((top_k_words("a b a c a b", 2) == W{"a", "b"}));
    ZTH_EXPECT_TRUE((top_k_words("x y y z z z", 3) == W{"z", "y", "x"}));
}
ZTH_TEST("works", "punctuation and whitespace separate words") {
    ZTH_EXPECT_TRUE((top_k_words("hello, world! hello; world? hello.\nhello\tworld", 2) == W{"hello", "world"}));
    ZTH_EXPECT_TRUE((top_k_words("cat-dog cat/dog cat", 2) == W{"cat", "dog"}));
}
ZTH_TEST("works", "words are compared without regard to case and reported in lower case") {
    ZTH_EXPECT_TRUE((top_k_words("The the THE tHe Fox fox", 2) == W{"the", "fox"}));
}
ZTH_TEST("works", "words with the same count come out in alphabetical order") {
    ZTH_EXPECT_TRUE((top_k_words("pear apple fig", 3) == W{"apple", "fig", "pear"}));
    ZTH_EXPECT_TRUE((top_k_words("b a d c b a d c", 4) == W{"a", "b", "c", "d"}));
}
ZTH_TEST("works", "a sentence") {
    const char* text = "The quick brown fox jumps over the lazy dog. The dog barks; the fox runs. A quick fox!";
    // the: 4, fox: 3, dog: 2, quick: 2, then the singletons alphabetically
    ZTH_EXPECT_TRUE((top_k_words(text, 4) == W{"the", "fox", "dog", "quick"}));
    ZTH_EXPECT_TRUE((top_k_words(text, 6) == W{"the", "fox", "dog", "quick", "a", "barks"}));
}
ZTH_TEST("works", "k = 1 gives the single most frequent word") {
    ZTH_EXPECT_TRUE((top_k_words("one two two three three three", 1) == W{"three"}));
}

// ---- Robust -------------------------------------------------------------------------------------------
ZTH_TEST("robust", "k = 0 gives nothing") {
    ZTH_EXPECT_TRUE(top_k_words("a b c", 0).empty());
    ZTH_EXPECT_TRUE(top_k_words("", 0).empty());
}
ZTH_TEST("robust", "an empty text, and a text with no letters, have no words") {
    ZTH_EXPECT_TRUE(top_k_words("", 3).empty());
    ZTH_EXPECT_TRUE(top_k_words("   \n\t ", 3).empty());
    ZTH_EXPECT_TRUE(top_k_words("123 456 !!! 7.89", 3).empty());
}
ZTH_TEST("robust", "k larger than the number of distinct words gives all of them") {
    ZTH_EXPECT_TRUE((top_k_words("b a b", 10) == W{"b", "a"}));
    ZTH_EXPECT_EQ(top_k_words("one", 1000000).size(), std::size_t{1});
}
ZTH_TEST("robust", "digits and apostrophes separate words; they are not part of them") {
    ZTH_EXPECT_TRUE((top_k_words("abc123abc abc", 2) == W{"abc"}));
    ZTH_EXPECT_TRUE((top_k_words("don't don't", 3) == W{"don", "t"}));
    ZTH_EXPECT_TRUE((top_k_words("r2d2 r2d2", 3) == W{"d", "r"}));
}
ZTH_TEST("robust", "bytes above 127 (UTF-8) are separators and do not crash anything") {
    ZTH_EXPECT_TRUE((top_k_words("caf\xC3\xA9 caf\xC3\xA9 na\xC3\xAFve", 3) == W{"caf", "na", "ve"}));
    ZTH_EXPECT_TRUE((top_k_words("\xFF\xFE abc \x80", 2) == W{"abc"}));
}
ZTH_TEST("robust", "a word at the very start and at the very end of the text") {
    ZTH_EXPECT_TRUE((top_k_words("end middle end", 1) == W{"end"}));
    ZTH_EXPECT_TRUE((top_k_words("x", 1) == W{"x"}));
    ZTH_EXPECT_TRUE((top_k_words("x y", 2) == W{"x", "y"}));
}
ZTH_TEST("robust", "a long text: the counts decide, the alphabet breaks ties") {
    S text;
    for (int i = 0; i < 50; ++i) text += "zeta ";
    for (int i = 0; i < 50; ++i) text += "alpha ";
    for (int i = 0; i < 70; ++i) text += "mid ";
    for (int i = 0; i < 5; ++i) text += "rare ";
    ZTH_EXPECT_TRUE((top_k_words(text, 3) == W{"mid", "alpha", "zeta"}));
    ZTH_EXPECT_TRUE((top_k_words(text, 4) == W{"mid", "alpha", "zeta", "rare"}));
}
ZTH_TEST("robust", "a std::string and a string literal are both accepted (std::string_view parameter)") {
    const S s = "b a b";
    ZTH_EXPECT_TRUE((top_k_words(s, 1) == W{"b"}));
    ZTH_EXPECT_TRUE((top_k_words(std::string_view("b a b"), 1) == W{"b"}));
}

// ---- Fast ---------------------------------------------------------------------------------------------
ZTH_TEST("fast", "counting 6000 long words does not allocate a std::string for each of them") {
    S text;
    for (int i = 0; i < 6000; ++i) {
        text += long_word(i % 40);
        text += ' ';
    }
    zth::mem::Scope scope;
    const W top = top_k_words(text, 3);
    ZTH_EXPECT_EQ(top.size(), std::size_t{3});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 700);
}
ZTH_TEST("fast", "the long-word text still gives the right answer") {
    S text;
    for (int i = 0; i < 100; ++i) text += long_word(1) + " ";
    for (int i = 0; i < 60; ++i) text += long_word(2) + " ";
    for (int i = 0; i < 80; ++i) text += long_word(3) + " ";
    const W top = top_k_words(text, 2);
    ZTH_EXPECT_EQ(top.size(), std::size_t{2});
    if (top.size() == 2) {
        ZTH_EXPECT_EQ(top[0], long_word(1));
        ZTH_EXPECT_EQ(top[1], long_word(3));
    }
}
