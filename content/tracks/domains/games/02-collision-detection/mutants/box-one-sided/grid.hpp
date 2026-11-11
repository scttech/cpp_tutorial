#pragma once
// A uniform grid: a hash map from cell to the ids in it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "shapes.hpp"
#include "types.hpp"

namespace col {

class UniformGrid {
public:
    explicit UniformGrid(double cell) : cell_(cell) {}

    void insert(int id, const Aabb& box) {
        // floor, not truncation: -0.5 is in cell -1, not cell 0
        const std::int64_t x0 = cell_of(box.min.x), x1 = cell_of(box.max.x);
        const std::int64_t y0 = cell_of(box.min.y), y1 = cell_of(box.max.y);
        for (std::int64_t cy = y0; cy <= y1; ++cy) {
            for (std::int64_t cx = x0; cx <= x1; ++cx) cells_[key(cx, cy)].push_back(id);
        }
    }

    std::vector<std::pair<int, int>> candidate_pairs() const {
        std::vector<std::pair<int, int>> pairs;
        for (const auto& cell : cells_) {
            const std::vector<int>& ids = cell.second;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                for (std::size_t j = i + 1; j < ids.size(); ++j) pairs.emplace_back(std::min(ids[i], ids[j]), std::max(ids[i], ids[j]));
            }
        }
        std::sort(pairs.begin(), pairs.end());
        pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());  // a pair that shares two cells is reported once
        return pairs;
    }

private:
    double cell_;
    std::unordered_map<std::uint64_t, std::vector<int>> cells_;

    std::int64_t cell_of(double v) const { return static_cast<std::int64_t>(std::floor(v / cell_)); }
    static std::uint64_t key(std::int64_t cx, std::int64_t cy) {
        return (static_cast<std::uint64_t>(cx) << 32) ^ (static_cast<std::uint64_t>(cy) & 0xFFFFFFFFull);
    }
};

inline std::vector<std::pair<int, int>> find_collisions(const std::vector<Aabb>& boxes, double cell) {
    UniformGrid grid(cell);
    for (std::size_t i = 0; i < boxes.size(); ++i) grid.insert(static_cast<int>(i), boxes[i]);
    std::vector<std::pair<int, int>> hits;
    for (const auto& pair : grid.candidate_pairs()) {
        if (overlaps(boxes[static_cast<std::size_t>(pair.first)], boxes[static_cast<std::size_t>(pair.second)])) hits.push_back(pair);
    }
    return hits;  // candidate_pairs() is sorted
}

}  // namespace col
