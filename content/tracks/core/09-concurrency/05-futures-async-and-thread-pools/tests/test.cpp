#include <atomic>
#include <chrono>
#include <fstream>
#include <latch>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "pool.hpp"
#include "zth_test.hpp"

using namespace std::chrono_literals;

namespace {

int thread_count_now() {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("Threads:", 0) == 0) return std::stoi(line.substr(8));
    }
    return 0;
}

int add(int a, int b) { return a + b; }

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "submit returns a future with the result; the arguments are passed on", 6) {
    ThreadPool pool(2);
    auto a = pool.submit([] { return 6 * 7; });
    auto b = pool.submit(add, 2, 3);
    auto c = pool.submit([](const std::string& s, int n) { return s + std::to_string(n); }, std::string("x"), 9);
    ZTH_EXPECT_EQ(a.get(), 42);
    ZTH_EXPECT_EQ(b.get(), 5);
    ZTH_EXPECT_EQ(c.get(), std::string("x9"));
}
ZTH_TEST_TIMEOUT("works", "a task that returns nothing; and a reference argument given with std::ref", 6) {
    ThreadPool pool(2);
    int x = 1;
    pool.submit([](int& v) { v += 10; }, std::ref(x)).get();
    ZTH_EXPECT_EQ(x, 11);
}
ZTH_TEST_TIMEOUT("works", "size() is the number of worker threads, and 0 threads is an error", 6) {
    ThreadPool pool(3);
    ZTH_EXPECT_EQ(pool.size(), std::size_t{3});
    ZTH_EXPECT_THROWS(std::invalid_argument, ThreadPool p(0));
}
ZTH_TEST_TIMEOUT("works", "tasks run on the worker threads, not on the thread that submits them", 6) {
    ThreadPool pool(2);
    const auto main_id = std::this_thread::get_id();
    auto id = pool.submit([] { return std::this_thread::get_id(); });
    ZTH_EXPECT_TRUE(id.get() != main_id);
}
ZTH_TEST_TIMEOUT("works", "a hundred tasks: every result comes back through its own future", 6) {
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 100; ++i) futures.push_back(pool.submit([i] { return i * i; }));
    bool right = true;
    for (int i = 0; i < 100; ++i) right = right && futures[static_cast<std::size_t>(i)].get() == i * i;
    ZTH_EXPECT_TRUE(right);
}
ZTH_TEST_TIMEOUT("works", "parallel_map gives the results in the order of the input", 6) {
    ThreadPool pool(3);
    const std::vector<int> input = {5, 4, 3, 2, 1, 0, 9, 8};
    ZTH_EXPECT_EQ(parallel_map(pool, input, [](int x) { return x * 10; }), (std::vector<int>{50, 40, 30, 20, 10, 0, 90, 80}));
}
ZTH_TEST_TIMEOUT("works", "the destructor finishes every queued task before it returns", 8) {
    std::atomic<int> done{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 40; ++i) {
            pool.submit([&done] {
                std::this_thread::sleep_for(1ms);
                ++done;
            });
        }
    }  // the pool is destroyed with most tasks still waiting in the queue
    ZTH_EXPECT_EQ(done.load(), 40);
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "an exception thrown by a task arrives through its future, and the pool keeps working", 6) {
    ThreadPool pool(2);
    auto failing = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    std::string what;
    try {
        failing.get();
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    ZTH_EXPECT_EQ(what, std::string("boom"));
    for (int i = 0; i < 10; ++i) ZTH_EXPECT_EQ(pool.submit([i] { return i; }).get(), i);  // both workers are alive
}
ZTH_TEST_TIMEOUT("robust", "N workers really work at the same time: three tasks that wait for each other all finish", 6) {
    ThreadPool pool(3);
    std::latch all_here(3);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) futures.push_back(pool.submit([&all_here] { all_here.arrive_and_wait(); }));
    for (auto& f : futures) f.get();  // with fewer than three workers this never finishes (the test would time out)
}
ZTH_TEST_TIMEOUT("robust", "several threads submitting at once lose no task", 8) {
    ThreadPool pool(3);
    std::atomic<int> ran{0};
    {
        std::vector<std::jthread> submitters;
        for (int s = 0; s < 4; ++s) {
            submitters.emplace_back([&] {
                for (int i = 0; i < 150; ++i) pool.submit([&ran] { ++ran; });
            });
        }
    }
    pool.shutdown();  // waits for the queue to be empty
    ZTH_EXPECT_EQ(ran.load(), 600);
}
ZTH_TEST_TIMEOUT("robust", "a task may submit another task (and the pool does not deadlock as long as nobody waits inside a task)", 6) {
    ThreadPool pool(2);
    std::latch inner_done(1);
    pool.submit([&] { pool.submit([&] { inner_done.count_down(); }); });
    inner_done.wait();
}
ZTH_TEST_TIMEOUT("robust", "move-only tasks and move-only results: a lambda that owns a unique_ptr, and a future of unique_ptr", 6) {
    ThreadPool pool(2);
    auto owned = std::make_unique<int>(41);
    auto result = pool.submit([p = std::move(owned)]() mutable { return std::make_unique<int>(*p + 1); });
    auto got = result.get();
    ZTH_EXPECT_TRUE(got != nullptr);
    if (got) ZTH_EXPECT_EQ(*got, 42);
}
ZTH_TEST_TIMEOUT("robust", "after shutdown(), submit throws std::runtime_error; shutdown() twice is harmless; queued tasks were completed", 6) {
    ThreadPool pool(2);
    std::atomic<int> done{0};
    for (int i = 0; i < 20; ++i) pool.submit([&done] { ++done; });
    pool.shutdown();
    ZTH_EXPECT_EQ(done.load(), 20);
    ZTH_EXPECT_THROWS(std::runtime_error, pool.submit([] { return 1; }));
    pool.shutdown();
}
ZTH_TEST_TIMEOUT("robust", "parallel_map rethrows a failure, and the pool still works afterwards", 6) {
    ThreadPool pool(2);
    const std::vector<int> input = {1, 2, 3, 4, 5};
    bool caught = false;
    try {
        parallel_map(pool, input, [](int x) {
            if (x == 3) throw std::runtime_error("three");
            return x;
        });
    } catch (const std::runtime_error&) {
        caught = true;
    }
    ZTH_EXPECT_TRUE(caught);
    ZTH_EXPECT_EQ(pool.submit([] { return 5; }).get(), 5);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "threads are REUSED: 200 slow tasks on a pool of 3 never put more than 3 extra threads in the process", 8) {
    std::atomic<bool> done{false};
    std::atomic<int> most{0};
    std::jthread sampler([&] {
        while (!done.load()) {
            const int now = thread_count_now();
            int seen = most.load();
            while (now > seen && !most.compare_exchange_weak(seen, now)) {
            }
            std::this_thread::sleep_for(200us);
        }
    });
    std::this_thread::sleep_for(20ms);
    const int base = thread_count_now();
    {
        ThreadPool pool(3);
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 200; ++i) futures.push_back(pool.submit([] { std::this_thread::sleep_for(2ms); }));
        for (auto& f : futures) f.get();
    }
    done = true;
    sampler.join();
    ZTH_EXPECT_TRUE(most.load() <= base + 3);
}
ZTH_TEST_TIMEOUT("fast", "idle workers sleep: a pool that has nothing to do for a quarter of a second uses almost no processor time", 6) {
    ThreadPool pool(3);
    pool.submit([] {}).get();  // the workers have started
    std::this_thread::sleep_for(60ms);
    const double before = ::zth::cpu_ms();
    std::this_thread::sleep_for(250ms);
    ZTH_EXPECT_TRUE(::zth::cpu_ms() - before < 100.0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "the pool is built from std::packaged_task and worker threads: no detach, no std::async inside it") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pool.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "packaged_task").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "detach").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "async").empty());
}
ZTH_TEST("clean", "workers wait on a condition variable with a predicate, and nothing sleeps, yields or spins") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("pool.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sleep_for").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "yield").empty());
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
    ZTH_EXPECT_TRUE(waits >= 1);
}
ZTH_TEST("clean", "no raw new/delete: tasks are owned by smart pointers") {
    ZTH_EXPECT_NO_RAW_MEMORY("pool.hpp");
}
