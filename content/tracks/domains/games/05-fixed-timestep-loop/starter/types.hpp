#pragma once
// READ-ONLY. Time, and a clock you can control.
#include <chrono>

namespace loop {

using Duration = std::chrono::nanoseconds;

// Where the loop gets the time from. In the game it reads the real clock; in the tests it is a ManualClock that only moves
// when the test says so. That makes a time-dependent program fully deterministic and testable.
class Clock {
public:
    virtual ~Clock() = default;
    virtual Duration now() const = 0;
};

class ManualClock final : public Clock {
public:
    Duration now() const override { return t_; }
    void advance(Duration d) { t_ += d; }
    void set(Duration t) { t_ = t; }

private:
    Duration t_{0};
};

}  // namespace loop
