#pragma once
// READ-ONLY. A model of the cache: the processor does not read single bytes from memory, it reads whole CACHE LINES (64 bytes on nearly
// every desktop and server processor). Touching one float pulls in the 64 bytes around it. So the number of DIFFERENT lines a loop
// touches is a good measure of how much memory traffic it causes, and it is the same on every run.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::size_t kCacheLine = 64;

// How many different cache lines contain these addresses?
inline std::size_t lines_touched(const std::vector<const void*>& addresses) {
    std::vector<std::uintptr_t> lines;
    lines.reserve(addresses.size());
    for (const void* p : addresses) lines.push_back(reinterpret_cast<std::uintptr_t>(p) / kCacheLine);
    std::sort(lines.begin(), lines.end());
    return static_cast<std::size_t>(std::unique(lines.begin(), lines.end()) - lines.begin());
}

// The memory each loop of a particle system reads or writes: step() uses x, y, vx and vy of every particle; sum_x() only x; total_mass() only mass.
template <class PS>
std::size_t lines_for_step(PS& ps) {
    std::vector<const void*> a;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        a.push_back(&ps.x(i));
        a.push_back(&ps.y(i));
        a.push_back(&ps.vx(i));
        a.push_back(&ps.vy(i));
    }
    return lines_touched(a);
}
template <class PS>
std::size_t lines_for_sum_x(PS& ps) {
    std::vector<const void*> a;
    for (std::size_t i = 0; i < ps.size(); ++i) a.push_back(&ps.x(i));
    return lines_touched(a);
}
template <class PS>
std::size_t lines_for_total_mass(PS& ps) {
    std::vector<const void*> a;
    for (std::size_t i = 0; i < ps.size(); ++i) a.push_back(&ps.mass(i));
    return lines_touched(a);
}
