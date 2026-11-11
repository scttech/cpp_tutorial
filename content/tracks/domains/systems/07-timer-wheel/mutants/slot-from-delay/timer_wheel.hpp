// MUTANT: the slot is chosen from the delay, not the deadline
#pragma once
// A hashed timing wheel: slot = deadline % slots, one slot visited per tick, lazy deletion on cancel, "collect, then fire" for re-entrancy.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
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

    explicit TimerWheel(std::size_t slots = 256) : slots_(slots == 0 ? 1 : slots) {}

    Id schedule(std::uint64_t delay, Callback callback) {
        if (!callback) return 0;
        if (delay == 0) delay = 1;
        const std::uint64_t deadline = delay > kForever - now_ ? kForever : now_ + delay;
        const Id id = next_id_++;
        auto& slot = slots_[static_cast<std::size_t>(delay % slots_.size())];
        slot.push_back(Timer{id, deadline, std::move(callback), true});
        index_[id] = &slot.back();
        return id;
    }

    bool cancel(Id id) {
        const auto it = index_.find(id);
        if (it == index_.end()) return false;
        Timer* timer = it->second;
        timer->live = false;           // swept when its slot is next visited
        timer->callback = nullptr;     // but whatever the callback captured is released now
        index_.erase(it);
        return true;
    }

    std::size_t advance(std::uint64_t ticks) {
        if (advancing_) return 0;
        advancing_ = true;
        std::size_t fired = 0;
        for (std::uint64_t i = 0; i < ticks; ++i) {
            ++now_;
            fired += run_tick();
        }
        advancing_ = false;
        return fired;
    }

    std::uint64_t now() const { return now_; }
    std::size_t pending() const { return index_.size(); }
    std::size_t slot_count() const { return slots_.size(); }

    std::size_t stored_nodes() const {
        std::size_t total = 0;
        for (const auto& slot : slots_) total += slot.size();
        return total;
    }

    std::optional<std::uint64_t> deadline(Id id) const {
        const auto it = index_.find(id);
        if (it == index_.end()) return std::nullopt;
        return it->second->deadline;
    }

    std::optional<std::uint64_t> next_deadline() const {
        std::optional<std::uint64_t> best;
        for (const auto& entry : index_) {
            if (!best || entry.second->deadline < *best) best = entry.second->deadline;
        }
        return best;
    }

    void clear() {
        for (auto& entry : index_) {
            entry.second->live = false;  // also reaches timers that are waiting to run in the current tick
            entry.second->callback = nullptr;
        }
        index_.clear();
        for (auto& slot : slots_) slot.clear();
    }

private:
    static constexpr std::uint64_t kForever = std::numeric_limits<std::uint64_t>::max();

    struct Timer {
        Id id;
        std::uint64_t deadline;
        Callback callback;
        bool live;
    };

    std::size_t run_tick() {
        std::list<Timer> due;
        auto& slot = slots_[static_cast<std::size_t>(now_ % slots_.size())];
        for (auto it = slot.begin(); it != slot.end();) {
            const auto next = std::next(it);
            if (!it->live) slot.erase(it);
            else if (it->deadline == now_) due.splice(due.end(), slot, it);
            it = next;
        }
        std::size_t fired = 0;
        for (Timer& timer : due) {
            if (!timer.live) continue;  // cancelled by a callback that ran earlier in this tick
            timer.live = false;
            index_.erase(timer.id);
            Callback callback = std::move(timer.callback);
            callback();
            ++fired;
        }
        return fired;
    }

    std::uint64_t now_ = 0;
    Id next_id_ = 1;
    bool advancing_ = false;
    std::vector<std::list<Timer>> slots_;
    std::unordered_map<Id, Timer*> index_;
};

}  // namespace wheel
