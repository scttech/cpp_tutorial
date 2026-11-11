#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "spsc_queue.hpp"
#include "types.hpp"
#include "zth_test.hpp"

using namespace evq;
using namespace std::chrono_literals;

namespace {

// Producer thread pushes 1..count (waiting while full), the consumer (this thread) pops until it has them all.
// Returns a problem description, or "".
template <std::size_t N>
std::string transfer(std::uint64_t count, bool slow_consumer = false, bool slow_producer = false) {
    SpscQueue<Event, N> q;
    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= count; ++i) {
            while (!q.try_push(Event::make(i))) std::this_thread::yield();
            if (slow_producer && i % 64 == 0) std::this_thread::sleep_for(100us);
        }
    });
    std::uint64_t expected = 1;
    std::string problem;
    Event e;
    while (expected <= count) {
        if (!q.try_pop(e)) {
            std::this_thread::yield();
            continue;
        }
        if (e.seq != expected && problem.empty()) problem = "event " + std::to_string(expected) + " expected, got " + std::to_string(e.seq);
        if (!e.consistent() && problem.empty()) problem = "torn event " + std::to_string(e.seq);
        ++expected;
        if (slow_consumer && expected % 64 == 0) std::this_thread::sleep_for(100us);
    }
    producer.join();
    if (!q.empty() && problem.empty()) problem = "the queue is not empty at the end";
    return problem;
}

struct MoveOnly {
    std::unique_ptr<int> p;
    MoveOnly() = default;
    explicit MoveOnly(int v) : p(std::make_unique<int>(v)) {}
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty queue; push then pop gives the item back") {
    SpscQueue<int, 4> q;
    ZTH_EXPECT_TRUE(q.empty());
    ZTH_EXPECT_EQ(q.size_approx(), std::size_t{0});
    ZTH_EXPECT_EQ(q.capacity(), std::size_t{4});
    int out = -1;
    ZTH_EXPECT_FALSE(q.try_pop(out));
    ZTH_EXPECT_EQ(out, -1);   // untouched
    ZTH_EXPECT_TRUE(q.try_push(7));
    ZTH_EXPECT_FALSE(q.empty());
    ZTH_EXPECT_EQ(q.size_approx(), std::size_t{1});
    ZTH_EXPECT_TRUE(q.try_pop(out));
    ZTH_EXPECT_EQ(out, 7);
    ZTH_EXPECT_TRUE(q.empty());
}
ZTH_TEST("works", "first in, first out") {
    SpscQueue<int, 8> q;
    for (int i = 1; i <= 8; ++i) ZTH_EXPECT_TRUE(q.try_push(i * 10));
    int out = 0;
    for (int i = 1; i <= 8; ++i) {
        ZTH_EXPECT_TRUE(q.try_pop(out));
        ZTH_EXPECT_EQ(out, i * 10);
    }
}
ZTH_TEST("works", "the queue holds exactly N items: the (N+1)th push fails, the queue is unchanged, and a pop makes room") {
    SpscQueue<int, 3> q;
    ZTH_EXPECT_TRUE(q.try_push(1));
    ZTH_EXPECT_TRUE(q.try_push(2));
    ZTH_EXPECT_TRUE(q.try_push(3));
    ZTH_EXPECT_EQ(q.size_approx(), std::size_t{3});
    ZTH_EXPECT_FALSE(q.try_push(4));
    ZTH_EXPECT_EQ(q.size_approx(), std::size_t{3});
    int out = 0;
    ZTH_EXPECT_TRUE(q.try_pop(out));
    ZTH_EXPECT_EQ(out, 1);
    ZTH_EXPECT_TRUE(q.try_push(4));
    ZTH_EXPECT_FALSE(q.try_push(5));
    for (int expected : {2, 3, 4}) {
        ZTH_EXPECT_TRUE(q.try_pop(out));
        ZTH_EXPECT_EQ(out, expected);
    }
    ZTH_EXPECT_FALSE(q.try_pop(out));
}
ZTH_TEST("works", "wrap-around: 1000 rounds of fill-and-drain through a queue of 5") {
    SpscQueue<int, 5> q;
    int next = 0, expect = 0;
    for (int round = 0; round < 1000; ++round) {
        const int n = 1 + round % 5;
        for (int i = 0; i < n; ++i) ZTH_EXPECT_TRUE(q.try_push(next++));
        int out = 0;
        for (int i = 0; i < n; ++i) {
            ZTH_EXPECT_TRUE(q.try_pop(out));
            ZTH_EXPECT_EQ(out, expect++);
        }
        ZTH_EXPECT_TRUE(q.empty());
    }
}
ZTH_TEST("works", "a queue of capacity 1 alternates push and pop") {
    SpscQueue<int, 1> q;
    int out = 0;
    for (int i = 0; i < 100; ++i) {
        ZTH_EXPECT_TRUE(q.try_push(i));
        ZTH_EXPECT_FALSE(q.try_push(-1));
        ZTH_EXPECT_TRUE(q.try_pop(out));
        ZTH_EXPECT_EQ(out, i);
        ZTH_EXPECT_FALSE(q.try_pop(out));
    }
}
ZTH_TEST("works", "move-only items travel through the queue") {
    SpscQueue<MoveOnly, 4> q;
    ZTH_EXPECT_TRUE(q.try_push(MoveOnly(11)));
    ZTH_EXPECT_TRUE(q.try_push(MoveOnly(22)));
    MoveOnly out;
    ZTH_EXPECT_TRUE(q.try_pop(out));
    ZTH_EXPECT_EQ(*out.p, 11);
    ZTH_EXPECT_TRUE(q.try_pop(out));
    ZTH_EXPECT_EQ(*out.p, 22);
}
ZTH_TEST("works", "size_approx follows pushes and pops when one thread does everything") {
    SpscQueue<int, 6> q;
    int out = 0;
    for (int i = 0; i < 20; ++i) {
        q.try_push(i);
        q.try_push(i);
        ZTH_EXPECT_EQ(q.size_approx(), std::size_t{2});
        q.try_pop(out);
        ZTH_EXPECT_EQ(q.size_approx(), std::size_t{1});
        q.try_pop(out);
        ZTH_EXPECT_EQ(q.size_approx(), std::size_t{0});
    }
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "two threads, 200000 events through a queue of 1024: in order, none lost, none torn (ThreadSanitizer is watching)", 20) {
    ZTH_EXPECT_EQ(transfer<1024>(200000), std::string());
}
ZTH_TEST_TIMEOUT("robust", "two threads through the smallest queues (capacity 1, 2, 3), where both threads constantly meet", 20) {
    ZTH_EXPECT_EQ(transfer<1>(30000), std::string());
    ZTH_EXPECT_EQ(transfer<2>(30000), std::string());
    ZTH_EXPECT_EQ(transfer<3>(30000), std::string());
}
ZTH_TEST_TIMEOUT("robust", "a slow consumer (the queue stays full) and a slow producer (the queue stays empty)", 20) {
    ZTH_EXPECT_EQ(transfer<16>(6000, /*slow_consumer=*/true, false), std::string());
    ZTH_EXPECT_EQ(transfer<16>(6000, false, /*slow_producer=*/true), std::string());
}
ZTH_TEST_TIMEOUT("robust", "capacities that are not a power of two work (7, 100)", 20) {
    ZTH_EXPECT_EQ(transfer<7>(50000), std::string());
    ZTH_EXPECT_EQ(transfer<100>(50000), std::string());
}
ZTH_TEST_TIMEOUT("robust", "move-only items across two threads: every pointer arrives exactly once", 20) {
    SpscQueue<MoveOnly, 8> q;
    constexpr int kCount = 20000;
    std::thread producer([&] {
        for (int i = 1; i <= kCount; ++i) {
            MoveOnly item(i);
            while (true) {
                // try_push takes the item by value: on failure the original must still be intact if the queue was full
                if (q.try_push(std::move(item))) break;
                std::this_thread::yield();
                item = MoveOnly(i);   // rebuild: the moved-from item may be empty
            }
        }
    });
    long long sum = 0;
    int received = 0;
    MoveOnly out;
    while (received < kCount) {
        if (!q.try_pop(out)) {
            std::this_thread::yield();
            continue;
        }
        ZTH_EXPECT_TRUE(out.p != nullptr);
        sum += *out.p;
        ++received;
    }
    producer.join();
    ZTH_EXPECT_EQ(sum, static_cast<long long>(kCount) * (kCount + 1) / 2);
}
ZTH_TEST_TIMEOUT("robust", "big items through a tiny queue: the consumer copies 512 bytes while the producer waits for the slot to be given back", 30) {
    struct Big {
        std::uint64_t v[64];
    };
    SpscQueue<Big, 2> q;
    constexpr std::uint64_t kCount = 40000;
    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= kCount; ++i) {
            Big b;
            for (auto& x : b.v) x = i;
            while (!q.try_push(b)) std::this_thread::yield();
        }
    });
    std::uint64_t got = 0;
    bool bad = false;
    Big out;
    while (got < kCount) {
        if (!q.try_pop(out)) {
            std::this_thread::yield();
            continue;
        }
        ++got;
        for (auto x : out.v) bad = bad || x != got;
    }
    producer.join();
    ZTH_EXPECT_FALSE(bad);
}
ZTH_TEST_TIMEOUT("robust", "size_approx and empty may be called from a third thread at any time; the value is always within 0..N", 20) {
    SpscQueue<int, 32> q;
    std::atomic<bool> stop{false};
    std::atomic<bool> bad{false};
    std::thread watcher([&] {
        while (!stop.load()) {
            const std::size_t s = q.size_approx();
            if (s > 32) bad = true;
            (void)q.empty();
        }
    });
    std::thread producer([&] {
        for (int i = 0; i < 40000; ++i) {
            while (!q.try_push(i)) std::this_thread::yield();
        }
    });
    int got = 0, out = 0;
    while (got < 40000) {
        if (q.try_pop(out)) ++got;
        else std::this_thread::yield();
    }
    producer.join();
    stop = true;
    watcher.join();
    ZTH_EXPECT_FALSE(bad.load());
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "head and tail live on different cache lines: the queue is at least 128 bytes even when it holds one byte") {
    ZTH_EXPECT_TRUE(sizeof(SpscQueue<char, 1>) >= 128);
    ZTH_EXPECT_TRUE(alignof(SpscQueue<char, 1>) >= 64);
}
ZTH_TEST("fast", "the indices are lock-free atomics (std::atomic<size_t> needs no hidden lock)") {
    ZTH_EXPECT_TRUE(std::atomic<std::size_t>::is_always_lock_free);
    ZTH_EXPECT_TRUE(sizeof(SpscQueue<int, 1000>) < 1000 * 4 + 1000);   // storage plus a few cache lines
}
ZTH_TEST_TIMEOUT("fast", "a million pushes and pops finish at once (ThreadSanitizer replaces operator new, so allocation is checked in the source: see the Clean tier)", 20) {
    SpscQueue<std::uint64_t, 256> q;
    std::uint64_t out = 0, sum = 0;
    for (std::uint64_t i = 0; i < 1000000; ++i) {
        q.try_push(i);
        if (q.try_pop(out)) sum += out;
    }
    ZTH_EXPECT_TRUE(sum > 0);
}
ZTH_TEST_TIMEOUT("fast", "wait-free operations: a push and a pop never wait, whatever the other thread does (they return false instead)", 20) {
    SpscQueue<int, 2> q;
    int out = 0;
    // full queue: push returns at once; empty queue: pop returns at once. A mutex held by nobody would also return, so look at the source too
    ZTH_EXPECT_TRUE(q.try_push(1));
    ZTH_EXPECT_TRUE(q.try_push(2));
    for (int i = 0; i < 100000; ++i) ZTH_EXPECT_FALSE(q.try_push(3));
    q.try_pop(out);
    q.try_pop(out);
    for (int i = 0; i < 100000; ++i) ZTH_EXPECT_FALSE(q.try_pop(out));
}
ZTH_TEST("fast", "memory orders: release on the store that publishes, acquire on the load that consumes (and relaxed for a thread's own index)") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("spsc_queue.hpp"));
    ZTH_EXPECT_FALSE(::zth::find_word(code, "memory_order_release").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "memory_order_acquire").empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "memory_order_relaxed").empty());
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("spsc_queue.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("spsc_queue.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "lock-free means no mutex, no lock_guard, no condition_variable, no sleep, no memory_order_seq_cst spelled out") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("spsc_queue.hpp"));
    for (const char* word : {"mutex", "lock_guard", "unique_lock", "scoped_lock", "condition_variable", "sleep_for", "yield", "memory_order_seq_cst", "atomic_flag", "volatile"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(std::string("spsc_queue.hpp uses `") + word + "`");
            return;
        }
    }
}
ZTH_TEST("clean", "storage is a std::array inside the object (no std::vector, no unique_ptr, no heap): the capacity is a template parameter") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("spsc_queue.hpp"));
    for (const char* word : {"vector", "unique_ptr", "shared_ptr", "make_unique", "make_shared", "deque", "list"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
    ZTH_EXPECT_FALSE(::zth::find_word(code, "array").empty());
}
