// MUTANT: the entry hook of the old state runs
#pragma once
// A linear walk over the table; exit -> action -> entry; hooks found by a linear walk over the (short) hook table.
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

    void start() {
        if (started_) return;
        started_ = true;
        if (const HooksT* h = hooks_of(state_); h != nullptr && h->on_entry != nullptr) h->on_entry(*ctx_);
    }

    Result dispatch(Event event) {
        start();
        bool matched = false;
        for (std::size_t i = 0; i < transition_count_; ++i) {
            const TransitionT& row = transitions_[i];
            if (row.event != event || !(row.any_state || row.from == state_)) continue;
            matched = true;
            if (row.guard != nullptr && !row.guard(*ctx_)) continue;
            take(row);
            return Result::Handled;
        }
        return matched ? Result::GuardRejected : Result::NoTransition;
    }

    std::size_t transitions_taken() const { return transitions_taken_; }
    std::size_t handled_internal() const { return handled_internal_; }

private:
    void take(const TransitionT& row) {
        if (row.internal) {
            if (row.action != nullptr) row.action(*ctx_);
            ++handled_internal_;
            return;
        }
        if (const HooksT* h = hooks_of(state_); h != nullptr && h->on_exit != nullptr) h->on_exit(*ctx_);
        if (row.action != nullptr) row.action(*ctx_);
        const State old = state_;
        state_ = row.to;
        if (const HooksT* h = hooks_of(old); h != nullptr && h->on_entry != nullptr) h->on_entry(*ctx_);
        ++transitions_taken_;
    }

    const HooksT* hooks_of(State s) const {
        for (std::size_t i = 0; i < hook_count_; ++i) {
            if (hooks_[i].state == s) return &hooks_[i];
        }
        return nullptr;
    }

    State state_;
    const TransitionT* transitions_;
    std::size_t transition_count_;
    const HooksT* hooks_;
    std::size_t hook_count_;
    Ctx* ctx_;
    bool started_ = false;
    std::size_t transitions_taken_ = 0;
    std::size_t handled_internal_ = 0;
};

}  // namespace fsm
