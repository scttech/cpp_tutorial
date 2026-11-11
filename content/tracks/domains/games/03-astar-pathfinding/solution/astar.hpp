#pragma once
// A*: a priority queue on f = g + h, flat vectors indexed by cell, each cell settled once.
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

#include "types.hpp"

namespace astar {

inline std::vector<Point> find_path(const Grid& grid, Point start, Point goal) {
    const int w = grid.width(), h = grid.height();
    auto inside = [&](Point p) { return p.x >= 0 && p.y >= 0 && p.x < w && p.y < h; };
    if (!inside(start) || !inside(goal) || grid.cost(start.x, start.y) == 0 || grid.cost(goal.x, goal.y) == 0) return {};
    if (start == goal) return {start};

    const std::size_t cells = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    auto index = [&](Point p) { return static_cast<std::size_t>(p.y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(p.x); };
    // every step costs at least 1, so the Manhattan distance never overestimates what is left
    auto heuristic = [&](Point p) { return static_cast<long long>(std::abs(p.x - goal.x) + std::abs(p.y - goal.y)); };

    std::vector<long long> best(cells, std::numeric_limits<long long>::max());  // cheapest known cost to reach each cell
    std::vector<std::size_t> parent(cells, cells);                              // where we came from (cells = none)
    std::vector<char> settled(cells, 0);                                        // taken from the queue: its cost is final

    struct Entry {
        long long f, g;
        std::size_t cell;
    };
    // smallest f first; among equal f the larger g (the entry closer to the goal) first
    auto later = [](const Entry& a, const Entry& b) { return a.f != b.f ? a.f > b.f : a.g < b.g; };
    std::priority_queue<Entry, std::vector<Entry>, decltype(later)> open(later);

    best[index(start)] = 0;
    open.push({heuristic(start), 0, index(start)});
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};

    while (!open.empty()) {
        const Entry e = open.top();
        open.pop();
        if (settled[e.cell]) continue;  // a stale entry: the cell was reached more cheaply since
        settled[e.cell] = 1;
        const Point p{static_cast<int>(e.cell % static_cast<std::size_t>(w)), static_cast<int>(e.cell / static_cast<std::size_t>(w))};
        if (p == goal) {  // the goal is final only when it is taken from the queue
            std::vector<Point> path;
            for (std::size_t c = e.cell; c != cells; c = parent[c]) path.push_back({static_cast<int>(c % static_cast<std::size_t>(w)), static_cast<int>(c / static_cast<std::size_t>(w))});
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int d = 0; d < 4; ++d) {
            const Point q{p.x + dx[d], p.y + dy[d]};
            if (!inside(q) || settled[index(q)]) continue;
            const int cost = grid.cost(q.x, q.y);
            if (cost == 0) continue;
            const long long g = e.g + cost;
            if (g < best[index(q)]) {
                best[index(q)] = g;
                parent[index(q)] = e.cell;
                open.push({g + heuristic(q), g, index(q)});
            }
        }
    }
    return {};
}

}  // namespace astar
