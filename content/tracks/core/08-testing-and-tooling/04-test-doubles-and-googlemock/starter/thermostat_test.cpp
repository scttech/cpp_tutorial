#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "thermostat.hpp"

// Test the Thermostat WITHOUT any real hardware: replace the Sensor and the Heater by MOCKS that you control. Press "Run Tests": your
// tests must pass on the real Thermostat and fail on every broken copy of it (see thermostat.hpp for what it promises).
//
// The Clean tier asks for the GoogleMock features of the lesson: mocks written with MOCK_METHOD (the Sensor mock is done below; you write
// the Heater mock), EXPECT_CALL with Times(...) or an action such as Return/Throw, a StrictMock or NiceMock, and no compiler warnings.

using ::testing::Return;
using ::testing::StrictMock;

// A mock of the Sensor: read_celsius() does whatever the test says. (Done for you: MOCK_METHOD(return type, name, (arguments), (specifiers)).)
class MockSensor : public Sensor {
public:
    MOCK_METHOD(double, read_celsius, (), (override));
};

// TODO: class MockHeater : public Heater, with MOCK_METHODs for turn_on() and turn_off().

TEST(Thermostat, StartsNotHeating) {
    StrictMock<MockSensor> sensor;
    // Heater is still a stub: replace this by your MockHeater.
    struct IdleHeater : Heater {
        void turn_on() override {}
        void turn_off() override {}
    } heater;
    Thermostat thermostat(sensor, heater, 20.0);
    EXPECT_FALSE(thermostat.heating());
}
