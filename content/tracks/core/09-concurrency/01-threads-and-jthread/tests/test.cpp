#include <atomic>
#include <chrono>
#include <latch>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "workers.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "fill_in_thread fills the caller's vector with 0..n-1", 5) {
    std::vector<int> v;
    fill_in_thread(v, 5);
    ZTH_EXPECT_EQ(v, (std::vector<int>{0, 1, 2, 3, 4}));
}
ZTH_TEST_TIMEOUT("works", "fill_in_thread replaces what was in the vector", 5) {
    std::vector<int> v = {9, 9, 9, 9, 9, 9, 9};
    fill_in_thread(v, 3);
    ZTH_EXPECT_EQ(v, (std::vector<int>{0, 1, 2}));
    fill_in_thread(v, 0);
    ZTH_EXPECT_TRUE(v.empty());
}
ZTH_TEST_TIMEOUT("works", "run_all runs every job, and returns only when they have all finished", 5) {
    std::atomic<int> started{0};
    std::atomic<int> finished{0};
    std::vector<std::function<void()>> jobs;
    for (int i = 0; i < 5; ++i) {
        jobs.push_back([&] {
            ++started;
            std::this_thread::sleep_for(20ms);
            ++finished;
        });
    }
    run_all(jobs);
    ZTH_EXPECT_EQ(started.load(), 5);
    ZTH_EXPECT_EQ(finished.load(), 5);
}
ZTH_TEST_TIMEOUT("works", "run_all starts the jobs together: four jobs that wait for each other all get through", 5) {
    std::latch all_here(4);  // each job waits until all four are running: one after the other they would wait for ever
    std::atomic<int> passed{0};
    std::vector<std::function<void()>> jobs(4, [&] {
        all_here.arrive_and_wait();
        ++passed;
    });
    run_all(jobs);
    ZTH_EXPECT_EQ(passed.load(), 4);
}
ZTH_TEST_TIMEOUT("works", "run_all gives every job a thread of its own, not the calling thread", 5) {
    std::set<std::thread::id> ids;
    std::mutex m;
    std::vector<std::function<void()>> jobs(4, [&] {
        std::lock_guard lock(m);
        ids.insert(std::this_thread::get_id());
    });
    run_all(jobs);
    ZTH_EXPECT_EQ(ids.size(), std::size_t{4});
    ZTH_EXPECT_TRUE(ids.count(std::this_thread::get_id()) == 0);
}
ZTH_TEST_TIMEOUT("works", "a Ticker ticks (at least three times) on a thread of its own, and the first tick comes at once", 5) {
    std::latch three(3);
    std::atomic<int> ticks{0};
    std::atomic<bool> other_thread{true};
    const auto main_id = std::this_thread::get_id();
    {
        Ticker ticker(1ms, [&] {
            if (std::this_thread::get_id() == main_id) other_thread = false;
            if (ticks.fetch_add(1) < 3) three.count_down();
        });
        three.wait();
    }
    ZTH_EXPECT_TRUE(ticks.load() >= 3);
    ZTH_EXPECT_TRUE(other_thread.load());
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "run_all: a job that throws does not stop the others, and its exception is rethrown afterwards", 5) {
    std::atomic<int> ran{0};
    std::vector<std::function<void()>> jobs;
    jobs.push_back([&] { ++ran; });
    jobs.push_back([] { throw std::runtime_error("boom"); });
    jobs.push_back([&] { ++ran; });
    bool caught = false;
    try {
        run_all(jobs);
    } catch (const std::runtime_error& e) {
        caught = std::string(e.what()) == "boom";
    }
    ZTH_EXPECT_TRUE(caught);
    ZTH_EXPECT_EQ(ran.load(), 2);
}
ZTH_TEST_TIMEOUT("robust", "run_all rethrows the exception of the LOWEST failing job, whatever finishes first", 5) {
    std::vector<std::function<void()>> jobs;
    jobs.push_back([] {});
    jobs.push_back([] {
        std::this_thread::sleep_for(30ms);
        throw std::runtime_error("first");
    });
    jobs.push_back([] { throw std::runtime_error("second"); });
    std::string what;
    try {
        run_all(jobs);
    } catch (const std::exception& e) {
        what = e.what();
    }
    ZTH_EXPECT_EQ(what, std::string("first"));
}
ZTH_TEST_TIMEOUT("robust", "run_all with no jobs, and with one job, just works", 5) {
    run_all({});
    int x = 0;
    run_all({[&x] { x = 7; }});
    ZTH_EXPECT_EQ(x, 7);
}
ZTH_TEST_TIMEOUT("robust", "fill_in_thread can be called again and again", 10) {
    std::vector<int> v;
    for (int n = 0; n < 30; ++n) {
        fill_in_thread(v, n);
        ZTH_EXPECT_EQ(v.size(), static_cast<std::size_t>(n));
    }
}
ZTH_TEST_TIMEOUT("robust", "a Ticker with a ten second period stops at once when it is destroyed", 5) {
    std::latch first(1);
    const auto before = std::chrono::steady_clock::now();
    {
        Ticker ticker(10s, [&] { first.count_down(); });
        first.wait();  // the first tick comes at once (not after ten seconds)
    }                  // ...and the destructor must not wait for the period to pass
    const auto took = std::chrono::steady_clock::now() - before;
    ZTH_EXPECT_TRUE(took < 2s);
}
ZTH_TEST_TIMEOUT("robust", "no tick happens after the Ticker has been destroyed", 5) {
    std::atomic<int> ticks{0};
    std::latch some(2);
    {
        Ticker ticker(1ms, [&] {
            if (++ticks <= 2) some.count_down();
        });
        some.wait();
    }
    const int at_destruction = ticks.load();
    std::this_thread::sleep_for(30ms);
    ZTH_EXPECT_EQ(ticks.load(), at_destruction);
}
ZTH_TEST_TIMEOUT("robust", "a Ticker does not tick faster than its period: with 50 ms, at most five ticks in 120 ms", 5) {
    std::atomic<int> ticks{0};
    {
        Ticker ticker(50ms, [&] { ++ticks; });
        std::this_thread::sleep_for(120ms);
    }
    ZTH_EXPECT_TRUE(ticks.load() >= 1 && ticks.load() <= 5);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "workers.hpp never detaches a thread") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("workers.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "detach").empty());
}
ZTH_TEST("clean", "the Ticker is built on std::jthread and a stop token, and waits without sleep_for") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("workers.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "jthread").empty());
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "stop_token").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sleep_for").empty());
}
ZTH_TEST("clean", "workers.hpp has no mutable global or static state and no raw new/delete") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("workers.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "static").empty());
    ZTH_EXPECT_NO_RAW_MEMORY("workers.hpp");
}
