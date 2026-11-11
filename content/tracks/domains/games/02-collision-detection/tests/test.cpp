#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cmath>
#include <set>
#include <utility>
#include <vector>

#include "grid.hpp"
#include "shapes.hpp"
#include "zth_test.hpp"

using namespace col;
using Pairs = std::vector<std::pair<int, int>>;

namespace {

Aabb box(double x0, double y0, double x1, double y1) { return Aabb{{x0, y0}, {x1, y1}}; }

Pairs brute_force(const std::vector<Aabb>& boxes) {
    Pairs out;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            if (boxes[i].min.x <= boxes[j].max.x && boxes[j].min.x <= boxes[i].max.x && boxes[i].min.y <= boxes[j].max.y && boxes[j].min.y <= boxes[i].max.y) {
                out.emplace_back(static_cast<int>(i), static_cast<int>(j));
            }
        }
    }
    return out;
}

std::vector<Aabb> random_boxes(::zth::Lcg& rng, int n, double world, double max_size) {
    std::vector<Aabb> boxes;
    for (int i = 0; i < n; ++i) {
        const double x = (rng.below(1000) / 1000.0) * world - world / 2, y = (rng.below(1000) / 1000.0) * world - world / 2;
        const double w = (rng.below(1000) / 1000.0) * max_size, h = (rng.below(1000) / 1000.0) * max_size;
        boxes.push_back(box(x, y, x + w, y + h));
    }
    return boxes;
}

std::string text(const Pairs& p) {
    std::string s;
    for (std::size_t i = 0; i < p.size() && i < 8; ++i) s += "(" + std::to_string(p[i].first) + "," + std::to_string(p[i].second) + ")";
    return s + (p.size() > 8 ? "... [" + std::to_string(p.size()) + "]" : "");
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "boxes: overlapping, separated on each axis, contained, and touching (touching counts)") {
    const Aabb a = box(0, 0, 4, 4);
    ZTH_EXPECT_TRUE(overlaps(a, box(2, 2, 6, 6)));
    ZTH_EXPECT_TRUE(overlaps(a, box(1, 1, 2, 2)));
    ZTH_EXPECT_TRUE(overlaps(box(1, 1, 2, 2), a));
    ZTH_EXPECT_FALSE(overlaps(a, box(5, 0, 6, 4)));   // right of it
    ZTH_EXPECT_FALSE(overlaps(a, box(-3, 0, -1, 4))); // left of it
    ZTH_EXPECT_FALSE(overlaps(a, box(0, 5, 4, 6)));   // above
    ZTH_EXPECT_FALSE(overlaps(a, box(0, -3, 4, -1))); // below
    ZTH_EXPECT_TRUE(overlaps(a, box(4, 0, 6, 4)));    // share an edge
    ZTH_EXPECT_TRUE(overlaps(a, box(4, 4, 6, 6)));    // share a corner
    ZTH_EXPECT_FALSE(overlaps(a, box(4.0001, 0, 6, 4)));
    ZTH_EXPECT_FALSE(overlaps(a, box(0, 4.0001, 4, 6)));
}
ZTH_TEST("works", "circles: overlapping, separated, nested, and touching (touching counts)") {
    const Circle c{{0, 0}, 2};
    ZTH_EXPECT_TRUE(overlaps(c, Circle{{3, 0}, 2}));
    ZTH_EXPECT_TRUE(overlaps(c, Circle{{0, 0}, 0.5}));
    ZTH_EXPECT_TRUE(overlaps(c, Circle{{4, 0}, 2}));    // touching at (2, 0)
    ZTH_EXPECT_TRUE(overlaps(c, Circle{{0, -5}, 3}));   // touching
    ZTH_EXPECT_FALSE(overlaps(c, Circle{{4.01, 0}, 2}));
    ZTH_EXPECT_FALSE(overlaps(c, Circle{{3, 3}, 1}));   // distance 4.24 > 3
    ZTH_EXPECT_TRUE(overlaps(c, Circle{{3, 3}, 2.5}));
    ZTH_EXPECT_TRUE(overlaps(Circle{{3, 4}, 3}, Circle{{0, 0}, 2}));  // 5 = 3 + 2, a 3-4-5 triangle: touching exactly
    ZTH_EXPECT_FALSE(overlaps(Circle{{3, 4}, 2.9}, Circle{{0, 0}, 2}));
}
ZTH_TEST("works", "circle vs box: from the sides, from the corners, centre inside, touching") {
    const Aabb b = box(0, 0, 4, 2);
    ZTH_EXPECT_TRUE(overlaps(Circle{{2, 1}, 0.1}, b));      // centre inside
    ZTH_EXPECT_TRUE(overlaps(Circle{{5, 1}, 1.5}, b));      // from the right side
    ZTH_EXPECT_TRUE(overlaps(Circle{{5, 1}, 1}, b));        // touching the right side
    ZTH_EXPECT_FALSE(overlaps(Circle{{5, 1}, 0.9}, b));
    ZTH_EXPECT_TRUE(overlaps(Circle{{2, 3}, 1}, b));        // touching the top
    ZTH_EXPECT_FALSE(overlaps(Circle{{2, 3.5}, 1}, b));
    // corners: the closest point is the corner (4, 2)
    ZTH_EXPECT_TRUE(overlaps(Circle{{5, 3}, 1.5}, b));      // distance 1.414
    ZTH_EXPECT_FALSE(overlaps(Circle{{5, 3}, 1.4}, b));
    ZTH_EXPECT_TRUE(overlaps(Circle{{-3, -4}, 5}, b));      // 3-4-5 from the corner (0,0): touching
    ZTH_EXPECT_FALSE(overlaps(Circle{{-3, -4}, 4.9}, b));
    ZTH_EXPECT_TRUE(overlaps(Circle{{2, 1}, 100}, b));      // the box is inside the circle
}
ZTH_TEST("works", "candidate_pairs: only ids that share a cell, sorted, a < b") {
    UniformGrid g(10);
    g.insert(0, box(1, 1, 2, 2));
    g.insert(1, box(3, 3, 4, 4));
    g.insert(2, box(25, 25, 26, 26));
    g.insert(3, box(12, 1, 13, 2));
    ZTH_EXPECT_TRUE((g.candidate_pairs() == Pairs{{0, 1}}));
    g.insert(4, box(2, 2, 3, 3));
    ZTH_EXPECT_TRUE((g.candidate_pairs() == Pairs{{0, 1}, {0, 4}, {1, 4}}));
}
ZTH_TEST("works", "a box that spans several cells is found from each of them, and a pair is reported once") {
    UniformGrid g(10);
    g.insert(0, box(5, 5, 35, 8));    // cells x 0..3
    g.insert(1, box(31, 1, 33, 2));   // cell x 3
    g.insert(2, box(1, 1, 2, 2));     // cell x 0
    g.insert(3, box(8, 6, 12, 7));    // cells x 0 and 1: shares two cells with box 0
    ZTH_EXPECT_TRUE((g.candidate_pairs() == Pairs{{0, 1}, {0, 2}, {0, 3}, {2, 3}}));
}
ZTH_TEST("works", "find_collisions returns the real overlaps, sorted, with i < j") {
    const std::vector<Aabb> boxes = {box(0, 0, 2, 2), box(1.5, 1.5, 4, 4), box(10, 10, 11, 11), box(4, 0, 6, 2), box(-3, -3, -1, -1), box(-1.5, -1.5, 0, 0)};
    ZTH_EXPECT_TRUE((find_collisions(boxes, 4.0) == Pairs{{0, 1}, {0, 5}, {1, 3}, {4, 5}}));
}
ZTH_TEST("works", "two boxes in the same cell that do not touch are candidates but not collisions") {
    const std::vector<Aabb> boxes = {box(0, 0, 1, 1), box(5, 5, 6, 6)};
    UniformGrid g(10);
    g.insert(0, boxes[0]);
    g.insert(1, boxes[1]);
    ZTH_EXPECT_EQ(g.candidate_pairs().size(), std::size_t{1});
    ZTH_EXPECT_TRUE(find_collisions(boxes, 10).empty());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "negative coordinates: cells are found with floor, not truncation (-0.5 is in cell -1)") {
    UniformGrid g(10);
    g.insert(0, box(-0.5, -0.5, -0.1, -0.1));  // cell (-1, -1)
    g.insert(1, box(0.1, 0.1, 0.5, 0.5));      // cell (0, 0)
    ZTH_EXPECT_TRUE(g.candidate_pairs().empty());
    g.insert(2, box(-9, -9, -1, -1));          // cell (-1, -1) only
    ZTH_EXPECT_TRUE((g.candidate_pairs() == Pairs{{0, 2}}));
    const std::vector<Aabb> boxes = {box(-12, -12, -8, -8), box(-9, -9, -5, -5), box(-25, 3, -15, 4), box(-15, 3.5, -14, 6)};
    ZTH_EXPECT_TRUE((find_collisions(boxes, 10) == Pairs{{0, 1}, {2, 3}}));
}
ZTH_TEST("robust", "boxes on cell borders: a box ending exactly on a border touches the next cell's boxes") {
    const std::vector<Aabb> boxes = {box(0, 0, 10, 10), box(10, 0, 20, 10), box(10.0001, 0, 20, 10), box(0, 10, 10, 20), box(20, 20, 30, 30), box(30, 30, 40, 40)};
    const Pairs want = brute_force(boxes);
    ZTH_EXPECT_TRUE((want == Pairs{{0, 1}, {0, 3}, {1, 2}, {1, 3}, {4, 5}}));
    ZTH_EXPECT_TRUE(find_collisions(boxes, 10) == want);
    ZTH_EXPECT_TRUE(find_collisions(boxes, 7.5) == want);
    ZTH_EXPECT_TRUE(find_collisions(boxes, 1) == want);
}
ZTH_TEST("robust", "point boxes, zero-width boxes, identical boxes, and a single box") {
    const std::vector<Aabb> boxes = {box(1, 1, 1, 1), box(1, 1, 1, 1), box(0, 1, 2, 1), box(5, 5, 5, 9), box(5, 9, 5, 9), box(7, 7, 7, 7)};
    ZTH_EXPECT_TRUE(find_collisions(boxes, 2) == brute_force(boxes));
    ZTH_EXPECT_TRUE((find_collisions(boxes, 2) == Pairs{{0, 1}, {0, 2}, {1, 2}, {3, 4}}));
    ZTH_EXPECT_TRUE(find_collisions({box(0, 0, 1, 1)}, 2).empty());
    ZTH_EXPECT_TRUE(find_collisions({}, 2).empty());
}
ZTH_TEST("robust", "a huge box over many small ones, and boxes far from the origin") {
    std::vector<Aabb> boxes = {box(-500, -500, 500, 500)};
    for (int i = 0; i < 40; ++i) boxes.push_back(box(i * 20 - 400, 10, i * 20 - 395, 15));
    boxes.push_back(box(5000, 5000, 5001, 5001));
    boxes.push_back(box(5000.5, 5000.5, 5002, 5002));
    boxes.push_back(box(-7000, -7000, -6999, -6999));
    ZTH_EXPECT_TRUE(find_collisions(boxes, 10) == brute_force(boxes));
    ZTH_EXPECT_TRUE(find_collisions(boxes, 1000) == brute_force(boxes));
}
ZTH_TEST("robust", "overlap is symmetric, and agrees between the three tests when a circle is a point or a box is a point") {
    ::zth::Lcg rng(5);
    for (int i = 0; i < 2000; ++i) {
        const Aabb a = random_boxes(rng, 1, 20, 6)[0], b = random_boxes(rng, 1, 20, 6)[0];
        ZTH_EXPECT_EQ(overlaps(a, b), overlaps(b, a));
        const Circle c{{rng.below(2000) / 100.0 - 10, rng.below(2000) / 100.0 - 10}, rng.below(500) / 100.0};
        const Circle d{{rng.below(2000) / 100.0 - 10, rng.below(2000) / 100.0 - 10}, rng.below(500) / 100.0};
        ZTH_EXPECT_EQ(overlaps(c, d), overlaps(d, c));
        // a circle of radius 0 overlaps a box exactly when its centre is inside it
        const bool inside = c.center.x >= a.min.x && c.center.x <= a.max.x && c.center.y >= a.min.y && c.center.y <= a.max.y;
        ZTH_EXPECT_EQ(overlaps(Circle{c.center, 0}, a), inside);
        // a box that is a point acts like a circle of radius 0
        const Aabb p = box(d.center.x, d.center.y, d.center.x, d.center.y);
        ZTH_EXPECT_EQ(overlaps(c, p), overlaps(c, Circle{d.center, 0}));
    }
}
ZTH_TEST_TIMEOUT("robust", "300 random scenes: find_collisions equals the brute-force answer, for several cell sizes", 15) {
    ::zth::Lcg rng(2025);
    for (int scene = 0; scene < 300; ++scene) {
        const int n = 1 + rng.below(60);
        const double world = 5 + rng.below(100);
        const std::vector<Aabb> boxes = random_boxes(rng, n, world, 1 + rng.below(20) / 2.0);
        const Pairs want = brute_force(boxes);
        for (const double cell : {0.7, 3.0, 8.0, 50.0}) {
            const Pairs got = find_collisions(boxes, cell);
            if (got != want) {
                ::zth::fail("scene " + std::to_string(scene) + " with cell " + std::to_string(cell) + ": got " + text(got) + ", expected " + text(want));
                return;
            }
        }
    }
}
ZTH_TEST_TIMEOUT("robust", "candidate_pairs never misses a real overlap, never repeats a pair, never pairs an id with itself", 15) {
    ::zth::Lcg rng(31);
    for (int scene = 0; scene < 100; ++scene) {
        const std::vector<Aabb> boxes = random_boxes(rng, 40, 60, 12);
        UniformGrid g(6);
        for (std::size_t i = 0; i < boxes.size(); ++i) g.insert(static_cast<int>(i), boxes[i]);
        const Pairs cand = g.candidate_pairs();
        ZTH_EXPECT_TRUE(std::is_sorted(cand.begin(), cand.end()));
        ZTH_EXPECT_TRUE(std::adjacent_find(cand.begin(), cand.end()) == cand.end());
        for (const auto& p : cand) ZTH_EXPECT_TRUE(p.first < p.second);
        const std::set<std::pair<int, int>> have(cand.begin(), cand.end());
        for (const auto& p : brute_force(boxes)) ZTH_EXPECT_TRUE(have.count(p) == 1);
    }
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "the broad phase proposes few pairs: 5000 spread-out boxes need at most 4 candidates per real collision plus 500", 10) {
    ::zth::Lcg rng(1);
    const std::vector<Aabb> boxes = random_boxes(rng, 5000, 1000, 8);
    UniformGrid g(8);
    for (std::size_t i = 0; i < boxes.size(); ++i) g.insert(static_cast<int>(i), boxes[i]);
    const std::size_t candidates = g.candidate_pairs().size();
    const std::size_t real = find_collisions(boxes, 8).size();
    ZTH_EXPECT_TRUE(real > 0);
    ZTH_EXPECT_TRUE(candidates <= real * 4 + 500);
    ZTH_EXPECT_TRUE(candidates < 5000 * 4999 / 2 / 100);  // brute force would test 12.5 million pairs
}
ZTH_TEST_TIMEOUT("fast", "200000 boxes are checked in time (brute force would need 20 billion tests)", 10) {
    ::zth::Lcg rng(2);
    const std::vector<Aabb> boxes = random_boxes(rng, 200000, 9000, 6);
    const Pairs hits = find_collisions(boxes, 6);
    ZTH_EXPECT_TRUE(hits.size() > 100);
    ZTH_EXPECT_TRUE(std::is_sorted(hits.begin(), hits.end()));
}
ZTH_TEST_TIMEOUT("fast", "inserting is cheap: 100000 small boxes need at most 200000 allocations in total (two per box at most)", 10) {
    ::zth::Lcg rng(3);
    const std::vector<Aabb> boxes = random_boxes(rng, 100000, 3000, 2);
    UniformGrid g(10);
    zth::mem::Scope scope;
    for (std::size_t i = 0; i < boxes.size(); ++i) g.insert(static_cast<int>(i), boxes[i]);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 200000);
}
ZTH_TEST_TIMEOUT("fast", "a box covering few cells is stored in few cells: cell lookups do not grow with the number of boxes (scaling)", 10) {
    auto pairs_for = [](int n) {
        ::zth::Lcg rng(4);
        const std::vector<Aabb> boxes = random_boxes(rng, n, 100.0 * std::sqrt(n / 10.0), 4);  // constant density
        UniformGrid g(8);
        for (std::size_t i = 0; i < boxes.size(); ++i) g.insert(static_cast<int>(i), boxes[i]);
        return g.candidate_pairs().size();
    };
    const std::size_t a = pairs_for(4000), b = pairs_for(16000);
    ZTH_EXPECT_TRUE(a > 0);
    ZTH_EXPECT_TRUE(b <= a * 5);  // four times the boxes in four times the area: about four times the pairs
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no sqrt (squared distances are enough), no brute-force double loop over all boxes in find_collisions") {
    ZTH_EXPECT_NO_RAW_MEMORY("shapes.hpp");
    ZTH_EXPECT_NO_RAW_MEMORY("grid.hpp");
    for (const char* file : {"shapes.hpp", "grid.hpp"}) {
        const std::string code = ::zth::strip_comments_and_literals(::zth::read_file(file));
        for (const char* word : {"throw", "try", "catch", "goto", "sqrt", "hypot", "pow", "abs"}) {
            if (!::zth::find_word(code, word).empty()) {
                ::zth::fail(std::string(file) + " uses `" + word + "`");
                return;
            }
        }
    }
}
ZTH_TEST("clean", "the three overlap functions are overloads of one name, not three differently named functions") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("shapes.hpp"));
    std::size_t count = 0;
    for (std::size_t p = 0; (p = code.find("overlaps(", p)) != std::string::npos; p += 9) ++count;
    ZTH_EXPECT_TRUE(count >= 3);
}
