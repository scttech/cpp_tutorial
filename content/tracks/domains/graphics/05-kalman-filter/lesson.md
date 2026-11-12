# A Kalman Filter

> **You're on the robotics team.** The robot's wheel encoders say it moved 0.98 m, the GPS says 1.2 m, and the IMU says it accelerated for 0.4 s. All three are wrong by a little and wrong differently. A **Kalman filter** is the recipe for combining a *model* of how things move with *noisy measurements* to get the best estimate, and, just as important, an honest statement of *how uncertain that estimate is*. It flew Apollo, it sits in every phone's GPS and every drone's autopilot, and it is about thirty lines of linear algebra.

## State and covariance

The filter carries two things:

- the **state** `x`, e.g. (position, velocity);
- the **covariance** `P`, a matrix saying how uncertain `x` is, including how the errors in different components are *correlated*. `P(0,0)` is the variance of the position error.

Two steps repeat forever:

1. **Predict.** Move the state forward with the model, `x = F x`, and let the uncertainty grow: `P = F P Fᵀ + Q`. `Q` is the *process noise*: the model is not perfect (the object might accelerate), so every prediction adds doubt. For the nearly-constant-velocity model, `Q = q * [[dt⁴/4, dt³/2], [dt³/2, dt²]]`: random accelerations over the interval `dt` change the position by `½ a dt²` and the velocity by `a dt`.
2. **Update.** A measurement `z = H·x + noise(R)` arrives. The *innovation* `y = z - H·x` is the surprise; its variance is `S = H P Hᵀ + R`. The **Kalman gain** `K = P Hᵀ / S` says how much of the surprise to believe, in every component of the state (that is how a *position* measurement corrects the *velocity*: through the correlation stored in `P`). `x += K y`, and `P` shrinks.

With a single scalar measurement `S` is a number: **no matrix inverse is needed**. Real systems with several sensors either process them one at a time like this, or invert a small `S` matrix.

## Three ways it goes wrong (and these tests check them)

- **Joseph form.** The textbook update `P = (I - K H) P` is correct only for the exact optimal gain, and rounding errors make it lose symmetry and positive-definiteness. The Joseph form `P = (I - K H) P (I - K H)ᵀ + K R Kᵀ` is a sum of two positive semi-definite terms, so it *cannot* go negative. In the tests a tiny `R` and a huge prior make the short form collapse to exactly `P = 0`: the filter believes it is perfectly certain, the gain becomes zero, and it **ignores every future measurement** forever. Then also enforce symmetry explicitly: replace `P` by `(P + Pᵀ) / 2`.
- **Outliers.** One wild reading (a GPS jump, a glitch) drags the estimate away and, worse, the filter then *trusts* it. The **normalised innovation squared** `y² / S` is chi-square distributed if the filter is healthy (for one measurement, 9 means "3 sigma"). Reject readings above a gate: a *gated* update. A rejected measurement must leave the filter **exactly** unchanged.
- **Garbage in.** NaN readings, `R <= 0`, non-finite model matrices: refuse them, change nothing, say so. A NaN that enters `P` contaminates every later step and never leaves.

## The tiers

- **Works**: the model, a by-hand scalar example, a two-state update, golden numbers from an independent implementation, steady state, convergence, gating, refusals, symmetry.
- **Robust**: collapse resistance (tiny `R`), 10000 steps with awkward `dt`, `q` and `R`, bad readings not disturbing the stream, gated vs ungated on 5% outliers, variable `dt`, a 3-state filter, fused sensors.
- **Fast**: no allocations; a million steps.
- **Clean**: no raw memory or exceptions; no matrix inverse; the Joseph form is written out.
