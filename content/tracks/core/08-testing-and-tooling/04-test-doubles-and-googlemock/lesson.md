# Test doubles and GoogleMock

A unit test should test **one unit**, quickly and reliably. But real code talks to other things: a temperature sensor, a database, the network, the clock, a file system. You do not want a test that needs a real sensor, takes two seconds, or fails when the network is down. The answer is to replace those collaborators with **test doubles**: stand-ins that you control.

## What a thermostat teaches

`thermostat.hpp` (read-only) has a `Thermostat` that does not know any real hardware. It receives a `Sensor&` and a `Heater&`: two **interfaces**, exactly like the ones you wrote in Module 7. In production, they are the real devices. In a test, they are doubles. This is called **dependency injection**: the object is *given* what it depends on instead of creating it, and it is the single design decision that makes code testable. (Code that calls `new RealSensor` inside, or reads a global, cannot be tested without the real thing.)

## The family of test doubles

| | what it does | example |
|---|---|---|
| **dummy** | fills a parameter, is never used | a `Heater` passed to something that only reads |
| **stub** | returns canned answers | a `Sensor` that always reads 15.0 |
| **fake** | a simplified but working implementation | an in-memory database instead of a real one |
| **spy** | records how it was called, for the test to look at afterwards | a `Heater` that counts `turn_on()` calls |
| **mock** | a double with **expectations** built in: it knows what calls it must receive, and fails the test if they do not happen | `EXPECT_CALL(heater, turn_on()).Times(1)` |

You can write each by hand (a class that implements the interface). A mocking library saves the typing and gives better failure messages. **GoogleMock** ships with GoogleTest (the same project, the same `main`) and is the usual choice for C++.

## GoogleMock in five steps

**1. Write a mock class** with `MOCK_METHOD(return type, name, (arguments), (specifiers))`:

```cpp
class MockHeater : public Heater {
public:
    MOCK_METHOD(void, turn_on, (), (override));
    MOCK_METHOD(void, turn_off, (), (override));
};
```

For a `const` method add `const` to the specifiers: `(const, override)`. The interface needs a **virtual destructor** and virtual methods: mocks work by overriding.

**2. Say what you expect** with `EXPECT_CALL(object, method(matchers))`:

```cpp
EXPECT_CALL(heater, turn_on());                       // exactly once (the default)
EXPECT_CALL(heater, turn_on()).Times(2);              // Times(0), Times(AtLeast(1)), Times(Between(1, 3))
```

**3. Say what a call returns or does** (actions):

```cpp
EXPECT_CALL(sensor, read_celsius()).WillOnce(Return(15.0)).WillOnce(Return(25.0));   // 1st call, 2nd call
EXPECT_CALL(sensor, read_celsius()).WillRepeatedly(Return(20.0));                    // every call
EXPECT_CALL(sensor, read_celsius()).WillOnce(Throw(std::runtime_error("unplugged")));
```

`Return`, `Throw`, `ReturnRef`, `SetArgPointee<N>(v)`, `Invoke(lambda)`, `DoAll(a, b)` are the usual actions.

**4. Match the arguments** when a method has some: `EXPECT_CALL(logger, write(StartsWith("error"), _))`, with `_` ("anything"), `Eq(3)`, `Gt(0)`, `HasSubstr("x")`, `AllOf(...)`, `Not(...)`, `ElementsAre(...)`. The same matchers work in `EXPECT_THAT(value, matcher)`.

**5. Choose how strict the mock is.** A call without any `EXPECT_CALL` is *uninteresting*. By default (a "naggy" mock) it prints a warning. `StrictMock<MockHeater>` makes it a **failure**, `NiceMock<MockHeater>` silences the warning. Use a `StrictMock` when "nothing else may happen" is part of the behavior (the heater must **not** be turned off when it is not on), a `NiceMock` for a collaborator that you do not care about.

Order matters sometimes: `{ InSequence s; EXPECT_CALL(a, f()); EXPECT_CALL(b, g()); }` demands that `f` is called before `g`.

The expectations are **checked when the mock is destroyed** (at the end of the test), so a call that never happened fails the test then, with a message that lists what was expected and what actually arrived.

## State testing and interaction testing

A test can check **state** ("after `update()`, `heating()` is true") or **interactions** ("`update()` called `turn_on()` once"). Prefer state when the code under test has observable state: it survives refactoring. Use interactions when the *call itself* is the behavior (send an email, turn a heater on, write a log line) and nothing else would show it.

Two traps: **over-specification** (a mock that dictates every call in the exact order, so any harmless refactoring breaks the test) and **testing the mock** (a test where all the behavior is set up in the mock and the code under test does nothing). Mock the *boundaries* of your code, not its inside, and do not mock value types such as `std::string`.

## Your task

`thermostat.hpp` documents what the `Thermostat` must do: switch the heater on below `target - hysteresis`, off above `target + hysteresis`, never twice in a row, and fail safe when the sensor throws or returns NaN. `thermostat_test.cpp` starts with `MockSensor` done. Write the `MockHeater` and the tests. As always, **Run Tests** runs your tests against the real thermostat, then against broken copies; a bug that your tests miss is listed by name with a hint.

- **Works**: the basic decisions, and how many times things happen.
- **Robust**: the boundary of the dead band, a sensor that throws, a NaN reading, a heater that is already off.
- **Clean**: mocks written with `MOCK_METHOD`, `EXPECT_CALL` with `Times`/`Return`/`Throw`, a `StrictMock` or `NiceMock`, and no compiler warnings.
