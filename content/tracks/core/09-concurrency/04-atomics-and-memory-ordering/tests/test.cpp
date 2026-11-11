#include <algorithm>
#include <atomic>
#include <chrono>
#include <latch>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "atomics.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

namespace {

template <class F>
void run_together(unsigned n, F body) {
    std::latch go(1);
    std::vector<std::jthread> threads;
    for (unsigned i = 0; i < n; ++i) {
        threads.emplace_back([&go, &body, i] {
            go.wait();
            body(i);
        });
    }
    go.count_down();
}

// An atomic int that makes the race in a "load, then store" update certain: the first `threads` calls of load() (on whatever threads) each take the
// current value and then WAIT until all of them have done so. A correct update loop does not mind; load()+store() overwrites everybody's result.
class StallingAtomic {
public:
    StallingAtomic(int initial, int threads) : value_(initial), threads_(threads), gate_(threads) {}
    int load(std::memory_order = std::memory_order_seq_cst) {
        const int v = value_.load();
        if (arrivals_.fetch_add(1) < threads_) gate_.arrive_and_wait();
        return v;
    }
    void store(int v, std::memory_order = std::memory_order_seq_cst) { value_.store(v); }
    bool compare_exchange_weak(int& expected, int desired, std::memory_order = std::memory_order_seq_cst) { return value_.compare_exchange_weak(expected, desired); }
    bool compare_exchange_weak(int& expected, int desired, std::memory_order, std::memory_order) { return value_.compare_exchange_weak(expected, desired); }
    bool compare_exchange_strong(int& expected, int desired, std::memory_order = std::memory_order_seq_cst) { return value_.compare_exchange_strong(expected, desired); }
    bool compare_exchange_strong(int& expected, int desired, std::memory_order, std::memory_order) { return value_.compare_exchange_strong(expected, desired); }
    int get() const { return value_.load(); }

private:
    std::atomic<int> value_;
    int threads_;
    std::latch gate_;
    std::atomic<int> arrivals_{0};
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "Counter: four threads adding 1 two thousand times each give exactly 8000", 6) {
    Counter c;
    run_together(4, [&](unsigned) {
        for (int i = 0; i < 2000; ++i) c.add(1);
    });
    ZTH_EXPECT_EQ(c.value(), 8000LL);
}
ZTH_TEST_TIMEOUT("works", "SpinLock protects ordinary data: four threads incrementing a plain int under the lock", 8) {
    SpinLock lock;
    int shared = 0;
    run_together(4, [&](unsigned) {
        for (int i = 0; i < 800; ++i) {
            std::lock_guard<SpinLock> guard(lock);
            ++shared;
        }
    });
    ZTH_EXPECT_EQ(shared, 3200);
}
ZTH_TEST_TIMEOUT("works", "SpinLock: try_lock fails while the lock is held and succeeds when it is free", 6) {
    SpinLock lock;
    lock.lock();
    ZTH_EXPECT_FALSE(lock.try_lock());
    lock.unlock();
    ZTH_EXPECT_TRUE(lock.try_lock());
    lock.unlock();
    ZTH_EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}
ZTH_TEST_TIMEOUT("works", "OneShot: a value set by one thread is seen, complete, by the thread that waits for it", 6) {
    OneShot<std::string> box;
    ZTH_EXPECT_FALSE(box.ready());
    std::string seen;
    std::jthread reader([&] { seen = box.get(); });
    std::jthread writer([&] { box.set(std::string(3000, 'x') + "end"); });
    writer.join();
    reader.join();
    ZTH_EXPECT_TRUE(box.ready());
    ZTH_EXPECT_EQ(seen.size(), std::size_t{3003});
    ZTH_EXPECT_EQ(seen.substr(3000), std::string("end"));
}
ZTH_TEST_TIMEOUT("works", "fetch_double doubles the value and returns the old one", 6) {
    std::atomic<int> a{5};
    ZTH_EXPECT_EQ(fetch_double(a), 5);
    ZTH_EXPECT_EQ(a.load(), 10);
    ZTH_EXPECT_EQ(fetch_double(a), 10);
    ZTH_EXPECT_EQ(a.load(), 20);
}
ZTH_TEST_TIMEOUT("works", "LockFreeStack: peek shows the newest value, drain returns everything newest first and empties the stack", 6) {
    LockFreeStack<int> s;
    ZTH_EXPECT_FALSE(s.peek().has_value());
    s.push(1);
    s.push(2);
    s.push(3);
    auto top = s.peek();
    ZTH_EXPECT_TRUE(top.has_value());
    if (top) ZTH_EXPECT_EQ(*top, 3);
    ZTH_EXPECT_EQ(s.drain(), (std::vector<int>{3, 2, 1}));
    ZTH_EXPECT_TRUE(s.drain().empty());
    ZTH_EXPECT_FALSE(s.peek().has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "Counter: negative additions, and value() read while others add", 6) {
    Counter c;
    std::atomic<long long> last{0};
    run_together(3, [&](unsigned t) {
        for (int i = 0; i < 1500; ++i) {
            if (t == 0) last = c.value();
            else c.add(t == 1 ? 2 : -1);
        }
    });
    ZTH_EXPECT_EQ(c.value(), 1500LL);
}
ZTH_TEST_TIMEOUT("robust", "SpinLock gives real mutual exclusion: never two threads inside at once", 8) {
    SpinLock lock;
    std::atomic<int> inside{0};
    std::atomic<int> most{0};
    run_together(4, [&](unsigned) {
        for (int i = 0; i < 500; ++i) {
            std::lock_guard<SpinLock> guard(lock);
            const int now = ++inside;
            int seen = most.load();
            while (now > seen && !most.compare_exchange_weak(seen, now)) {
            }
            --inside;
        }
    });
    ZTH_EXPECT_EQ(most.load(), 1);
}
ZTH_TEST_TIMEOUT("robust", "OneShot: several readers all see the value; get() before set() waits", 6) {
    OneShot<int> box;
    std::latch started(3);
    std::atomic<int> sum{0};
    std::vector<std::jthread> readers;
    for (int i = 0; i < 3; ++i) {
        readers.emplace_back([&] {
            started.count_down();
            sum += box.get();
        });
    }
    started.wait();
    std::this_thread::sleep_for(40ms);  // the readers are waiting now
    ZTH_EXPECT_EQ(sum.load(), 0);
    box.set(7);
    readers.clear();
    ZTH_EXPECT_EQ(sum.load(), 21);
}
ZTH_TEST_TIMEOUT("robust", "fetch_double is ONE atomic step: four threads that all read the same old value still end at 5 * 2 * 2 * 2 * 2", 6) {
    StallingAtomic a(5, 4);
    std::vector<int> olds(4, 0);
    {
        std::vector<std::jthread> threads;
        for (int i = 0; i < 4; ++i) threads.emplace_back([&a, &olds, i] { olds[static_cast<std::size_t>(i)] = fetch_double(a); });
    }
    ZTH_EXPECT_EQ(a.get(), 80);
    std::sort(olds.begin(), olds.end());
    ZTH_EXPECT_EQ(olds, (std::vector<int>{5, 10, 20, 40}));  // every thread got its own turn
}
ZTH_TEST_TIMEOUT("robust", "LockFreeStack: four threads push 1500 values each while another reads the top; nothing is lost or invented", 8) {
    LockFreeStack<int> s;
    std::atomic<bool> bad{false};
    run_together(5, [&](unsigned t) {
        if (t == 4) {
            for (int i = 0; i < 1500; ++i) {
                auto top = s.peek();
                if (top && (*top < 0 || *top >= 6000)) bad = true;
            }
        } else {
            for (int i = 0; i < 1500; ++i) s.push(static_cast<int>(t) * 1500 + i);
        }
    });
    ZTH_EXPECT_FALSE(bad.load());
    std::vector<int> all = s.drain();
    ZTH_EXPECT_EQ(all.size(), std::size_t{6000});
    std::sort(all.begin(), all.end());
    bool exact = true;
    for (std::size_t i = 0; i < all.size(); ++i) exact = exact && all[i] == static_cast<int>(i);
    ZTH_EXPECT_TRUE(exact);
}
ZTH_TEST_TIMEOUT("robust", "LockFreeStack of strings: the values survive the trip (and the destructor frees what was not drained)", 6) {
    LockFreeStack<std::string> s;
    for (int i = 0; i < 100; ++i) s.push(std::string(50, static_cast<char>('a' + i % 26)));
    auto top = s.peek();
    ZTH_EXPECT_TRUE(top.has_value());
    if (top) ZTH_EXPECT_EQ(top->size(), std::size_t{50});
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "OneShot::get() sleeps while it waits: a thread that waits a quarter of a second uses almost no processor time", 6) {
    OneShot<int> box;
    std::jthread reader([&] { (void)box.get(); });
    std::this_thread::sleep_for(60ms);
    const double before = ::zth::cpu_ms();
    std::this_thread::sleep_for(250ms);
    const double used = ::zth::cpu_ms() - before;
    box.set(1);
    reader.join();
    ZTH_EXPECT_TRUE(used < 100.0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "the memory orders are chosen, not defaulted by luck: SpinLock and OneShot use acquire and release explicitly") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("atomics.hpp"));
    ZTH_EXPECT_TRUE(code.find("memory_order_acquire") != std::string::npos);
    ZTH_EXPECT_TRUE(code.find("memory_order_release") != std::string::npos);
}
ZTH_TEST("clean", "a lock-free file has no mutex in it, and no volatile pretending to be an atomic") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("atomics.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "mutex").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "lock_guard").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "volatile").empty());
}
ZTH_TEST("clean", "OneShot waits on the atomic instead of looping: atomics.hpp calls wait() and notify_all()") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("atomics.hpp"));
    ZTH_EXPECT_TRUE(code.find(".wait(") != std::string::npos);
    ZTH_EXPECT_TRUE(code.find("notify_all(") != std::string::npos || code.find("notify_one(") != std::string::npos);
}
