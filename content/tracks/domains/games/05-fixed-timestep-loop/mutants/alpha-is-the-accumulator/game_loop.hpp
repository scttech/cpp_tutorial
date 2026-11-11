// MUTANT: alpha returns the accumulator in milliseconds
#pragma once
// A fixed-timestep loop with an accumulator of integer nanoseconds. The clock is only ever read through the Clock interface.
#include <cstdint>

#include "types.hpp"

namespace loop {

class FixedStepLoop {
public:
    explicit FixedStepLoop(Duration step, int max_steps_per_frame = 5) : step_(step), max_steps_(max_steps_per_frame < 1 ? 1 : max_steps_per_frame) {}

    template <class Update>
    int frame(const Clock& clock, Update&& update) {
        const Duration now = clock.now();
        if (!started_) {  // the first frame only sets the reference
            started_ = true;
            last_ = now;
            return 0;
        }
        const Duration dt = now - last_;
        last_ = now;
        if (dt > Duration{0}) acc_ += dt;  // a clock that went backwards adds nothing
        if (step_ <= Duration{0}) return 0;

        int run = 0;
        while (acc_ >= step_ && run < max_steps_) {
            update(step_);
            acc_ -= step_;
            ++run;
        }
        steps_ += run;
        if (acc_ >= step_) {  // a hitch: the whole steps we could not run are dropped, only the fraction is kept
            const std::int64_t whole = acc_ / step_;
            dropped_ += whole;
            acc_ -= step_ * whole;
        }
        return run;
    }

    double alpha() const { return static_cast<double>(acc_.count()) / 1000000.0; }
    Duration accumulator() const { return acc_; }
    std::int64_t steps_total() const { return steps_; }
    std::int64_t dropped_steps() const { return dropped_; }
    Duration simulated_time() const { return step_ * steps_; }

    void reset(const Clock& clock) {
        last_ = clock.now();
        started_ = true;
        acc_ = Duration{0};
    }

private:
    Duration step_;
    int max_steps_;
    Duration acc_{0};
    Duration last_{0};
    bool started_ = false;
    std::int64_t steps_ = 0;
    std::int64_t dropped_ = 0;
};

}  // namespace loop
