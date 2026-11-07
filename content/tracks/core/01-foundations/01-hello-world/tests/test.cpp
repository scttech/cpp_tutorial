#include "zth_test.hpp"

ZTH_TEST("works", "prints exactly \"Hello, World!\" and a newline") {
    auto r = ZTH_RUN_USER_MAIN();
    ZTH_EXPECT_EQ(r.out, "Hello, World!\n");
}

ZTH_TEST("works", "exits with status 0") {
    auto r = ZTH_RUN_USER_MAIN();
    ZTH_EXPECT_EQ(r.exit_code, 0);
}
