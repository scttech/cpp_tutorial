#pragma once
// READ-ONLY. A number type that COUNTS what is done to it. The counters are how this site measures speed: they do not depend on the computer,
// the compiler or what else is running, so the same code always gets the same score.
#include <cstddef>
#include <functional>

namespace probe {
inline unsigned long long compares = 0;  // operator== and operator<
inline unsigned long long adds = 0;      // operator+
inline unsigned long long copies = 0;    // copy construction and copy assignment (moves are free)
inline void reset() { compares = adds = copies = 0; }
inline unsigned long long ops() { return compares + adds; }
}  // namespace probe

struct Num {
    long long v = 0;
    Num() = default;
    Num(long long x) : v(x) {}
    Num(const Num& o) : v(o.v) { ++probe::copies; }
    Num(Num&&) noexcept = default;
    Num& operator=(const Num& o) {
        v = o.v;
        ++probe::copies;
        return *this;
    }
    Num& operator=(Num&&) noexcept = default;
    friend bool operator==(const Num& a, const Num& b) {
        ++probe::compares;
        return a.v == b.v;
    }
    friend bool operator<(const Num& a, const Num& b) {
        ++probe::compares;
        return a.v < b.v;
    }
    friend Num operator+(const Num& a, const Num& b) {
        ++probe::adds;
        return Num(a.v + b.v);
    }
};

template <>
struct std::hash<Num> {
    std::size_t operator()(const Num& n) const noexcept { return std::hash<long long>()(n.v); }
};
