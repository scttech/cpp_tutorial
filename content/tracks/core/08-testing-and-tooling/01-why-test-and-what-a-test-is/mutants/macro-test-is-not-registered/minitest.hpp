// MUTANT: MT_TEST declares the function but forgets to register it.
#pragma once

#include <cstddef>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// minitest: a unit-test framework in about 80 lines. Real frameworks (GoogleTest, Catch2) are bigger, but this is their core.
//
//   mt::add("name", fn)                    registers a test (a function without arguments), in order
//   mt::count()                            how many tests are registered
//   mt::clear()                            forgets all of them
//   mt::record_failure(file, line, text)   called by the check macros: the test that is running now has failed
//   mt::run_all(out)                       runs every test in order, writes a report to `out`, returns how many tests FAILED
//
// The macros: MT_TEST(name), MT_CHECK(cond), MT_CHECK_EQ(a, b); see the lesson for the report format.

namespace mt {

using TestFn = std::function<void()>;

namespace detail {

struct Test {
    std::string name;
    TestFn fn;
};

struct Registry {
    std::vector<Test> tests;
    std::vector<std::string> failures;  // of the test that is running now: "file:line: message"
};

// Built the first time it is used, so it exists whenever a static MT_TEST registers itself, whatever the order of initialization.
inline Registry& registry() {
    static Registry r;
    return r;
}

}  // namespace detail

inline void add(std::string name, TestFn fn) { detail::registry().tests.push_back({std::move(name), std::move(fn)}); }

inline std::size_t count() { return detail::registry().tests.size(); }

inline void clear() { detail::registry().tests.clear(); }

inline void record_failure(const char* file, int line, std::string message) {
    detail::registry().failures.push_back(std::string(file) + ":" + std::to_string(line) + ": " + message);
}

inline int run_all(std::ostream& out) {
    detail::Registry& r = detail::registry();
    int passed = 0;
    int failed = 0;
    for (const detail::Test& test : r.tests) {
        r.failures.clear();
        test.fn();
        if (r.failures.empty()) {
            ++passed;
            out << "PASS " << test.name << "\n";
        } else {
            ++failed;
            out << "FAIL " << test.name << "\n";
            for (const std::string& f : r.failures) out << "  " << f << "\n";
        }
    }
    out << passed << " passed, " << failed << " failed\n";
    return failed;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { add(name, fn); }
};

}  // namespace mt

#define MT_TEST(name)                                       \
    static void name();                                     \
    static void name()

#define MT_CHECK(cond)                                                                      \
    do {                                                                                    \
        if (!(cond)) ::mt::record_failure(__FILE__, __LINE__, "MT_CHECK(" #cond ") failed"); \
    } while (0)

#define MT_CHECK_EQ(a, b)                                                                                                      \
    do {                                                                                                                       \
        const auto& mt_left = (a);                                                                                             \
        const auto& mt_right = (b);                                                                                            \
        if (!(mt_left == mt_right)) {                                                                                          \
            std::ostringstream mt_text;                                                                                        \
            mt_text << "MT_CHECK_EQ(" #a ", " #b ") failed: " << mt_left << " != " << mt_right;                                \
            ::mt::record_failure(__FILE__, __LINE__, mt_text.str());                                                           \
        }                                                                                                                      \
    } while (0)
