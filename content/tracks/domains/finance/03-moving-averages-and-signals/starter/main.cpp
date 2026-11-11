// A price stream, a 3- and a 5-period moving average, and the signals they produce. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "averages.hpp"

int main() {
    const double prices[] = {10, 10.2, 10.1, 10.4, 10.8, 11.2, 11.5, 11.3, 10.9, 10.4, 9.9, 9.5, 9.7, 10.1, 10.6, 11.0};
    ma::Sma fast(3), slow(5);
    ma::Ema ema(5);
    ma::Crossover cross(3, 5);
    for (double p : prices) {
        const auto f = fast.push(p);
        const auto s = slow.push(p);
        const double e = ema.push(p);
        const ma::Signal signal = cross.push(p);
        std::printf("price %5.1f  sma3 %s  sma5 %s  ema5 %6.3f  %s\n", p, f ? std::to_string(*f).substr(0, 6).c_str() : "  -   ", s ? std::to_string(*s).substr(0, 6).c_str() : "  -   ", e, signal == ma::Signal::None ? "" : ma::name(signal));
    }
}
