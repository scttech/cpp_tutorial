# Game Loop and Fixed Timestep

> **You're an engineer at a game studio.** The physics feels different on every machine: on the dev laptop at 144 fps the ball falls slowly, on the build server (no GPU, 12 fps) it teleports and goes through the floor. Replays desync because the simulation depends on how long each frame happened to take. The fix is the most famous pattern in game programming (Glenn Fiedler's *Fix Your Timestep!*): **simulate in fixed steps, render as often as you like.**

## The loop

Every frame, measure how much time has passed and add it to an **accumulator**. While the accumulator holds at least one `step` (say 10 ms), run `update(step)` and take one step off. What is left (less than a step) waits for the next frame; as a fraction of a step it is `alpha()`, used by renderers to interpolate between the last two states.

```cpp
acc += frame_time;
while (acc >= step) { update(step); acc -= step; }
alpha = acc / step;
```

Because `update` always receives the same `step`, the simulation is **deterministic** and independent of the frame rate: the same total time gives the same number of updates however it is cut up.

## What you must handle

- **The clock is injected.** `frame(clock, update)` reads `clock.now()` (a `Clock` interface, `types.hpp`). The tests use a `ManualClock` that moves only when told, so an hour of game time takes microseconds and every run is identical. The loop must never read `steady_clock` or sleep.
- **The first frame** has no previous time: it records the reference and runs nothing.
- **Integer time.** `std::chrono::nanoseconds` accumulates exactly; a `double` accumulator drifts (after a million frames the step count is off). Convert to `double` only for `alpha()`.
- **The spiral of death.** If a frame takes 200 ms but a step is 10 ms, 20 updates are due; running them takes even longer, so the next frame has even more to catch up on. Cap the work per frame at `max_steps_per_frame`, and **drop** the whole steps beyond the cap (count them in `dropped_steps()`) instead of postponing them. Keep the fractional remainder.
- **A clock that goes backwards** (NTP adjustment, a bug) adds no time; the new time becomes the reference.
- **`reset(clock)`** after a pause (a menu, a breakpoint) so the paused time is not simulated.
- A `step <= 0` must not loop forever.
- **The callback is a template parameter**, not a `std::function`: the call is inlined and nothing allocates.

## The tiers

- **Works**: first frame, whole steps and the accumulator, the step handed to `update`, alpha, frame-rate independence, determinism.
- **Robust**: exact multiples, hitches with dropping, exactly `max_steps`, silly `max_steps`/`step`, a backwards clock, `reset`, a million frames with exact integer arithmetic, random frame times with an accounting identity.
- **Fast**: no allocation in 2 million frames; work bounded by `max_steps` not by the pause length (a 1 ns step and one-hour pauses); an hour of game time in no real time.
- **Clean**: no raw memory, no real clock or sleeping, integer accumulator, template callback.
