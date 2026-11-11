#include <cstdio>
#include <string>

#include "answers.hpp"
#include "zth_test.hpp"

// ================================================ Works =======================================================
ZTH_TEST("works", "1. the function with the most SELF time is the one with the biggest Self percentage in perf-report.txt") {
    ZTH_EXPECT_EQ(std::string(hottest_by_self_time()), std::string("blur_row"));
}
ZTH_TEST("works", "2. process_image and everything it calls take 98 % (the Children column, rounded), not its own 12 %") {
    ZTH_EXPECT_EQ(percent_in_process_image_and_callees(), 98);
}
ZTH_TEST("works", "3. 0.44 instructions per cycle and 52 % cache misses: the processor mostly waits for memory") {
    ZTH_EXPECT_TRUE(bottleneck_from_perf_stat() == Bottleneck::Memory);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "4. clamp_pixel is called 48 million times (and costs only 6 %): a call count is not a cost") {
    ZTH_EXPECT_EQ(std::string(most_called_function()), std::string("clamp_pixel"));
}
ZTH_TEST("robust", "5. Amdahl: blur_row is 61 % of the run; 10 times faster gives 1 / (0.39 + 0.061) = 2.2 times, not 10 times") {
    ZTH_EXPECT_NEAR(best_speedup_if_blur_row_is_10x_faster(), 2.217, 0.05);
}
ZTH_TEST("robust", "6. a profile of an unoptimized build is NOT a reliable guide: -O0 keeps everything the optimizer would remove or inline") {
    ZTH_EXPECT_FALSE(debug_build_profile_is_reliable());
}
