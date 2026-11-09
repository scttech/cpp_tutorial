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
// The report (exactly this format; one line per test, then one line per failure, then a summary):
//
//   PASS leap_years
//   FAIL century_years
//     main.cpp:23: MT_CHECK_EQ(is_leap_year(2100), true) failed: 0 != 1
//     main.cpp:24: MT_CHECK(2 + 2 == 5) failed
//   2 passed, 1 failed
//
// The macros (a macro is the only way to get __FILE__, __LINE__ and the TEXT of an expression):
//
//   MT_TEST(name)          defines a test function `name` and registers it before main() runs
//   MT_CHECK(cond)         records a failure "MT_CHECK(<cond text>) failed" if cond is false, and the test CARRIES ON
//   MT_CHECK_EQ(a, b)      records "MT_CHECK_EQ(<a text>, <b text>) failed: <value of a> != <value of b>" if a == b is false
//
// Tiers: Works (add/run/report/macros), Robust (several failures per test, isolation between tests, arguments evaluated once, values
// of any printable type), Clean (no raw new/delete, no `using namespace` in the header, every macro wrapped in do { } while (0)).

namespace mt {

// TODO 1: the registry. Keep the tests (name + std::function<void()>) and the failures of the test that is running now in ONE object that
//         is created the first time it is needed: a function-local static returned by reference. Why not a global variable? MT_TEST
//         registers tests during static initialization, in an order you do not control.
using TestFn = std::function<void()>;

inline void add(std::string name, TestFn fn) {
    (void)name;
    (void)fn;
}

inline std::size_t count() { return 0; }

inline void clear() {}

inline void record_failure(const char* file, int line, std::string message) {
    (void)file;
    (void)line;
    (void)message;
}

// TODO 2: run every test in registration order. Before each test forget the failures of the previous one. Write PASS or FAIL, then the
//         failures (two spaces, "file:line: message"), then at the end "<passed> passed, <failed> failed". Return the number of failures.
inline int run_all(std::ostream& out) {
    (void)out;
    return 0;
}

// TODO 3 (used by MT_TEST): an object whose constructor registers a test. A static object of this type runs add() before main().
struct Registrar {
    Registrar(const char* name, void (*fn)()) {
        (void)name;
        (void)fn;
    }
};

}  // namespace mt

// TODO 4: the macros. Rules: (1) wrap CHECK macros in do { ... } while (0), so that `if (x) MT_CHECK(y); else ...` works;
// (2) evaluate each argument ONCE (copy it into a variable); (3) use #cond / #a / #b to paste the source text into the message;
// (4) format the values with an std::ostringstream, so that any type with operator<< works.
#define MT_TEST(name) static void name()

#define MT_CHECK(cond) ((void)0)

#define MT_CHECK_EQ(a, b) ((void)0)
