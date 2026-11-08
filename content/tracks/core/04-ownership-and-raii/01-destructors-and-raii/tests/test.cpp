#define ZTH_TRACK_ALLOCATIONS
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "guard.hpp"
#include "zth_test.hpp"

namespace {
using Log = std::vector<std::string>;
}

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a guard runs its function when the scope ends, and not before") {
    int runs = 0;
    {
        ScopeGuard g([&] { ++runs; });
        ZTH_EXPECT_EQ(runs, 0);
    }
    ZTH_EXPECT_EQ(runs, 1);
}
ZTH_TEST("works", "dismiss() cancels the cleanup, and active() says whether it will still run") {
    int runs = 0;
    {
        ScopeGuard g([&] { ++runs; });
        ZTH_EXPECT_TRUE(g.active());
        g.dismiss();
        ZTH_EXPECT_FALSE(g.active());
    }
    ZTH_EXPECT_EQ(runs, 0);
}
ZTH_TEST("works", "a Session logs `open <name>` when it is made and `close <name>` when it ends") {
    Log log;
    {
        Session s(log, "db");
        ZTH_EXPECT_EQ(log, (Log{"open db"}));
        ZTH_EXPECT_EQ(s.name(), std::string("db"));
    }
    ZTH_EXPECT_EQ(log, (Log{"open db", "close db"}));
}
ZTH_TEST("works", "sessions in one scope are closed in the REVERSE order they were opened") {
    Log log;
    {
        Session a(log, "a");
        Session b(log, "b");
        Session c(log, "c");
    }
    ZTH_EXPECT_EQ(log, (Log{"open a", "open b", "open c", "close c", "close b", "close a"}));
}
ZTH_TEST("works", "a Session releases its token: nothing leaks and nothing is freed twice") {
    zth::mem::Scope scope;
    {
        Log log;
        Session a(log, "a");
        Session b(log, "b");
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("works", "every session has its own token: 1, 2, 3, ...") {
    Log log;
    Session a(log, "a");
    Session b(log, "b");
    Session c(log, "c");
    ZTH_EXPECT_EQ(b.token(), a.token() + 1);
    ZTH_EXPECT_EQ(c.token(), b.token() + 1);
}

// ---- Robust --------------------------------------------------------------------------------------
namespace {
int early_return(int& runs, bool leave) {
    ScopeGuard g([&] { ++runs; });
    if (leave) return 1;
    return 2;
}
void throws_through(int& runs) {
    ScopeGuard g([&] { ++runs; });
    throw std::runtime_error("boom");
}
void session_throws(Log& log) {
    Session s(log, "s");
    throw std::runtime_error("boom");
}
}  // namespace

ZTH_TEST("robust", "the cleanup also runs on an early return") {
    int a = 0, b = 0;
    ZTH_EXPECT_EQ(early_return(a, true), 1);
    ZTH_EXPECT_EQ(early_return(b, false), 2);
    ZTH_EXPECT_EQ(a, 1);
    ZTH_EXPECT_EQ(b, 1);
}
ZTH_TEST("robust", "the cleanup runs when an exception passes through, and the exception still reaches the caller") {
    int runs = 0;
    bool caught = false;
    try {
        throws_through(runs);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    ZTH_EXPECT_TRUE(caught);
    ZTH_EXPECT_EQ(runs, 1);
}
ZTH_TEST("robust", "a Session is closed (and its token released) when an exception passes through") {
    zth::mem::Scope scope;
    {
        Log log;
        try {
            session_throws(log);
        } catch (const std::runtime_error&) {
        }
        ZTH_EXPECT_EQ(log, (Log{"open s", "close s"}));
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "the cleanup runs exactly once, even if dismiss() is called more than once") {
    int runs = 0;
    {
        ScopeGuard g([&] { ++runs; });
    }
    ZTH_EXPECT_EQ(runs, 1);
    {
        ScopeGuard g([&] { ++runs; });
        g.dismiss();
        g.dismiss();
    }
    ZTH_EXPECT_EQ(runs, 1);
}
ZTH_TEST("robust", "a cleanup that throws does not end the program (a destructor must not let exceptions out)") {
    bool ran = false;
    {
        ScopeGuard g([&] {
            ran = true;
            throw std::runtime_error("cleanup failed");
        });
    }
    ZTH_EXPECT_TRUE(ran);  // we got here: the program is still alive
}
ZTH_TEST("robust", "a guard built from an empty function is harmless") {
    {
        ScopeGuard g{std::function<void()>{}};
        ZTH_EXPECT_FALSE(g.active());
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "guards run in the reverse order of their creation") {
    std::string order;
    {
        ScopeGuard a([&] { order += "a"; });
        ScopeGuard b([&] { order += "b"; });
        ScopeGuard c([&] { order += "c"; });
    }
    ZTH_EXPECT_EQ(order, std::string("cba"));
}
ZTH_TEST("robust", "an inner scope cleans up before the outer one") {
    Log log;
    {
        Session outer(log, "outer");
        {
            Session inner(log, "inner");
        }
        log.push_back("between");
    }
    ZTH_EXPECT_EQ(log, (Log{"open outer", "open inner", "close inner", "between", "close outer"}));
}
ZTH_TEST("robust", "a guard in a loop body runs once per iteration, whether it ends by continue, break or falling off the end") {
    int runs = 0;
    for (int i = 0; i < 5; ++i) {
        ScopeGuard g([&] { ++runs; });
        if (i == 1) continue;
        if (i == 3) break;
    }
    ZTH_EXPECT_EQ(runs, 4);  // i = 0, 1, 2, 3
}
ZTH_TEST("robust", "many sessions in a loop do not leak") {
    zth::mem::Scope scope;
    {
        Log log;
        for (int i = 0; i < 200; ++i) Session s(log, "x");
        ZTH_EXPECT_EQ(log.size(), std::size_t{400});
    }
    ZTH_EXPECT_NO_MEMORY_BUGS(scope);
}
ZTH_TEST("robust", "ScopeGuard and Session cannot be copied (that would run the cleanup twice / free the token twice)") {
    ZTH_EXPECT_FALSE((std::is_copy_constructible_v<ScopeGuard>));
    ZTH_EXPECT_FALSE((std::is_copy_assignable_v<ScopeGuard>));
    ZTH_EXPECT_FALSE((std::is_copy_constructible_v<Session>));
    ZTH_EXPECT_FALSE((std::is_copy_assignable_v<Session>));
}
ZTH_TEST("robust", "destructors do not throw (noexcept)") {
    ZTH_EXPECT_TRUE((std::is_nothrow_destructible_v<ScopeGuard>));
    ZTH_EXPECT_TRUE((std::is_nothrow_destructible_v<Session>));
}
