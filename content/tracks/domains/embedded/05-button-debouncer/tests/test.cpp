#define ZTH_TRACK_ALLOCATIONS
#include <cstdint>
#include <string>
#include <vector>

#include "button.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace btn;

namespace {

// One letter per event; '.' for a quiet tick; several events in one tick in brackets, always in the order P L R C D.
std::string play(Button& b, const std::string& raw) {
    std::string out;
    for (char c : raw) {
        const std::uint8_t ev = b.update(c == '1');
        std::string s;
        if (ev & kPressed) s += 'P';
        if (ev & kLongPress) s += 'L';
        if (ev & kReleased) s += 'R';
        if (ev & kClick) s += 'C';
        if (ev & kDoubleClick) s += 'D';
        out += s.empty() ? std::string(".") : (s.size() == 1 ? s : "[" + s + "]");
    }
    return out;
}

struct Golden {
    const char* name;
    ButtonConfig config;
    const char* raw;
    const char* expected;
};

// Generated from an independent model of the specification.
const Golden kGolden[] = {
    {"a single clean press", {2, 5, 6}, "0011110000",
     "...P...[RC].."},
    {"contact bounce is filtered out", {3, 50, 20}, "0101101110000111111010100000",
     "........P..[RC]...P.........[RD].."},
    {"a long press", {2, 5, 6}, "00111111111100000",
     "...P....L....R..."},
    {"a double click", {2, 50, 6}, "001100001100000000",
     "...P.[RC]...P.[RD]......"},
    {"two clicks too far apart", {2, 50, 6}, "0011000000000011000000",
     "...P.[RC].........P.[RC]...."},
    {"a triple click is a click, a double click, then a click", {2, 50, 10}, "00110000110000110000110000000000",
     "...P.[RC]...P.[RD]...P.[RC]...P.[RD]........"},
    {"debounce 1 follows the input at once", {1, 4, 5}, "0110100111110",
     ".P.[RC]P[RD].P...LR"},
    {"debounce 0 is the same as 1", {0, 4, 5}, "0110100111110",
     ".P.[RC]P[RD].P...LR"},
    {"a short glitch never reaches the debounced state", {4, 50, 20}, "00011100011100001110000",
     "......................."},
    {"long press disabled", {2, 0, 6}, "0011111111111111111100000",
     "...P.................[RC]..."},
    {"long press exactly at the limit: released one tick before", {2, 6, 6}, "001111111000",
     "...P.....LR."},
    {"long press and release in the same tick", {2, 4, 6}, "0011111100",
     "...P...L.R"},
    {"quick clicks: click, double click, click, double click", {1, 50, 20}, "0101010101010101",
     ".P[RC]P[RD]P[RC]P[RD]P[RC]P[RD]P[RC]P"},
    {"a long press inside an open click window ends without click or double click", {1, 3, 30}, "010011111110",
     ".P[RC].P..L...R"},
    {"a long press closes the window: the next click is a plain click", {1, 3, 30}, "0100111111100100",
     ".P[RC].P..L...R.P[RC]."},
    {"three presses with one long press in the middle", {1, 4, 30}, "01010111111101010",
     ".P[RC]P[RD]P...L..RP[RC]P[RD]"},
    {"random script 1", {3, 0, 0}, "00000000101110000000000000111111110000000000000111110000000011011111000000000000011111011111000001111111111111",
     "............P..[RC]............P.......[RC]............P....[RC]..........P....[RC]............P..........[RC]....P.........."},
    {"random script 2", {2, 15, 4}, "0001110000000000000000000000111111111111111111111100000000111110000000000000111111111111111111111100011",
     "....P..[RC].....................P..............L......R.......P....[RC]............P..............L......R..P"},
    {"random script 3", {3, 3, 0}, "00000000000001111111100111111110000000000000000000000100000000000000000000001011100000000000000000000001101111111111111",
     "...............P..L..............R..............................................P..[LR]........................P..L......."},
    {"random script 4", {4, 0, 9}, "010111111111111101111111111111001111111111111111111111010111111111111101100000000101000000000000011100000000",
     "......P.....................................................................[RC]..............................."},
    {"random script 5", {0, 15, 20}, "0011111111111110000000011111111000000000000010000011000000001111111111111111111111000001111111111111000",
     "..P............[RC].......P.......[RD]............P[RC]....P.[RD].......P..............L......R....P............[RC].."},
    {"random script 6", {4, 3, 20}, "00111111110100000000000000000000001111111111111111111111000111110001101000000000000011000000000000010011111111001111111111111",
     ".....P..L......R.....................P..L.................................R..............................P..L................"},
    {"random script 7", {2, 0, 0}, "0101100000000000000000000001111100011111111011111111111110000000000000100000000000000000000001111111111111111111111",
     "....P.[RC].....................P....[RC]..P.....................[RC]...................................P...................."},
    {"random script 8", {4, 8, 9}, "000000000000000000000010100000000000000000000001111111111111000110000000000000000000000111110000011111111111110000011111111111111111111110000000000000",
     "..................................................P.......L.........R.....................P....[RC]....P.......L....R....P.......L.............R........."},
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a button starts released and quiet") {
    Button b;
    ZTH_EXPECT_FALSE(b.pressed());
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{0});
    ZTH_EXPECT_EQ(static_cast<int>(b.update(false)), 0);
    ZTH_EXPECT_EQ(play(b, "0000000000"), std::string(".........."));
}
ZTH_TEST("works", "the default configuration is 3 / 50 / 20") {
    ButtonConfig c;
    ZTH_EXPECT_EQ(c.debounce_ticks, std::uint16_t{3});
    ZTH_EXPECT_EQ(c.long_press_ticks, std::uint16_t{50});
    ZTH_EXPECT_EQ(c.double_click_ticks, std::uint16_t{20});
    Button b;   // defaults: three consecutive ticks flip the state
    ZTH_EXPECT_EQ(play(b, "011100"), std::string("...P.."));
}
ZTH_TEST("works", "the debounced state flips exactly when the raw level has differed for debounce_ticks consecutive ticks") {
    Button b({3, 50, 20});
    ZTH_EXPECT_EQ(static_cast<int>(b.update(true)), 0);
    ZTH_EXPECT_EQ(static_cast<int>(b.update(true)), 0);
    ZTH_EXPECT_FALSE(b.pressed());
    ZTH_EXPECT_EQ(static_cast<int>(b.update(true)), static_cast<int>(kPressed));
    ZTH_EXPECT_TRUE(b.pressed());
    ZTH_EXPECT_EQ(static_cast<int>(b.update(true)), 0);
}
ZTH_TEST("works", "a bounce back to the old level restarts the count") {
    Button b({3, 50, 20});
    ZTH_EXPECT_EQ(play(b, "1101101110"), std::string("........P."));   // runs of 2, 2, then 3 ones: only the third run counts
    ZTH_EXPECT_TRUE(b.pressed());
}
ZTH_TEST("works", "release is debounced the same way") {
    Button b({2, 50, 20});
    play(b, "0011");
    ZTH_EXPECT_TRUE(b.pressed());
    ZTH_EXPECT_EQ(play(b, "10100"), std::string("....[RC]"));
}
ZTH_TEST("works", "held_ticks counts the ticks since the Pressed event and is 0 when released") {
    Button b({1, 100, 20});
    b.update(true);
    ZTH_EXPECT_TRUE(b.pressed());
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{0});
    b.update(true);
    b.update(true);
    b.update(true);
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{3});
    b.update(false);
    ZTH_EXPECT_FALSE(b.pressed());
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{0});
}
ZTH_TEST("works", "a plain click: Released and Click arrive in the same tick") {
    Button b({2, 50, 6});
    const std::uint8_t press = [&] { b.update(true); return b.update(true); }();
    ZTH_EXPECT_EQ(static_cast<int>(press), static_cast<int>(kPressed));
    b.update(false);
    const std::uint8_t release = b.update(false);
    ZTH_EXPECT_EQ(static_cast<int>(release), static_cast<int>(kReleased | kClick));
}
ZTH_TEST("works", "reset returns to the initial state, including an open click window and a held button") {
    Button b({1, 3, 10});
    play(b, "1111");
    b.reset();
    ZTH_EXPECT_FALSE(b.pressed());
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{0});
    ZTH_EXPECT_EQ(play(b, "010"), std::string(".P[RC]"));   // a window left open by the first press would make this a double click
    b.reset();
    ZTH_EXPECT_EQ(play(b, "010"), std::string(".P[RC]"));
}
ZTH_TEST("works", "golden scripts: every tick of every script, from an independent model of the specification") {
    for (const Golden& g : kGolden) {
        Button b(g.config);
        const std::string got = play(b, g.raw);
        if (got != g.expected) {
            zth::fail(std::string("script `") + g.name + "` (debounce " + std::to_string(g.config.debounce_ticks) + ", long " + std::to_string(g.config.long_press_ticks) + ", double " +
                      std::to_string(g.config.double_click_ticks) + "):\n  input    " + g.raw + "\n  expected " + g.expected + "\n  got      " + got);
            return;
        }
    }
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a long press is reported once, long_press_ticks ticks after the Pressed event, and cancels the click") {
    Button b({1, 5, 20});
    ZTH_EXPECT_EQ(play(b, "1111111111" "0"), std::string("P....L....R"));   // P at tick 0; held reaches 5 at tick 5
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{0});
    ZTH_EXPECT_EQ(play(b, "010"), std::string(".P[RC]"));   // the long press left no click window behind: a plain click again, not a double
}
ZTH_TEST("robust", "a press released one tick before the limit is a click; released exactly at the limit it is a long press and a release in the same tick, with no click") {
    Button b({1, 5, 20});
    ZTH_EXPECT_EQ(play(b, "1111" "0"), std::string("P...[RC]"));
    Button c({1, 5, 20});
    ZTH_EXPECT_EQ(play(c, "11111" "0"), std::string("P....[LR]"));
}
ZTH_TEST("robust", "the second click of a double click replaces Click; the window is measured from the first RELEASE and includes its last tick") {
    Button a({1, 50, 6});
    ZTH_EXPECT_EQ(play(a, "01" "0" "0000" "1" "0"), std::string(".P[RC]....P[RD]"));   // second release 6 ticks after the first: inside the window
    Button b({1, 50, 6});
    ZTH_EXPECT_EQ(play(b, "01" "0" "00000" "1" "0"), std::string(".P[RC].....P[RC]")); // 7 ticks: too late
}
ZTH_TEST("robust", "the counters saturate instead of wrapping: a button held for 70000 ticks, a window that stays closed") {
    Button b({1, 100, 20});
    b.update(true);
    for (int i = 0; i < 70000; ++i) b.update(true);
    ZTH_EXPECT_TRUE(b.pressed());
    ZTH_EXPECT_EQ(b.held_ticks(), std::uint16_t{65535});
    ZTH_EXPECT_EQ(static_cast<int>(b.update(true)), 0);   // no second long press when the counter would wrap past 100 again
    Button c({1, 0, 20});
    for (int i = 0; i < 70000; ++i) c.update(false);
    ZTH_EXPECT_EQ(play(c, "010"), std::string(".P[RC]"));
}
ZTH_TEST("robust", "a very long idle time closes the click window for good (the window counter must not wrap around and reopen it)") {
    Button b({1, 50, 3});
    play(b, "010");   // a click opens the window
    for (int i = 0; i < 70000; ++i) b.update(false);
    ZTH_EXPECT_EQ(play(b, "010"), std::string(".P[RC]"));
}
ZTH_TEST("robust", "events of different ticks never leak into each other: 5000 random ticks, every returned mask is a valid combination") {
    zth::Lcg rng(8);
    Button b({2, 7, 12});
    int pressed_events = 0, released_events = 0;
    for (int i = 0; i < 5000; ++i) {
        const std::uint8_t ev = b.update(rng.below(3) != 0 ? ((i / 11) % 2 == 0) : (rng.below(2) == 0));
        pressed_events += (ev & kPressed) ? 1 : 0;
        released_events += (ev & kReleased) ? 1 : 0;
        ZTH_EXPECT_TRUE((ev & ~(kPressed | kReleased | kClick | kLongPress | kDoubleClick)) == 0);
        ZTH_EXPECT_FALSE((ev & kClick) && (ev & kDoubleClick));
        ZTH_EXPECT_FALSE((ev & kLongPress) && (ev & kPressed));
        ZTH_EXPECT_FALSE((ev & kPressed) && (ev & kReleased));
        if (ev & (kClick | kDoubleClick)) ZTH_EXPECT_TRUE(ev & kReleased);
        if (!pressed_events) continue;
    }
    ZTH_EXPECT_TRUE(pressed_events - released_events == 0 || pressed_events - released_events == 1);   // presses and releases alternate
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "no heap and a tiny object: it is meant for an interrupt handler") {
    zth::mem::Scope scope;
    Button b;
    for (int i = 0; i < 100000; ++i) b.update((i / 7) % 3 == 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sizeof(Button) <= 24);
}
ZTH_TEST_TIMEOUT("fast", "ten million ticks", 20) {
    Button b({3, 50, 20});
    std::uint32_t events = 0;
    for (std::uint32_t i = 0; i < 10000000; ++i) events += b.update(((i >> 5) & 3) == 1 || (i % 97) == 0) != 0;
    ZTH_EXPECT_TRUE(events > 1000);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("button.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("button.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "interrupt-handler style: no floating point, no standard library containers, strings or time")  {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("button.hpp"));
    for (const char* word : {"float", "double", "vector", "string", "map", "chrono", "function", "unique_ptr", "shared_ptr", "iostream", "malloc"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
