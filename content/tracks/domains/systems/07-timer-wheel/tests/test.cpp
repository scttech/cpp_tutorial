#define ZTH_TRACK_ALLOCATIONS
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "timer_wheel.hpp"
#include "zth_test.hpp"

using wheel::TimerWheel;
using Id = TimerWheel::Id;

namespace {

struct Fired {
    int tag;
    std::uint64_t at;
};

// Schedules a callback that records (tag, now) in `log`.
Id note(TimerWheel& w, std::vector<Fired>& log, std::uint64_t delay, int tag) {
    return w.schedule(delay, [&w, &log, tag] { log.push_back({tag, w.now()}); });
}

std::string show(const std::vector<Fired>& log) {
    std::string s;
    for (const Fired& f : log) s += std::to_string(f.tag) + "@" + std::to_string(f.at) + " ";
    return s;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a new wheel is empty and time passes") {
    TimerWheel w(8);
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{0});
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    ZTH_EXPECT_EQ(w.slot_count(), std::size_t{8});
    ZTH_EXPECT_EQ(w.advance(5), std::size_t{0});
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{5});
    ZTH_EXPECT_EQ(w.advance(0), std::size_t{0});
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{5});
    ZTH_EXPECT_FALSE(w.next_deadline().has_value());
    ZTH_EXPECT_EQ(TimerWheel().slot_count(), std::size_t{256});
    ZTH_EXPECT_EQ(TimerWheel(0).slot_count(), std::size_t{1});
}
ZTH_TEST("works", "a timer fires exactly when the clock reaches its deadline, once") {
    TimerWheel w(8);
    std::vector<Fired> log;
    note(w, log, 5, 1);
    ZTH_EXPECT_EQ(w.pending(), std::size_t{1});
    ZTH_EXPECT_EQ(w.advance(4), std::size_t{0});
    ZTH_EXPECT_TRUE(log.empty());
    ZTH_EXPECT_EQ(w.advance(1), std::size_t{1});
    ZTH_EXPECT_EQ(show(log), std::string("1@5 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    ZTH_EXPECT_EQ(w.advance(100), std::size_t{0});
    ZTH_EXPECT_EQ(log.size(), std::size_t{1});
}
ZTH_TEST("works", "the deadline counts from the moment of scheduling") {
    TimerWheel w(8);
    std::vector<Fired> log;
    w.advance(10);
    const Id id = note(w, log, 3, 1);
    ZTH_EXPECT_TRUE(w.deadline(id).has_value());
    ZTH_EXPECT_EQ(w.deadline(id).value_or(0), std::uint64_t{13});
    w.advance(2);
    ZTH_EXPECT_TRUE(log.empty());
    w.advance(1);
    ZTH_EXPECT_EQ(show(log), std::string("1@13 "));
}
ZTH_TEST("works", "a delay of 0 means the next tick") {
    TimerWheel w(4);
    std::vector<Fired> log;
    note(w, log, 0, 1);
    ZTH_EXPECT_EQ(w.advance(1), std::size_t{1});
    ZTH_EXPECT_EQ(show(log), std::string("1@1 "));
    w.advance(6);
    note(w, log, 0, 2);
    w.advance(1);
    ZTH_EXPECT_EQ(show(log), std::string("1@1 2@8 "));
}
ZTH_TEST("works", "timers fire in order of deadline, and in scheduling order for the same deadline") {
    TimerWheel w(16);
    std::vector<Fired> log;
    note(w, log, 7, 1);
    note(w, log, 3, 2);
    note(w, log, 7, 3);
    note(w, log, 3, 4);
    note(w, log, 5, 5);
    note(w, log, 7, 6);
    ZTH_EXPECT_EQ(w.advance(10), std::size_t{6});
    ZTH_EXPECT_EQ(show(log), std::string("2@3 4@3 5@5 1@7 3@7 6@7 "));
}
ZTH_TEST("works", "deadlines more than one trip around the wheel wait for their turn in the same slot") {
    TimerWheel w(8);   // 3, 11, 19, 27 all live in slot 3
    std::vector<Fired> log;
    note(w, log, 27, 4);
    note(w, log, 3, 1);
    note(w, log, 19, 3);
    note(w, log, 11, 2);
    w.advance(10);
    ZTH_EXPECT_EQ(show(log), std::string("1@3 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{3});
    w.advance(1);
    ZTH_EXPECT_EQ(show(log), std::string("1@3 2@11 "));
    w.advance(16);
    ZTH_EXPECT_EQ(show(log), std::string("1@3 2@11 3@19 4@27 "));
}
ZTH_TEST("works", "one advance over many ticks runs every due timer, each at its own time") {
    TimerWheel w(5);
    std::vector<Fired> log;
    for (int i = 1; i <= 12; ++i) note(w, log, static_cast<std::uint64_t>(i * 3), i);
    ZTH_EXPECT_EQ(w.advance(100), std::size_t{12});
    ZTH_EXPECT_EQ(log.size(), std::size_t{12});
    for (std::size_t i = 0; i < log.size() && i < 12; ++i) {
        ZTH_EXPECT_EQ(log[i].tag, static_cast<int>(i + 1));
        ZTH_EXPECT_EQ(log[i].at, static_cast<std::uint64_t>((i + 1) * 3));   // now() inside the callback is the deadline, not 100
    }
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{100});
}
ZTH_TEST("works", "cancel: a pending timer never fires; unknown, fired and cancelled ids give false") {
    TimerWheel w(8);
    std::vector<Fired> log;
    const Id a = note(w, log, 5, 1);
    const Id b = note(w, log, 5, 2);
    const Id c = note(w, log, 9, 3);
    ZTH_EXPECT_TRUE(w.cancel(b));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{2});
    ZTH_EXPECT_FALSE(w.cancel(b));
    ZTH_EXPECT_FALSE(w.cancel(12345));
    ZTH_EXPECT_FALSE(w.cancel(0));
    w.advance(6);
    ZTH_EXPECT_EQ(show(log), std::string("1@5 "));
    ZTH_EXPECT_FALSE(w.cancel(a));   // already fired
    ZTH_EXPECT_TRUE(w.cancel(c));
    w.advance(20);
    ZTH_EXPECT_EQ(show(log), std::string("1@5 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
}
ZTH_TEST("works", "ids are 1, 2, 3 ... and an empty callback schedules nothing") {
    TimerWheel w(8);
    const Id a = w.schedule(1, [] {});
    const Id b = w.schedule(1, [] {});
    ZTH_EXPECT_EQ(a, Id{1});
    ZTH_EXPECT_EQ(b, Id{2});
    ZTH_EXPECT_EQ(w.schedule(5, nullptr), Id{0});
    ZTH_EXPECT_EQ(w.schedule(5, TimerWheel::Callback{}), Id{0});
    ZTH_EXPECT_EQ(w.pending(), std::size_t{2});
    ZTH_EXPECT_EQ(w.schedule(1, [] {}), Id{3});   // the refused calls used no id
    w.advance(3);
    ZTH_EXPECT_EQ(w.schedule(1, [] {}), Id{4});
}
ZTH_TEST("works", "deadline() and next_deadline() look at pending timers only") {
    TimerWheel w(8);
    const Id a = w.schedule(30, [] {});
    const Id b = w.schedule(12, [] {});
    const Id c = w.schedule(20, [] {});
    ZTH_EXPECT_EQ(w.next_deadline().value_or(0), std::uint64_t{12});
    ZTH_EXPECT_EQ(w.deadline(a).value_or(0), std::uint64_t{30});
    w.cancel(b);
    ZTH_EXPECT_FALSE(w.deadline(b).has_value());
    ZTH_EXPECT_EQ(w.next_deadline().value_or(0), std::uint64_t{20});
    w.advance(25);
    ZTH_EXPECT_FALSE(w.deadline(c).has_value());   // fired
    ZTH_EXPECT_EQ(w.next_deadline().value_or(0), std::uint64_t{30});
    ZTH_EXPECT_FALSE(w.deadline(999).has_value());
}
ZTH_TEST("works", "a callback can cancel a timer that is due in the same tick, later in the order: it does not run") {
    TimerWheel w(8);
    std::vector<Fired> log;
    Id victim = 0;
    w.schedule(5, [&] {
        log.push_back({1, w.now()});
        ZTH_EXPECT_TRUE(w.cancel(victim));
    });
    victim = note(w, log, 5, 2);
    note(w, log, 5, 3);
    w.advance(6);
    ZTH_EXPECT_EQ(show(log), std::string("1@5 3@5 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
}
ZTH_TEST("works", "a callback can cancel a timer of a later tick, even one in the same slot") {
    TimerWheel w(4);
    std::vector<Fired> log;
    Id victim = 0;
    w.schedule(1, [&] { ZTH_EXPECT_TRUE(w.cancel(victim)); });
    victim = note(w, log, 9, 1);   // slot 1, like the timer above
    note(w, log, 13, 2);
    w.advance(20);
    ZTH_EXPECT_EQ(show(log), std::string("2@13 "));
}
ZTH_TEST("works", "a callback can schedule: delay 1 means the next tick, never the current one") {
    TimerWheel w(8);
    std::vector<Fired> log;
    w.schedule(2, [&] {
        log.push_back({1, w.now()});
        note(w, log, 1, 2);
        note(w, log, 0, 3);
    });
    ZTH_EXPECT_EQ(w.advance(2), std::size_t{1});
    ZTH_EXPECT_EQ(show(log), std::string("1@2 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{2});
    ZTH_EXPECT_EQ(w.advance(1), std::size_t{2});
    ZTH_EXPECT_EQ(show(log), std::string("1@2 2@3 3@3 "));
}
ZTH_TEST("works", "a callback that schedules a delay of one full wheel lands in its own slot and waits a whole trip") {
    TimerWheel w(8);
    std::vector<Fired> log;
    w.schedule(3, [&] {
        log.push_back({1, w.now()});
        note(w, log, 8, 2);
    });
    w.advance(10);
    ZTH_EXPECT_EQ(show(log), std::string("1@3 "));
    w.advance(1);
    ZTH_EXPECT_EQ(show(log), std::string("1@3 2@11 "));
}
ZTH_TEST("works", "a chain of timers, each scheduling the next") {
    TimerWheel w(4);
    int count = 0;
    std::function<void()> again = [&] {
        if (++count < 50) w.schedule(3, again);
    };
    w.schedule(3, again);
    w.advance(1000);
    ZTH_EXPECT_EQ(count, 50);
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
}
ZTH_TEST("works", "while a callback runs its timer is no longer pending, and cancelling it is false") {
    TimerWheel w(8);
    Id self = 0;
    std::size_t pending_inside = 99;
    bool cancel_result = true;
    self = w.schedule(4, [&] {
        pending_inside = w.pending();
        cancel_result = w.cancel(self);
    });
    w.schedule(100, [] {});
    w.advance(5);
    ZTH_EXPECT_EQ(pending_inside, std::size_t{1});
    ZTH_EXPECT_FALSE(cancel_result);
}
ZTH_TEST("works", "advance() called from inside a callback does nothing") {
    TimerWheel w(8);
    std::size_t inner = 99;
    std::uint64_t now_after_inner = 0;
    std::vector<Fired> log;
    w.schedule(3, [&] {
        inner = w.advance(5);
        now_after_inner = w.now();
    });
    note(w, log, 5, 1);
    w.advance(10);
    ZTH_EXPECT_EQ(inner, std::size_t{0});
    ZTH_EXPECT_EQ(now_after_inner, std::uint64_t{3});
    ZTH_EXPECT_EQ(show(log), std::string("1@5 "));
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{10});
    ZTH_EXPECT_EQ(w.advance(1), std::size_t{0});   // and the guard is released afterwards
    ZTH_EXPECT_EQ(w.now(), std::uint64_t{11});
}
ZTH_TEST("works", "clear() cancels everything, also from inside a callback (timers due in the same tick do not run)") {
    TimerWheel w(8);
    std::vector<Fired> log;
    note(w, log, 3, 1);
    note(w, log, 50, 2);
    w.clear();
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    w.advance(60);
    ZTH_EXPECT_TRUE(log.empty());
    w.schedule(2, [&] {
        log.push_back({1, w.now()});
        w.clear();
    });
    note(w, log, 2, 2);
    note(w, log, 5, 3);
    w.advance(10);
    ZTH_EXPECT_EQ(show(log), std::string("1@62 "));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    note(w, log, 1, 4);   // and the wheel still works
    w.advance(1);
    ZTH_EXPECT_EQ(show(log), std::string("1@62 4@71 "));
}
ZTH_TEST("works", "a huge delay saturates instead of wrapping around") {
    TimerWheel w(8);
    std::vector<Fired> log;
    w.advance(10);
    const Id id = note(w, log, ~0ULL, 1);
    ZTH_EXPECT_EQ(w.deadline(id).value_or(0), ~0ULL);
    note(w, log, ~0ULL - 5, 2);
    ZTH_EXPECT_EQ(w.deadline(2).value_or(0), ~0ULL);
    w.advance(1000);
    ZTH_EXPECT_TRUE(log.empty());
    ZTH_EXPECT_EQ(w.pending(), std::size_t{2});
    ZTH_EXPECT_TRUE(w.cancel(id));
    ZTH_EXPECT_EQ(note(w, log, 1ULL << 40, 3), Id{3});
    ZTH_EXPECT_EQ(w.deadline(3).value_or(0), (1ULL << 40) + 1010);
}
ZTH_TEST("works", "a cancelled timer lets go of its callback at once") {
    TimerWheel w(8);
    auto resource = std::make_shared<int>(7);
    const Id id = w.schedule(100, [resource] { (void)resource; });
    ZTH_EXPECT_EQ(resource.use_count(), 2);
    ZTH_EXPECT_TRUE(w.cancel(id));
    ZTH_EXPECT_EQ(resource.use_count(), 1);
    const Id id2 = w.schedule(100, [resource] { (void)resource; });
    w.clear();
    ZTH_EXPECT_EQ(resource.use_count(), 1);
    (void)id2;
}
ZTH_TEST("works", "cancelled timers do not stay in the wheel forever: their nodes are swept when the slot comes round") {
    TimerWheel w(8);
    for (int i = 0; i < 100; ++i) {
        const Id id = w.schedule(1000 + static_cast<std::uint64_t>(i), [] {});
        w.cancel(id);
    }
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    w.advance(8);   // every slot visited once
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{0});
    const Id keep = w.schedule(500, [] {});
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{1});
    w.advance(8);
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{1});   // a live timer stays, however many times its slot is visited
    ZTH_EXPECT_TRUE(w.cancel(keep));
    w.advance(8);
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{0});
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "every timer fires exactly once, at exactly its deadline, for several wheel sizes") {
    for (std::size_t slots : {std::size_t{1}, std::size_t{2}, std::size_t{7}, std::size_t{64}, std::size_t{256}}) {
        TimerWheel w(slots);
        zth::Lcg rng(500 + slots);
        std::vector<std::uint64_t> due(5000), seen(5000, 0), at(5000, 0);
        for (int i = 0; i < 5000; ++i) {
            const std::uint64_t delay = static_cast<std::uint64_t>(rng.below(3000));
            due[static_cast<std::size_t>(i)] = w.now() + (delay == 0 ? 1 : delay);
            w.schedule(delay, [&, i] {
                ++seen[static_cast<std::size_t>(i)];
                at[static_cast<std::size_t>(i)] = w.now();
            });
        }
        while (w.now() < 3100) w.advance(static_cast<std::uint64_t>(1 + rng.below(40)));
        for (std::size_t i = 0; i < 5000; ++i) {
            if (seen[i] != 1 || at[i] != due[i]) {
                zth::fail("slots = " + std::to_string(slots) + ": timer " + std::to_string(i) + " fired " + std::to_string(seen[i]) + " times, at " + std::to_string(at[i]) + " instead of " + std::to_string(due[i]));
                return;
            }
        }
        ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    }
}
ZTH_TEST("robust", "timers that schedule and cancel other timers: the firing log equals a straightforward simulation") {
    // behaviour of timer `id`, decided by the id alone (so that the wheel and the simulation agree)
    auto spawn_delay = [](std::uint64_t id) -> std::uint64_t { return id % 7 == 0 ? 1 + id % 13 : 0; };
    auto cancel_target = [](std::uint64_t id) -> std::uint64_t { return id % 5 == 0 ? id + 2 : 0; };
    for (std::size_t slots : {std::size_t{1}, std::size_t{5}, std::size_t{16}, std::size_t{100}}) {
        // ---- the wheel
        TimerWheel w(slots);
        std::vector<std::pair<std::uint64_t, std::uint64_t>> wheel_log;
        std::function<void(Id)> fire;
        auto make = [&](std::uint64_t delay) {
            // the id of the new timer is only known after schedule() returns, so the callback asks the wheel through a shared slot
            auto my_id = std::make_shared<Id>(0);
            const Id id = w.schedule(delay, [&, my_id] { fire(*my_id); });
            *my_id = id;
            return id;
        };
        fire = [&](Id id) {
            wheel_log.push_back({id, w.now()});
            if (spawn_delay(id) > 0) make(spawn_delay(id));
            if (cancel_target(id) > 0) w.cancel(cancel_target(id));
        };
        // ---- the simulation: deadline -> ids in scheduling order
        std::map<std::uint64_t, std::vector<std::uint64_t>> sim_due;
        std::vector<bool> sim_cancelled(100000, false);
        std::vector<bool> sim_fired(100000, false);
        std::uint64_t sim_now = 0, sim_next_id = 1;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> sim_log;
        auto sim_make = [&](std::uint64_t delay) {
            const std::uint64_t id = sim_next_id++;
            sim_due[sim_now + (delay == 0 ? 1 : delay)].push_back(id);
            return id;
        };
        zth::Lcg rng(31 + slots);
        for (int i = 0; i < 300; ++i) {
            const std::uint64_t d = static_cast<std::uint64_t>(rng.below(120));
            make(d);
            sim_make(d);
        }
        for (int step = 0; step < 400; ++step) {
            const std::uint64_t ticks = static_cast<std::uint64_t>(rng.below(4));
            w.advance(ticks);
            for (std::uint64_t t = 0; t < ticks; ++t) {
                ++sim_now;
                auto found = sim_due.find(sim_now);
                if (found == sim_due.end()) continue;
                const std::vector<std::uint64_t> ids = found->second;   // copy: callbacks add to other entries
                for (std::uint64_t id : ids) {
                    if (sim_cancelled[id]) continue;
                    sim_fired[id] = true;
                    sim_log.push_back({id, sim_now});
                    if (spawn_delay(id) > 0) sim_make(spawn_delay(id));
                    const std::uint64_t victim = cancel_target(id);
                    if (victim > 0 && victim < sim_next_id && !sim_fired[victim]) sim_cancelled[victim] = true;
                }
                sim_due.erase(sim_now);
            }
            if (rng.below(10) == 0) {
                const std::uint64_t d = static_cast<std::uint64_t>(rng.below(60));
                make(d);
                sim_make(d);
            }
            if (wheel_log != sim_log) {
                zth::fail("slots = " + std::to_string(slots) + ", step " + std::to_string(step) + ": the wheel fired " + std::to_string(wheel_log.size()) + " timers, the simulation " + std::to_string(sim_log.size()) +
                          " (the logs differ)");
                return;
            }
        }
        ZTH_EXPECT_TRUE(wheel_log.size() > 100);
    }
}
ZTH_TEST("robust", "ids that were cancelled stay cancelled when new timers reuse their slot and deadline") {
    TimerWheel w(4);
    std::vector<Fired> log;
    for (int round = 0; round < 50; ++round) {
        const Id a = note(w, log, 8, round * 2);
        w.cancel(a);
        note(w, log, 8, round * 2 + 1);
        w.advance(8);
    }
    ZTH_EXPECT_EQ(log.size(), std::size_t{50});
    for (std::size_t i = 0; i < log.size() && i < 50; ++i) ZTH_EXPECT_EQ(log[i].tag % 2, 1);
}
ZTH_TEST("robust", "advance in tiny steps and in huge steps gives the same firing log") {
    std::vector<Fired> a, b;
    {
        TimerWheel w(13);
        zth::Lcg rng(9);
        for (int i = 0; i < 2000; ++i) note(w, a, static_cast<std::uint64_t>(rng.below(5000)), i);
        for (int i = 0; i < 5200; ++i) w.advance(1);
    }
    {
        TimerWheel w(13);
        zth::Lcg rng(9);
        for (int i = 0; i < 2000; ++i) note(w, b, static_cast<std::uint64_t>(rng.below(5000)), i);
        w.advance(5200);
    }
    ZTH_EXPECT_EQ(a.size(), std::size_t{2000});
    bool same = a.size() == b.size();
    for (std::size_t i = 0; same && i < a.size(); ++i) same = a[i].tag == b[i].tag && a[i].at == b[i].at;
    ZTH_EXPECT_TRUE(same);
}
ZTH_TEST("robust", "destroying a wheel with pending timers releases their callbacks") {
    auto resource = std::make_shared<int>(1);
    {
        TimerWheel w(16);
        for (int i = 0; i < 100; ++i) w.schedule(static_cast<std::uint64_t>(5 + i * 50), [resource] { (void)resource; });
        w.advance(100);
        ZTH_EXPECT_TRUE(resource.use_count() > 50);
    }
    ZTH_EXPECT_EQ(resource.use_count(), 1);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "a tick looks at one slot: 200000 timers spread over 200000 ticks (a scan of all timers per tick would need 2 * 10^10 steps)", 20) {
    TimerWheel w(1024);
    std::size_t fired = 0;
    for (int i = 1; i <= 200000; ++i) w.schedule(static_cast<std::uint64_t>(i), [&fired] { ++fired; });
    ZTH_EXPECT_EQ(w.pending(), std::size_t{200000});
    w.advance(200000);
    ZTH_EXPECT_EQ(fired, std::size_t{200000});
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{0});
}
ZTH_TEST_TIMEOUT("fast", "cancel is O(1): schedule 200000 timers, cancel them all, newest first", 20) {
    TimerWheel w(256);
    std::vector<Id> ids;
    ids.reserve(200000);
    for (int i = 0; i < 200000; ++i) ids.push_back(w.schedule(1000 + static_cast<std::uint64_t>(i), [] {}));
    for (std::size_t i = ids.size(); i-- > 0;) ZTH_EXPECT_TRUE(w.cancel(ids[i]));
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
    w.advance(256);
    ZTH_EXPECT_EQ(w.stored_nodes(), std::size_t{0});
}
ZTH_TEST_TIMEOUT("fast", "200000 timers due in the same tick run in linear time (collect, then fire)", 20) {
    TimerWheel w(64);
    std::vector<int> order;
    order.reserve(200000);
    for (int i = 0; i < 200000; ++i) w.schedule(5, [&order, i] { order.push_back(i); });
    ZTH_EXPECT_EQ(w.advance(5), std::size_t{200000});
    bool ordered = order.size() == 200000;
    for (std::size_t i = 0; ordered && i < order.size(); ++i) ordered = order[i] == static_cast<int>(i);
    ZTH_EXPECT_TRUE(ordered);
}
ZTH_TEST_TIMEOUT("fast", "idle ticks are cheap: 5 million ticks with three timers", 20) {
    TimerWheel w(256);
    int fired = 0;
    w.schedule(10, [&] { ++fired; });
    w.schedule(2500000, [&] { ++fired; });
    w.schedule(4999999, [&] { ++fired; });
    w.advance(5000000);
    ZTH_EXPECT_EQ(fired, 3);
}
ZTH_TEST("fast", "a timer costs two heap nodes (list node and index node) and no more; ticks and firing allocate nothing") {
    TimerWheel w(512);
    constexpr int kTimers = 10000;
    zth::mem::Scope scope;
    for (int i = 0; i < kTimers; ++i) w.schedule(1 + static_cast<std::uint64_t>(i % 2000), [] {});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 2 * kTimers + 64);
    zth::mem::Scope firing;
    w.advance(2100);
    ZTH_EXPECT_ALLOCS_AT_MOST(firing, 0);
    ZTH_EXPECT_EQ(w.pending(), std::size_t{0});
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("timer_wheel.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timer_wheel.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a wheel, not a heap or a tree: no priority_queue, map, set, heap algorithms or sorting") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timer_wheel.hpp"));
    for (const char* word : {"priority_queue", "map", "multimap", "set", "multiset", "make_heap", "push_heap", "pop_heap", "sort", "stable_sort", "partial_sort", "lower_bound", "upper_bound"})
        ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "logical time only: no clocks, sleeping or threads") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timer_wheel.hpp"));
    for (const char* word : {"chrono", "steady_clock", "system_clock", "sleep_for", "sleep_until", "thread", "mutex", "atomic", "time", "clock"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "pending timers are moved with splice, not copied or re-created") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("timer_wheel.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "splice").empty());
}
