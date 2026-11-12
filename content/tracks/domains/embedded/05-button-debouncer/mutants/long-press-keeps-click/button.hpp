// MUTANT: a long press still produces a click
#pragma once
// A handful of small counters, updated in a fixed order each tick.
#include <cstdint>

#include "types.hpp"

namespace btn {

class Button {
public:
    explicit Button(const ButtonConfig& config = ButtonConfig{}) : config_(config), debounce_(config.debounce_ticks == 0 ? 1 : config.debounce_ticks) {}

    std::uint8_t update(bool raw) {
        std::uint8_t events = kNone;

        // A. timers of the state we were in
        if (stable_) {
            if (held_ < 0xFFFF) ++held_;
            if (!long_reported_ && config_.long_press_ticks > 0 && held_ == config_.long_press_ticks) {
                events |= kLongPress;
                long_reported_ = true;
            }
        }
        if (window_open_) {
            if (since_click_ < 0xFFFF) ++since_click_;
            if (since_click_ > config_.double_click_ticks) window_open_ = false;
        }

        // B. debounce
        if (raw != stable_) {
            if (++count_ >= debounce_) {
                stable_ = raw;
                count_ = 0;
                if (stable_) {
                    events |= kPressed;
                    held_ = 0;
                    long_reported_ = false;
                } else {
                    events |= kReleased;
                    if (false) {
                        window_open_ = false;
                    } else if (window_open_) {
                        events |= kDoubleClick;
                        window_open_ = false;
                    } else {
                        events |= kClick;
                        window_open_ = true;
                        since_click_ = 0;
                    }
                }
            }
        } else {
            count_ = 0;
        }
        return events;
    }

    bool pressed() const { return stable_; }
    std::uint16_t held_ticks() const { return stable_ ? held_ : std::uint16_t{0}; }

    void reset() {
        stable_ = false;
        count_ = 0;
        held_ = 0;
        long_reported_ = false;
        window_open_ = false;
        since_click_ = 0;
    }

private:
    ButtonConfig config_;
    std::uint16_t debounce_;
    bool stable_ = false;
    bool long_reported_ = false;
    bool window_open_ = false;
    std::uint16_t count_ = 0;
    std::uint16_t held_ = 0;
    std::uint16_t since_click_ = 0;
};

}  // namespace btn
