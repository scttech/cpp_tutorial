// MUTANT: the members are reordered, but not well: sizeof is 24.
#pragma once

#include <array>
#include <cstddef>
#include <vector>

struct Record {
    char tag;
    double value;
    int count;
    char kind;
    short id;
};

struct alignas(64) ShardCounter {
    long long value = 0;
};

class ParticleSystem {
public:
    void add(float x, float y, float vx, float vy, float mass, const char* name) {
        std::array<char, 32> n{};
        for (std::size_t i = 0; i < 31 && name[i] != '\0'; ++i) n[i] = name[i];
        x_.push_back(x);
        y_.push_back(y);
        vx_.push_back(vx);
        vy_.push_back(vy);
        mass_.push_back(mass);
        names_.push_back(n);
    }
    std::size_t size() const { return x_.size(); }

    float& x(std::size_t i) { return x_[i]; }
    float& y(std::size_t i) { return y_[i]; }
    float& vx(std::size_t i) { return vx_[i]; }
    float& vy(std::size_t i) { return vy_[i]; }
    float& mass(std::size_t i) { return mass_[i]; }
    const char* name(std::size_t i) const { return names_[i].data(); }

    void step(float dt) {
        for (std::size_t i = 0; i < x_.size(); ++i) {
            x_[i] += vx_[i] * dt;
            y_[i] += vy_[i] * dt;
        }
    }
    double sum_x() const {
        double s = 0;
        for (float v : x_) s += v;
        return s;
    }
    double total_mass() const {
        double s = 0;
        for (float v : mass_) s += v;
        return s;
    }

private:
    // one array per field: a loop that needs only x reads only x; the cold names are never in the way
    std::vector<float> x_, y_, vx_, vy_, mass_;
    std::vector<std::array<char, 32>> names_;
};
