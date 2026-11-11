#pragma once
// YOUR BROAD PHASE: a uniform grid. Checking every pair of n boxes costs n(n-1)/2 tests; a grid only proposes pairs that
// share a cell.
//
//   UniformGrid(cell)             square cells of side `cell`; cell (i, j) covers [i*cell, (i+1)*cell) x [j*cell, (j+1)*cell)
//   insert(id, box)               put the id into EVERY cell its box touches (a box may span many cells; coordinates may be negative)
//   candidate_pairs()             every pair (a, b), a < b, of ids that share at least one cell: sorted, each pair once
//   find_collisions(boxes, cell)  the pairs (i, j), i < j, of boxes that really overlap, sorted: broad phase, then overlaps()
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
        (void)id;
        (void)box;  // TODO
    }

    std::vector<std::pair<int, int>> candidate_pairs() const {
        return {};  // TODO
    }

private:
    double cell_;
    // TODO: the cells
};

inline std::vector<std::pair<int, int>> find_collisions(const std::vector<Aabb>& boxes, double cell) {
    (void)boxes;
    (void)cell;
    return {};  // TODO
}

}  // namespace col
