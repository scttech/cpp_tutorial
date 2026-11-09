#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "callbacks.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;
using Log = std::vector<std::string>;

int twice(int x) { return 2 * x; }
int plus_ten(int x) { return x + 10; }

// a handler that carries a counting member: copying the handler copies the Tracked
struct CountingHandler {
    zth::Tracked marker{1};
    int* calls;
    explicit CountingHandler(int* c) : calls(c) {}
    void operator()(const std::string&) const { ++*calls; }
};

// a callable that counts how often it is called, in ITS OWN member (so a copy of it would count separately)
struct CountingStep {
    int calls = 0;
    zth::Tracked marker{2};
    int operator()(int x) {
        ++calls;
        return x + 1;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "subscribe returns the ids 1, 2, 3, ...") {
    EventBus bus;
    ZTH_EXPECT_EQ(bus.subscribe("a", [](const S&) {}), 1);
    ZTH_EXPECT_EQ(bus.subscribe("b", [](const S&) {}), 2);
    ZTH_EXPECT_EQ(bus.subscribe("a", [](const S&) {}), 3);
}
ZTH_TEST("works", "publish calls the handlers of the topic, in subscription order, with the payload") {
    EventBus bus;
    Log log;
    bus.subscribe("t", [&log](const S& p) { log.push_back("first:" + p); });
    bus.subscribe("t", [&log](const S& p) { log.push_back("second:" + p); });
    bus.subscribe("t", [&log](const S& p) { log.push_back("third:" + p); });
    ZTH_EXPECT_EQ(bus.publish("t", "x"), std::size_t{3});
    ZTH_EXPECT_TRUE((log == Log{"first:x", "second:x", "third:x"}));
}
ZTH_TEST("works", "publish only reaches the subscribers of that topic") {
    EventBus bus;
    Log log;
    bus.subscribe("a", [&log](const S& p) { log.push_back("a:" + p); });
    bus.subscribe("b", [&log](const S& p) { log.push_back("b:" + p); });
    ZTH_EXPECT_EQ(bus.publish("b", "1"), std::size_t{1});
    ZTH_EXPECT_TRUE((log == Log{"b:1"}));
}
ZTH_TEST("works", "unsubscribe removes one handler") {
    EventBus bus;
    Log log;
    const int first = bus.subscribe("t", [&log](const S&) { log.push_back("first"); });
    bus.subscribe("t", [&log](const S&) { log.push_back("second"); });
    ZTH_EXPECT_TRUE(bus.unsubscribe(first));
    ZTH_EXPECT_EQ(bus.publish("t", ""), std::size_t{1});
    ZTH_EXPECT_TRUE((log == Log{"second"}));
}
ZTH_TEST("works", "subscriber_count") {
    EventBus bus;
    ZTH_EXPECT_EQ(bus.subscriber_count("t"), std::size_t{0});
    const int id = bus.subscribe("t", [](const S&) {});
    bus.subscribe("t", [](const S&) {});
    bus.subscribe("u", [](const S&) {});
    ZTH_EXPECT_EQ(bus.subscriber_count("t"), std::size_t{2});
    bus.unsubscribe(id);
    ZTH_EXPECT_EQ(bus.subscriber_count("t"), std::size_t{1});
}
ZTH_TEST("works", "make_counter counts up from its start value") {
    auto c = make_counter(5);
    ZTH_EXPECT_EQ(c(), 5);
    ZTH_EXPECT_EQ(c(), 6);
    ZTH_EXPECT_EQ(c(), 7);
}
ZTH_TEST("works", "two counters have independent state") {
    auto a = make_counter(0);
    auto b = make_counter(100);
    ZTH_EXPECT_EQ(a(), 0);
    ZTH_EXPECT_EQ(a(), 1);
    ZTH_EXPECT_EQ(b(), 100);
    ZTH_EXPECT_EQ(a(), 2);
    ZTH_EXPECT_EQ(b(), 101);
}
ZTH_TEST("works", "make_multipliers: the i-th function multiplies by factors[i]") {
    auto m = make_multipliers({2, 10, -1});
    ZTH_EXPECT_EQ(m.size(), std::size_t{3});
    ZTH_EXPECT_EQ(m[0](3), 6);
    ZTH_EXPECT_EQ(m[1](3), 30);
    ZTH_EXPECT_EQ(m[2](3), -3);
}
ZTH_TEST("works", "compose(f, g)(x) is g(f(x))") {
    ZTH_EXPECT_EQ(compose(plus_ten, twice)(1), 22);   // twice(plus_ten(1))
    ZTH_EXPECT_EQ(compose(twice, plus_ten)(1), 12);   // plus_ten(twice(1))
}
ZTH_TEST("works", "compose accepts lambdas and mixes callables of different kinds") {
    auto h = compose([](int x) { return x * x; }, [](int y) { return y + 1; });
    ZTH_EXPECT_EQ(h(4), 17);
    auto k = compose(twice, [](int y) { return y - 1; });
    ZTH_EXPECT_EQ(k(5), 9);
}
ZTH_TEST("works", "apply_n applies the function n times") {
    ZTH_EXPECT_EQ(apply_n(twice, 3, 1), 8);
    ZTH_EXPECT_EQ(apply_n([](int x) { return x + 3; }, 4, 0), 12);
}
ZTH_TEST("works", "make_once calls the function only the first time") {
    int calls = 0;
    auto once = make_once([&calls] { ++calls; });
    once();
    once();
    once();
    ZTH_EXPECT_EQ(calls, 1);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "publish to a topic nobody subscribed to calls nothing") {
    EventBus bus;
    ZTH_EXPECT_EQ(bus.publish("nobody", "x"), std::size_t{0});
    bus.subscribe("a", [](const S&) {});
    ZTH_EXPECT_EQ(bus.publish("b", "x"), std::size_t{0});
}
ZTH_TEST("robust", "unsubscribe of an unknown or already removed id is false and changes nothing") {
    EventBus bus;
    const int id = bus.subscribe("t", [](const S&) {});
    ZTH_EXPECT_FALSE(bus.unsubscribe(999));
    ZTH_EXPECT_FALSE(bus.unsubscribe(0));
    ZTH_EXPECT_TRUE(bus.unsubscribe(id));
    ZTH_EXPECT_FALSE(bus.unsubscribe(id));
    ZTH_EXPECT_EQ(bus.subscriber_count("t"), std::size_t{0});
}
ZTH_TEST("robust", "ids are never reused, even after an unsubscribe") {
    EventBus bus;
    const int a = bus.subscribe("t", [](const S&) {});
    bus.unsubscribe(a);
    const int b = bus.subscribe("t", [](const S&) {});
    ZTH_EXPECT_EQ(a, 1);
    ZTH_EXPECT_EQ(b, 2);
}
ZTH_TEST("robust", "unsubscribing from the middle keeps the order of the others") {
    EventBus bus;
    Log log;
    bus.subscribe("t", [&log](const S&) { log.push_back("1"); });
    const int mid = bus.subscribe("t", [&log](const S&) { log.push_back("2"); });
    bus.subscribe("t", [&log](const S&) { log.push_back("3"); });
    bus.unsubscribe(mid);
    bus.publish("t", "");
    ZTH_EXPECT_TRUE((log == Log{"1", "3"}));
}
ZTH_TEST("robust", "the same callable can be subscribed twice and is called twice") {
    EventBus bus;
    int n = 0;
    auto bump = [&n](const S&) { ++n; };
    bus.subscribe("t", bump);
    bus.subscribe("t", bump);
    ZTH_EXPECT_EQ(bus.publish("t", ""), std::size_t{2});
    ZTH_EXPECT_EQ(n, 2);
}
ZTH_TEST("robust", "any callable works as a handler: a function, a function object, a lambda with state") {
    EventBus bus;
    static int plain_calls = 0;
    plain_calls = 0;
    bus.subscribe("t", [](const S&) { ++plain_calls; });
    int object_calls = 0;
    bus.subscribe("t", CountingHandler(&object_calls));
    int state = 0;
    bus.subscribe("t", [&state, extra = 5](const S& p) { state += extra + static_cast<int>(p.size()); });
    bus.publish("t", "abc");
    ZTH_EXPECT_EQ(plain_calls, 1);
    ZTH_EXPECT_EQ(object_calls, 1);
    ZTH_EXPECT_EQ(state, 8);
}
ZTH_TEST("robust", "a handler that was copied into the bus keeps its own captured value") {
    EventBus bus;
    Log log;
    {
        S prefix = "hi ";  // gone after this block: a by-value capture must survive it
        bus.subscribe("t", [&log, prefix](const S& p) { log.push_back(prefix + p); });
    }
    bus.publish("t", "there");
    ZTH_EXPECT_TRUE((log == Log{"hi there"}));
}
ZTH_TEST("robust", "a copy of a counter continues from the state it was copied at, independently") {
    auto c = make_counter(0);
    c();
    c();
    auto d = c;  // a copy of the closure copies its state (next == 2)
    ZTH_EXPECT_EQ(c(), 2);
    ZTH_EXPECT_EQ(c(), 3);
    ZTH_EXPECT_EQ(d(), 2);
}
ZTH_TEST("robust", "make_counter with a negative start value, and a long run") {
    auto c = make_counter(-2);
    ZTH_EXPECT_EQ(c(), -2);
    ZTH_EXPECT_EQ(c(), -1);
    ZTH_EXPECT_EQ(c(), 0);
    int last = 0;
    for (int i = 0; i < 1000; ++i) last = c();
    ZTH_EXPECT_EQ(last, 1000);
}
ZTH_TEST("robust", "make_multipliers of nothing; and the functions outlive the vector they were built from") {
    ZTH_EXPECT_TRUE(make_multipliers({}).empty());
    std::vector<std::function<int(int)>> m;
    {
        std::vector<int> factors{3, 4, 5};
        m = make_multipliers(factors);
        factors.assign(3, 0);  // changing or destroying the source must not matter
    }
    ZTH_EXPECT_EQ(m[0](2), 6);
    ZTH_EXPECT_EQ(m[1](2), 8);
    ZTH_EXPECT_EQ(m[2](2), 10);
}
ZTH_TEST("robust", "make_multipliers: many factors, each function keeps its own") {
    std::vector<int> factors;
    for (int i = 0; i < 50; ++i) factors.push_back(i);
    auto m = make_multipliers(factors);
    for (int i = 0; i < 50; ++i) {
        if (m[static_cast<std::size_t>(i)](10) != 10 * i) {
            ::zth::fail("function " + std::to_string(i) + " does not multiply by " + std::to_string(i));
            return;
        }
    }
}
ZTH_TEST("robust", "compose with a result of another type, and three functions") {
    auto length_then_double = compose([](const S& s) { return static_cast<int>(s.size()); }, twice);
    ZTH_EXPECT_EQ(length_then_double(S("abcd")), 8);
    auto three = compose(compose(plus_ten, twice), [](int x) { return x - 1; });
    ZTH_EXPECT_EQ(three(0), 19);
}
ZTH_TEST("robust", "compose passes several arguments on to the first function") {
    auto sum_then_double = compose([](int a, int b) { return a + b; }, twice);
    ZTH_EXPECT_EQ(sum_then_double(2, 3), 10);
}
ZTH_TEST("robust", "the closure compose returns owns its functions (they may be temporaries)") {
    auto h = compose([n = 3](int x) { return x * n; }, [m = 4](int y) { return y + m; });
    ZTH_EXPECT_EQ(h(2), 10);
}
ZTH_TEST("robust", "apply_n with n = 0 returns x, and a function that has side effects runs n times") {
    ZTH_EXPECT_EQ(apply_n(twice, 0, 7), 7);
    int calls = 0;
    auto f = [&calls](int x) {
        ++calls;
        return x + 1;
    };
    ZTH_EXPECT_EQ(apply_n(f, 5, 0), 5);
    ZTH_EXPECT_EQ(calls, 5);
}
ZTH_TEST("robust", "apply_n calls the CALLER's function object, not a copy of it") {
    CountingStep step;
    ZTH_EXPECT_EQ(apply_n(step, 4, 0), 4);
    ZTH_EXPECT_EQ(step.calls, 4);  // a copy would have counted in the copy
}
ZTH_TEST("robust", "apply_n accepts a function pointer, a function reference and an rvalue lambda") {
    int (*fp)(int) = twice;
    ZTH_EXPECT_EQ(apply_n(fp, 2, 3), 12);
    ZTH_EXPECT_EQ(apply_n(twice, 2, 3), 12);
    ZTH_EXPECT_EQ(apply_n([](int x) { return x - 1; }, 3, 0), -3);
}
ZTH_TEST("robust", "two make_once functions are independent, and the first call's side effects are kept") {
    int a = 0, b = 0;
    auto once_a = make_once([&a] { a += 10; });
    auto once_b = make_once([&b] { b += 20; });
    once_a();
    once_b();
    once_a();
    once_b();
    ZTH_EXPECT_EQ(a, 10);
    ZTH_EXPECT_EQ(b, 20);
}
ZTH_TEST("robust", "make_once does not call the function when it is never called") {
    int calls = 0;
    auto once = make_once([&calls] { ++calls; });
    (void)once;
    ZTH_EXPECT_EQ(calls, 0);
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "subscribing an rvalue handler copies the callable no more than zero times") {
    EventBus bus;
    int calls = 0;
    zth::Tracked::reset();
    bus.subscribe("t", CountingHandler(&calls));
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
}
ZTH_TEST("fast", "publish does not copy or move the handlers") {
    EventBus bus;
    int calls = 0;
    bus.subscribe("t", CountingHandler(&calls));
    bus.subscribe("t", CountingHandler(&calls));
    bus.subscribe("other", CountingHandler(&calls));
    zth::Tracked::reset();
    for (int i = 0; i < 10; ++i) bus.publish("t", "payload");
    ZTH_EXPECT_EQ(calls, 20);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "apply_n does not copy the callable (a forwarding reference, not a std::function or a by-value parameter)") {
    CountingStep step;
    zth::Tracked::reset();
    apply_n(step, 100, 0);
    ZTH_EXPECT_EQ(zth::Tracked::copies, 0);
    ZTH_EXPECT_EQ(zth::Tracked::moves, 0);
}
ZTH_TEST("fast", "apply_n of 1000000 steps is quick (no std::function indirection per call)") {
    auto inc = [](int x) { return x + 1; };
    ZTH_EXPECT_EQ(apply_n(inc, 1000000, 0), 1000000);
}
ZTH_TEST("fast", "publish to a topic with many subscribers and many topics") {
    EventBus bus;
    long total = 0;
    for (int i = 0; i < 2000; ++i) bus.subscribe("t" + std::to_string(i % 20), [&total](const S&) { ++total; });
    for (int round = 0; round < 50; ++round) bus.publish("t3", "x");
    ZTH_EXPECT_EQ(total, 100L * 50);
}
