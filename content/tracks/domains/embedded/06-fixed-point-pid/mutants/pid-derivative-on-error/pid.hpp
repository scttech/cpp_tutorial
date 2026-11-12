// MUTANT: the derivative acts on the error (derivative kick)
#pragma once
// Saturating Q16.16 arithmetic in 64-bit intermediates, and a PID controller built only from those operations.
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
    static constexpr Fixed from_int(int i) { return saturate(static_cast<std::int64_t>(i) * kOne); }
    static constexpr Fixed from_ratio(std::int64_t num, std::int64_t den) {
        if (den == 0) return by_zero(num);
        return saturate(round_div(abs64(num) * kOne, abs64(den), (num < 0) != (den < 0)));
    }
    constexpr std::int32_t raw() const { return raw_; }
    constexpr int to_int() const { return static_cast<int>(raw_ / kOne); }   // integer division truncates toward zero

    friend constexpr Fixed operator+(Fixed a, Fixed b) { return saturate(static_cast<std::int64_t>(a.raw_) + b.raw_); }
    friend constexpr Fixed operator-(Fixed a, Fixed b) { return saturate(static_cast<std::int64_t>(a.raw_) - b.raw_); }
    friend constexpr Fixed operator-(Fixed a) { return saturate(-static_cast<std::int64_t>(a.raw_)); }
    friend constexpr Fixed abs(Fixed a) { return saturate(abs64(a.raw_)); }
    friend constexpr Fixed operator*(Fixed a, Fixed b) {
        const std::int64_t p = static_cast<std::int64_t>(a.raw_) * b.raw_;
        const std::int64_t magnitude = (abs64(p) + kOne / 2) / kOne;
        return saturate(p < 0 ? -magnitude : magnitude);
    }
    friend constexpr Fixed operator/(Fixed a, Fixed b) {
        if (b.raw_ == 0) return by_zero(a.raw_);
        return saturate(round_div(abs64(a.raw_) * kOne, abs64(b.raw_), (a.raw_ < 0) != (b.raw_ < 0)));
    }
    friend constexpr bool operator==(Fixed a, Fixed b) { return a.raw_ == b.raw_; }
    friend constexpr bool operator!=(Fixed a, Fixed b) { return a.raw_ != b.raw_; }
    friend constexpr bool operator<(Fixed a, Fixed b) { return a.raw_ < b.raw_; }
    friend constexpr bool operator<=(Fixed a, Fixed b) { return a.raw_ <= b.raw_; }
    friend constexpr bool operator>(Fixed a, Fixed b) { return a.raw_ > b.raw_; }
    friend constexpr bool operator>=(Fixed a, Fixed b) { return a.raw_ >= b.raw_; }

private:
    static constexpr std::int64_t kOne = 65536;
    static constexpr std::int64_t abs64(std::int64_t v) { return v < 0 ? -v : v; }
    static constexpr Fixed saturate(std::int64_t v) {
        if (v > std::numeric_limits<std::int32_t>::max()) return from_raw(std::numeric_limits<std::int32_t>::max());
        if (v < std::numeric_limits<std::int32_t>::min()) return from_raw(std::numeric_limits<std::int32_t>::min());
        return from_raw(static_cast<std::int32_t>(v));
    }
    static constexpr Fixed by_zero(std::int64_t numerator) {
        if (numerator > 0) return from_raw(std::numeric_limits<std::int32_t>::max());
        if (numerator < 0) return from_raw(std::numeric_limits<std::int32_t>::min());
        return Fixed{};
    }
    // |num| / |den| rounded to nearest (ties away from zero), with the sign applied afterwards
    static constexpr std::int64_t round_div(std::int64_t num, std::int64_t den, bool negative) {
        const std::int64_t q = (num + den / 2) / den;
        return negative ? -q : q;
    }

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
        if (dt.raw() <= 0) return last_output_;
        const Fixed error = setpoint - measurement;
        const Fixed p = config_.kp * error;
        integral_ = clamp(integral_ + (config_.ki * error) * dt, config_.out_min, config_.out_max);
        const Fixed d = has_previous_ ? (config_.kd * (((setpoint - measurement) - previous_) / dt)) : Fixed{};
        previous_ = setpoint - measurement;
        has_previous_ = true;
        last_output_ = clamp((p + integral_) + d, config_.out_min, config_.out_max);
        return last_output_;
    }

    Fixed integral() const { return integral_; }

    void reset() {
        integral_ = Fixed{};
        previous_ = Fixed{};
        has_previous_ = false;
        last_output_ = Fixed{};
    }

private:
    PidConfig config_;
    Fixed integral_{};
    Fixed previous_{};
    bool has_previous_ = false;
    Fixed last_output_{};
};

}  // namespace ctl
