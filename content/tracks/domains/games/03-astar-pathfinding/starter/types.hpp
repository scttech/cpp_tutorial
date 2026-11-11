#pragma once
// READ-ONLY. The map, and the counter that the Fast tests read.
#include <cstddef>
#include <string>
#include <vector>

namespace astar {

namespace stats {
inline unsigned long long lookups = 0;  // calls of Grid::cost
inline void reset() { lookups = 0; }
}  // namespace stats

struct Point {
    int x = 0, y = 0;
    friend bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
    friend bool operator!=(const Point& a, const Point& b) { return !(a == b); }
};

// A rectangular map. Every cell has a cost to ENTER it: 0 is a wall (cannot be entered), 1 is open ground, 2..9 is rough ground.
class Grid {
public:
    Grid(int width, int height, int cost = 1) : w_(width), h_(height), cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), cost) {}

    // from text: '#' wall, '.' open ground (1), '2'..'9' rough ground, 'S' and 'G' open ground that also set start() and goal()
    explicit Grid(const std::vector<std::string>& rows) : w_(rows.empty() ? 0 : static_cast<int>(rows[0].size())), h_(static_cast<int>(rows.size())) {
        cells_.assign(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_), 1);
        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                int cost = 1;
                if (c == '#') cost = 0;
                else if (c >= '2' && c <= '9') cost = c - '0';
                else if (c == 'S') start_ = {x, y};
                else if (c == 'G') goal_ = {x, y};
                cells_[index(x, y)] = cost;
            }
        }
    }

    int width() const { return w_; }
    int height() const { return h_; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
    void set(int x, int y, int cost) { cells_[index(x, y)] = cost; }
    Point start() const { return start_; }
    Point goal() const { return goal_; }

    // the cost to enter the cell (0 for a wall and for anything outside the map). EVERY call is counted.
    int cost(int x, int y) const {
        ++stats::lookups;
        return inside(x, y) ? cells_[index(x, y)] : 0;
    }
    // the same, not counted (for drawing and for the tests' own checks)
    int peek(int x, int y) const { return inside(x, y) ? cells_[index(x, y)] : 0; }

private:
    int w_, h_;
    std::vector<int> cells_;
    Point start_, goal_;
    std::size_t index(int x, int y) const { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x); }
};

}  // namespace astar
