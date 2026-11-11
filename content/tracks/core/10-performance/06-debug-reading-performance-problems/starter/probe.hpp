#pragma once
// READ-ONLY. Tracked is an int that COUNTS copies, moves and comparisons (like the Item and Num of earlier lessons).
#include <cstddef>

namespace probe {
inline unsigned long long compares = 0, copies = 0, moves = 0;
inline void reset() { compares = copies = moves = 0; }
}  // namespace probe

struct Tracked {
    int v = 0;
    Tracked() = default;
    Tracked(int x) : v(x) {}
    Tracked(const Tracked& o) : v(o.v) { ++probe::copies; }
    Tracked(Tracked&& o) noexcept : v(o.v) { ++probe::moves; }
    Tracked& operator=(const Tracked& o) {
        v = o.v;
        ++probe::copies;
        return *this;
    }
    Tracked& operator=(Tracked&& o) noexcept {
        v = o.v;
        ++probe::moves;
        return *this;
    }
    friend bool operator==(const Tracked& a, const Tracked& b) {
        ++probe::compares;
        return a.v == b.v;
    }
    friend bool operator<(const Tracked& a, const Tracked& b) {
        ++probe::compares;
        return a.v < b.v;
    }
};
