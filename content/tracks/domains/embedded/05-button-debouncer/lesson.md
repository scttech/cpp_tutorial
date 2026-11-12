# Button Debouncer

> **You're a firmware engineer.** A mechanical switch does not produce a clean edge: when the contacts close they *bounce*, giving 1-0-1-1-0-1-1-1 for a few milliseconds. Read raw, one press can look like five. Your product also wants more than "down" and "up": a **click**, a **double click**, and a **long press** (to open the menu or power off). All of it must run inside a 1 ms timer interrupt, so: integer counters only, no heap, no floating point, a handful of bytes of state.

## Debouncing = counting consecutive samples

Keep the **debounced** (stable) state and a counter. When the raw level differs from the stable one, add 1; when it equals it, reset the counter to 0. When the counter reaches `debounce_ticks`, the stable state flips and the counter restarts. A glitch shorter than `debounce_ticks` (or a bounce that returns to the old level) never gets there. Two details: a debounce of `0` must act as `1` (flip at once), and the count is of **consecutive** ticks, not total.

## Events are bit flags

More than one thing can happen in a single tick (the release of a short press is `Released | Click`), so `update()` returns a `std::uint8_t` of `Event` flags, not one enum value. You test with `ev & kClick` and combine with `|`.

## The order of work in a tick matters

Timers (`held`, the click window) are advanced **first**, using the state the button was in *before* this tick; then the debounce logic may flip the state. If you advance them after, a press and its first `held++` land in the same tick and every long-press and double-click boundary moves by one. The golden scripts pin down every boundary tick by tick.

| Concept | Rule |
|---|---|
| `Pressed` | stable state became pressed; `held = 0`, "long reported" cleared |
| `LongPress` | once per press, when `held` reaches `long_press_ticks` (0 = disabled); a long press **cancels the click** and closes the window |
| `Click` | a short release when no window is open; opens the window |
| `DoubleClick` | a short release while the window is open; **replaces** `Click` and closes the window (so the third click is a plain click again) |
| window | open after a click; counts ticks since that release; closes when it exceeds `double_click_ticks` |

## Counters saturate

`held` and the window counter are `uint16_t`. After 65536 ticks (65 seconds at 1 kHz) a counter that wraps would report a second long press or reopen a closed click window. Increment only while below `0xFFFF`.

## The tiers

- **Works**: the debounce boundary, bounces, held ticks, the combined `Released | Click` flags, `reset`, and **20 golden scripts** (twelve hand-picked and eight random ones) checked tick by tick against an independent model.
- **Robust**: long-press timing, the click that a long press cancels, window boundaries, 70000-tick saturation tests, invariants over 5000 random ticks.
- **Fast**: no heap, `sizeof(Button) <= 24`, ten million ticks.
- **Clean**: no raw memory or exceptions, no floating point, no containers, strings or `<chrono>` (this runs in an interrupt).
