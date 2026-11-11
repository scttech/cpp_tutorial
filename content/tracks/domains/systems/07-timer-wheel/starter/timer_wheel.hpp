#pragma once
// YOUR TIMER WHEEL: schedule callbacks to run after a number of ticks of a LOGICAL clock; cancel them; advance time. No real clock, no threads.
//
//   TimerWheel(slots = 256)             slots == 0 counts as 1
//   Id schedule(delay, callback)        fires when now() reaches now() + delay.  delay 0 counts as 1 (never "in the past").  An empty callback
//                                       schedules nothing and returns 0. Ids are 1, 2, 3 ... (never reused). A delay too big for the clock saturates at UINT64_MAX.
//   bool cancel(id)                     true if the timer was pending (it will never fire; its callback is released at once); false otherwise
//   size_t advance(ticks)               moves the clock forward ONE TICK AT A TIME, and at each tick runs the timers whose deadline is that tick,
//                                       in the order they were scheduled. Returns how many ran. During a callback now() is that timer's deadline.
//   now(), pending() (timers neither fired nor cancelled), slot_count(), stored_nodes() (nodes still held in the slots, dead ones included)
//   deadline(id)                        the deadline of a pending timer         next_deadline()   the earliest pending deadline (may scan)
//   clear()                             cancels everything
//
// CALLBACKS can do anything: schedule new timers (a delay of 1 means the NEXT tick, never the current one), cancel other timers (even one due in
// this very tick, later in the order: it must not run), call pending()/now(), clear(). A call to advance() from inside a callback does nothing and returns 0.
// While a timer's callback runs, the timer is no longer pending (cancel(its own id) is false).
//
// HOW (hashed wheel): `slots` lists of timers; a timer lives in slot  deadline % slots.  A tick looks at ONE slot only. Timers further away than one
// trip around the wheel simply stay there until their deadline comes round. Keep an unordered_map id -> timer pointer for O(1) cancel; std::list nodes
// never move, so the pointer stays valid, also when a node is spliced to another list. Cancel just marks the node dead ("lazy deletion"); the next
// visit of its slot throws it away. To run a tick safely: first move the due timers of the slot into a local list (splice), THEN run them.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wheel {

class TimerWheel {
public:
    using Id = std::uint64_t;
    using Callback = std::function<void()>;

    explicit TimerWheel(std::size_t slots = 256) : slot_count_(slots == 0 ? 1 : slots) {}

    Id schedule(std::uint64_t delay, Callback callback) {
        (void)delay;
        (void)callback;
        return 0;  // TODO
    }
    bool cancel(Id id) {
        (void)id;
        return false;  // TODO
    }
    std::size_t advance(std::uint64_t ticks) {
        (void)ticks;
        return 0;  // TODO
    }

    std::uint64_t now() const { return 0; }       // TODO
    std::size_t pending() const { return 0; }     // TODO
    std::size_t slot_count() const { return slot_count_; }
    std::size_t stored_nodes() const { return 0; }  // TODO
    std::optional<std::uint64_t> deadline(Id id) const {
        (void)id;
        return std::nullopt;  // TODO
    }
    std::optional<std::uint64_t> next_deadline() const { return std::nullopt; }  // TODO
    void clear() {}                                                             // TODO

private:
    std::size_t slot_count_;
    // TODO: the clock, the slots (std::vector<std::list<Timer>>), the index, the next id, a flag for "inside advance()"
};

}  // namespace wheel
