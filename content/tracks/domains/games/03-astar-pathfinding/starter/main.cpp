// A maze with rough ground (digits cost more to cross): the cheapest path is drawn with *. Read-only. Press Run.
#include <cstdio>
#include <string>
#include <vector>

#include "astar.hpp"

int main() {
    const std::vector<std::string> rows = {
        "S..#......", ".#.#.####.", ".#...#....", ".#####.##.", ".....#..#.", "####.#.##.", "...9.#....", ".###.####.", "...5....#G",
    };
    const astar::Grid grid(rows);
    astar::stats::reset();
    const std::vector<astar::Point> path = astar::find_path(grid, grid.start(), grid.goal());
    const unsigned long long looked = astar::stats::lookups;

    std::vector<std::string> picture = rows;
    long long total = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) total += grid.peek(path[i].x, path[i].y);
        if (picture[static_cast<std::size_t>(path[i].y)][static_cast<std::size_t>(path[i].x)] == '.') picture[static_cast<std::size_t>(path[i].y)][static_cast<std::size_t>(path[i].x)] = '*';
    }
    for (const auto& row : picture) std::printf("%s\n", row.c_str());
    if (path.empty()) std::printf("no path\n");
    else std::printf("%zu cells, total cost %lld, %llu cell lookups\n", path.size(), total, looked);
}
