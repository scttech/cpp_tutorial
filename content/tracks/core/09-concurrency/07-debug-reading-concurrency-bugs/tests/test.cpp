#include <atomic>
#include <chrono>
#include <latch>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "jobs.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "1. count_in_parallel: four threads counting to 1000 give exactly 4000", 4) {
    ZTH_EXPECT_EQ(count_in_parallel(4, 1000), 4000LL);
}
ZTH_TEST_TIMEOUT("works", "2. Pair: forward() on one thread, then backward() on another, moves the money without any lock-order problem", 4) {
    Pair pair;
    std::jthread f([&] {
        for (int i = 0; i < 10; ++i) pair.forward();
    });
    f.join();
    std::jthread b([&] {
        for (int i = 0; i < 4; ++i) pair.backward();
    });
    b.join();
    ZTH_EXPECT_EQ(pair.a(), 94);
    ZTH_EXPECT_EQ(pair.b(), 6);
}
ZTH_TEST_TIMEOUT("works", "3. Event: a wait that starts AFTER set() comes back at once", 4) {
    Event event;
    event.set();
    std::atomic<bool> came_back{false};
    std::jthread waiter([&] {
        event.wait();
        came_back = true;
    });
    waiter.join();
    ZTH_EXPECT_TRUE(came_back.load());
}
ZTH_TEST_TIMEOUT("works", "4. sum_in_background returns the right sum (and the thread has been joined)", 4) {
    ZTH_EXPECT_EQ(sum_in_background({1, 2, 3, 4, 5}), 15LL);
    ZTH_EXPECT_EQ(sum_in_background({}), 0LL);
}
ZTH_TEST_TIMEOUT("works", "5. Worker: it loops, stop() ends it, and loops() can be read while it runs", 4) {
    Worker worker;
    worker.start();
    while (worker.loops() < 50) std::this_thread::yield();
    worker.stop();
    ZTH_EXPECT_TRUE(worker.loops() >= 50);
}
ZTH_TEST_TIMEOUT("works", "6. run_in_thread: a job that throws makes run_in_thread throw the same exception in the caller", 4) {
    std::string what;
    try {
        run_in_thread([] { throw std::runtime_error("the job failed"); });
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    ZTH_EXPECT_EQ(what, std::string("the job failed"));
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "1. count_in_parallel with 8 threads, with no threads' worth of work, and with one thread", 6) {
    ZTH_EXPECT_EQ(count_in_parallel(8, 700), 5600LL);
    ZTH_EXPECT_EQ(count_in_parallel(3, 0), 0LL);
    ZTH_EXPECT_EQ(count_in_parallel(1, 500), 500LL);
}
ZTH_TEST_TIMEOUT("robust", "2. Pair: forward() and backward() running at the same time never deadlock, and no money is lost", 6) {
    Pair pair;
    std::latch go(1);
    {
        std::jthread f([&] {
            go.wait();
            for (int i = 0; i < 1500; ++i) pair.forward();
        });
        std::jthread b([&] {
            go.wait();
            for (int i = 0; i < 1500; ++i) pair.backward();
        });
        go.count_down();
    }
    ZTH_EXPECT_EQ(pair.a() + pair.b(), 100);
    ZTH_EXPECT_EQ(pair.a(), 100);
}
ZTH_TEST_TIMEOUT("robust", "3. Event: a waiter that starts BEFORE set() sleeps until the event happens (it does not come back early), and several waiters are all released", 6) {
    Event event;
    std::atomic<int> back{0};
    std::latch started(3);
    std::vector<std::jthread> waiters;
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&] {
            started.count_down();
            event.wait();
            ++back;
        });
    }
    started.wait();
    std::this_thread::sleep_for(60ms);
    ZTH_EXPECT_EQ(back.load(), 0);  // nobody was released by anything but set()
    event.set();
    waiters.clear();
    ZTH_EXPECT_EQ(back.load(), 3);
}
ZTH_TEST_TIMEOUT("robust", "4. sum_in_background: a big input, several times in a row", 6) {
    std::vector<int> big(200000, 3);
    for (int i = 0; i < 5; ++i) ZTH_EXPECT_EQ(sum_in_background(big), 600000LL);
}
ZTH_TEST_TIMEOUT("robust", "5. Worker: stop() is prompt, may be called twice, and a Worker that was never started can be stopped", 6) {
    Worker never_started;
    never_started.stop();
    Worker worker;
    worker.start();
    std::this_thread::sleep_for(20ms);
    worker.stop();
    worker.stop();
    const long long after_stop = worker.loops();
    std::this_thread::sleep_for(20ms);
    ZTH_EXPECT_EQ(worker.loops(), after_stop);  // really stopped
}
ZTH_TEST_TIMEOUT("robust", "6. run_in_thread: a job that does not throw just runs (on another thread); the exception type and message are kept", 6) {
    int x = 0;
    const auto main_id = std::this_thread::get_id();
    std::thread::id job_id;
    run_in_thread([&] {
        x = 5;
        job_id = std::this_thread::get_id();
    });
    ZTH_EXPECT_EQ(x, 5);
    ZTH_EXPECT_TRUE(job_id != main_id);
    bool right_type = false;
    try {
        run_in_thread([] { throw std::invalid_argument("bad"); });
    } catch (const std::invalid_argument& e) {
        right_type = std::string(e.what()) == "bad";
    }
    ZTH_EXPECT_TRUE(right_type);
}
