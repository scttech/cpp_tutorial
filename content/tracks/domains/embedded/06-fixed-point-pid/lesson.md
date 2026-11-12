# Fixed-Point PID Controller

> **You're a firmware engineer** on a thermostat. The chip is a Cortex-M0: no floating-point unit, so every `float` operation is a 100-cycle library call, and the control loop must run every millisecond. Control engineers hand you a PID formula in real numbers; your job is to implement it in **Q16.16 fixed point**, with arithmetic that is exact, deterministic (bit-for-bit the same on every run, so you can replay a field failure in the test bench) and *safe at the extremes*.

## Q16.16: an integer with an implied binary point

Store `value * 65536` in an `int32_t`. 1.0 is `65536`, 0.5 is `32768`, the range is about ±32768 and the resolution about 0.000015. Addition and subtraction are plain integer operations; the interesting part is everything else.

- **Multiply:** `(a.raw * b.raw) / 65536`. The product of two 32-bit raw values needs **64 bits**; do it in `int64_t`, then scale back. In 32 bits `200 * 200` overflows (undefined behaviour in C++!).
- **Divide:** `(a.raw * 65536) / b.raw`, again in 64 bits, to keep the fractional bits.
- **Rounding:** truncation bias accumulates over thousands of control steps. Round **to nearest, ties away from zero**: add half the divisor to the *magnitude*, divide, then apply the sign (negative numbers need care: C++ integer division truncates toward zero, and `>>` of a negative number rounds toward minus infinity).
- **Saturation, not wrap-around:** if the true result does not fit, return the largest/smallest value. A wrapped error of `+40000` becomes `-25536`: the heater would be driven at *full power the wrong way*. Negating or taking `abs` of the minimum raw value (−2³¹) must give the maximum, not −2³¹ again.
- **Division by zero** must not trap (a trapped CPU is a heater stuck on): saturate by the sign of the dividend.

## The controller

`output = Kp·e + I + D`, with these decisions that real controllers make:

1. **Anti-windup.** If the output is limited (a heater cannot exceed 100 %) while a big error persists, an unbounded integral keeps growing ("winds up") and after the error flips sign the controller needs a long time to unwind: overshoot, oscillation. Here the integral term itself is **clamped to the output range**.
2. **Derivative on the measurement.** `d(error)/dt` spikes when the *setpoint* jumps ("derivative kick"). Differentiating the measurement avoids it, with a minus sign: `D = -Kd · (m - m_prev) / dt`.
3. **First sample has no derivative** (there is no previous measurement; using 0 would create a fake spike).
4. **Non-positive `dt`** (a timer glitch): do nothing and return the previous output; dividing by `dt` and integrating over it would corrupt the state.

The exact order of operations matters for bit-exact reproducibility, so `pid.hpp` states it.

## The tiers

- **Works**: construction and saturation of `from_int`/`from_ratio`, every operator at the edges (including `INT32_MIN`), rounding ties, division by zero, a **golden table of about 280 operations** from an independent model, the PID terms one at a time (P, I, anti-windup, D, kick, first sample, `dt <= 0`, `reset`).
- **Robust**: four closed-loop scenarios checked **bit for bit** against the model (a heater simulation, windup, a setpoint step with a large `Kd`, a ramp), extreme inputs, determinism.
- **Fast**: no heap, `sizeof(Fixed) == 4`, millions of operations.
- **Clean**: no raw memory or exceptions; **no floating point** (no `float`, `double`, `<cmath>`), no containers; all operations `constexpr`.
