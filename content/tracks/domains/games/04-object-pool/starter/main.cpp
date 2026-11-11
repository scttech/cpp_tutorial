// A pool of bullets: fire some, let them expire, fire more. A stale handle is recognised. Read-only. Press Run.
#include <cstdio>

#include "pool.hpp"

struct Bullet {
    float x, y, vx, vy;
    int ttl;
};

int main() {
    static pool::Pool<Bullet, 8> bullets;  // no heap memory is used: the 8 slots are inside the pool
    pool::Handle live[8];
    for (int i = 0; i < 8; ++i) live[i] = bullets.create(Bullet{static_cast<float>(i), 0, 1, 0.5f, 3 + i});
    std::printf("fired 8: size %zu, full: %s\n", bullets.size(), bullets.full() ? "yes" : "no");
    std::printf("a ninth bullet: %s\n", bullets.create(Bullet{}) == pool::kInvalidHandle ? "refused (the pool is full)" : "created?!");

    bullets.destroy(live[2]);
    bullets.destroy(live[5]);
    const pool::Handle replacement = bullets.create(Bullet{99, 99, 0, 0, 1});
    std::printf("the replacement reuses slot %u (the slot freed last), generation %u\n", replacement.index, replacement.generation);
    std::printf("the old handle to slot 5 still works? %s\n", bullets.get(live[5]) ? "yes (BUG)" : "no, it is stale");
    bullets.for_each([](pool::Handle h, Bullet& b) { std::printf("  slot %u gen %u at (%.0f, %.0f)\n", h.index, h.generation, b.x, b.y); });
}
