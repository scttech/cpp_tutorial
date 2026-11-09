#pragma once

#include <cmath>
#include <exception>

// A thermostat that talks to the outside world through two INTERFACES: that is what lets a test replace the real hardware.
// THIS is the code under test: you test it, you do not edit it.

class Sensor {
public:
    virtual ~Sensor() = default;
    virtual double read_celsius() = 0;  // may throw (a std::exception) when the sensor fails; may return NaN for a garbage reading
};

class Heater {
public:
    virtual ~Heater() = default;
    virtual void turn_on() = 0;
    virtual void turn_off() = 0;
};

// update() reads the sensor ONCE and then decides, with a dead band of +-hysteresis around the target so that the heater does not
// flutter on and off:
//
//   reading <  target - hysteresis   too cold: turn the heater on  (only if it is not already on: never twice in a row)
//   reading >  target + hysteresis   too warm: turn the heater off (only if it is on)
//   anything in between (including exactly target -+ hysteresis): do nothing
//
// If the sensor THROWS, or returns NaN, the thermostat does not know the temperature: it turns a heating heater off (safety first)
// and update() returns false; it never lets the exception escape. After a normal reading update() returns true.
class Thermostat {
public:
    Thermostat(Sensor& sensor, Heater& heater, double target, double hysteresis = 1.0)
        : sensor_(sensor), heater_(heater), target_(target), hysteresis_(hysteresis) {}

    bool update() {
        double celsius = 0.0;
        try {
            celsius = sensor_.read_celsius();
        } catch (const std::exception&) {
            switch_off();
            return false;
        }
        if (std::isnan(celsius)) {
            switch_off();
            return false;
        }
        if (celsius < target_ - hysteresis_) {
            heater_.turn_on();
            heating_ = true;
        } else if (celsius > target_ + hysteresis_) {
            switch_off();
        }
        return true;
    }

    bool heating() const { return heating_; }

private:
    void switch_off() {
        if (heating_) {
            heater_.turn_off();
            heating_ = false;
        }
    }

    Sensor& sensor_;
    Heater& heater_;
    double target_;
    double hysteresis_;
    bool heating_ = false;
};
