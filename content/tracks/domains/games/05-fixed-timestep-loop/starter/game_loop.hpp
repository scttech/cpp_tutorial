#pragma once
// YOUR FIXED-TIMESTEP LOOP. The simulation advances in steps of exactly `step`, however long the frames take; leftover time
// is kept in an accumulator for the next frame.
//
//   FixedStepLoop(step, max_steps_per_frame = 5)
//   frame(clock, update)   Call once per frame. Reads clock.now(), adds the time since the previous frame to the accumulator, and
//                          calls update(step) while the accumulator holds a whole step (at most max_steps_per_frame times).
//                          Returns the number of updates run.
//        - the FIRST call only records the time: no time has passed yet, so no update runs
//        - if the clock went backwards, no time is added (and the new time becomes the reference)
//        - if more whole steps are due than max_steps_per_frame (a long hitch), the extra whole steps are DROPPED, not postponed:
//          afterwards the accumulator is below one step. dropped_steps() counts them. (Otherwise one slow frame makes the next
//          one slower still: the "spiral of death".)
//        - a step <= 0 never runs an update; max_steps_per_frame < 1 means 1
//   alpha()                the accumulator as a fraction of a step, in [0, 1): how far between two simulated states we are
//   accumulator(), steps_total(), dropped_steps(), simulated_time() (= steps_total * step)
//   reset(clock)           forget the old reference time and the accumulator (after a pause); counters stay
//
// Use integer time (std::chrono::nanoseconds) for the accumulator: floating point drifts. The update callback is a template
// parameter, not a std::function. The loop never reads a real clock itself.
#include <cstdint>

#include "types.hpp"

namespace loop {

class FixedStepLoop {
public:
    explicit FixedStepLoop(Duration step, int max_steps_per_frame = 5) : step_(step), max_steps_(max_steps_per_frame) {}

    template <class Update>
    int frame(const Clock& clock, Update&& update) {
        (void)clock;
        (void)update;
        return 0;  // TODO
    }

    double alpha() const { return 0; }  // TODO
    Duration accumulator() const { return acc_; }
    std::int64_t steps_total() const { return 0; }
    std::int64_t dropped_steps() const { return 0; }
    Duration simulated_time() const { return Duration{0}; }
    void reset(const Clock& clock) { (void)clock; }

private:
    Duration step_;
    int max_steps_;
    Duration acc_{0};
    // TODO: the reference time, whether the first frame has happened, the counters
};

}  // namespace loop
