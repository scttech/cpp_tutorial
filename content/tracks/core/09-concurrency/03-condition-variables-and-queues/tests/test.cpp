#include <algorithm>
#include <atomic>
#include <chrono>
#include <latch>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "queue.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

namespace {
template <class T>
T val(std::optional<T> o) {  // a copy: `*q.pop()` alone would point into a temporary that is already gone
    return *o;
}
}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "values come out in the order they went in; size() and capacity() tell the truth", 6) {
    BoundedQueue<int> q(4);
    ZTH_EXPECT_EQ(q.capacity(), std::size_t{4});
    ZTH_EXPECT_TRUE(q.push(10));
    ZTH_EXPECT_TRUE(q.push(20));
    ZTH_EXPECT_TRUE(q.push(30));
    ZTH_EXPECT_EQ(q.size(), std::size_t{3});
    ZTH_EXPECT_EQ(val(q.pop()), 10);
    ZTH_EXPECT_EQ(val(q.pop()), 20);
    ZTH_EXPECT_EQ(q.size(), std::size_t{1});
    ZTH_EXPECT_EQ(val(q.pop()), 30);
}
ZTH_TEST_TIMEOUT("works", "a capacity of 0 is an error", 6) {
    ZTH_EXPECT_THROWS(std::invalid_argument, BoundedQueue<int> q(0));
}
ZTH_TEST_TIMEOUT("works", "try_pop never waits: nothing on an empty queue, the oldest value otherwise", 6) {
    BoundedQueue<int> q(2);
    ZTH_EXPECT_FALSE(q.try_pop().has_value());
    q.push(7);
    auto v = q.try_pop();
    ZTH_EXPECT_TRUE(v.has_value());
    if (v) ZTH_EXPECT_EQ(*v, 7);
    ZTH_EXPECT_FALSE(q.try_pop().has_value());
}
ZTH_TEST_TIMEOUT("works", "pop WAITS for a value: a consumer that arrives first gets the value pushed later", 6) {
    BoundedQueue<int> q(2);
    std::latch about_to_pop(1);
    std::optional<int> got;
    std::jthread consumer([&] {
        about_to_pop.count_down();
        got = q.pop();
    });
    about_to_pop.wait();
    std::this_thread::sleep_for(40ms);  // the consumer is now waiting in pop()
    q.push(42);
    consumer.join();
    ZTH_EXPECT_TRUE(got.has_value());
    if (got) ZTH_EXPECT_EQ(*got, 42);
}
ZTH_TEST_TIMEOUT("works", "push WAITS while the queue is full: a producer of 5 values into room for 2 stops at 2, and the rest follow as values are popped", 6) {
    BoundedQueue<int> q(2);
    std::jthread producer([&] {
        for (int i = 1; i <= 5; ++i) q.push(i);
    });
    std::this_thread::sleep_for(60ms);
    ZTH_EXPECT_EQ(q.size(), std::size_t{2});  // not 5: the producer is waiting
    for (int expected = 1; expected <= 5; ++expected) {
        auto v = q.pop();
        ZTH_EXPECT_TRUE(v.has_value());
        if (v) ZTH_EXPECT_EQ(*v, expected);
    }
}
ZTH_TEST_TIMEOUT("works", "3 producers and 1 consumer: nothing is lost, nothing is duplicated, and each producer's values arrive in order", 6) {
    BoundedQueue<std::pair<int, int>> q(5);
    std::vector<std::jthread> producers;
    for (int p = 0; p < 3; ++p) {
        producers.emplace_back([&q, p] {
            for (int i = 0; i < 400; ++i) q.push({p, i});
        });
    }
    std::vector<int> last(3, -1);
    int received = 0;
    bool in_order = true;
    while (received < 1200) {
        auto item = q.pop();
        if (!item) break;
        if (item->second != last[static_cast<std::size_t>(item->first)] + 1) in_order = false;
        last[static_cast<std::size_t>(item->first)] = item->second;
        ++received;
    }
    ZTH_EXPECT_EQ(received, 1200);
    ZTH_EXPECT_TRUE(in_order);
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "close() wakes up every consumer that is waiting: they all return an empty result", 6) {
    BoundedQueue<int> q(2);
    std::atomic<int> woke{0};
    std::latch ready(3);
    std::vector<std::jthread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([&] {
            ready.count_down();
            if (!q.pop().has_value()) ++woke;
        });
    }
    ready.wait();
    std::this_thread::sleep_for(40ms);
    q.close();
    consumers.clear();  // joins
    ZTH_EXPECT_EQ(woke.load(), 3);
}
ZTH_TEST_TIMEOUT("robust", "close() wakes a producer that is waiting for room: its push returns false", 6) {
    BoundedQueue<int> q(1);
    q.push(1);  // full
    std::atomic<int> result{-1};
    std::jthread producer([&] { result = q.push(2) ? 1 : 0; });
    std::this_thread::sleep_for(40ms);
    q.close();
    producer.join();
    ZTH_EXPECT_EQ(result.load(), 0);
}
ZTH_TEST_TIMEOUT("robust", "after close(): the values that were queued are still delivered, then pop() returns empty at once; push() returns false", 6) {
    BoundedQueue<int> q(5);
    q.push(1);
    q.push(2);
    q.close();
    ZTH_EXPECT_TRUE(q.closed());
    ZTH_EXPECT_FALSE(q.push(3));
    ZTH_EXPECT_EQ(val(q.pop()), 1);
    ZTH_EXPECT_EQ(val(q.pop()), 2);
    ZTH_EXPECT_FALSE(q.pop().has_value());  // closed and empty: must not wait
    ZTH_EXPECT_FALSE(q.pop().has_value());
}
ZTH_TEST_TIMEOUT("robust", "a closed, empty queue answers pop() immediately, also when the close came before the wait started", 6) {
    BoundedQueue<int> q(2);
    q.close();
    ZTH_EXPECT_FALSE(q.pop().has_value());
}
ZTH_TEST_TIMEOUT("robust", "4 producers and 4 consumers on a queue with room for 4: all 6000 values arrive exactly once, and size() is safe to read meanwhile", 8) {
    BoundedQueue<int> q(4);
    std::atomic<long long> sum{0};
    std::atomic<int> count{0};
    std::vector<std::jthread> consumers;
    for (int c = 0; c < 4; ++c) {
        consumers.emplace_back([&] {
            while (auto v = q.pop()) {
                sum += *v;
                ++count;
            }
        });
    }
    std::size_t biggest = 0;
    {
        std::vector<std::jthread> producers;
        for (int p = 0; p < 4; ++p) {
            producers.emplace_back([&q] {
                for (int i = 1; i <= 1500; ++i) q.push(i);
            });
        }
        for (int i = 0; i < 300; ++i) biggest = std::max(biggest, q.size());
    }  // the producers are joined: everything has been pushed
    q.close();
    consumers.clear();
    ZTH_EXPECT_TRUE(biggest <= 4);
    ZTH_EXPECT_EQ(count.load(), 6000);
    ZTH_EXPECT_EQ(sum.load(), 4LL * 1500 * 1501 / 2);
}
ZTH_TEST_TIMEOUT("robust", "move-only values work: a queue of std::unique_ptr<int>", 6) {
    BoundedQueue<std::unique_ptr<int>> q(2);
    q.push(std::make_unique<int>(5));
    auto p = q.pop();
    ZTH_EXPECT_TRUE(p.has_value());
    if (p && *p) ZTH_EXPECT_EQ(**p, 5);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "waiting is free: a consumer and a producer that wait for a quarter of a second use almost no processor time", 6) {
    BoundedQueue<int> empty_queue(2);
    BoundedQueue<int> full_queue(1);
    full_queue.push(1);
    std::jthread consumer([&] { empty_queue.pop(); });
    std::jthread producer([&] { full_queue.push(2); });
    std::this_thread::sleep_for(60ms);  // both have reached their wait by now
    const double before = ::zth::cpu_ms();
    std::this_thread::sleep_for(250ms);
    const double used = ::zth::cpu_ms() - before;
    empty_queue.close();  // let them go
    full_queue.close();
    consumer.join();
    producer.join();
    ZTH_EXPECT_TRUE(used < 100.0);  // a thread that spins (or yields in a loop) would use about 250 ms each
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "every wait has a predicate: no `wait(lock)` that could miss a wake-up or wake up for nothing") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("queue.hpp"));
    std::size_t waits = 0;
    for (std::size_t at = code.find(".wait("); at != std::string::npos; at = code.find(".wait(", at + 1)) {
        ++waits;
        int depth = 0;
        bool comma = false;
        for (std::size_t i = at + 5; i < code.size(); ++i) {
            if (code[i] == '(') ++depth;
            if (code[i] == ')' && --depth == 0) break;
            if (code[i] == ',' && depth == 1) comma = true;
        }
        ZTH_EXPECT_TRUE(comma);
    }
    ZTH_EXPECT_TRUE(waits >= 2);
}
ZTH_TEST("clean", "queue.hpp waits on condition variables: no sleeping, yielding or spinning") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("queue.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sleep_for").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "yield").empty());
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "condition_variable").empty());
    ZTH_EXPECT_NO_RAW_MEMORY("queue.hpp");
}
