// Interpolating a table, and solving f(x) = 0 three ways. Read-only. Press Run.
#include <cmath>
#include <cstdio>

#include "numerics.hpp"

int main() {
    const auto line = numeric::Interp::make({0, 1, 2, 4}, {0, 2, 1, 5});
    const auto curve = numeric::Spline::make({0, 1, 2, 3, 4.5}, {0, 1, 0, 2, 1});
    if (!line || !curve) return 1;
    for (double x : {0.5, 1.5, 3.0, 4.0}) std::printf("x = %.1f   linear %.4f   spline %.4f\n", x, line->operator()(x).value_or(-99), x <= 4.5 ? (*curve)(x).value_or(-99) : -99);
    std::printf("x = 5.0   linear %s\n", (*line)(5.0) ? "(value)" : "(outside the table)");

    auto f = [](double x) { return std::cos(x) - x; };
    auto df = [](double x) { return -std::sin(x) - 1.0; };
    if (const auto r = numeric::bisect(f, 0.0, 1.0)) std::printf("bisection       : x = %.12f after %d iterations\n", r->x, r->iterations);
    if (const auto r = numeric::newton(f, df, 0.5)) std::printf("Newton          : x = %.12f after %d iterations\n", r->x, r->iterations);
    if (const auto r = numeric::safe_newton(f, df, 0.0, 1.0)) std::printf("safe Newton     : x = %.12f after %d iterations\n", r->x, r->iterations);

    auto g = [](double x) { return std::atan(x); };
    auto dg = [](double x) { return 1.0 / (1.0 + x * x); };
    std::printf("atan from 1.5   : Newton %s, safe Newton %s\n", numeric::newton(g, dg, 1.5) ? "converged" : "diverged (refused)", numeric::safe_newton(g, dg, -10.0, 20.0) ? "converged" : "failed");
}
