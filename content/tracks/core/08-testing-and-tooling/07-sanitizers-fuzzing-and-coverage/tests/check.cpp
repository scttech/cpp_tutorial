#include <string>

#include "zth_test.hpp"

namespace {
std::string code() { return ::zth::strip_comments_and_literals(::zth::read_file("fuzz_test.cpp")); }
}  // namespace

ZTH_TEST("clean", "your fuzz test is deterministic: a fixed seed, no random_device and no clock") {
    const std::string c = code();
    ZTH_EXPECT_TRUE(::zth::find_word(c, "random_device").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(c, "chrono").empty());
    ZTH_EXPECT_TRUE(c.find("time(") == std::string::npos);
    ZTH_EXPECT_TRUE(c.find("srand") == std::string::npos);
}

ZTH_TEST("clean", "your fuzz test builds valid inputs with make_packet (structure-aware generation)") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "make_packet").size() >= 1);
}

ZTH_TEST("clean", "your fuzz test checks a property and stops the program when it is violated (abort, exit or a non-zero return)") {
    const std::string c = code();
    const bool stops = !::zth::find_word(c, "abort").empty() || !::zth::find_word(c, "exit").empty() || !::zth::find_word(c, "EXIT_FAILURE").empty() ||
                       c.find("return 1") != std::string::npos;
    ZTH_EXPECT_TRUE(stops);
    ZTH_EXPECT_TRUE(::zth::find_word(c, "packets").size() >= 1);  // it looks at what parse() returned
}
