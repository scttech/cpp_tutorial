// A simulated robot follows a path with pure pursuit. Read-only. Press Run.
#include <cmath>
#include <cstdio>

#include "pursuit.hpp"

using namespace robot;

int main() {
    const auto path = Path::make({{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 3}});
    if (!path) return 1;
    std::printf("path: %zu segments, length %.1f\n", path->segment_count(), path->total_length());

    Params params;
    params.lookahead = 1.5;
    params.max_speed = 2.0;
    params.max_curvature = 1.0;
    params.goal_tolerance = 0.2;
    auto follower = Follower::make(*path, params);
    if (!follower) return 1;

    Pose pose{1.0, -2.0, 2.5};   // starts off the path, facing the wrong way
    const double dt = 0.05;
    double worst_error = 0;
    int steps = 0;
    for (; steps < 4000; ++steps) {
        const Command cmd = follower->step(pose);
        if (steps % 80 == 0 || cmd.done)
            std::printf("t=%6.2f  pose (%6.2f, %6.2f, %5.2f)  v=%.2f  curvature %+.2f  segment %zu%s\n", steps * dt, pose.x, pose.y, pose.theta, cmd.v, cmd.kappa, follower->segment(),
                        cmd.done ? "   DONE" : "");
        if (cmd.done) break;
        pose = advance(pose, cmd.v, cmd.kappa, dt);
        if (steps > 100) worst_error = std::fmax(worst_error, closest_on_path(*path, {pose.x, pose.y}, 0, 99).distance);
    }
    std::printf("finished after %d steps; worst distance from the path after the first 5 s: %.3f\n", steps, worst_error);
    std::printf("wrap_angle(3*pi/2) = %.4f  wrap_angle(-pi) = %.4f\n", wrap_angle(1.5 * kPi), wrap_angle(-kPi));
}
