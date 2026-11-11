#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "astar.hpp"
#include "zth_test.hpp"

using namespace astar;

namespace {

// The reference: Dijkstra, with a plain priority queue. Returns -1 when the goal cannot be reached.
long long cheapest(const Grid& g, Point s, Point t) {
    if (g.peek(s.x, s.y) == 0 || g.peek(t.x, t.y) == 0) return -1;
    const int w = g.width(), h = g.height();
    std::vector<long long> d(static_cast<std::size_t>(w * h), std::numeric_limits<long long>::max());
    using P = std::pair<long long, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> q;
    d[static_cast<std::size_t>(s.y * w + s.x)] = 0;
    q.push({0, s.y * w + s.x});
    while (!q.empty()) {
        auto [dist, cell] = q.top();
        q.pop();
        if (dist > d[static_cast<std::size_t>(cell)]) continue;
        const int x = cell % w, y = cell / w;
        if (x == t.x && y == t.y) return dist;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; ++k) {
            const int nx = x + dx[k], ny = y + dy[k];
            const int c = g.peek(nx, ny);
            if (c == 0) continue;
            const long long nd = dist + c;
            if (nd < d[static_cast<std::size_t>(ny * w + nx)]) {
                d[static_cast<std::size_t>(ny * w + nx)] = nd;
                q.push({nd, ny * w + nx});
            }
        }
    }
    return -1;
}

// "" if the path is a valid path from s to t, else what is wrong with it; cost is set to its cost
std::string problem(const Grid& g, const std::vector<Point>& path, Point s, Point t, long long& cost) {
    cost = 0;
    if (path.empty()) return "the path is empty";
    if (path.front() != s) return "the path does not start at the start";
    if (path.back() != t) return "the path does not end at the goal";
    for (std::size_t i = 1; i < path.size(); ++i) {
        const int step = std::abs(path[i].x - path[i - 1].x) + std::abs(path[i].y - path[i - 1].y);
        if (step != 1) return "step " + std::to_string(i) + " is not to a neighbouring cell (up, down, left or right)";
        const int c = g.peek(path[i].x, path[i].y);
        if (c == 0) return "step " + std::to_string(i) + " enters a wall";
        cost += c;
    }
    return "";
}

void expect_best(const Grid& g, Point s, Point t) {
    const std::vector<Point> path = find_path(g, s, t);
    const long long want = cheapest(g, s, t);
    if (want < 0) {
        if (!path.empty()) ::zth::fail("there is no path from (" + std::to_string(s.x) + "," + std::to_string(s.y) + ") to (" + std::to_string(t.x) + "," + std::to_string(t.y) + "), but one was returned");
        return;
    }
    long long cost = 0;
    const std::string why = problem(g, path, s, t, cost);
    if (!why.empty()) ::zth::fail(why);
    else if (cost != want) ::zth::fail("the path costs " + std::to_string(cost) + ", the cheapest costs " + std::to_string(want));
}

Grid random_grid(::zth::Lcg& rng, int w, int h, int wall_percent, int max_cost) {
    Grid g(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) g.set(x, y, rng.below(100) < wall_percent ? 0 : 1 + rng.below(max_cost));
    }
    return g;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a straight path on open ground: both ends included, one step at a time") {
    const Grid g(10, 5);
    const auto path = find_path(g, {0, 2}, {6, 2});
    long long cost = 0;
    ZTH_EXPECT_EQ(problem(g, path, {0, 2}, {6, 2}, cost), std::string());
    ZTH_EXPECT_EQ(path.size(), std::size_t{7});
    ZTH_EXPECT_EQ(cost, 6LL);
}
ZTH_TEST("works", "the path goes around a wall") {
    const Grid g({"S..#....", "...#.##.", "...#.#G.", ".....#..", "........"});
    long long cost = 0;
    const auto path = find_path(g, g.start(), g.goal());
    ZTH_EXPECT_EQ(problem(g, path, g.start(), g.goal(), cost), std::string());
    ZTH_EXPECT_EQ(cost, cheapest(g, g.start(), g.goal()));
    ZTH_EXPECT_EQ(cost, 12LL);
}
ZTH_TEST("works", "start equals goal gives just that cell") {
    const Grid g(4, 4);
    const auto path = find_path(g, {2, 1}, {2, 1});
    ZTH_EXPECT_EQ(path.size(), std::size_t{1});
    ZTH_EXPECT_TRUE(path[0] == (Point{2, 1}));
}
ZTH_TEST("works", "no path: the goal is walled in, the map is cut in two") {
    ZTH_EXPECT_TRUE(find_path(Grid({"S.#..", "..#..", "..#.G"}), {0, 0}, {4, 2}).empty());
    ZTH_EXPECT_TRUE(find_path(Grid({"S....", ".###.", ".#G#.", ".###."}), {0, 0}, {2, 2}).empty());
}
ZTH_TEST("works", "rough ground costs more: a longer path over cheap ground beats a short one over expensive ground") {
    const Grid g({"S999G", "....."});  // straight: 9+9+9+1 = 28; around through the bottom row: 1+1+1+1+1 = 5
    long long cost = 0;
    const auto path = find_path(g, g.start(), g.goal());
    ZTH_EXPECT_EQ(problem(g, path, g.start(), g.goal(), cost), std::string());
    ZTH_EXPECT_EQ(cost, 6LL);
    const Grid g2({"S3G", "..."});  // through the 3 costs 3+1 = 4; around it costs 1+1+1+1 = 4: either is fine, but never more
    cost = 0;
    const auto p2 = find_path(g2, g2.start(), g2.goal());
    ZTH_EXPECT_EQ(problem(g2, p2, g2.start(), g2.goal(), cost), std::string());
    ZTH_EXPECT_EQ(cost, 4LL);
}
ZTH_TEST("works", "the cost of a path counts the cells entered, not the start: moving onto a cost-5 goal costs 5") {
    const Grid g({"S5", ".."});
    long long cost = 0;
    const auto path = find_path(g, {0, 0}, {1, 0});
    ZTH_EXPECT_EQ(problem(g, path, {0, 0}, {1, 0}, cost), std::string());
    ZTH_EXPECT_EQ(cost, 5LL);  // the direct step enters the 5; going around (down, right, up) would enter 1 + 1 + 5
}
ZTH_TEST("works", "a spiral maze has one long way") {
    const Grid g({"S.......", "#######.", "........", ".#######", "........", "#######.", "G......."});
    long long cost = 0;
    const auto path = find_path(g, {0, 0}, {0, 6});
    ZTH_EXPECT_EQ(problem(g, path, {0, 0}, {0, 6}, cost), std::string());
    ZTH_EXPECT_EQ(cost, cheapest(g, {0, 0}, {0, 6}));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "start or goal in a wall, or outside the map, gives no path") {
    const Grid g({"S#.", "...", ".#G"});
    ZTH_EXPECT_TRUE(find_path(g, {1, 0}, {2, 2}).empty());   // start in a wall
    ZTH_EXPECT_TRUE(find_path(g, {0, 0}, {1, 2}).empty());   // goal in a wall
    ZTH_EXPECT_TRUE(find_path(g, {-1, 0}, {2, 2}).empty());
    ZTH_EXPECT_TRUE(find_path(g, {0, 0}, {3, 2}).empty());
    ZTH_EXPECT_TRUE(find_path(g, {0, 0}, {2, 3}).empty());
    ZTH_EXPECT_TRUE(find_path(g, {0, -5}, {2, 2}).empty());
    ZTH_EXPECT_TRUE(find_path(g, {1, 0}, {1, 0}).empty());   // start == goal but it is a wall
    ZTH_EXPECT_FALSE(find_path(g, {0, 0}, {2, 2}).empty());
}
ZTH_TEST("robust", "tiny maps: 1x1, 1xN, Nx1, and a 2x2 with a wall") {
    ZTH_EXPECT_EQ(find_path(Grid(1, 1), {0, 0}, {0, 0}).size(), std::size_t{1});
    ZTH_EXPECT_EQ(find_path(Grid(1, 7), {0, 0}, {0, 6}).size(), std::size_t{7});
    ZTH_EXPECT_EQ(find_path(Grid(7, 1), {6, 0}, {0, 0}).size(), std::size_t{7});
    Grid g(2, 2);
    g.set(1, 0, 0);
    expect_best(g, {0, 0}, {1, 1});
    g.set(0, 1, 0);
    ZTH_EXPECT_TRUE(find_path(g, {0, 0}, {1, 1}).empty());
}
ZTH_TEST("robust", "the edges of the map: walking along the border and into the corners") {
    const Grid g({"S....", ".###.", ".###.", ".###.", "....G"});
    expect_best(g, {0, 0}, {4, 4});
    expect_best(g, {4, 4}, {0, 0});
    expect_best(g, {0, 4}, {4, 0});
}
ZTH_TEST("robust", "a wall of cost 0 in the middle of rough ground; the cheapest route may be longer in steps") {
    Grid g({"S1111", "99991", "11111", "19999", "1111G"});
    g.set(4, 3, 30);  // the direct way down the right edge is blocked by very rough ground
    long long cost = 0;
    const auto path = find_path(g, g.start(), g.goal());
    ZTH_EXPECT_EQ(problem(g, path, g.start(), g.goal(), cost), std::string());
    ZTH_EXPECT_EQ(cost, cheapest(g, g.start(), g.goal()));
    ZTH_EXPECT_TRUE(path.size() > 9);  // the cheap way winds: more than the 9 cells of the shortest route
}
ZTH_TEST_TIMEOUT("robust", "400 random maps with walls and rough ground: the path is valid and exactly as cheap as Dijkstra's (heuristic is admissible)", 15) {
    ::zth::Lcg rng(4242);
    for (int i = 0; i < 400; ++i) {
        const int w = 1 + rng.below(14), h = 1 + rng.below(14);
        const Grid g = random_grid(rng, w, h, rng.below(45), 1 + rng.below(9));
        const Point s{rng.below(w), rng.below(h)}, t{rng.below(w), rng.below(h)};
        expect_best(g, s, t);
        if (::zth::g_failed) {
            ::zth::fail("map " + std::to_string(i) + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");
            return;
        }
    }
}
ZTH_TEST_TIMEOUT("robust", "a long snake maze: the path has about 20000 cells (no recursion in the reconstruction)", 15) {
    const int w = 199, h = 199;
    Grid g(w, h);
    for (int y = 1; y < h; y += 2) {
        for (int x = 0; x < w; ++x) g.set(x, y, 0);
        g.set(((y / 2) % 2 == 0) ? w - 1 : 0, y, 1);  // the gap alternates between the right and the left end
    }
    const auto path = find_path(g, {0, 0}, {0, h - 1});
    long long cost = 0;
    ZTH_EXPECT_EQ(problem(g, path, {0, 0}, {0, h - 1}, cost), std::string());
    ZTH_EXPECT_TRUE(path.size() > 15000);
}
ZTH_TEST("robust", "the grid is left alone and the search can be repeated with the same result") {
    const Grid g({"S.#..", "..#..", "....G"});
    const auto a = find_path(g, g.start(), g.goal());
    const auto b = find_path(g, g.start(), g.goal());
    ZTH_EXPECT_TRUE(a == b || a.size() == b.size());
    long long cost = 0;
    ZTH_EXPECT_EQ(problem(g, b, g.start(), g.goal(), cost), std::string());
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "an open 200x200 map, corner to corner: at most 3000 cell lookups (Dijkstra would look at every cell: 160000)", 10) {
    const Grid g(200, 200);
    stats::reset();
    const auto path = find_path(g, {0, 0}, {199, 199});
    ZTH_EXPECT_EQ(path.size(), std::size_t{399});
    ZTH_EXPECT_TRUE(stats::lookups <= 3000ULL);
}
ZTH_TEST_TIMEOUT("fast", "each cell is expanded once: an unreachable goal on a 100x100 map costs at most 2.6 lookups per cell", 10) {
    Grid g(100, 100);
    for (int i = 0; i < 100; ++i) {
        g.set(97, i, 0);  // a wall across the map, with no gap
    }
    stats::reset();
    ZTH_EXPECT_TRUE(find_path(g, {0, 0}, {99, 50}).empty());
    ZTH_EXPECT_TRUE(stats::lookups <= 25000ULL);
}
ZTH_TEST_TIMEOUT("fast", "the heuristic is used: with rough ground in the way the search still does not explore the whole map", 10) {
    Grid g(120, 120);
    for (int y = 40; y < 80; ++y) {
        for (int x = 40; x < 80; ++x) g.set(x, y, 5);
    }
    stats::reset();
    expect_best(g, {10, 60}, {110, 60});
    stats::reset();
    const auto path = find_path(g, {10, 60}, {110, 60});
    ZTH_EXPECT_FALSE(path.empty());
    ZTH_EXPECT_TRUE(stats::lookups <= 5000ULL);
}
ZTH_TEST_TIMEOUT("fast", "stale queue entries are skipped: random costs 1 to 9 on a 100x100 map need at most 20500 lookups", 10) {
    ::zth::Lcg rng(77);
    Grid g(100, 100);
    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 100; ++x) g.set(x, y, 1 + rng.below(9));
    }
    expect_best(g, {0, 0}, {99, 99});
    stats::reset();
    ZTH_EXPECT_FALSE(find_path(g, {0, 0}, {99, 99}).empty());
    ZTH_EXPECT_TRUE(stats::lookups <= 20500ULL);
}
ZTH_TEST_TIMEOUT("fast", "doubling the map doubles the work, not quadruples it (open map, diagonal)", 10) {
    auto work = [](int n) {
        const Grid g(n, n);
        stats::reset();
        (void)find_path(g, {0, 0}, {n - 1, n - 1});
        return stats::lookups;
    };
    const auto a = work(100), b = work(200);
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5 / 2);
}
ZTH_TEST_TIMEOUT("fast", "memory: a search over a 200x200 map makes at most 100 allocations (flat vectors, no node per cell)", 10) {
    const Grid g(200, 200);
    zth::mem::Scope scope;
    (void)find_path(g, {0, 0}, {199, 199});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 100);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("astar.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("astar.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a std::priority_queue is the open list, and the per-cell data is in flat vectors (no std::map / std::set / std::list)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("astar.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "priority_queue").empty());
    for (const char* word : {"map", "multimap", "unordered_map", "list", "deque"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("astar.hpp uses `") + word + "`: index flat std::vectors by cell number instead");
            return;
        }
    }
    ZTH_EXPECT_TRUE(code.find("std::set") == std::string::npos && code.find("std::unordered_set") == std::string::npos);
}
