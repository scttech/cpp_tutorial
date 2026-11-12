#pragma once
// YOUR STATE MACHINE ENGINE: it walks a table of transitions. The table, the hooks and the context are supplied by the caller (see types.hpp).
//
//   Machine(initial, transitions, n_transitions, hooks, n_hooks, ctx)    keeps POINTERS to the tables and the context (no copies, no heap)
//   state()                         the current state
//   start()                         runs the initial state's on_entry hook, once. Further calls do nothing. dispatch() calls start() first if it was not called.
//   Result dispatch(event)          looks at the rows IN TABLE ORDER; a row matches if (row.any_state || row.from == state) && row.event == event.
//                                   For a matching row: if it has a guard, call it; if the guard says false go on to the NEXT matching row.
//                                   The first row whose guard passes (or that has none) is taken:
//                                     * internal row: run its action. Nothing else happens. State, hooks unchanged.
//                                     * normal row:   on_exit(current state), then the row's action, then on_entry(new state), then state = new state.
//                                       A row whose `to` equals the current state is a real transition: exit and entry hooks DO run.
//                                   Result::Handled if a row was taken; Result::GuardRejected if there were matching rows but all guards said no;
//                                   Result::NoTransition if no row matched (the event is ignored).
//   transitions_taken()             how many non-internal transitions have happened; handled_internal() how many internal rows ran
//   A guard is only called for rows that match the state and the event, and never again after a row has been taken.
#include <cstddef>

#include "types.hpp"

namespace fsm {

template <class State, class Event, class Ctx>
class Machine {
public:
    using TransitionT = Transition<State, Event, Ctx>;
    using HooksT = Hooks<State, Ctx>;

    Machine(State initial, const TransitionT* transitions, std::size_t transition_count, const HooksT* hooks, std::size_t hook_count, Ctx& ctx)
        : state_(initial), transitions_(transitions), transition_count_(transition_count), hooks_(hooks), hook_count_(hook_count), ctx_(&ctx) {}

    State state() const { return state_; }

    void start() {}  // TODO

    Result dispatch(Event event) {
        (void)event;
        return Result::NoTransition;  // TODO
    }

    std::size_t transitions_taken() const { return 0; }  // TODO
    std::size_t handled_internal() const { return 0; }   // TODO

private:
    State state_;
    const TransitionT* transitions_;
    std::size_t transition_count_;
    const HooksT* hooks_;
    std::size_t hook_count_;
    Ctx* ctx_;
    // TODO: whether start() has run, the two counters
};

}  // namespace fsm
