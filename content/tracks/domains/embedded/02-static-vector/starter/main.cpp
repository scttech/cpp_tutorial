// A fixed-capacity vector on the stack: no heap, no exceptions. It works with types that have no default constructor. Read-only. Press Run.
#include <cstdio>

#include "static_vector.hpp"

struct Sensor {
    int id;
    explicit Sensor(int i) : id(i) { std::printf("  Sensor %d built\n", id); }
    Sensor(const Sensor& o) : id(o.id) { std::printf("  Sensor %d copied\n", id); }
    Sensor(Sensor&& o) noexcept : id(o.id) { std::printf("  Sensor %d moved\n", id); }
    Sensor& operator=(const Sensor&) = default;
    ~Sensor() { std::printf("  Sensor %d destroyed\n", id); }
};

int main() {
    std::printf("make a vector of capacity 4 (no Sensor exists yet):\n");
    emb::StaticVector<Sensor, 4> sensors;
    std::printf("add two:\n");
    sensors.emplace_back(1);
    sensors.emplace_back(2);
    std::printf("size %zu of %zu, full: %s\n", sensors.size(), sensors.capacity(), sensors.full() ? "yes" : "no");
    std::printf("remove the first:\n");
    sensors.erase(0);
    std::printf("leave the scope:\n");
}
