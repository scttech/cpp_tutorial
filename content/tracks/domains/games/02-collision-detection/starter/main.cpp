// Falling crates and balls: which pairs touch? Read-only. Press Run.
#include <cstdio>
#include <vector>

#include "grid.hpp"
#include "shapes.hpp"

int main() {
    std::vector<col::Aabb> boxes = {{{0, 0}, {2, 2}}, {{1.5, 1.5}, {4, 4}}, {{10, 10}, {11, 11}}, {{4, 0}, {6, 2}}, {{-3, -3}, {-1, -1}}, {{-1.5, -1.5}, {0, 0}}};
    for (const auto& pair : col::find_collisions(boxes, 4.0)) std::printf("box %d touches box %d\n", pair.first, pair.second);

    const col::Circle ball{{5, 5}, 1.5};
    std::printf("ball vs box 1: %s\n", col::overlaps(ball, boxes[1]) ? "hit" : "miss");
    std::printf("ball vs box 3: %s\n", col::overlaps(ball, boxes[3]) ? "hit" : "miss");
    std::printf("ball vs ball : %s\n", col::overlaps(ball, col::Circle{{7, 5}, 0.5}) ? "hit" : "miss");

    col::UniformGrid grid(4.0);
    for (std::size_t i = 0; i < boxes.size(); ++i) grid.insert(static_cast<int>(i), boxes[i]);
    std::printf("the grid proposes %zu pairs out of %zu possible\n", grid.candidate_pairs().size(), boxes.size() * (boxes.size() - 1) / 2);
}
