#pragma once
// YOUR PATHFINDER.
//
// find_path(grid, start, goal) returns the cheapest path from start to goal as a list of cells, INCLUDING both ends, moving
// up/down/left/right. The cost of a path is the sum of the costs of the cells it ENTERS (the start cell is free).
//   - start == goal: just {start}
//   - start or goal is a wall or outside the map: no path (an empty vector); also when the goal cannot be reached
//   - among several cheapest paths, any one will do
//
// A*: keep a priority queue ordered by f = g + h, where g is the cost so far and h is a guess of what is left. h must never
// OVERestimate: with every cell costing at least 1, the Manhattan distance to the goal is safe. Read the grid only through
// grid.cost(x, y) (each call is counted) and look at each cell as little as you can.
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

#include "types.hpp"

namespace astar {

inline std::vector<Point> find_path(const Grid& grid, Point start, Point goal) {
    (void)grid;
    (void)start;
    (void)goal;
    return {};  // TODO
}

}  // namespace astar
