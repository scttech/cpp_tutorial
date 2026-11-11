// A simulation with a fixed step of 10 ms, fed with the frame times of a fake clock: a steady 16 ms, then a hitch of 120 ms,
// then a very long pause. Read-only. Press Run.
#include <chrono>
#include <cstdio>

#include "game_loop.hpp"

int main() {
    using namespace std::chrono_literals;
    loop::ManualClock clock;
    loop::FixedStepLoop game(10ms, /*max_steps_per_frame=*/5);

    double x = 0;  // a ball moving at 100 units per second
    auto update = [&](loop::Duration step) { x += 100.0 * std::chrono::duration<double>(step).count(); };

    const loop::Duration frames[] = {0ms, 16ms, 16ms, 16ms, 120ms, 16ms, 5000ms, 16ms};
    int n = 0;
    for (const loop::Duration dt : frames) {
        clock.advance(dt);
        const int steps = game.frame(clock, update);
        std::printf("frame %d (+%4lld ms): %d update(s), x = %6.2f, alpha = %.2f, dropped so far: %lld\n", n++, static_cast<long long>(dt.count() / 1000000), steps, x, game.alpha(),
                    static_cast<long long>(game.dropped_steps()));
    }
    std::printf("simulated %lld ms in %lld steps\n", static_cast<long long>(game.simulated_time().count() / 1000000), static_cast<long long>(game.steps_total()));
}
