#pragma once
// READ-ONLY. The vocabulary of the table-driven state machine. Everything is plain data: tables can live in flash (const) and need no heap.
#include <cstddef>

namespace fsm {

enum class Result {
    Handled,        // a transition was taken
    NoTransition,   // no row matches the current state and the event: the event is ignored (this is normal, not an error)
    GuardRejected,  // rows match the state and the event, but every one of their guards said no
};

// One row of the transition table.
template <class State, class Event, class Ctx>
struct Transition {
    State from;                      // the state it applies to (ignored when any_state is true)
    Event event;
    State to;                        // ignored when internal is true
    bool (*guard)(const Ctx&) = nullptr;  // nullptr = always allowed
    void (*action)(Ctx&) = nullptr;       // nullptr = nothing to do
    bool internal = false;           // run the action only: no state change, no exit/entry hooks
    bool any_state = false;          // a wildcard row: applies in every state
};

// Optional hooks of a state.
template <class State, class Ctx>
struct Hooks {
    State state;
    void (*on_entry)(Ctx&) = nullptr;
    void (*on_exit)(Ctx&) = nullptr;
};

}  // namespace fsm
