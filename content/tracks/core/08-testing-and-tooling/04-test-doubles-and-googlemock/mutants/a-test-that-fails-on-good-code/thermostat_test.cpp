// MUTANT: a wrong expectation: the tests must pass on the real thermostat first.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

#include "thermostat.hpp"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Throw;

class MockSensor : public Sensor {
public:
    MOCK_METHOD(double, read_celsius, (), (override));
};

class MockHeater : public Heater {
public:
    MOCK_METHOD(void, turn_on, (), (override));
    MOCK_METHOD(void, turn_off, (), (override));
};

// A StrictMock turns every call that has no EXPECT_CALL into a failure, so a test also says what must NOT happen.
class ThermostatTest : public ::testing::Test {
protected:
    StrictMock<MockSensor> sensor;
    StrictMock<MockHeater> heater;
    Thermostat thermostat{sensor, heater, 20.0, 1.0};  // heats below 19, switches off above 21

    void reading_is(double celsius) { EXPECT_CALL(sensor, read_celsius()).WillRepeatedly(Return(celsius)); }
};

// ---- the everyday behavior ----------------------------------------------------------------------------------------

TEST_F(ThermostatTest, AColdRoomTurnsTheHeaterOn) {
    reading_is(15.0);
    EXPECT_CALL(heater, turn_on());
    EXPECT_TRUE(thermostat.update());
    EXPECT_FALSE(thermostat.heating());
}

TEST_F(ThermostatTest, AWarmRoomTurnsAHeatingHeaterOff) {
    EXPECT_CALL(sensor, read_celsius()).WillOnce(Return(15.0)).WillOnce(Return(25.0));
    {
        InSequence order;
        EXPECT_CALL(heater, turn_on());
        EXPECT_CALL(heater, turn_off());
    }
    thermostat.update();
    thermostat.update();
    EXPECT_FALSE(thermostat.heating());
}

TEST_F(ThermostatTest, UpdateReadsTheSensorExactlyOnce) {
    EXPECT_CALL(sensor, read_celsius()).Times(1).WillOnce(Return(20.0));
    thermostat.update();
}

TEST_F(ThermostatTest, AColdRoomTurnsTheHeaterOnOnlyOnceNoMatterHowOftenItIsChecked) {
    reading_is(15.0);
    EXPECT_CALL(heater, turn_on()).Times(1);
    thermostat.update();
    thermostat.update();
    thermostat.update();
}

TEST_F(ThermostatTest, AWarmRoomWithTheHeaterOffDoesNothing) {
    reading_is(25.0);  // no EXPECT_CALL on the heater: the StrictMock fails the test if the thermostat touches it
    thermostat.update();
    thermostat.update();
}

// ---- the edges -----------------------------------------------------------------------------------------------------

TEST_F(ThermostatTest, ExactlyOnTheEdgeOfTheDeadBandNothingHappens) {
    EXPECT_CALL(sensor, read_celsius()).WillOnce(Return(19.0)).WillOnce(Return(15.0)).WillOnce(Return(21.0));
    thermostat.update();                       // 19.0 is not below 19.0: nothing (the StrictMock heater has no expectation yet)
    EXPECT_CALL(heater, turn_on()).Times(1);   // only for the 15.0 that follows
    thermostat.update();                       // 15.0: on
    thermostat.update();                       // 21.0 is not above 21.0: stays on, no turn_off
    EXPECT_TRUE(thermostat.heating());
}

TEST_F(ThermostatTest, ANotANumberReadingTurnsAHeatingHeaterOffAndIsReported) {
    EXPECT_CALL(sensor, read_celsius()).WillOnce(Return(15.0)).WillOnce(Return(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_CALL(heater, turn_on());
    EXPECT_CALL(heater, turn_off());
    EXPECT_TRUE(thermostat.update());
    EXPECT_FALSE(thermostat.update());
}

TEST_F(ThermostatTest, ASensorThatThrowsTurnsAHeatingHeaterOffAndIsReportedNotThrown) {
    EXPECT_CALL(sensor, read_celsius()).WillOnce(Return(15.0)).WillOnce(Throw(std::runtime_error("sensor unplugged")));
    EXPECT_CALL(heater, turn_on());
    EXPECT_CALL(heater, turn_off());
    EXPECT_TRUE(thermostat.update());
    bool ok = true;
    EXPECT_NO_THROW(ok = thermostat.update());
    EXPECT_FALSE(ok);
    EXPECT_FALSE(thermostat.heating());
}
