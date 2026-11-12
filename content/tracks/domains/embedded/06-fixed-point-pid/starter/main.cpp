// A heater controlled by a PID loop in Q16.16 fixed point: no floating point anywhere. Read-only. Press Run.
#include <cstdint>
#include <cstdio>

#include "pid.hpp"

using ctl::Fixed;

int main() {
    // gains and limits as exact ratios
    ctl::PidConfig cfg{Fixed::from_ratio(5, 2), Fixed::from_ratio(1, 2), Fixed::from_ratio(1, 10), Fixed::from_int(0), Fixed::from_int(100)};
    ctl::Pid pid(cfg);
    const Fixed setpoint = Fixed::from_int(60);   // degrees
    const Fixed dt = Fixed::from_ratio(1, 10);    // 0.1 s per step
    Fixed temperature = Fixed::from_int(20);
    for (int step = 0; step <= 100; ++step) {
        const Fixed heater = pid.update(setpoint, temperature, dt);   // 0..100 % power
        // the plant: temperature rises with heater power and leaks to the surroundings (20 degrees)
        const Fixed heating = heater * Fixed::from_ratio(1, 20);
        const Fixed leak = (temperature - Fixed::from_int(20)) * Fixed::from_ratio(1, 50);
        temperature = temperature + (heating - leak) * dt * Fixed::from_int(10);
        if (step % 10 == 0) std::printf("t=%4.1fs  power %6.2f%%  temperature %6.2f  (integral %.2f)\n", step / 10.0, heater.raw() / 65536.0, temperature.raw() / 65536.0, pid.integral().raw() / 65536.0);
    }
}
