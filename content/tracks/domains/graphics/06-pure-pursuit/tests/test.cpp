#define ZTH_TRACK_ALLOCATIONS
#include <cmath>
#include <limits>
#include <vector>

#include "pursuit.hpp"
#include "zth_test.hpp"

using namespace robot;

namespace {

const double kNan = std::numeric_limits<double>::quiet_NaN();
const double kInf = std::numeric_limits<double>::infinity();

bool near(Pose a, Pose b, double eps = 1e-12) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(std::remainder(a.theta - b.theta, 2 * kPi)) <= eps;
}

Path path_of(std::vector<Vec2> pts) { return *Path::make(std::move(pts)); }

struct RunResult {
    int steps = 0;
    bool done = false;
    double worst_error = 0;      // largest distance to the path after the first `settle` steps
    bool segments_monotone = true;
    bool commands_ok = true;
    Pose final_pose;
};
RunResult simulate(const Path& path, Pose start, const Params& params, double dt, int max_steps, int settle) {
    RunResult r;
    auto follower = Follower::make(path, params);
    Pose pose = start;
    std::size_t last_segment = 0;
    for (; r.steps < max_steps; ++r.steps) {
        const Command cmd = follower->step(pose);
        if (follower->segment() < last_segment) r.segments_monotone = false;
        last_segment = follower->segment();
        if (!(std::fabs(cmd.kappa) <= params.max_curvature + 1e-12) || !(cmd.v >= 0.0 && cmd.v <= params.max_speed + 1e-12) || !std::isfinite(cmd.v) || !std::isfinite(cmd.kappa)) r.commands_ok = false;
        if (cmd.done) {
            r.done = true;
            break;
        }
        pose = advance(pose, cmd.v, cmd.kappa, dt);
        if (r.steps >= settle) r.worst_error = std::fmax(r.worst_error, closest_on_path(path, {pose.x, pose.y}, 0, 1000000).distance);
    }
    r.final_pose = pose;
    return r;
}

Params default_params() {
    Params p;
    p.lookahead = 1.5;
    p.max_speed = 2.0;
    p.max_curvature = 1.0;
    p.goal_tolerance = 0.2;
    return p;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "wrap_angle maps into (-pi, pi], with -pi becoming +pi") {
    ZTH_EXPECT_NEAR(wrap_angle(0.0), 0.0, 0);
    ZTH_EXPECT_NEAR(wrap_angle(1.0), 1.0, 0);
    ZTH_EXPECT_NEAR(wrap_angle(kPi), kPi, 0);
    ZTH_EXPECT_NEAR(wrap_angle(-kPi), kPi, 0);
    ZTH_EXPECT_NEAR(wrap_angle(1.5 * kPi), -0.5 * kPi, 1e-15);
    ZTH_EXPECT_NEAR(wrap_angle(-1.5 * kPi), 0.5 * kPi, 1e-15);
    ZTH_EXPECT_NEAR(wrap_angle(2 * kPi), 0.0, 1e-15);
    ZTH_EXPECT_NEAR(wrap_angle(7.0), 7.0 - 2 * kPi, 1e-14);
    ZTH_EXPECT_NEAR(wrap_angle(-7.0), -7.0 + 2 * kPi, 1e-14);
    ZTH_EXPECT_NEAR(wrap_angle(100 * kPi + 0.25), 0.25, 1e-12);
    ZTH_EXPECT_TRUE(std::isnan(wrap_angle(kNan)) && std::isnan(wrap_angle(kInf)) && std::isnan(wrap_angle(-kInf)));
}
ZTH_TEST("works", "advance: straight lines and exact circular arcs") {
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 2.0, 0.0, 3.0), {6, 0, 0}));
    ZTH_EXPECT_TRUE(near(advance({1, 2, kPi / 2}, 1.0, 0.0, 4.0), {1, 6, kPi / 2}));
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 1.0, 1.0, kPi / 2), {1, 1, kPi / 2}));     // a quarter of the unit circle, counter-clockwise
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 1.0, 1.0, kPi), {0, 2, kPi}));            // a half circle
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 1.0, -1.0, kPi / 2), {1, -1, -kPi / 2}));  // clockwise
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 1.0, 1.0, 2 * kPi), {0, 0, 0}, 1e-12));   // a full circle comes back
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, 2.0, 0.5, kPi), {0, 4, kPi}));            // radius 2: half a circle moves by the diameter
    ZTH_EXPECT_TRUE(near(advance({3, 4, 1.0}, 5.0, 2.0, 0.0), {3, 4, 1.0}));          // no time passes
    ZTH_EXPECT_TRUE(near(advance({3, 4, 1.0}, 0.0, 2.0, 5.0), {3, 4, 1.0}));          // no speed: it does not turn on the spot
    ZTH_EXPECT_TRUE(near(advance({0, 0, 0}, -1.0, 0.0, 2.0), {-2, 0, 0}));            // reversing
}
ZTH_TEST("works", "advance keeps theta in (-pi, pi] and refuses garbage by returning the pose unchanged") {
    const Pose p = advance({0, 0, 3.0}, 1.0, 1.0, 1.0);
    ZTH_EXPECT_TRUE(p.theta > -kPi && p.theta <= kPi);
    ZTH_EXPECT_NEAR(p.theta, wrap_angle(4.0), 1e-14);
    const Pose start{1, 2, 0.5};
    ZTH_EXPECT_TRUE(near(advance(start, kNan, 1.0, 1.0), start, 0));
    ZTH_EXPECT_TRUE(near(advance(start, 1.0, kInf, 1.0), start, 0));
    ZTH_EXPECT_TRUE(near(advance(start, 1.0, 1.0, kNan), start, 0));
    ZTH_EXPECT_TRUE(near(advance(start, 1.0, 1.0, -0.1), start, 0));
}
ZTH_TEST("works", "circle_segment: two roots, a tangent (one root), none, and segments that start or end inside") {
    const Roots two = circle_segment({0, 0}, 5.0, {-10, 0}, {10, 0});
    ZTH_EXPECT_TRUE(two.count == 2 && std::fabs(two.t[0] - 0.25) < 1e-15 && std::fabs(two.t[1] - 0.75) < 1e-15);
    const Roots inside_start = circle_segment({0, 0}, 5.0, {0, 0}, {10, 0});   // starts at the centre
    ZTH_EXPECT_TRUE(inside_start.count == 1 && std::fabs(inside_start.t[0] - 0.5) < 1e-15);
    const Roots inside_end = circle_segment({0, 0}, 5.0, {-10, 0}, {0, 0});
    ZTH_EXPECT_TRUE(inside_end.count == 1 && std::fabs(inside_end.t[0] - 0.5) < 1e-15);
    const Roots tangent = circle_segment({0, 0}, 5.0, {-10, 5}, {10, 5});
    ZTH_EXPECT_TRUE(tangent.count == 1 && std::fabs(tangent.t[0] - 0.5) < 1e-15);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 5.0, {-10, 5.001}, {10, 5.001}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 5.0, {-1, 0}, {1, 0}).count, 0);        // wholly inside the circle
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 5.0, {6, 0}, {20, 0}).count, 0);        // the line meets the circle, the segment does not
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 5.0, {5, 5}, {5, 5}).count, 0);         // a point
    const Roots end_touch = circle_segment({0, 0}, 5.0, {5, -3}, {5, 0});        // the line is tangent at (5, 0), the segment's end
    ZTH_EXPECT_TRUE(end_touch.count == 1 && std::fabs(end_touch.t[0] - 1.0) < 1e-12);
    const Roots diagonal = circle_segment({1, 1}, 2.0, {-5, -5}, {7, 7});
    ZTH_EXPECT_TRUE(diagonal.count == 2 && diagonal.t[0] < diagonal.t[1]);
    ZTH_EXPECT_NEAR(distance({-5 + 12 * diagonal.t[0], -5 + 12 * diagonal.t[0]}, {1, 1}), 2.0, 1e-12);
}
ZTH_TEST("works", "circle_segment refuses bad input") {
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 0.0, {-1, 0}, {1, 0}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, -1.0, {-1, 0}, {1, 0}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, kNan, {-1, 0}, {1, 0}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, kInf, {-1, 0}, {1, 0}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({kNan, 0}, 1.0, {-1, 0}, {1, 0}).count, 0);
    ZTH_EXPECT_EQ(circle_segment({0, 0}, 1.0, {kInf, 0}, {1, 0}).count, 0);
}
ZTH_TEST("works", "Path::make removes consecutive duplicates and needs two distinct finite points") {
    const auto p = Path::make({{0, 0}, {0, 0}, {3, 4}, {3, 4}, {3, 4}, {3, 10}});
    ZTH_EXPECT_TRUE(p.has_value());
    ZTH_EXPECT_EQ(p->size(), std::size_t{3});
    ZTH_EXPECT_EQ(p->segment_count(), std::size_t{2});
    ZTH_EXPECT_NEAR(p->segment_length(0), 5.0, 1e-15);
    ZTH_EXPECT_NEAR(p->segment_length(1), 6.0, 1e-15);
    ZTH_EXPECT_NEAR(p->total_length(), 11.0, 1e-15);
    ZTH_EXPECT_FALSE(Path::make({}).has_value());
    ZTH_EXPECT_FALSE(Path::make({{1, 1}}).has_value());
    ZTH_EXPECT_FALSE(Path::make({{1, 1}, {1, 1}, {1, 1}}).has_value());
    ZTH_EXPECT_FALSE(Path::make({{0, 0}, {kNan, 1}}).has_value());
    ZTH_EXPECT_FALSE(Path::make({{0, 0}, {1, kInf}}).has_value());
    ZTH_EXPECT_TRUE(Path::make({{0, 0}, {1, 0}, {0, 0}}).has_value() && Path::make({{0, 0}, {1, 0}, {0, 0}})->size() == 3);   // going back to the start is fine (not a consecutive duplicate)
}
ZTH_TEST("works", "closest_on_path: interior projection, clamping at the ends, ties to the earlier segment, and the segment window") {
    const Path path = path_of({{0, 0}, {10, 0}, {10, 10}});
    const Closest c = closest_on_path(path, {4, 3}, 0, 1);
    ZTH_EXPECT_TRUE(c.segment == 0 && std::fabs(c.t - 0.4) < 1e-15 && std::fabs(c.distance - 3.0) < 1e-15 && near(Pose{c.point.x, c.point.y, 0}, Pose{4, 0, 0}));
    const Closest before = closest_on_path(path, {-5, 2}, 0, 1);
    ZTH_EXPECT_TRUE(before.segment == 0 && before.t == 0.0 && std::fabs(before.distance - std::sqrt(29.0)) < 1e-14);
    const Closest after = closest_on_path(path, {10, 20}, 0, 1);
    ZTH_EXPECT_TRUE(after.segment == 1 && after.t == 1.0 && std::fabs(after.distance - 10.0) < 1e-14);
    const Closest corner = closest_on_path(path, {12, -2}, 0, 1);   // equally near the end of segment 0 and the start of segment 1
    ZTH_EXPECT_TRUE(corner.segment == 0 && corner.t == 1.0);
    const Closest windowed = closest_on_path(path, {4, 3}, 1, 1);   // only segment 1 is searched
    ZTH_EXPECT_TRUE(windowed.segment == 1 && std::fabs(windowed.t - 0.3) < 1e-15 && std::fabs(windowed.distance - 6.0) < 1e-14);
    const Closest clamped = closest_on_path(path, {4, 3}, 0, 99);   // indices beyond the path are clamped
    ZTH_EXPECT_TRUE(clamped.segment == 0);
    const Closest high_first = closest_on_path(path, {4, 3}, 7, 9);
    ZTH_EXPECT_TRUE(high_first.segment == 1);
}
ZTH_TEST("works", "lookahead_point: the far intersection on the current segment, on the next segment, and nothing when out of reach") {
    const Path path = path_of({{0, 0}, {10, 0}, {10, 10}});
    const auto a = lookahead_point(path, {2, 0}, 3.0, 0, 0.2);
    ZTH_EXPECT_TRUE(a.has_value() && std::fabs(a->x - 5.0) < 1e-12 && std::fabs(a->y) < 1e-12);   // the forward intersection, not the one behind the robot at x = -1 (outside) / x = 5
    const auto b = lookahead_point(path, {9, 0}, 3.0, 0, 0.9);   // the circle leaves the first segment before it reaches x = 12: the point is on segment 1
    ZTH_EXPECT_TRUE(b.has_value() && std::fabs(b->x - 10.0) < 1e-12 && std::fabs(b->y - std::sqrt(9.0 - 1.0)) < 1e-12);
    const auto behind = lookahead_point(path, {6, 0}, 3.0, 0, 0.6);   // roots at x = 3 (behind: t = 0.3 < 0.6, ignored) and x = 9
    ZTH_EXPECT_TRUE(behind.has_value() && std::fabs(behind->x - 9.0) < 1e-12);
    ZTH_EXPECT_FALSE(lookahead_point(path, {10, 9}, 3.0, 1, 0.9).has_value());   // the rest of the path (1 unit) lies inside the circle
    ZTH_EXPECT_FALSE(lookahead_point(path, {5, 20}, 3.0, 0, 0.5).has_value());   // too far from the path
    const auto start = lookahead_point(path, {0, 0}, 4.0, 0, 0.0);
    ZTH_EXPECT_TRUE(start.has_value() && std::fabs(start->x - 4.0) < 1e-12);
}
ZTH_TEST("works", "lookahead_point: with two valid roots on the first segment it takes the larger; later segments are not consulted once one has an answer") {
    const Path line = path_of({{0, 0}, {10, 0}});
    const auto far_root = lookahead_point(line, {5, 1}, 3.0, 0, 0.0);
    ZTH_EXPECT_TRUE(far_root.has_value() && std::fabs(far_root->x - (5 + std::sqrt(8.0))) < 1e-12 && std::fabs(far_root->y) < 1e-12);
    const Path u_shape = path_of({{0, 0}, {10, 0}, {10, 3}, {0, 3}});   // the circle around (5, 1.5) crosses the first AND the last leg
    const auto first_leg = lookahead_point(u_shape, {5, 1.5}, 2.4, 0, 0.2);
    ZTH_EXPECT_TRUE(first_leg.has_value() && std::fabs(first_leg->y) < 1e-12 && std::fabs(first_leg->x - (5 + std::sqrt(5.76 - 2.25))) < 1e-12);
    const auto from_last = lookahead_point(u_shape, {5, 1.5}, 2.4, 2, 0.0);
    ZTH_EXPECT_TRUE(from_last.has_value() && std::fabs(from_last->y - 3.0) < 1e-12 && std::fabs(from_last->x - (5 - std::sqrt(5.76 - 2.25))) < 1e-12);   // that leg runs right to left: its larger root is the left one
}
ZTH_TEST("works", "speed and steering near the end: the goal is the carrot when the end is in sight, and the speed falls to a floor of 10 percent") {
    const Path path = path_of({{0, 0}, {10, 0}});
    auto f = Follower::make(path, default_params());   // lookahead 1.5, tolerance 0.2, max speed 2
    const Command off = f->step({9.0, 0.5, 0});         // no intersection left: aim at the goal (10, 0): lx = 1, ly = -0.5, kappa = -1 / 1.25
    ZTH_EXPECT_NEAR(off.kappa, -0.8, 1e-12);
    auto g = Follower::make(path, default_params());
    ZTH_EXPECT_NEAR(g->step({9.25, 0.0, 0}).v, 2.0 * 0.5, 1e-12);       // 0.75 from the goal, lookahead 1.5: half speed
    auto h = Follower::make(path, default_params());
    ZTH_EXPECT_NEAR(h->step({9.7, 0.0, 0}).v, 2.0 * (0.3 / 1.5), 1e-12);
    Params far = default_params();
    far.lookahead = 5.0;
    auto k = Follower::make(path, far);
    ZTH_EXPECT_NEAR(k->step({9.7, 0.0, 0}).v, 2.0 * 0.1, 1e-12);        // 0.3 / 5 = 0.06 is below the floor
    ZTH_EXPECT_NEAR(k->step({9.7, 0.0, 0}).kappa, 0.0, 1e-12);
}
ZTH_TEST("works", "Follower::make validates its parameters") {
    const Path path = path_of({{0, 0}, {10, 0}});
    Params p = default_params();
    ZTH_EXPECT_TRUE(Follower::make(path, p).has_value());
    p.goal_tolerance = 0.0;
    ZTH_EXPECT_TRUE(Follower::make(path, p).has_value());
    for (int field = 0; field < 4; ++field)
        for (double bad : {0.0, -1.0, kNan, kInf}) {
            Params q = default_params();
            double* f = field == 0 ? &q.lookahead : field == 1 ? &q.max_speed : field == 2 ? &q.max_curvature : &q.goal_tolerance;
            *f = bad;
            const bool valid = field == 3 && bad == 0.0;
            ZTH_EXPECT_EQ(Follower::make(path, q).has_value(), valid);
        }
}
ZTH_TEST("works", "a robot on the path pointing along it drives straight: curvature 0 at full speed; off to the side it steers towards the path") {
    const Path path = path_of({{0, 0}, {100, 0}});
    auto f = Follower::make(path, default_params());
    const Command straight = f->step({5, 0, 0});
    ZTH_EXPECT_NEAR(straight.kappa, 0.0, 1e-12);
    ZTH_EXPECT_NEAR(straight.v, 2.0, 1e-12);
    ZTH_EXPECT_FALSE(straight.done);
    auto g = Follower::make(path, default_params());
    const Command left_of_path = g->step({5, 1.0, 0});   // above the path, heading along it: must turn right (negative)
    ZTH_EXPECT_TRUE(left_of_path.kappa < -0.05 && left_of_path.v < 2.0 && left_of_path.v > 0.0);
    auto h = Follower::make(path, default_params());
    const Command right_of_path = h->step({5, -1.0, 0});
    ZTH_EXPECT_TRUE(right_of_path.kappa > 0.05);
    ZTH_EXPECT_NEAR(right_of_path.kappa, -left_of_path.kappa, 1e-12);   // mirror image
}
ZTH_TEST("works", "the curvature is 2 ly / d^2 for the carrot: a known configuration") {
    const Path path = path_of({{0, 0}, {100, 0}});
    Params p = default_params();
    p.lookahead = 5.0;
    p.max_curvature = 10.0;
    auto f = Follower::make(path, p);
    // robot at (0, 3) facing +x: the circle of radius 5 around it meets the path at x = 4 (the carrot is at (4, 0)): lx = 4, ly = -3, d2 = 25, kappa = -6 / 25
    const Command c = f->step({0, 3, 0});
    ZTH_EXPECT_NEAR(c.kappa, -6.0 / 25.0, 1e-12);
    // the same robot facing +y: the carrot (4, 0) is at lx = -3 (behind), ly = -4... it is rotated by 90 degrees: lx = dy*... compute directly
    auto g = Follower::make(path, p);
    const Command d = g->step({0, 3, kPi / 2});   // facing +y; carrot at (4, 0): dx = 4, dy = -3 -> lx = -3, ly = -4: behind and to the right
    ZTH_EXPECT_NEAR(d.kappa, -10.0, 1e-12);    // turn as hard as possible towards the right
}
ZTH_TEST("works", "a carrot behind the robot means a full-lock turn towards its side; the speed is reduced but not zero") {
    const Path path = path_of({{0, 0}, {100, 0}});
    Params p = default_params();
    auto f = Follower::make(path, p);
    const Command behind_left = f->step({10, 0.5, kPi});   // facing back along the path
    ZTH_EXPECT_NEAR(std::fabs(behind_left.kappa), p.max_curvature, 1e-12);
    ZTH_EXPECT_TRUE(behind_left.v > 0.0 && behind_left.v < p.max_speed * 0.25);
    auto g = Follower::make(path, p);
    const Path backwards = path_of({{0, 0}, {-100, 0}});
    auto h = Follower::make(backwards, p);
    const Command turn = h->step({-10, 0.0, 0.0});       // the carrot is exactly behind (ly == 0): counter-clockwise
    ZTH_EXPECT_NEAR(turn.kappa, p.max_curvature, 1e-12);
    (void)g;
}
ZTH_TEST("works", "finishing: inside the tolerance of the last point the command is done, the speed 0, and it stays done") {
    const Path path = path_of({{0, 0}, {10, 0}});
    Params p = default_params();
    p.goal_tolerance = 0.25;
    auto f = Follower::make(path, p);
    const Command near_goal = f->step({9.9, 0.0, 0});   // 0.1 from the end
    ZTH_EXPECT_TRUE(near_goal.done && near_goal.v == 0.0 && near_goal.kappa == 0.0);
    ZTH_EXPECT_TRUE(f->step({0.0, 0.0, 0}).done);   // done stays done, whatever the pose
    auto g = Follower::make(path, p);
    ZTH_EXPECT_FALSE(g->step({9.74, 0.0, 0}).done);   // 0.26 away: not yet
    auto h = Follower::make(path, p);
    ZTH_EXPECT_TRUE(h->step({9.75, 0.0, 0}).done);    // exactly the tolerance (both numbers are exact in binary): done, the comparison is <=
}
ZTH_TEST("works", "a pose that is not finite gives an idle command and changes nothing") {
    const Path path = path_of({{0, 0}, {10, 0}});
    auto f = Follower::make(path, default_params());
    for (Pose bad : {Pose{kNan, 0, 0}, Pose{0, kInf, 0}, Pose{0, 0, kNan}}) {
        const Command c = f->step(bad);
        ZTH_EXPECT_TRUE(c.v == 0.0 && c.kappa == 0.0 && !c.done);
    }
    ZTH_EXPECT_EQ(f->segment(), std::size_t{0});
    ZTH_EXPECT_FALSE(f->step({1, 0, 0}).done);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "closed loop on a straight path: a robot starting 3 m off and facing 90 degrees away converges onto the line and arrives") {
    const Path path = path_of({{0, 0}, {60, 0}});
    for (double theta : {0.0, 1.0, 1.57, 2.5, 3.14, -1.0, -2.5}) {
        const RunResult r = simulate(path, {0.0, 3.0, theta}, default_params(), 0.05, 6000, 0);
        ZTH_EXPECT_TRUE(r.done);
        ZTH_EXPECT_TRUE(r.segments_monotone && r.commands_ok);
        ZTH_EXPECT_TRUE(distance({r.final_pose.x, r.final_pose.y}, {60, 0}) <= 0.2 + 0.2);
    }
    const RunResult settled = simulate(path, {0.0, 3.0, 0.0}, default_params(), 0.05, 6000, 400);   // after 20 s the lateral error is tiny
    ZTH_EXPECT_TRUE(settled.done && settled.worst_error < 0.05);
}
ZTH_TEST("robust", "closed loop on a square path: it follows the corners (cutting them by a bounded amount) and finishes") {
    const Path path = path_of({{0, 0}, {20, 0}, {20, 20}, {0, 20}, {0, 3}});
    const RunResult r = simulate(path, {1.0, -2.0, 2.5}, default_params(), 0.05, 8000, 200);
    ZTH_EXPECT_TRUE(r.done && r.segments_monotone && r.commands_ok);
    ZTH_EXPECT_TRUE(r.worst_error < 2.0);   // pure pursuit cuts corners: the cut is bounded by about the lookahead distance
    ZTH_EXPECT_TRUE(distance({r.final_pose.x, r.final_pose.y}, {0, 3}) <= 0.4);
}
ZTH_TEST("robust", "closed loop on a circle-shaped polygon: the tracking error is small and steady") {
    std::vector<Vec2> pts;
    for (int i = 0; i <= 68; ++i) pts.push_back({20.0 * std::cos(i * 2 * kPi / 72), 20.0 * std::sin(i * 2 * kPi / 72)});   // stops 40 degrees short of closing: a path that ends where it starts is 'done' at once
    const Path path = path_of(pts);
    Params p = default_params();
    p.lookahead = 2.0;
    p.max_curvature = 2.0;
    const RunResult r = simulate(path, {20.0, 0.0, kPi / 2}, p, 0.02, 20000, 300);
    ZTH_EXPECT_TRUE(r.done && r.segments_monotone && r.commands_ok);
    ZTH_EXPECT_TRUE(r.worst_error < 0.25);
}
ZTH_TEST("robust", "headings around +-pi: the robot does not spin the long way, wrapped angles behave like their equivalents") {
    const Path path = path_of({{0, 0}, {-60, 0}});   // the path runs in the -x direction
    const RunResult a = simulate(path, {0.0, 1.0, kPi - 0.01}, default_params(), 0.05, 4000, 0);
    const RunResult b = simulate(path, {0.0, 1.0, -kPi + 0.01}, default_params(), 0.05, 4000, 0);
    ZTH_EXPECT_TRUE(a.done && b.done && a.commands_ok && b.commands_ok);
    ZTH_EXPECT_TRUE(a.steps < 1200 && b.steps < 1200);
    // the same situation seen with the heading written differently gives the same command
    auto f = Follower::make(path, default_params()), g = Follower::make(path, default_params());
    const Command c1 = f->step({-10, 1.0, kPi - 0.01}), c2 = g->step({-10, 1.0, kPi - 0.01 - 6 * kPi});
    ZTH_EXPECT_NEAR(c1.kappa, c2.kappa, 1e-9);
    ZTH_EXPECT_NEAR(c1.v, c2.v, 1e-9);
}
ZTH_TEST("robust", "random gentle paths and random start poses: the follower never produces NaN, never exceeds its limits, never goes back along the path") {
    zth::Lcg rng(2025);
    int finished = 0;
    for (int round = 0; round < 60; ++round) {
        std::vector<Vec2> pts = {{0, 0}};
        double heading = (rng.below(628) - 314) / 100.0;
        Vec2 cur{0, 0};
        const int n = 3 + rng.below(8);
        for (int i = 0; i < n; ++i) {
            heading += (rng.below(100) - 50) / 100.0;   // turns of at most ~29 degrees
            const double len = 6.0 + rng.below(100) / 10.0;
            cur = {cur.x + len * std::cos(heading), cur.y + len * std::sin(heading)};
            pts.push_back(cur);
        }
        const Path path = path_of(pts);
        Params p = default_params();
        p.lookahead = 1.0 + rng.below(30) / 10.0;
        const Pose start{(rng.below(600) - 300) / 100.0, (rng.below(600) - 300) / 100.0, (rng.below(628) - 314) / 100.0};
        const RunResult r = simulate(path, start, p, 0.05, 12000, 0);
        ZTH_EXPECT_TRUE(r.segments_monotone);
        ZTH_EXPECT_TRUE(r.commands_ok);
        ZTH_EXPECT_TRUE(is_finite(r.final_pose));
        if (r.done) ++finished;
    }
    ZTH_EXPECT_TRUE(finished >= 50);   // gentle paths are almost always completed
}
ZTH_TEST("robust", "advance: two half steps equal one full step on an exact arc, and a tiny curvature is a straight line without noise") {
    zth::Lcg rng(7);
    for (int i = 0; i < 1000; ++i) {
        const Pose p{(rng.below(2001) - 1000) / 10.0, (rng.below(2001) - 1000) / 10.0, (rng.below(6283) - 3141) / 1000.0};
        const double v = (rng.below(4001) - 2000) / 1000.0, k = (rng.below(4001) - 2000) / 1000.0, dt = rng.below(5000) / 1000.0;
        const Pose whole = advance(p, v, k, dt);
        const Pose halves = advance(advance(p, v, k, dt / 2), v, k, dt / 2);
        ZTH_EXPECT_TRUE(near(whole, halves, 1e-10));
        ZTH_EXPECT_TRUE(whole.theta > -kPi && whole.theta <= kPi);
    }
    for (double k : {1e-3, 1e-6, 1e-9, 1e-12, 1e-15, 0.0, -1e-12}) {
        const Pose p = advance({0, 0, 0.3}, 2.0, k, 5.0);
        const double u = k * 10.0 / 2.0;   // half the turn angle
        const double chord = 10.0 * (u == 0.0 ? 1.0 : std::sin(u) / u);
        ZTH_EXPECT_NEAR(p.x, chord * std::cos(0.3 + u), 1e-9);
        ZTH_EXPECT_NEAR(p.y, chord * std::sin(0.3 + u), 1e-9);
    }
    const Pose straightish = advance({0, 0, 0}, 1.0, 1e-12, 100.0);
    ZTH_EXPECT_NEAR(straightish.x, 100.0, 1e-9);
    ZTH_EXPECT_NEAR(straightish.y, 5e-9, 1e-12);   // the arc really does curve by d^2 kappa / 2
    ZTH_EXPECT_NEAR(std::hypot(advance({0, 0, 0}, 1.0, 1e-5, 1000.0).x, advance({0, 0, 0}, 1.0, 1e-5, 1000.0).y), 1000.0 * std::sin(0.005) / 0.005, 1e-6);
}
ZTH_TEST("robust", "circle_segment against brute force: every root is on the circle, every crossing of the circle is found") {
    zth::Lcg rng(31);
    for (int round = 0; round < 3000; ++round) {
        const Vec2 c{(rng.below(201) - 100) / 10.0, (rng.below(201) - 100) / 10.0};
        const double r = 0.1 + rng.below(100) / 10.0;
        Vec2 a{(rng.below(401) - 200) / 10.0, (rng.below(401) - 200) / 10.0}, b{(rng.below(401) - 200) / 10.0, (rng.below(401) - 200) / 10.0};
        if (rng.below(8) == 0) b = a;
        const Roots roots = circle_segment(c, r, a, b);
        for (int k = 0; k < roots.count; ++k) {
            ZTH_EXPECT_TRUE(roots.t[k] >= 0.0 && roots.t[k] <= 1.0);
            ZTH_EXPECT_NEAR(distance(a + (b - a) * roots.t[k], c), r, 1e-9 * (1 + r));
            if (k > 0) ZTH_EXPECT_TRUE(roots.t[k] > roots.t[k - 1]);
        }
        // the number of crossings equals the number of sign changes of (distance - r) along a fine sampling (tangent cases aside)
        int crossings = 0;
        double prev = distance(a, c) - r;
        for (int s = 1; s <= 2000; ++s) {
            const double cur = distance(a + (b - a) * (s / 2000.0), c) - r;
            if ((prev < 0) != (cur < 0)) ++crossings;
            prev = cur;
        }
        if (!(a.x == b.x && a.y == b.y)) ZTH_EXPECT_TRUE(roots.count >= crossings);
    }
}
ZTH_TEST("robust", "circle_segment with a segment 2e8 long through a circle of radius 1: the textbook discriminant has no digits left") {
    const Roots r = circle_segment({0, 0}, 1.0, {-1e8, 0.5}, {1e8, 0.5});
    ZTH_EXPECT_TRUE(r.count == 2);
    ZTH_EXPECT_NEAR(r.t[0], 0.5 - std::sqrt(0.75) / 2e8, 1e-13);
    ZTH_EXPECT_NEAR(r.t[1], 0.5 + std::sqrt(0.75) / 2e8, 1e-13);
    ZTH_EXPECT_NEAR(r.t[1] - r.t[0], std::sqrt(3.0) / 2e8, 3e-16);
}
ZTH_TEST("robust", "circle_segment is stable far from the origin: a circle of radius 1 at (1e8, 1e8) crossed by a long segment") {
    const Roots r = circle_segment({1e8, 1e8}, 1.0, {1e8 - 5, 1e8 + 0.5}, {1e8 + 5, 1e8 + 0.5});
    ZTH_EXPECT_TRUE(r.count == 2);
    ZTH_EXPECT_NEAR(r.t[0], 0.5 - std::sqrt(0.75) / 10, 1e-7);
    ZTH_EXPECT_NEAR(r.t[1], 0.5 + std::sqrt(0.75) / 10, 1e-7);
    const Roots miss = circle_segment({1e8, 1e8}, 1.0, {1e8 - 5, 1e8 + 1.01}, {1e8 + 5, 1e8 + 1.01});
    ZTH_EXPECT_EQ(miss.count, 0);
}
ZTH_TEST("robust", "closest_on_path against an independent distance-to-segment formula, for random paths and points") {
    zth::Lcg rng(4);
    for (int round = 0; round < 500; ++round) {
        std::vector<Vec2> pts;
        for (int i = 0, n = 2 + rng.below(8); i < n; ++i) pts.push_back({(rng.below(401) - 200) / 10.0, (rng.below(401) - 200) / 10.0});
        const auto path = Path::make(pts);
        if (!path) continue;
        const Vec2 q{(rng.below(601) - 300) / 10.0, (rng.below(601) - 300) / 10.0};
        const Closest c = closest_on_path(*path, q, 0, 100);
        double best = kInf;
        for (std::size_t i = 0; i < path->segment_count(); ++i) {
            const Vec2 a = path->points()[i], b = path->points()[i + 1];
            const Vec2 d = b - a;
            const double len2 = dot(d, d);
            double t = dot(q - a, d) / len2;
            double dist;
            if (t <= 0) dist = distance(q, a);
            else if (t >= 1) dist = distance(q, b);
            else dist = std::fabs(cross(d, q - a)) / std::sqrt(len2);   // the perpendicular distance
            best = std::min(best, dist);
        }
        ZTH_EXPECT_NEAR(c.distance, best, 1e-9);
        ZTH_EXPECT_NEAR(distance(q, c.point), c.distance, 1e-9);
    }
}
ZTH_TEST("robust", "the nearest-point search covers only a window of segments ahead: a far-away leg that is closer in space is not adopted") {
    std::vector<Vec2> pts;
    for (int i = 0; i <= 20; ++i) pts.push_back({i * 5.0, 0.0});          // out along y = 0 (20 segments)
    for (int i = 20; i >= 0; --i) pts.push_back({i * 5.0, 1.0});          // and back along y = 1 (20 segments)
    const Path path = path_of(pts);
    auto f = Follower::make(path, default_params());
    f->step({32.0, 0.9, 0.0});   // 0.9 from the outbound leg, 0.1 from the return leg that is 30+ segments ahead
    ZTH_EXPECT_TRUE(f->segment() <= 8);
    ZTH_EXPECT_EQ(f->segment(), std::size_t{6});
    f->step({33.0, 0.9, 0.0});
    ZTH_EXPECT_EQ(f->segment(), std::size_t{6});
    f->step({41.0, 0.9, 0.0});   // window is segment 6 .. 14: the return leg is still out of reach
    ZTH_EXPECT_EQ(f->segment(), std::size_t{8});
}
ZTH_TEST("robust", "the follower does not jump ahead on a path that doubles back next to itself") {
    // a hairpin: the second leg passes 1 m from the first; the follower must stay on the leg it is on until it gets there
    const Path path = path_of({{0, 0}, {30, 0}, {30, 1.0}, {0, 1.0}});
    Params p = default_params();
    p.lookahead = 0.8;
    p.max_curvature = 3.0;
    auto f = Follower::make(path, p);
    Pose pose{0.0, 0.0, 0.0};
    std::size_t max_seen = 0;
    bool reached_far_end = false;
    for (int i = 0; i < 3000; ++i) {
        const Command cmd = f->step(pose);
        if (cmd.done) break;
        pose = advance(pose, cmd.v, cmd.kappa, 0.05);
        max_seen = std::max(max_seen, f->segment());
        if (pose.x > 29.0) reached_far_end = true;
        if (!reached_far_end && f->segment() == 2) {
            zth::fail("the follower skipped the first leg of the hairpin");
            return;
        }
    }
    ZTH_EXPECT_TRUE(max_seen >= 1);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "step, advance and the geometry helpers never allocate") {
    const Path path = path_of({{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}});
    auto f = Follower::make(path, default_params());
    Pose pose{1, -1, 1};
    zth::mem::Scope scope;
    double sink = 0;
    for (int i = 0; i < 5000; ++i) {
        const Command c = f->step(pose);
        pose = advance(pose, c.v, c.kappa, 0.02);
        sink += wrap_angle(pose.theta * 100) + circle_segment({pose.x, pose.y}, 2.0, {0, 0}, {10, 0}).count + closest_on_path(path, {pose.x, pose.y}, 0, 3).distance;
    }
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(std::isfinite(sink));
}
ZTH_TEST_TIMEOUT("fast", "a million control steps on a 1000-segment path (the search is a window, not the whole path)", 20) {
    std::vector<Vec2> pts;
    for (int i = 0; i <= 1000; ++i) pts.push_back({i * 5.0, 3.0 * std::sin(i * 0.2)});
    const Path path = path_of(pts);
    Params p = default_params();
    p.lookahead = 3.0;
    p.max_speed = 4.0;
    p.goal_tolerance = 0.5;
    auto f = Follower::make(path, p);
    Pose pose{0, 0, 0};
    int steps = 0;
    for (; steps < 1000000; ++steps) {
        const Command c = f->step(pose);
        if (c.done) break;
        pose = advance(pose, c.v, c.kappa, 0.05);
    }
    ZTH_EXPECT_TRUE(f->segment() > 900);
    ZTH_EXPECT_TRUE(steps > 1000);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto, no std::function or virtual calls") {
    ZTH_EXPECT_NO_RAW_MEMORY("pursuit.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pursuit.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto", "function", "virtual", "new"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "angles with fmod (not a loop that subtracts 2 pi) and exact arcs (no Euler stepping); no acos or atan2 needed") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pursuit.hpp"));
    for (const char* word : {"while", "acos", "asin", "atan2", "atan", "pow"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "fmod").empty() && ::zth::find_word(code, "remainder").empty());
}
