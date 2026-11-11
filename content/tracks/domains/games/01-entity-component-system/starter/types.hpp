#pragma once
// READ-ONLY. Entities, the components used by the examples and tests, and the counters that the Fast tests read.
#include <cstdint>

namespace ecs {

namespace stats {
inline unsigned long long copies = 0;  // components COPIED
inline unsigned long long moves = 0;   // components MOVED
inline void reset() { copies = moves = 0; }
}  // namespace stats

using EntityId = std::uint32_t;

// A handle to an entity: a slot number and the generation of that slot. When an entity is destroyed its slot is reused for a
// new entity with a higher generation, so old handles to the destroyed entity can be recognised as stale.
struct Entity {
    EntityId index = 0xFFFFFFFFu;
    std::uint32_t generation = 0;
    friend bool operator==(const Entity& a, const Entity& b) { return a.index == b.index && a.generation == b.generation; }
};
inline constexpr Entity kNullEntity{};

// sits inside the components below: every copy or move of a component counts
struct Counter {
    Counter() = default;
    Counter(const Counter&) { ++stats::copies; }
    Counter(Counter&&) noexcept { ++stats::moves; }
    Counter& operator=(const Counter&) {
        ++stats::copies;
        return *this;
    }
    Counter& operator=(Counter&&) noexcept {
        ++stats::moves;
        return *this;
    }
};

struct Position { float x = 0, y = 0; [[no_unique_address]] Counter counter{}; };
struct Velocity { float dx = 0, dy = 0; [[no_unique_address]] Counter counter{}; };
struct Health { int hp = 100; [[no_unique_address]] Counter counter{}; };
struct Frozen {};  // a tag: a component without data

}  // namespace ecs
