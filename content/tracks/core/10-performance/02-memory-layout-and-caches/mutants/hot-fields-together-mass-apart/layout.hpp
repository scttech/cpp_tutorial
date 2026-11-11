// MUTANT: x, y, vx and vy in one 16-byte struct: step() is fine, but sum_x() reads every fourth float.
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
        hot_.push_back(Hot{x, y, vx, vy});
        mass_.push_back(mass);
        names_.push_back(n);
    }
    std::size_t size() const { return hot_.size(); }

    float& x(std::size_t i) { return hot_[i].x; }
    float& y(std::size_t i) { return hot_[i].y; }
    float& vx(std::size_t i) { return hot_[i].vx; }
    float& vy(std::size_t i) { return hot_[i].vy; }
    float& mass(std::size_t i) { return mass_[i]; }
    const char* name(std::size_t i) const { return names_[i].data(); }

    void step(float dt) {
        for (auto& p : hot_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
    }
    double sum_x() const {
        double s = 0;
        for (auto& p : hot_) s += p.x;
        return s;
    }
    double total_mass() const {
        double s = 0;
        for (float v : mass_) s += v;
        return s;
    }

private:
    // one array per field: a loop that needs only x reads only x; the cold names are never in the way
    struct Hot {
        float x, y, vx, vy;
    };
    std::vector<Hot> hot_;
    std::vector<float> mass_;
    std::vector<std::array<char, 32>> names_;
};
