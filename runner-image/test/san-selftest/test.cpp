// Sanitizer self-test (built with -fsanitize=address,undefined by tools/validate-content/sanitizer-selftest.mjs).
// A test named "expect-error <kind>: ..." must END with status "error" and a message "stopped by a sanitizer: ..."
// that mentions <kind>. "expect-fail ..." must fail normally. Everything else must pass.
// ZTH_TRACK_ALLOCATIONS is on, to prove the harness's allocation tracking and AddressSanitizer work together.
#define ZTH_TRACK_ALLOCATIONS
#include <climits>
#include <vector>

#include "zth_test.hpp"

int sum_to(const std::vector<int>& v, int n);
int read_freed();
int add(int a, int b);
int divide(int a, int b);
int ok_sum(const std::vector<int>& v);

ZTH_TEST("works", "a correct call passes, with sanitizers on") {
    ZTH_EXPECT_EQ(sum_to({1, 2, 3}, 2), 6);
    ZTH_EXPECT_EQ(ok_sum({4, 5, 6}), 15);
    ZTH_EXPECT_EQ(add(2, 3), 5);
}
ZTH_TEST("works", "a plain wrong answer is still an ordinary failure") { ZTH_EXPECT_EQ(add(1, 1), 3); }
ZTH_TEST("works", "expect-error heap-buffer-overflow: reading one past the end of a vector") { ZTH_EXPECT_EQ(sum_to({1, 2, 3}, 3), 6); }
ZTH_TEST("works", "expect-error use-after-poison: reading freed memory (the harness poisons what it keeps after a delete)") { ZTH_EXPECT_EQ(read_freed(), 5); }
ZTH_TEST("works", "expect-error signed integer overflow: INT_MAX + 1") { ZTH_EXPECT_EQ(add(INT_MAX, 1), INT_MIN); }
ZTH_TEST("works", "expect-error division by zero: dividing by zero") { ZTH_EXPECT_EQ(divide(1, 0), 0); }
ZTH_TEST("works", "a test after a sanitizer error still runs and passes") { ZTH_EXPECT_EQ(divide(8, 2), 4); }
ZTH_TEST("works", "expect-fail a leak is reported by the harness even under sanitizers") {
    zth::mem::Scope scope;
    int* leaked = new int[3];
    (void)leaked;
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
