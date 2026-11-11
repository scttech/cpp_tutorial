#pragma once

#include <array>
#include <cstddef>
#include <vector>

// ---- Part 1: padding ------------------------------------------------------------------------------------------------------------------
// The compiler puts each member at an address that suits its type (a double at a multiple of 8, an int at a multiple of 4 ...), so there are
// unused "padding" bytes between members, and at the end. Press Run: the fields add up to 16 bytes, sizeof says more.
// TASK: put the SAME members in a better ORDER so that sizeof(Record) is 16. (Largest alignment first is the usual rule.)
struct Record {
    char tag;
    double value;
    char kind;
    int count;
    short id;
};

// ---- Part 2: false sharing ------------------------------------------------------------------------------------------------------------
// Several threads each update their own counter, and the counters sit side by side in an array. Different variables, but the SAME cache line:
// when one thread writes its counter, the other processors must throw that line away and fetch it again, and every update gets slow.
// TASK: change ShardCounter so that every counter in an array occupies a cache line of its own (kCacheLine = 64 bytes).
struct ShardCounter {
    long long value = 0;
};

// ---- Part 3: array of structs to struct of arrays -----------------------------------------------------------------------------------
// Each Particle is 52 bytes. A loop that only needs x still pulls in the cold fields (the name) with every cache line.
// TASK: restructure ParticleSystem so that each kind of value is stored in an array of its own. Keep the public functions and their results exactly
// as they are. step(), sum_x() and total_mass() must touch as few cache lines as possible (main.cpp shows how many they touch now).
struct Particle {
    float x, y, vx, vy;  // hot: used in every step
    float mass;
    char name[32];  // cold: used almost never
};

class ParticleSystem {
public:
    // name: at most 31 characters are kept, always followed by a '\0'
    void add(float x, float y, float vx, float vy, float mass, const char* name) {
        Particle p{x, y, vx, vy, mass, {}};
        for (std::size_t i = 0; i < 31 && name[i] != '\0'; ++i) p.name[i] = name[i];
        items_.push_back(p);
    }
    std::size_t size() const { return items_.size(); }

    float& x(std::size_t i) { return items_[i].x; }
    float& y(std::size_t i) { return items_[i].y; }
    float& vx(std::size_t i) { return items_[i].vx; }
    float& vy(std::size_t i) { return items_[i].vy; }
    float& mass(std::size_t i) { return items_[i].mass; }
    const char* name(std::size_t i) const { return items_[i].name; }

    void step(float dt) {
        for (Particle& p : items_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
    }
    double sum_x() const {
        double s = 0;
        for (const Particle& p : items_) s += p.x;
        return s;
    }
    double total_mass() const {
        double s = 0;
        for (const Particle& p : items_) s += p.mass;
        return s;
    }

private:
    std::vector<Particle> items_;
};
