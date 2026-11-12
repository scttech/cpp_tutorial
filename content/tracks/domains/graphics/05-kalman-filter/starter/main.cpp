// Tracking a moving object from noisy position readings. Read-only. Press Run.
#include <cmath>
#include <cstdio>

#include "kalman.hpp"

using namespace kf;

int main() {
    // state: (position, velocity). The object moves at 1 unit/s; we only measure its position, with noise.
    auto filter = Kalman<2>::make({0.0, 0.0}, Mat<2>{{10.0, 0.0, 0.0, 10.0}});
    if (!filter) return 1;
    const double dt = 0.1;
    const auto model = constant_velocity(dt, 0.5);
    std::printf("  t    reading   position  velocity   std(pos)\n");
    for (int i = 1; i <= 40; ++i) {
        filter->predict(model.F, model.Q);
        const double truth = 1.0 * dt * i;
        const double reading = truth + 0.3 * std::sin(i * 1.7) + (i == 25 ? 4.0 : 0.0);   // sample 25 is a wild outlier
        const auto r = filter->update({1.0, 0.0}, reading, 0.09, 9.0);                      // gate: ignore readings more than 3 sigma away
        if (i % 4 == 0 || !r)
            std::printf("%4.1f   %7.3f   %8.3f  %8.3f   %8.3f%s\n", dt * i, reading, filter->state()[0], filter->state()[1], std::sqrt(filter->covariance()(0, 0)),
                        r ? "" : "   <- rejected as an outlier");
    }
}
