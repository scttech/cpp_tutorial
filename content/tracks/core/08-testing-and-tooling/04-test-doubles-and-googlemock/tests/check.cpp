#include <string>

#include "zth_test.hpp"

namespace {
std::string code() { return ::zth::strip_comments_and_literals(::zth::read_file("thermostat_test.cpp")); }
}  // namespace

ZTH_TEST("clean", "you wrote mocks with MOCK_METHOD (at least three methods: the Sensor and both Heater functions)") {
    ZTH_EXPECT_TRUE(::zth::find_word(code(), "MOCK_METHOD").size() >= 3);
}

ZTH_TEST("clean", "your tests set expectations on the mocks with EXPECT_CALL, using Times(...), Return(...) or Throw(...)") {
    const std::string c = code();
    ZTH_EXPECT_TRUE(::zth::find_word(c, "EXPECT_CALL").size() >= 3);
    ZTH_EXPECT_TRUE(c.find("Times") != std::string::npos || c.find("Return") != std::string::npos || c.find("Throw") != std::string::npos);
}

ZTH_TEST("clean", "your tests use a StrictMock or a NiceMock to say what happens with calls that nobody expected") {
    const std::string c = code();
    ZTH_EXPECT_TRUE(c.find("StrictMock") != std::string::npos || c.find("NiceMock") != std::string::npos);
}
