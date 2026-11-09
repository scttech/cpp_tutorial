#include <sstream>
#include <string>

#include "minitest.hpp"
#include "zth_test.hpp"

namespace {

// Two tests registered by the MT_TEST macro of the learner, before main() runs.
MT_TEST(registered_by_the_macro_and_passing) { MT_CHECK(1 + 1 == 2); }
MT_TEST(registered_by_the_macro_and_failing) { MT_CHECK_EQ(1 + 1, 3); }

std::string report(int* failed = nullptr) {
    std::ostringstream out;
    const int n = mt::run_all(out);
    if (failed) *failed = n;
    return out.str();
}

struct Printable {
    int v;
    bool operator==(const Printable& o) const { return v == o.v; }
};
std::ostream& operator<<(std::ostream& os, const Printable& p) { return os << "P" << p.v; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a passing test is reported as PASS, with a summary line") {
    mt::clear();
    mt::add("t1", [] { MT_CHECK(true); });
    int failed = -1;
    ZTH_EXPECT_EQ(report(&failed), std::string("PASS t1\n1 passed, 0 failed\n"));
    ZTH_EXPECT_EQ(failed, 0);
}

ZTH_TEST("works", "a failing check makes the test FAIL, with file, line and the text of the condition") {
    mt::clear();
    int line = 0;
    mt::add("t1", [&] {
        line = __LINE__ + 1;
        MT_CHECK(1 > 2);
    });
    const std::string out = report();
    ZTH_EXPECT_CONTAINS(out, "FAIL t1\n");
    ZTH_EXPECT_CONTAINS(out, ":" + std::to_string(line) + ": MT_CHECK(1 > 2) failed\n");
    ZTH_EXPECT_CONTAINS(out, "\n  ");  // failure lines are indented by two spaces
    ZTH_EXPECT_CONTAINS(out, "0 passed, 1 failed\n");
}

ZTH_TEST("works", "MT_CHECK_EQ shows the text of both expressions and both values") {
    mt::clear();
    mt::add("t1", [] {
        int apples = 3;
        MT_CHECK_EQ(apples, 4);
    });
    ZTH_EXPECT_CONTAINS(report(), "MT_CHECK_EQ(apples, 4) failed: 3 != 4\n");
}

ZTH_TEST("works", "run_all returns the number of tests that failed, not the number of failures") {
    mt::clear();
    mt::add("a", [] { MT_CHECK(true); });
    mt::add("b", [] {
        MT_CHECK(false);
        MT_CHECK(false);
    });
    mt::add("c", [] { MT_CHECK_EQ(1, 2); });
    int failed = -1;
    const std::string out = report(&failed);
    ZTH_EXPECT_EQ(failed, 2);
    ZTH_EXPECT_CONTAINS(out, "1 passed, 2 failed\n");
}

ZTH_TEST("works", "the tests run in the order they were added") {
    mt::clear();
    mt::add("first", [] {});
    mt::add("second", [] { MT_CHECK(false); });
    mt::add("third", [] {});
    const std::string out = report();
    const auto a = out.find("PASS first");
    const auto b = out.find("FAIL second");
    const auto c = out.find("PASS third");
    ZTH_EXPECT_TRUE(a != std::string::npos && b != std::string::npos && c != std::string::npos);
    ZTH_EXPECT_TRUE(a < b && b < c);
}

ZTH_TEST("works", "count() and clear() manage the registry") {
    mt::clear();
    ZTH_EXPECT_EQ(mt::count(), std::size_t{0});
    mt::add("a", [] {});
    mt::add("b", [] {});
    ZTH_EXPECT_EQ(mt::count(), std::size_t{2});
    mt::clear();
    ZTH_EXPECT_EQ(mt::count(), std::size_t{0});
}

ZTH_TEST("works", "MT_TEST registers a test before main(): both macro-registered tests run") {
    const std::string out = report();  // no clear(): the registry still holds what the static initializers added
    ZTH_EXPECT_CONTAINS(out, "PASS registered_by_the_macro_and_passing\n");
    ZTH_EXPECT_CONTAINS(out, "FAIL registered_by_the_macro_and_failing\n");
    ZTH_EXPECT_CONTAINS(out, "MT_CHECK_EQ(1 + 1, 3) failed: 2 != 3\n");
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a failed check does not stop the test: later checks still run and are reported, in order") {
    mt::clear();
    int reached = 0;
    mt::add("t", [&] {
        MT_CHECK(false);
        ++reached;
        MT_CHECK_EQ(1, 2);
        ++reached;
    });
    const std::string out = report();
    ZTH_EXPECT_EQ(reached, 2);
    const auto first = out.find("MT_CHECK(false) failed");
    const auto second = out.find("MT_CHECK_EQ(1, 2) failed: 1 != 2");
    ZTH_EXPECT_TRUE(first != std::string::npos && second != std::string::npos);
    ZTH_EXPECT_TRUE(first < second);
}

ZTH_TEST("robust", "the failures of one test never show up in the next one") {
    mt::clear();
    mt::add("bad", [] { MT_CHECK(false); });
    mt::add("good", [] { MT_CHECK(true); });
    const std::string out = report();
    ZTH_EXPECT_CONTAINS(out, "FAIL bad\n");
    ZTH_EXPECT_CONTAINS(out, "PASS good\n");
    ZTH_EXPECT_CONTAINS(out, "1 passed, 1 failed\n");
}

ZTH_TEST("robust", "each argument of a macro is evaluated exactly once, whether the check passes or fails") {
    mt::clear();
    int calls = 0;
    auto next = [&] { return ++calls; };
    mt::add("t", [&] {
        MT_CHECK(next() == 1);
        MT_CHECK_EQ(next(), 2);
        MT_CHECK_EQ(next(), 99);  // fails: the message must use the value that was computed, not call next() again
    });
    const std::string out = report();
    ZTH_EXPECT_EQ(calls, 3);
    ZTH_EXPECT_CONTAINS(out, "failed: 3 != 99\n");
}

ZTH_TEST("robust", "MT_CHECK_EQ prints strings, doubles, characters and user types with operator<<") {
    mt::clear();
    mt::add("t", [] {
        MT_CHECK_EQ(std::string("ab"), std::string("ac"));
        MT_CHECK_EQ(1.5, 2.5);
        MT_CHECK_EQ('x', 'y');
        MT_CHECK_EQ((Printable{1}), (Printable{2}));
    });
    const std::string out = report();
    ZTH_EXPECT_CONTAINS(out, "failed: ab != ac\n");
    ZTH_EXPECT_CONTAINS(out, "failed: 1.5 != 2.5\n");
    ZTH_EXPECT_CONTAINS(out, "failed: x != y\n");
    ZTH_EXPECT_CONTAINS(out, "failed: P1 != P2\n");
}

ZTH_TEST("robust", "MT_CHECK_EQ compares a std::string with a string literal") {
    mt::clear();
    mt::add("t", [] { MT_CHECK_EQ(std::string("abc"), "abc"); });
    ZTH_EXPECT_EQ(report(), std::string("PASS t\n1 passed, 0 failed\n"));
}

ZTH_TEST("robust", "an empty registry reports 0 passed, 0 failed and returns 0") {
    mt::clear();
    int failed = -1;
    ZTH_EXPECT_EQ(report(&failed), std::string("0 passed, 0 failed\n"));
    ZTH_EXPECT_EQ(failed, 0);
}

ZTH_TEST("robust", "running twice gives the same report (nothing is left over from the first run)") {
    mt::clear();
    mt::add("bad", [] { MT_CHECK(false); });
    mt::add("good", [] {});
    const std::string once = report();
    const std::string twice = report();
    ZTH_EXPECT_EQ(once, twice);
}

ZTH_TEST("robust", "two tests with the same name both run") {
    mt::clear();
    mt::add("same", [] {});
    mt::add("same", [] { MT_CHECK(false); });
    ZTH_EXPECT_CONTAINS(report(), "1 passed, 1 failed\n");
}

ZTH_TEST("robust", "a check outside any test is harmless, and the next run is not affected") {
    mt::clear();
    mt::record_failure("nowhere.cpp", 1, "stray");
    mt::add("t", [] {});
    ZTH_EXPECT_EQ(report(), std::string("PASS t\n1 passed, 0 failed\n"));
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "minitest.hpp does not use new, delete, malloc or free") {
    ZTH_EXPECT_NO_RAW_MEMORY("minitest.hpp");
}

ZTH_TEST("clean", "minitest.hpp has no `using namespace` (a header must not impose one on its includers)") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("minitest.hpp").using_directive);
}

ZTH_TEST("clean", "the check macros are wrapped in do { } while (0), so `if (x) MT_CHECK(y); else ...` is one statement") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("minitest.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "while").size() >= 2);
    ZTH_EXPECT_TRUE(::zth::find_word(code, "do").size() >= 2);
}
