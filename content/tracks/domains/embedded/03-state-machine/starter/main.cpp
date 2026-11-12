// A door controller as a transition table. The machine itself has no switch statements and no heap. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "machine.hpp"
#include "types.hpp"

enum class State { Closed, Opening, Open, Locked };
enum class Event { Press, Done, Lock, Unlock, Obstacle };
struct Door {
    int battery = 60;
    std::string log;
};
using T = fsm::Transition<State, Event, Door>;
using H = fsm::Hooks<State, Door>;

static bool has_power(const Door& d) { return d.battery > 10; }
static void note(Door& d, const char* what) { d.log += what; d.log += ' '; }

static const T kTable[] = {
    {State::Closed, Event::Press, State::Opening, has_power, [](Door& d) { note(d, "motor-on"); }},
    {State::Opening, Event::Done, State::Open, nullptr, [](Door& d) { note(d, "motor-off"); }},
    {State::Open, Event::Press, State::Closed},
    {State::Opening, Event::Obstacle, State::Closed},
    {State::Closed, Event::Lock, State::Locked},
    {State::Locked, Event::Unlock, State::Closed},
    {State::Closed, Event::Obstacle, State::Closed, nullptr, [](Door& d) { note(d, "beep"); }, /*internal=*/true},
};
static const H kHooks[] = {
    {State::Open, [](Door& d) { note(d, "light-on"); }, [](Door& d) { note(d, "light-off"); }},
    {State::Locked, [](Door& d) { note(d, "bolt-in"); }, [](Door& d) { note(d, "bolt-out"); }},
};

int main() {
    Door door;
    fsm::Machine<State, Event, Door> m(State::Closed, kTable, sizeof kTable / sizeof kTable[0], kHooks, sizeof kHooks / sizeof kHooks[0], door);
    const Event events[] = {Event::Press, Event::Done, Event::Unlock, Event::Press, Event::Lock, Event::Obstacle, Event::Unlock};
    const char* names[] = {"Press", "Done", "Unlock", "Press", "Lock", "Obstacle", "Unlock"};
    for (std::size_t i = 0; i < sizeof events / sizeof events[0]; ++i) {
        const fsm::Result r = m.dispatch(events[i]);
        std::printf("%-8s -> %-13s state %d\n", names[i], r == fsm::Result::Handled ? "handled" : r == fsm::Result::NoTransition ? "ignored" : "guard rejected", static_cast<int>(m.state()));
    }
    std::printf("log: %s\n", door.log.c_str());
    door.battery = 5;
    m.dispatch(Event::Unlock);
    std::printf("with a flat battery: %s\n", m.dispatch(Event::Press) == fsm::Result::GuardRejected ? "the guard refuses to open" : "?");
}
