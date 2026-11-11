#define ZTH_TRACK_ALLOCATIONS
#include <chrono>
#include <cstdint>
#include <vector>

#include "game_loop.hpp"
#include "zth_test.hpp"

using namespace loop;
using namespace std::chrono_literals;

namespace {

// advance the clock by dt, run one frame, return the number of updates
template <class L>
int tick(L& game, ManualClock& clock, Duration dt, int* calls = nullptr) {
    clock.advance(dt);
    return game.frame(clock, [&](Duration) {
        if (calls) ++*calls;
    });
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "the first frame only records the time; no update runs") {
    ManualClock clock;
    clock.set(5s);
    FixedStepLoop game(10ms);
    int calls = 0;
    ZTH_EXPECT_EQ(game.frame(clock, [&](Duration) { ++calls; }), 0);
    ZTH_EXPECT_EQ(calls, 0);
    ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{0});
    ZTH_EXPECT_TRUE(game.accumulator() == Duration{0});
}
ZTH_TEST("works", "whole steps are run, the rest waits in the accumulator") {
    ManualClock clock;
    FixedStepLoop game(10ms);
    tick(game, clock, 0ms);
    ZTH_EXPECT_EQ(tick(game, clock, 16ms), 1);
    ZTH_EXPECT_TRUE(game.accumulator() == 6ms);
    ZTH_EXPECT_EQ(tick(game, clock, 4ms), 1);   // 6 + 4 = 10: exactly one step
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
    ZTH_EXPECT_EQ(tick(game, clock, 9ms), 0);
    ZTH_EXPECT_TRUE(game.accumulator() == 9ms);
    ZTH_EXPECT_EQ(tick(game, clock, 26ms), 3);  // 9 + 26 = 35
    ZTH_EXPECT_TRUE(game.accumulator() == 5ms);
    ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{5});
    ZTH_EXPECT_TRUE(game.simulated_time() == 50ms);
}
ZTH_TEST("works", "update receives the step, not the frame time") {
    ManualClock clock;
    FixedStepLoop game(25ms);
    tick(game, clock, 0ms);
    std::vector<Duration> seen;
    clock.advance(80ms);
    game.frame(clock, [&](Duration d) { seen.push_back(d); });
    ZTH_EXPECT_EQ(seen.size(), std::size_t{3});
    for (const Duration d : seen) ZTH_EXPECT_TRUE(d == 25ms);
}
ZTH_TEST("works", "alpha is the leftover fraction of a step") {
    ManualClock clock;
    FixedStepLoop game(10ms);
    tick(game, clock, 0ms);
    tick(game, clock, 15ms);
    ZTH_EXPECT_EQ(game.alpha(), 0.5);
    tick(game, clock, 7ms);  // 5 + 7 = 12 -> one step, 2 left
    ZTH_EXPECT_NEAR(game.alpha(), 0.2, 1e-12);
    tick(game, clock, 8ms);  // 2 + 8 = 10 -> one step, 0 left
    ZTH_EXPECT_EQ(game.alpha(), 0.0);
}
ZTH_TEST("works", "the same total time gives the same number of steps however it is cut into frames") {
    for (const int frame_ms : {1, 3, 7, 10, 16, 33, 50}) {
        ManualClock clock;
        FixedStepLoop game(10ms, 1000);
        tick(game, clock, 0ms);
        for (int t = 0; t < 1000 / frame_ms; ++t) tick(game, clock, std::chrono::milliseconds(frame_ms));
        const int total_ms = (1000 / frame_ms) * frame_ms;
        ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{total_ms / 10});
    }
}
ZTH_TEST("works", "a simulation is deterministic: the same frame times give the same state, whatever the real speed") {
    auto run = [](const std::vector<int>& frame_ms) {
        ManualClock clock;
        FixedStepLoop game(10ms, 100);
        long long state = 1;
        tick(game, clock, 0ms);
        for (int ms : frame_ms) {
            clock.advance(std::chrono::milliseconds(ms));
            game.frame(clock, [&](Duration d) { state = state * 31 + d.count() / 1000000; });
        }
        return state;
    };
    const std::vector<int> a = {16, 17, 16, 33, 16, 5, 5, 50, 11};
    ZTH_EXPECT_EQ(run(a), run(a));
    // and the same total time in different frame sizes gives the same simulation
    ZTH_EXPECT_EQ(run({40, 40, 40}), run({20, 20, 20, 20, 20, 20}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a frame of exactly one step (or several) leaves nothing in the accumulator") {
    ManualClock clock;
    FixedStepLoop game(10ms);
    tick(game, clock, 0ms);
    ZTH_EXPECT_EQ(tick(game, clock, 10ms), 1);
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
    ZTH_EXPECT_EQ(tick(game, clock, 30ms), 3);
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
    ZTH_EXPECT_EQ(game.alpha(), 0.0);
    ZTH_EXPECT_EQ(tick(game, clock, 0ms), 0);  // a frame in which the clock did not move
}
ZTH_TEST("robust", "a long hitch runs at most max_steps updates and DROPS the rest (no spiral of death)") {
    ManualClock clock;
    FixedStepLoop game(10ms, 5);
    tick(game, clock, 0ms);
    int calls = 0;
    ZTH_EXPECT_EQ(tick(game, clock, 10s + 3ms, &calls), 5);
    ZTH_EXPECT_EQ(calls, 5);
    ZTH_EXPECT_TRUE(game.accumulator() == 3ms);                // the fraction survives
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{1000 - 5});
    ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{5});
    ZTH_EXPECT_TRUE(game.simulated_time() == 50ms);              // only the steps that ran count as simulated time
    // the next normal frame is normal again
    ZTH_EXPECT_EQ(tick(game, clock, 7ms), 1);
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{995});
}
ZTH_TEST("robust", "a frame that needs exactly max_steps drops nothing; one more step is dropped") {
    ManualClock clock;
    FixedStepLoop game(10ms, 3);
    tick(game, clock, 0ms);
    ZTH_EXPECT_EQ(tick(game, clock, 30ms), 3);
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{0});
    ZTH_EXPECT_EQ(tick(game, clock, 40ms), 3);
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{1});
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
}
ZTH_TEST("robust", "max_steps below 1 means 1; max_steps 1 works") {
    for (const int bad : {0, -4}) {
        ManualClock clock;
        FixedStepLoop game(10ms, bad);
        tick(game, clock, 0ms);
        ZTH_EXPECT_EQ(tick(game, clock, 50ms), 1);
    }
}
ZTH_TEST_TIMEOUT("robust", "a step of zero or less never runs an update and never hangs", 5) {
    for (const Duration step : {0ms, -5ms}) {
        ManualClock clock;
        FixedStepLoop game(step);
        tick(game, clock, 0ms);
        int calls = 0;
        ZTH_EXPECT_EQ(tick(game, clock, 1s, &calls), 0);
        ZTH_EXPECT_EQ(calls, 0);
        ZTH_EXPECT_EQ(game.alpha(), 0.0);
    }
}
ZTH_TEST("robust", "a clock that goes backwards adds no time, and the new time becomes the reference") {
    ManualClock clock;
    FixedStepLoop game(10ms);
    tick(game, clock, 100ms);  // first frame
    ZTH_EXPECT_EQ(tick(game, clock, 6ms), 0);
    ZTH_EXPECT_TRUE(game.accumulator() == 6ms);
    clock.set(40ms);  // 66 ms back
    ZTH_EXPECT_EQ(game.frame(clock, [](Duration) {}), 0);
    ZTH_EXPECT_TRUE(game.accumulator() == 6ms);
    ZTH_EXPECT_EQ(tick(game, clock, 5ms), 1);  // counted from 40 ms: 6 + 5 = 11
    ZTH_EXPECT_TRUE(game.accumulator() == 1ms);
}
ZTH_TEST("robust", "reset(clock) forgets the pause: the time that passed before it is not simulated") {
    ManualClock clock;
    FixedStepLoop game(10ms);
    tick(game, clock, 0ms);
    tick(game, clock, 34ms);   // 3 steps, 4 ms left
    clock.advance(2h);  // the game was paused
    game.reset(clock);
    ZTH_EXPECT_TRUE(game.accumulator() == 0ms);
    ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{3});   // the counters survive a reset
    ZTH_EXPECT_TRUE(game.simulated_time() == 30ms);
    ZTH_EXPECT_EQ(tick(game, clock, 12ms), 1);
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{0});
    ZTH_EXPECT_TRUE(game.accumulator() == 2ms);
    ZTH_EXPECT_EQ(game.steps_total(), std::int64_t{4});
}
ZTH_TEST_TIMEOUT("robust", "a million frames of 16.666667 ms: the step count is exact (integer time does not drift)", 10) {
    ManualClock clock;
    FixedStepLoop game(10ms, 10);
    tick(game, clock, 0ms);
    const Duration frame = 16666667ns;
    for (int i = 0; i < 1000000; ++i) tick(game, clock, frame);
    const Duration total = frame * 1000000;
    ZTH_EXPECT_EQ(game.steps_total(), static_cast<std::int64_t>(total / 10ms));
    ZTH_EXPECT_TRUE(game.accumulator() == total % 10ms);
    ZTH_EXPECT_EQ(game.dropped_steps(), std::int64_t{0});
}
ZTH_TEST_TIMEOUT("robust", "random frame times: steps + dropped always account for all the time, alpha stays in [0, 1)", 10) {
    ::zth::Lcg rng(8);
    ManualClock clock;
    FixedStepLoop game(7ms, 4);
    tick(game, clock, 0ms);
    Duration total{0};
    for (int i = 0; i < 20000; ++i) {
        const Duration dt = std::chrono::microseconds(rng.below(60000));
        total += dt;
        const int n = tick(game, clock, dt);
        ZTH_EXPECT_TRUE(n >= 0 && n <= 4);
        ZTH_EXPECT_TRUE(game.alpha() >= 0.0 && game.alpha() < 1.0);
        ZTH_EXPECT_TRUE(game.accumulator() >= Duration{0} && game.accumulator() < 7ms);
    }
    ZTH_EXPECT_TRUE(total == 7ms * (game.steps_total() + game.dropped_steps()) + game.accumulator());
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "the loop never allocates: 2 million frames with a lambda callback", 10) {
    ManualClock clock;
    FixedStepLoop game(10ms, 8);
    tick(game, clock, 0ms);
    long long sum = 0;
    zth::mem::Scope scope;
    for (int i = 0; i < 2000000; ++i) {
        clock.advance(11ms);
        game.frame(clock, [&](Duration d) { sum += d.count(); });
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(sum > 0);
}
ZTH_TEST_TIMEOUT("fast", "the work of a frame is proportional to the updates it runs, never to the length of the pause", 10) {
    ManualClock clock;
    FixedStepLoop game(1ns, 3);  // a 1 ns step: a pause of one hour is 3.6 trillion steps if they were postponed
    tick(game, clock, 0ns);
    int calls = 0;
    for (int i = 0; i < 1000; ++i) tick(game, clock, 1h, &calls);
    ZTH_EXPECT_EQ(calls, 3000);
    ZTH_EXPECT_TRUE(game.dropped_steps() > 0);
}
ZTH_TEST_TIMEOUT("fast", "simulating one hour at 60 updates per second takes no real waiting: the loop only reads the injected clock", 10) {
    ManualClock clock;
    FixedStepLoop game(std::chrono::nanoseconds(16666667), 5);
    tick(game, clock, 0ms);
    long long updates = 0;
    for (int i = 0; i < 3600 * 60; ++i) {
        clock.advance(std::chrono::nanoseconds(16666667));
        game.frame(clock, [&](Duration) { ++updates; });
    }
    ZTH_EXPECT_EQ(updates, 3600LL * 60);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("game_loop.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("game_loop.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the loop never touches a real clock or sleeps: time comes only from the Clock it is given") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("game_loop.hpp"));
    for (const char* word : {"steady_clock", "system_clock", "high_resolution_clock", "sleep_for", "sleep_until", "this_thread", "clock", "time", "gettimeofday"}) {
        if (std::string(word) == "clock" || std::string(word) == "time") continue;  // parameter names
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("game_loop.hpp uses `") + word + "`");
            return;
        }
    }
    ZTH_EXPECT_TRUE(code.find("::now()") == std::string::npos || code.find("clock.now()") != std::string::npos);
}
ZTH_TEST("clean", "integer time in the accumulator, a template callback: no float accumulator, no std::function") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("game_loop.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "float").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "function").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "template").empty());
}
