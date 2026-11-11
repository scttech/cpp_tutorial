// A tiny simulation: movers have a Position and a Velocity, some are frozen, one is destroyed. Read-only. Press Run.
#include <cstdio>

#include "registry.hpp"

int main() {
    ecs::Registry world;
    ecs::Entity ids[6];
    for (int i = 0; i < 6; ++i) {
        ids[i] = world.create();
        world.add(ids[i], ecs::Position{static_cast<float>(i), 0});
        if (i % 2 == 0) world.add(ids[i], ecs::Velocity{1.0f, 0.5f * static_cast<float>(i)});
    }
    world.add(ids[2], ecs::Frozen{});
    world.add(ids[4], ecs::Health{40});

    for (int step = 0; step < 3; ++step) {
        world.each<ecs::Position, ecs::Velocity>([&](ecs::Entity, ecs::Position& p, ecs::Velocity& v) {
            p.x += v.dx;
            p.y += v.dy;
        });
    }
    world.destroy(ids[4]);
    const ecs::Entity reborn = world.create();  // takes over the slot of the destroyed entity
    world.add(reborn, ecs::Position{100, 100});

    std::printf("entity 4 is alive: %s, the new entity reuses slot %u with generation %u\n", world.alive(ids[4]) ? "yes" : "no", reborn.index, reborn.generation);
    world.each<ecs::Position>([&](ecs::Entity e, ecs::Position& p) {
        std::printf("entity %u.%u at (%.1f, %.1f)%s%s\n", e.index, e.generation, p.x, p.y, world.has<ecs::Velocity>(e) ? " moving" : "", world.has<ecs::Frozen>(e) ? " frozen" : "");
    });
    std::printf("%zu positions, %zu velocities\n", world.count<ecs::Position>(), world.count<ecs::Velocity>());
}
