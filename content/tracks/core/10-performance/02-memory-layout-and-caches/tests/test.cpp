#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cacheline.hpp"
#include "layout.hpp"
#include "zth_test.hpp"

namespace {

ParticleSystem make(std::size_t n) {
    ParticleSystem ps;
    for (std::size_t i = 0; i < n; ++i) {
        const std::string name = "p" + std::to_string(i);
        ps.add(static_cast<float>(i), static_cast<float>(2 * i), 1.0f, -0.5f, 2.0f, name.c_str());
    }
    return ps;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "add() and size(): the fields come back as they were put in") {
    ParticleSystem ps;
    ps.add(1.5f, 2.5f, 3.5f, 4.5f, 5.5f, "alpha");
    ps.add(-1.0f, 0.0f, 0.25f, 8.0f, 9.0f, "beta");
    ZTH_EXPECT_EQ(ps.size(), std::size_t{2});
    ZTH_EXPECT_EQ(ps.x(0), 1.5f);
    ZTH_EXPECT_EQ(ps.y(0), 2.5f);
    ZTH_EXPECT_EQ(ps.vx(0), 3.5f);
    ZTH_EXPECT_EQ(ps.vy(0), 4.5f);
    ZTH_EXPECT_EQ(ps.mass(0), 5.5f);
    ZTH_EXPECT_EQ(std::string(ps.name(0)), std::string("alpha"));
    ZTH_EXPECT_EQ(ps.x(1), -1.0f);
    ZTH_EXPECT_EQ(ps.mass(1), 9.0f);
    ZTH_EXPECT_EQ(std::string(ps.name(1)), std::string("beta"));
}
ZTH_TEST("works", "step() moves every particle by velocity times dt") {
    ParticleSystem ps = make(100);
    ps.step(2.0f);
    for (std::size_t i = 0; i < 100; ++i) {
        ZTH_EXPECT_EQ(ps.x(i), static_cast<float>(i) + 2.0f);
        ZTH_EXPECT_EQ(ps.y(i), static_cast<float>(2 * i) - 1.0f);
        ZTH_EXPECT_EQ(ps.vx(i), 1.0f);  // the velocity does not change
    }
    ps.step(0.5f);
    ZTH_EXPECT_EQ(ps.x(10), 12.5f);
}
ZTH_TEST("works", "sum_x() and total_mass()") {
    ParticleSystem ps = make(10);  // x = 0..9, mass 2 each
    ZTH_EXPECT_EQ(ps.sum_x(), 45.0);
    ZTH_EXPECT_EQ(ps.total_mass(), 20.0);
    ps.step(1.0f);
    ZTH_EXPECT_EQ(ps.sum_x(), 55.0);
}
ZTH_TEST("works", "Record still has all of its members") {
    Record r{};
    r.tag = 'a';
    r.value = 2.5;
    r.kind = 'k';
    r.count = 7;
    r.id = 300;
    ZTH_EXPECT_EQ(r.tag, 'a');
    ZTH_EXPECT_EQ(r.value, 2.5);
    ZTH_EXPECT_EQ(r.kind, 'k');
    ZTH_EXPECT_EQ(r.count, 7);
    ZTH_EXPECT_EQ(static_cast<int>(r.id), 300);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "an empty system: size 0, sums 0, step does nothing") {
    ParticleSystem ps;
    ZTH_EXPECT_EQ(ps.size(), std::size_t{0});
    ZTH_EXPECT_EQ(ps.sum_x(), 0.0);
    ZTH_EXPECT_EQ(ps.total_mass(), 0.0);
    ps.step(1.0f);
    ZTH_EXPECT_EQ(ps.size(), std::size_t{0});
}
ZTH_TEST("robust", "names: 31 characters are kept, longer ones are cut, and there is always a terminating '\\0'") {
    ParticleSystem ps;
    const std::string long_name(60, 'z');
    ps.add(0, 0, 0, 0, 1, long_name.c_str());
    ps.add(0, 0, 0, 0, 1, std::string(31, 'q').c_str());
    ps.add(0, 0, 0, 0, 1, "");
    ZTH_EXPECT_EQ(std::strlen(ps.name(0)), std::size_t{31});
    ZTH_EXPECT_EQ(std::string(ps.name(0)), std::string(31, 'z'));
    ZTH_EXPECT_EQ(std::string(ps.name(1)), std::string(31, 'q'));
    ZTH_EXPECT_EQ(std::strlen(ps.name(2)), std::size_t{0});
}
ZTH_TEST("robust", "the data survives growth: add() many particles and check the first and the last") {
    ParticleSystem ps = make(5000);
    ZTH_EXPECT_EQ(ps.size(), std::size_t{5000});
    ZTH_EXPECT_EQ(ps.x(0), 0.0f);
    ZTH_EXPECT_EQ(ps.x(4999), 4999.0f);
    ZTH_EXPECT_EQ(std::string(ps.name(4999)), std::string("p4999"));
    ZTH_EXPECT_EQ(std::string(ps.name(0)), std::string("p0"));
}
ZTH_TEST("robust", "a write through the accessor is seen by the loops") {
    ParticleSystem ps = make(4);
    ps.x(2) = 100.0f;
    ps.mass(3) = 10.0f;
    ZTH_EXPECT_EQ(ps.sum_x(), 0.0 + 1.0 + 100.0 + 3.0);
    ZTH_EXPECT_EQ(ps.total_mass(), 2.0 + 2.0 + 2.0 + 10.0);
}
ZTH_TEST("robust", "two counters of an array never share a cache line (the first or the last counter included)") {
    ShardCounter counters[8];
    std::vector<const void*> addresses;
    for (auto& c : counters) addresses.push_back(&c.value);
    ZTH_EXPECT_EQ(lines_touched(addresses), std::size_t{8});
    std::vector<ShardCounter> in_vector(8);
    std::vector<const void*> a2;
    for (auto& c : in_vector) a2.push_back(&c.value);
    ZTH_EXPECT_EQ(lines_touched(a2), std::size_t{8});
    in_vector[3].value += 5;
    ZTH_EXPECT_EQ(in_vector[3].value, 5LL);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "Record is 16 bytes: the same members, in an order that needs no padding") {
    ZTH_EXPECT_TRUE(sizeof(Record) <= 16);
}
ZTH_TEST("fast", "step() on 4096 particles touches at most 1100 cache lines (the four hot arrays are 1024 lines; an array of 52-byte structs is 3300)") {
    ParticleSystem ps = make(4096);
    ZTH_EXPECT_TRUE(lines_for_step(ps) <= 1100);
}
ZTH_TEST("fast", "sum_x() touches at most 270 lines: x is stored on its own, 16 floats to a line") {
    ParticleSystem ps = make(4096);
    ZTH_EXPECT_TRUE(lines_for_sum_x(ps) <= 270);
}
ZTH_TEST("fast", "total_mass() touches at most 270 lines too") {
    ParticleSystem ps = make(4096);
    ZTH_EXPECT_TRUE(lines_for_total_mass(ps) <= 270);
}
