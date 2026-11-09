#include <string>

#include "zth_test.hpp"

namespace {
std::string code() { return ::zth::strip_comments_and_literals(::zth::read_file("ring_buffer_test.cpp")); }
}  // namespace

ZTH_TEST("clean", "your tests use a fixture: a class derived from ::testing::Test and at least two TEST_F") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "TEST_F").size() >= 2);
    ZTH_EXPECT_TRUE(code().find("testing::Test") != std::string::npos);
}

ZTH_TEST("clean", "your tests include a parameterized test: TEST_P with INSTANTIATE_TEST_SUITE_P") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "TEST_P").size() >= 1);
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "INSTANTIATE_TEST_SUITE_P").size() >= 1);
}

ZTH_TEST("clean", "your tests use ASSERT_* at least once, where a failed check makes the rest of the test pointless") {
    const std::string c = code();
    ZTH_EXPECT_TRUE(c.find("ASSERT_") != std::string::npos);
}
