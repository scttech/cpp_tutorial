// MUTANT: the starter layout: every loop drags the cold name through the cache.
#pragma once

#include <array>
#include <cstddef>
#include <vector>

struct Record {
    double value;
    int count;
    short id;
    char tag;
    char kind;
};

struct alignas(64) ShardCounter {
    long long value = 0;
};

class ParticleSystem {
public:
    void add(float x, float y, float vx, float vy, float mass, const char* name) {
        std::array<char, 32> n{};
        for (std::size_t i = 0; i < 31 && name[i] != '\0'; ++i) n[i] = name[i];
        all_.push_back(P{x, y, vx, vy, mass, n});
    }
    std::size_t size() const { return all_.size(); }

    float& x(std::size_t i) { return all_[i].x; }
    float& y(std::size_t i) { return all_[i].y; }
    float& vx(std::size_t i) { return all_[i].vx; }
    float& vy(std::size_t i) { return all_[i].vy; }
    float& mass(std::size_t i) { return all_[i].mass; }
    const char* name(std::size_t i) const { return all_[i].name.data(); }

    void step(float dt) {
        for (auto& p : all_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
    }
    double sum_x() const {
        double s = 0;
        for (auto& p : all_) s += p.x;
        return s;
    }
    double total_mass() const {
        double s = 0;
        for (auto& p : all_) s += p.mass;
        return s;
    }

private:
    struct P {
        float x, y, vx, vy, mass;
        std::array<char, 32> name;
    };
    std::vector<P> all_;
};
