#pragma once
// READ-ONLY. Configuration and events of the button logic. Time is a number of calls to update(): one call = one tick (say, 1 ms from a timer interrupt).
#include <cstdint>

namespace btn {

struct ButtonConfig {
    std::uint16_t debounce_ticks = 3;      // the raw level must differ from the stable one for this many CONSECUTIVE ticks to count (0 is treated as 1)
    std::uint16_t long_press_ticks = 50;   // a press held this many ticks AFTER the Pressed event is a long press (0 disables long presses)
    std::uint16_t double_click_ticks = 20; // a second click this many ticks (or fewer) after the first release is a double click
};

// What happened during one tick, as bit flags: more than one can happen in the same tick (for example Released and Click).
enum Event : std::uint8_t {
    kNone = 0,
    kPressed = 1,      // the debounced state became "pressed"
    kReleased = 2,     // the debounced state became "released"
    kClick = 4,        // a release that was not a long press and not the second of a double click
    kLongPress = 8,    // the press has lasted long_press_ticks (reported once per press, while still held)
    kDoubleClick = 16, // a release that was not a long press, within the window after the previous click's release (replaces kClick)
};

}  // namespace btn
