#pragma once
// READ-ONLY. Handles, and test objects that count what happens to them.
#include <cstdint>

namespace pool {

namespace stats {
inline unsigned long long constructed = 0, destroyed = 0, copies = 0, moves = 0;
inline void reset() { constructed = destroyed = copies = moves = 0; }
inline long long live() { return static_cast<long long>(constructed) - static_cast<long long>(destroyed); }
}  // namespace stats

// A handle names a slot AND the generation of the object in it. When the object is destroyed the slot's generation goes up, so
// a handle that is kept too long (a "stale" handle) can be recognised and refused, even if the slot now holds another object.
struct Handle {
    std::uint32_t index = 0xFFFFFFFFu;
    std::uint32_t generation = 0;
    friend bool operator==(const Handle& a, const Handle& b) { return a.index == b.index && a.generation == b.generation; }
};
inline constexpr Handle kInvalidHandle{};

// An object that counts every construction, copy, move and destruction.
struct Tracked {
    int a = 0, b = 0;
    Tracked() { ++stats::constructed; }
    explicit Tracked(int a_) : a(a_) { ++stats::constructed; }
    Tracked(int a_, int b_) : a(a_), b(b_) { ++stats::constructed; }
    Tracked(const Tracked& o) : a(o.a), b(o.b) {
        ++stats::constructed;
        ++stats::copies;
    }
    Tracked(Tracked&& o) noexcept : a(o.a), b(o.b) {
        ++stats::constructed;
        ++stats::moves;
    }
    Tracked& operator=(const Tracked&) = default;
    ~Tracked() { ++stats::destroyed; }
};

// needs 32-byte alignment
struct alignas(32) Wide {
    double v[4] = {1, 2, 3, 4};
};

}  // namespace pool
