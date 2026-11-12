#pragma once
// YOUR FIXED-POINT MATH AND PID CONTROLLER, for a microcontroller without a floating-point unit.
//
// Fixed: a Q16.16 number stored in an int32_t (16 integer bits, 16 fractional bits): value = raw / 65536. Range about -32768 .. 32767.9999.
//   from_raw(r), raw(), from_int(i), from_ratio(num, den), to_int()
//   +  -  *  /  unary -  abs(), comparisons (== != < <= > >=)
//   EVERY operation SATURATES: a result above the largest value is the largest value, below the smallest is the smallest. It never wraps
//   around (a wrapped error of +40000 becomes -25536 and a controller would drive the heater the wrong way at full power).
//   The exact rules (the tests check them bit for bit):
//     add / subtract / negate / abs : exact in 64 bits, then saturate (abs and negation of the minimum raw value give the maximum)
//     multiply : p = a.raw * b.raw in 64 bits; the result is p / 65536 ROUNDED TO NEAREST, ties away from zero; then saturate
//     divide   : raw = a.raw * 65536 / b.raw, rounded to nearest, ties away from zero; then saturate.
//                Division by zero never crashes: it gives the maximum for a positive a, the minimum for a negative a and 0 for 0.
//     from_int(i)       i * 65536, saturated
//     from_ratio(n, d)  n / d as a division of integers n and d (int64), rounded the same way; d == 0 as for divide
//     to_int()          the integer part, truncated toward zero
//
// Pid: a PID controller with these exact rules (all the arithmetic uses the saturating operations above, in this order):
//   update(setpoint, measurement, dt):
//     if dt <= 0: return the previous output and change NOTHING (a clock that did not advance must not corrupt the integral)
//     error      = setpoint - measurement
//     p          = kp * error
//     integral   = clamp( integral + (ki * error) * dt , out_min, out_max )        // anti-windup: the integral term can never exceed the output range
//     d          = has previous measurement ? -( kd * ((measurement - previous_measurement) / dt) ) : 0
//                  (derivative on the MEASUREMENT, not on the error: a jump of the setpoint must not produce a spike; the first sample has no derivative)
//     output     = clamp( (p + integral) + d , out_min, out_max )
//     remember the measurement and the output
//   reset(): back to the initial state.   integral(): the current integral term.
#include <cstdint>
#include <limits>

namespace ctl {

class Fixed {
public:
    constexpr Fixed() = default;
    static constexpr Fixed from_raw(std::int32_t raw) {
        Fixed f;
        f.raw_ = raw;
        return f;
    }
    static constexpr Fixed from_int(int i) {
        (void)i;
        return Fixed{};  // TODO
    }
    static constexpr Fixed from_ratio(std::int64_t num, std::int64_t den) {
        (void)num;
        (void)den;
        return Fixed{};  // TODO
    }
    constexpr std::int32_t raw() const { return raw_; }
    constexpr int to_int() const { return 0; }  // TODO

    friend constexpr Fixed operator+(Fixed a, Fixed b) { return a; /* TODO */ (void)b; }
    friend constexpr Fixed operator-(Fixed a, Fixed b) { return a; /* TODO */ (void)b; }
    friend constexpr Fixed operator*(Fixed a, Fixed b) { return a; /* TODO */ (void)b; }
    friend constexpr Fixed operator/(Fixed a, Fixed b) { return a; /* TODO */ (void)b; }
    friend constexpr Fixed operator-(Fixed a) { return a; /* TODO */ }
    friend constexpr Fixed abs(Fixed a) { return a; /* TODO */ }
    friend constexpr bool operator==(Fixed a, Fixed b) { return a.raw_ == b.raw_; }
    friend constexpr bool operator!=(Fixed a, Fixed b) { return a.raw_ != b.raw_; }
    friend constexpr bool operator<(Fixed a, Fixed b) { return a.raw_ < b.raw_; }
    friend constexpr bool operator<=(Fixed a, Fixed b) { return a.raw_ <= b.raw_; }
    friend constexpr bool operator>(Fixed a, Fixed b) { return a.raw_ > b.raw_; }
    friend constexpr bool operator>=(Fixed a, Fixed b) { return a.raw_ >= b.raw_; }

private:
    std::int32_t raw_ = 0;
};

constexpr Fixed clamp(Fixed x, Fixed lo, Fixed hi) { return x < lo ? lo : (x > hi ? hi : x); }

struct PidConfig {
    Fixed kp, ki, kd;
    Fixed out_min, out_max;
};

class Pid {
public:
    explicit Pid(const PidConfig& config) : config_(config) {}

    Fixed update(Fixed setpoint, Fixed measurement, Fixed dt) {
        (void)setpoint;
        (void)measurement;
        (void)dt;
        return Fixed{};  // TODO
    }
    Fixed integral() const { return Fixed{}; }  // TODO
    void reset() {}                              // TODO

private:
    PidConfig config_;
    // TODO: the integral, the previous measurement (and whether there is one), the last output
};

}  // namespace ctl
