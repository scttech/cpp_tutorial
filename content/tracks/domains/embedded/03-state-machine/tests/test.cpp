#define ZTH_TRACK_ALLOCATIONS
#include <string>
#include <vector>

#include "machine.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace fsm;

namespace {

enum class S { A, B, C, D };
enum class E { Go, Back, Skip, Ping, Reset };

struct Ctx {
    std::string log;
    int guard_calls = 0;
    bool allow = true;
    int counter = 0;
};
using T = Transition<S, E, Ctx>;
using H = Hooks<S, Ctx>;
using M = Machine<S, E, Ctx>;

bool yes(const Ctx& c) { return (const_cast<Ctx&>(c).guard_calls++, true); }
bool no(const Ctx& c) { return (const_cast<Ctx&>(c).guard_calls++, false); }
bool flag(const Ctx& c) { return (const_cast<Ctx&>(c).guard_calls++, c.allow); }
void act_x(Ctx& c) { c.log += "x "; }
void act_y(Ctx& c) { c.log += "y "; }
void act_count(Ctx& c) { ++c.counter; }
void in_a(Ctx& c) { c.log += "in-A "; }
void out_a(Ctx& c) { c.log += "out-A "; }
void in_b(Ctx& c) { c.log += "in-B "; }
void out_b(Ctx& c) { c.log += "out-B "; }
void in_c(Ctx& c) { c.log += "in-C "; }

const H kHooks[] = {{S::A, in_a, out_a}, {S::B, in_b, out_b}, {S::C, in_c, nullptr}};

template <std::size_t N>
M make(Ctx& ctx, const T (&table)[N], const H* hooks = kHooks, std::size_t nh = 3, S initial = S::A) {
    return M(initial, table, N, hooks, nh, ctx);
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "a matching row changes the state") {
    static const T table[] = {{S::A, E::Go, S::B}, {S::B, E::Go, S::C}};
    Ctx ctx;
    M m = make(ctx, table, nullptr, 0);
    ZTH_EXPECT_TRUE(m.state() == S::A);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::Handled);
    ZTH_EXPECT_TRUE(m.state() == S::B);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::Handled);
    ZTH_EXPECT_TRUE(m.state() == S::C);
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{2});
}
ZTH_TEST("works", "an event with no row for the current state is ignored: the state does not change and nothing runs") {
    static const T table[] = {{S::A, E::Go, S::B, nullptr, act_x}, {S::B, E::Back, S::A}};
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Back) == Result::NoTransition);   // a row exists, but for state B
    ZTH_EXPECT_TRUE(m.dispatch(E::Ping) == Result::NoTransition);   // no row at all
    ZTH_EXPECT_TRUE(m.state() == S::A);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A "));   // only the start hook
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{0});
}
ZTH_TEST("works", "a transition runs exit hook, then action, then entry hook, in that order") {
    static const T table[] = {{S::A, E::Go, S::B, nullptr, act_x}};
    Ctx ctx;
    M m = make(ctx, table);
    m.dispatch(E::Go);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A out-A x in-B "));
}
ZTH_TEST("works", "a missing action, a missing hook or a missing hook table is fine") {
    static const T table[] = {{S::A, E::Go, S::C}, {S::C, E::Go, S::D}, {S::D, E::Go, S::A}};
    Ctx ctx;
    M m = make(ctx, table);   // A has hooks, C has only an entry hook, D has none
    m.dispatch(E::Go);
    m.dispatch(E::Go);
    m.dispatch(E::Go);
    ZTH_EXPECT_TRUE(m.state() == S::A);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A out-A in-C in-A "));
    Ctx ctx2;
    M bare = make(ctx2, table, nullptr, 0);
    bare.dispatch(E::Go);
    ZTH_EXPECT_TRUE(bare.state() == S::C);
    ZTH_EXPECT_EQ(ctx2.log, std::string(""));
}
ZTH_TEST("works", "a guard that says no blocks the row: GuardRejected, no hooks, no action, no state change") {
    static const T table[] = {{S::A, E::Go, S::B, no, act_x}};
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::GuardRejected);
    ZTH_EXPECT_TRUE(m.state() == S::A);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A "));
    ZTH_EXPECT_EQ(ctx.guard_calls, 1);
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{0});
}
ZTH_TEST("works", "a guard that depends on the context can change its mind") {
    static const T table[] = {{S::A, E::Go, S::B, flag}};
    Ctx ctx;
    ctx.allow = false;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::GuardRejected);
    ctx.allow = true;
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::Handled);
    ZTH_EXPECT_TRUE(m.state() == S::B);
}
ZTH_TEST("works", "rows are tried in table order and the first one that passes wins; a failed guard falls through to the next matching row") {
    static const T table[] = {
        {S::A, E::Go, S::B, no},
        {S::A, E::Go, S::C, yes, act_y},
        {S::A, E::Go, S::D},   // never reached
    };
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::Handled);
    ZTH_EXPECT_TRUE(m.state() == S::C);
    ZTH_EXPECT_EQ(ctx.guard_calls, 2);   // the guard of the third row is never called
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A out-A y in-C "));
}
ZTH_TEST("works", "all guards failing gives GuardRejected, a mix of rows for other states does not count") {
    static const T table[] = {{S::B, E::Go, S::C, yes}, {S::A, E::Go, S::B, no}, {S::A, E::Go, S::C, no}};
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::GuardRejected);
    ZTH_EXPECT_EQ(ctx.guard_calls, 2);   // the guard of the row for state B is never called
}
ZTH_TEST("works", "an internal row runs its action and nothing else: no state change, no hooks") {
    static const T table[] = {{S::A, E::Ping, S::A, nullptr, act_count, /*internal=*/true}};
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Ping) == Result::Handled);
    ZTH_EXPECT_TRUE(m.dispatch(E::Ping) == Result::Handled);
    ZTH_EXPECT_EQ(ctx.counter, 2);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A "));
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{0});
    ZTH_EXPECT_EQ(m.handled_internal(), std::size_t{2});
}
ZTH_TEST("works", "an internal row ignores its `to` state") {
    static const T table[] = {{S::A, E::Ping, S::D, nullptr, act_count, true}};
    Ctx ctx;
    M m = make(ctx, table);
    m.dispatch(E::Ping);
    ZTH_EXPECT_TRUE(m.state() == S::A);
}
ZTH_TEST("works", "a transition to the same state is an external self-transition: exit and entry hooks run") {
    static const T table[] = {{S::A, E::Reset, S::A, nullptr, act_x}};
    Ctx ctx;
    M m = make(ctx, table);
    m.dispatch(E::Reset);
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A out-A x in-A "));
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{1});
}
ZTH_TEST("works", "a wildcard row applies in every state, after the more specific rows that come before it") {
    static const T table[] = {
        {S::B, E::Reset, S::B, nullptr, act_y, true},   // specific: B ignores Reset except for a beep
        {S::A, E::Reset, S::A, no},                      // specific but guarded off
        {S::A, E::Reset, S::A, nullptr, nullptr, false, /*any_state=*/true},   // the wildcard (its `from` is ignored)
    };
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_TRUE(m.dispatch(E::Reset) == Result::Handled);   // state A: guard of row 2 fails, the wildcard takes it
    ZTH_EXPECT_EQ(m.transitions_taken(), std::size_t{1});
    Ctx ctx2;
    M b(S::B, table, 3, kHooks, 3, ctx2);
    b.dispatch(E::Reset);
    ZTH_EXPECT_EQ(b.handled_internal(), std::size_t{1});   // the specific row came first
    ZTH_EXPECT_TRUE(b.state() == S::B);
}
ZTH_TEST("works", "a wildcard row with a different target moves the machine from any state") {
    static const T table[] = {{S::A, E::Reset, S::D, nullptr, nullptr, false, true}};
    for (S s : {S::A, S::B, S::C, S::D}) {
        Ctx ctx;
        M m(s, table, 1, nullptr, 0, ctx);
        ZTH_EXPECT_TRUE(m.dispatch(E::Reset) == Result::Handled);
        ZTH_EXPECT_TRUE(m.state() == S::D);
    }
}
ZTH_TEST("works", "start() runs the initial entry hook once, and dispatch() starts the machine if nobody did") {
    static const T table[] = {{S::A, E::Go, S::B}};
    Ctx ctx;
    M m = make(ctx, table);
    ZTH_EXPECT_EQ(ctx.log, std::string(""));    // constructing does not run anything
    m.start();
    m.start();
    ZTH_EXPECT_EQ(ctx.log, std::string("in-A "));
    Ctx ctx2;
    M n = make(ctx2, table);
    n.dispatch(E::Go);
    ZTH_EXPECT_EQ(ctx2.log, std::string("in-A out-A in-B "));
    n.start();   // already started by dispatch
    ZTH_EXPECT_EQ(ctx2.log, std::string("in-A out-A in-B "));
}
ZTH_TEST("works", "a realistic machine: a vending slot") {
    enum class V { Idle, HasCoin, Dispensing, OutOfOrder };
    enum class Ev { Coin, Select, Done, Fault, Service };
    struct Slot {
        int stock = 1, coins = 0, dispensed = 0;
    };
    using VT = Transition<V, Ev, Slot>;
    static const VT table[] = {
        {V::Idle, Ev::Coin, V::HasCoin, nullptr, [](Slot& s) { ++s.coins; }},
        {V::HasCoin, Ev::Coin, V::HasCoin, nullptr, [](Slot& s) { ++s.coins; }, true},
        {V::HasCoin, Ev::Select, V::Dispensing, [](const Slot& s) { return s.stock > 0; }},
        {V::HasCoin, Ev::Select, V::Idle, nullptr, [](Slot& s) { s.coins = 0; }},   // no stock: refund
        {V::Dispensing, Ev::Done, V::Idle, nullptr, [](Slot& s) { --s.stock; ++s.dispensed; s.coins = 0; }},
        {V::Idle, Ev::Fault, V::OutOfOrder, nullptr, nullptr, false, true},
        {V::Idle, Ev::Service, V::Idle, nullptr, [](Slot& s) { s.stock = 5; }, true},
    };
    Slot slot;
    Machine<V, Ev, Slot> m(V::Idle, table, sizeof table / sizeof table[0], nullptr, 0, slot);
    m.dispatch(Ev::Coin);
    m.dispatch(Ev::Coin);
    ZTH_EXPECT_EQ(slot.coins, 2);
    ZTH_EXPECT_TRUE(m.dispatch(Ev::Select) == Result::Handled);
    ZTH_EXPECT_TRUE(m.state() == V::Dispensing);
    m.dispatch(Ev::Done);
    ZTH_EXPECT_EQ(slot.dispensed, 1);
    ZTH_EXPECT_EQ(slot.stock, 0);
    m.dispatch(Ev::Coin);
    m.dispatch(Ev::Select);   // out of stock: the first Select row's guard fails, the second row refunds
    ZTH_EXPECT_TRUE(m.state() == V::Idle);
    ZTH_EXPECT_EQ(slot.coins, 0);
    m.dispatch(Ev::Service);
    ZTH_EXPECT_EQ(slot.stock, 5);
    m.dispatch(Ev::Fault);
    ZTH_EXPECT_TRUE(m.state() == V::OutOfOrder);
    ZTH_EXPECT_TRUE(m.dispatch(Ev::Coin) == Result::NoTransition);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "random tables and random events agree with a straightforward reference interpreter") {
    zth::Lcg rng(4711);
    static bool (*const guards[3])(const Ctx&) = {nullptr, yes, flag};
    static void (*const actions[3])(Ctx&) = {nullptr, act_x, act_y};
    for (int round = 0; round < 300; ++round) {
        std::vector<T> table;
        const int rows = 1 + rng.below(14);
        for (int i = 0; i < rows; ++i) {
            T t{static_cast<S>(rng.below(4)), static_cast<E>(rng.below(5)), static_cast<S>(rng.below(4)), guards[rng.below(3)], actions[rng.below(3)], rng.below(4) == 0, rng.below(6) == 0};
            table.push_back(t);
        }
        Ctx a, b;
        a.allow = b.allow = rng.below(2) == 0;
        M m(S::A, table.data(), table.size(), kHooks, 3, a);
        // reference
        S state = S::A;
        b.log += "in-A ";
        const H* hooks = kHooks;
        auto hook = [&](S s) -> const H* {
            for (int i = 0; i < 3; ++i)
                if (hooks[i].state == s) return &hooks[i];
            return nullptr;
        };
        std::size_t taken = 0, internal = 0;
        for (int step = 0; step < 60; ++step) {
            if (rng.below(10) == 0) a.allow = b.allow = !a.allow;
            const E ev = static_cast<E>(rng.below(5));
            const Result got = m.dispatch(ev);
            Result want = Result::NoTransition;
            for (const T& row : table) {
                if (row.event != ev || !(row.any_state || row.from == state)) continue;
                want = Result::GuardRejected;
                if (row.guard && !row.guard(b)) continue;
                want = Result::Handled;
                if (row.internal) {
                    if (row.action) row.action(b);
                    ++internal;
                } else {
                    if (const H* h = hook(state); h && h->on_exit) h->on_exit(b);
                    if (row.action) row.action(b);
                    state = row.to;
                    if (const H* h = hook(state); h && h->on_entry) h->on_entry(b);
                    ++taken;
                }
                break;
            }
            if (got != want || m.state() != state || a.log != b.log || a.guard_calls != b.guard_calls || m.transitions_taken() != taken || m.handled_internal() != internal) {
                zth::fail("round " + std::to_string(round) + ", step " + std::to_string(step) + ": the machine and the reference interpreter disagree (log `" + a.log + "` vs `" + b.log + "`)");
                return;
            }
        }
    }
}
ZTH_TEST("robust", "two machines share one table and keep separate states") {
    static const T table[] = {{S::A, E::Go, S::B, nullptr, act_count}, {S::B, E::Go, S::C, nullptr, act_count}};
    Ctx c1, c2;
    M m1 = make(c1, table, nullptr, 0), m2 = make(c2, table, nullptr, 0);
    m1.dispatch(E::Go);
    m1.dispatch(E::Go);
    m2.dispatch(E::Go);
    ZTH_EXPECT_TRUE(m1.state() == S::C);
    ZTH_EXPECT_TRUE(m2.state() == S::B);
    ZTH_EXPECT_EQ(c1.counter, 2);
    ZTH_EXPECT_EQ(c2.counter, 1);
}
ZTH_TEST("robust", "an empty table ignores everything") {
    Ctx ctx;
    M m(S::A, nullptr, 0, nullptr, 0, ctx);
    for (E e : {E::Go, E::Back, E::Skip, E::Ping, E::Reset}) ZTH_EXPECT_TRUE(m.dispatch(e) == Result::NoTransition);
    ZTH_EXPECT_TRUE(m.state() == S::A);
}
ZTH_TEST("robust", "actions may read the context that the guards looked at, and entry hooks see the NEW state's data") {
    struct P {
        int level = 0;
        int seen_in_entry = -1;
    };
    using PT = Transition<S, E, P>;
    static const PT table[] = {{S::A, E::Go, S::B, [](const P& p) { return p.level >= 3; }, [](P& p) { p.level += 10; }}};
    static const Hooks<S, P> hooks[] = {{S::B, [](P& p) { p.seen_in_entry = p.level; }, nullptr}};
    P p;
    Machine<S, E, P> m(S::A, table, 1, hooks, 1, p);
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::GuardRejected);
    p.level = 3;
    ZTH_EXPECT_TRUE(m.dispatch(E::Go) == Result::Handled);
    ZTH_EXPECT_EQ(p.seen_in_entry, 13);   // the action ran before the entry hook
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "no heap: a million dispatches allocate nothing") {
    static const T table[] = {{S::A, E::Go, S::B}, {S::B, E::Go, S::C}, {S::C, E::Go, S::D}, {S::D, E::Go, S::A}, {S::A, E::Ping, S::A, nullptr, act_count, true}};
    Ctx ctx;
    M m = make(ctx, table, nullptr, 0);
    zth::mem::Scope scope;
    for (int i = 0; i < 1000000; ++i) m.dispatch(i % 5 == 4 ? E::Ping : E::Go);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
    ZTH_EXPECT_TRUE(m.transitions_taken() > 100000);
}
ZTH_TEST("fast", "a guard is called only for rows that match, and never after a row was taken") {
    static const T table[] = {
        {S::B, E::Go, S::C, yes},
        {S::A, E::Back, S::C, yes},
        {S::A, E::Go, S::B, yes},
        {S::A, E::Go, S::C, yes},
        {S::A, E::Go, S::D, yes},
    };
    Ctx ctx;
    M m = make(ctx, table, nullptr, 0);
    m.dispatch(E::Go);
    ZTH_EXPECT_EQ(ctx.guard_calls, 1);
}
ZTH_TEST("fast", "a dispatch looks at each row at most once: a 2000-row table, guards counted") {
    // rows 0..1998 never match (other event), the last row matches
    static T table[2000];
    for (int i = 0; i < 1999; ++i) table[i] = T{S::A, E::Skip, S::B, yes};
    table[1999] = T{S::A, E::Go, S::B, yes};
    Ctx ctx;
    M m = make(ctx, table, nullptr, 0);
    for (int i = 0; i < 100; ++i) {
        m.dispatch(E::Go);
        m.dispatch(E::Reset);   // matches nothing: S::B has no rows
    }
    ZTH_EXPECT_EQ(ctx.guard_calls, 1);
}
ZTH_TEST("fast", "the machine is small: pointers, a state and two counters") {
    ZTH_EXPECT_TRUE(sizeof(M) <= 8 * sizeof(void*) + 32);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("machine.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("machine.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "firmware style: no standard containers, no std::function, no virtual dispatch, no RTTI") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("machine.hpp"));
    for (const char* word : {"vector", "map", "unordered_map", "set", "array", "function", "string", "virtual", "typeid", "dynamic_cast", "shared_ptr", "unique_ptr", "any", "variant"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "a table walk, not a hand-written switch over the states") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("machine.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "switch").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "case").empty());
}
