#pragma once
// YOUR BUTTON LOGIC: debounce a raw switch signal and turn it into press, release, click, double-click and long-press events.
// Everything is integer arithmetic on small counters: it runs inside a timer interrupt on a tiny MCU. No heap, no floating point.
//
//   Button(config)               starts released, all counters zero (use types.hpp for the config and the Event flags)
//   std::uint8_t update(bool raw)   call once per tick with the raw (bouncing) level; returns the Event flags that happened in this tick
//   pressed()                    the debounced state          held_ticks()   ticks since the Pressed event (0 when released)
//   reset()                      back to the initial state
//
// ORDER OF THE WORK IN ONE TICK (it matters!):
//   A. timers of the state we were in before this tick:
//        * if currently pressed: held++ (saturating at 65535); when held reaches long_press_ticks (and long_press_ticks > 0) and no long press was
//          reported for this press yet: report kLongPress, once.
//        * if a click window is open: since_click++; when it exceeds double_click_ticks the window closes.
//   B. debounce: if raw differs from the debounced state, count it; when the count reaches debounce_ticks (0 counts as 1) the debounced state flips
//      and the count restarts. If raw equals the debounced state, the count resets to 0 (bounces that return to the old level do not add up).
//      * flip to pressed: kPressed; held = 0; the "long press reported" flag is cleared.
//      * flip to released: kReleased, and then
//          - if a long press was reported during this press: no click at all and the click window closes.
//          - else if a click window is open: kDoubleClick (instead of kClick) and the window closes (a third click starts again with a plain click).
//          - else: kClick, the window opens (since_click = 0).
#include <cstdint>

#include "types.hpp"

namespace btn {

class Button {
public:
    explicit Button(const ButtonConfig& config = ButtonConfig{}) : config_(config) {}

    std::uint8_t update(bool raw) {
        (void)raw;
        return kNone;  // TODO
    }

    bool pressed() const { return false; }              // TODO
    std::uint16_t held_ticks() const { return 0; }      // TODO
    void reset() {}                                      // TODO

private:
    ButtonConfig config_;
    // TODO: the debounced state, the debounce counter, held, the long-press flag, the click window (open flag + counter)
};

}  // namespace btn
