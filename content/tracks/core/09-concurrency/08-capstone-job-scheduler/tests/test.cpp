#include <atomic>
#include <chrono>
#include <fstream>
#include <latch>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "scheduler.hpp"
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

// A single-worker scheduler whose only worker is kept busy by a job that waits for `release`, so that everything submitted meanwhile stays in the queue.
struct Blocked {
    Scheduler scheduler{1};
    std::latch release{1};
    std::latch started{1};
    JobHandle<void> blocker;
    std::mutex m;
    std::vector<std::string> order;

    Blocked() {
        blocker = scheduler.submit(Priority::Normal, [this] {
            started.count_down();
            release.wait();
        });
        started.wait();  // the worker has taken the blocker: from now on, everything else waits in the queue
    }
    auto record(Priority p, const std::string& name) {
        return scheduler.submit(p, [this, name] {
            std::lock_guard lock(m);
            order.push_back(name);
        });
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "jobs run, and their results come back through the handles (a value, a string, nothing)", 4) {
    Scheduler s(3);
    auto a = s.submit(Priority::Normal, [] { return 6 * 7; });
    auto b = s.submit(Priority::High, [] { return std::string("done"); });
    std::atomic<int> x{0};
    auto c = s.submit(Priority::Low, [&x] { x = 5; });
    ZTH_EXPECT_EQ(a.get(), 42);
    ZTH_EXPECT_EQ(b.get(), std::string("done"));
    c.get();
    ZTH_EXPECT_EQ(x.load(), 5);
}
ZTH_TEST_TIMEOUT("works", "worker_count() is what was asked for; a scheduler without workers is an error", 4) {
    Scheduler s(4);
    ZTH_EXPECT_EQ(s.worker_count(), std::size_t{4});
    ZTH_EXPECT_THROWS(std::invalid_argument, Scheduler t(0));
}
ZTH_TEST_TIMEOUT("works", "jobs run on the workers, not on the thread that submits them", 4) {
    Scheduler s(2);
    const auto main_id = std::this_thread::get_id();
    ZTH_EXPECT_TRUE(s.submit(Priority::Normal, [] { return std::this_thread::get_id(); }).get() != main_id);
}
ZTH_TEST_TIMEOUT("works", "the highest priority runs first, whatever the order of submission", 6) {
    Blocked b;
    auto l1 = b.record(Priority::Low, "low-1");
    auto n1 = b.record(Priority::Normal, "normal-1");
    auto h1 = b.record(Priority::High, "high-1");
    auto l2 = b.record(Priority::Low, "low-2");
    auto h2 = b.record(Priority::High, "high-2");
    auto n2 = b.record(Priority::Normal, "normal-2");
    b.release.count_down();
    b.scheduler.wait_idle();
    ZTH_EXPECT_EQ(b.order, (std::vector<std::string>{"high-1", "high-2", "normal-1", "normal-2", "low-1", "low-2"}));
}
ZTH_TEST_TIMEOUT("works", "jobs of the same priority run in the order they were submitted", 6) {
    Blocked b;
    std::vector<JobHandle<void>> handles;
    std::vector<std::string> expected;
    for (int i = 0; i < 30; ++i) {
        expected.push_back("job-" + std::to_string(i));
        handles.push_back(b.record(Priority::Normal, expected.back()));
    }
    b.release.count_down();
    b.scheduler.wait_idle();
    ZTH_EXPECT_EQ(b.order, expected);
}
ZTH_TEST_TIMEOUT("works", "pending() counts the queued jobs, and stats() the finished ones", 6) {
    Blocked b;
    auto h1 = b.record(Priority::Normal, "a");
    auto h2 = b.record(Priority::Normal, "b");
    auto h3 = b.record(Priority::High, "c");
    ZTH_EXPECT_EQ(b.scheduler.pending(), std::size_t{3});  // the blocker is running, so it is not pending
    ZTH_EXPECT_EQ(b.scheduler.stats().completed, std::size_t{0});
    b.release.count_down();
    b.scheduler.wait_idle();
    ZTH_EXPECT_EQ(b.scheduler.pending(), std::size_t{0});
    ZTH_EXPECT_EQ(b.scheduler.stats().completed, std::size_t{4});
}
ZTH_TEST_TIMEOUT("works", "wait_idle waits for jobs that are RUNNING, not only for the queue to be empty; with nothing to do it returns at once", 6) {
    Scheduler s(2);
    s.wait_idle();
    std::atomic<int> finished{0};
    for (int i = 0; i < 4; ++i) {
        s.submit(Priority::Normal, [&finished] {
            std::this_thread::sleep_for(40ms);
            ++finished;
        });
    }
    s.wait_idle();
    ZTH_EXPECT_EQ(finished.load(), 4);

    // called while a job is RUNNING and the queue is already empty
    std::atomic<bool> long_done{false};
    std::latch started{1};
    s.submit(Priority::Normal, [&] {
        started.count_down();
        std::this_thread::sleep_for(200ms);
        long_done = true;
    });
    started.wait();
    s.wait_idle();
    ZTH_EXPECT_TRUE(long_done.load());
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "an exception thrown by a job arrives through its handle; it is counted as failed; the scheduler goes on", 6) {
    Scheduler s(1);
    auto bad = s.submit(Priority::Normal, []() -> int { throw std::runtime_error("boom"); });
    std::string what;
    try {
        bad.get();
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    ZTH_EXPECT_EQ(what, std::string("boom"));
    ZTH_EXPECT_EQ(s.submit(Priority::Normal, [] { return 1; }).get(), 1);  // the only worker is still alive
    s.wait_idle();
    ZTH_EXPECT_EQ(s.stats().failed, std::size_t{1});
    ZTH_EXPECT_EQ(s.stats().completed, std::size_t{1});
}
ZTH_TEST_TIMEOUT("robust", "cancel() takes a queued job out: true, the job never runs, get() throws JobCancelled, the stats say so", 6) {
    Blocked b;
    auto keep = b.record(Priority::Normal, "keep");
    auto drop = b.record(Priority::Normal, "drop");
    ZTH_EXPECT_EQ(b.scheduler.pending(), std::size_t{2});
    ZTH_EXPECT_TRUE(drop.cancel());
    ZTH_EXPECT_EQ(b.scheduler.pending(), std::size_t{1});
    ZTH_EXPECT_FALSE(drop.cancel());  // already cancelled
    b.release.count_down();
    b.scheduler.wait_idle();
    ZTH_EXPECT_EQ(b.order, (std::vector<std::string>{"keep"}));
    bool cancelled = false;
    try {
        drop.get();
    } catch (const JobCancelled&) {
        cancelled = true;
    }
    ZTH_EXPECT_TRUE(cancelled);
    ZTH_EXPECT_EQ(b.scheduler.stats().cancelled, std::size_t{1});
    ZTH_EXPECT_EQ(b.scheduler.stats().completed, std::size_t{2});  // the blocker and "keep"
}
ZTH_TEST_TIMEOUT("robust", "a job that is running or finished cannot be cancelled", 6) {
    Blocked b;
    ZTH_EXPECT_FALSE(b.blocker.cancel());  // running
    auto quick = b.record(Priority::Normal, "q");
    b.release.count_down();
    b.scheduler.wait_idle();
    ZTH_EXPECT_FALSE(quick.cancel());  // finished
    quick.get();
    ZTH_EXPECT_EQ(b.scheduler.stats().cancelled, std::size_t{0});
}
ZTH_TEST_TIMEOUT("robust", "shutdown(Drain), and the destructor, run every queued job first", 8) {
    std::atomic<int> ran{0};
    {
        Scheduler s(2);
        for (int i = 0; i < 60; ++i) {
            s.submit(i % 3 == 0 ? Priority::High : Priority::Low, [&ran] {
                std::this_thread::sleep_for(1ms);
                ++ran;
            });
        }
    }  // destroyed with most jobs still queued
    ZTH_EXPECT_EQ(ran.load(), 60);
}
ZTH_TEST_TIMEOUT("robust", "shutdown(Discard): the running job finishes, the queued jobs are cancelled and their handles say so", 6) {
    Blocked b;
    auto q1 = b.record(Priority::Normal, "q1");
    auto q2 = b.record(Priority::High, "q2");
    std::jthread stopper([&] { b.scheduler.shutdown(ShutdownMode::Discard); });
    std::this_thread::sleep_for(40ms);  // the queued jobs are cancelled at once; the worker is still inside the blocker
    int cancelled = 0;
    for (auto* h : {&q1, &q2}) {
        try {
            h->get();
        } catch (const JobCancelled&) {
            ++cancelled;
        }
    }
    ZTH_EXPECT_EQ(cancelled, 2);
    b.release.count_down();
    stopper.join();
    ZTH_EXPECT_TRUE(b.order.empty());  // neither queued job ever ran
    ZTH_EXPECT_EQ(b.scheduler.stats().cancelled, std::size_t{2});
    ZTH_EXPECT_EQ(b.scheduler.stats().completed, std::size_t{1});  // the blocker
}
ZTH_TEST_TIMEOUT("robust", "after shutdown, submit throws std::runtime_error; shutdown twice is harmless", 6) {
    Scheduler s(2);
    s.submit(Priority::Normal, [] {}).get();
    s.shutdown();
    ZTH_EXPECT_THROWS(std::runtime_error, s.submit(Priority::Normal, [] {}));
    s.shutdown();
    s.shutdown(ShutdownMode::Discard);
}
ZTH_TEST_TIMEOUT("robust", "N workers really work at the same time: three jobs that wait for each other all finish", 6) {
    Scheduler s(3);
    std::latch all_here(3);
    std::vector<JobHandle<void>> handles;
    for (int i = 0; i < 3; ++i) handles.push_back(s.submit(Priority::Normal, [&all_here] { all_here.arrive_and_wait(); }));
    for (auto& h : handles) h.get();
}
ZTH_TEST_TIMEOUT("robust", "four threads submit 150 jobs each, with all priorities, at the same time: every job runs exactly once", 8) {
    Scheduler s(3);
    std::atomic<int> ran{0};
    {
        std::vector<std::jthread> submitters;
        for (int t = 0; t < 4; ++t) {
            submitters.emplace_back([&s, &ran, t] {
                for (int i = 0; i < 150; ++i) s.submit(static_cast<Priority>((i + t) % 3), [&ran] { ++ran; });
            });
        }
    }
    s.wait_idle();
    ZTH_EXPECT_EQ(ran.load(), 600);
    ZTH_EXPECT_EQ(s.stats().completed, std::size_t{600});
}
ZTH_TEST_TIMEOUT("robust", "a job may submit and cancel other jobs", 6) {
    Blocked b;
    std::atomic<int> inner_ran{0};
    auto victim = b.record(Priority::Low, "victim");
    std::atomic<bool> cancelled{false};
    auto outer = b.scheduler.submit(Priority::High, [&] {
        b.scheduler.submit(Priority::Normal, [&inner_ran] { ++inner_ran; });  // submitted from inside a job
        cancelled = victim.cancel();                                          // and a cancel from inside a job
    });
    b.release.count_down();
    outer.get();
    b.scheduler.wait_idle();
    ZTH_EXPECT_EQ(inner_ran.load(), 1);
    ZTH_EXPECT_TRUE(cancelled.load());
    ZTH_EXPECT_TRUE(b.order.empty());
}
ZTH_TEST_TIMEOUT("robust", "get() blocks until the job is done: a thread that asks first gets the result later", 6) {
    Blocked b;
    std::atomic<int> got{0};
    auto h = b.scheduler.submit(Priority::Normal, [] { return 99; });
    std::jthread waiter([&] { got = h.get(); });
    std::this_thread::sleep_for(40ms);
    ZTH_EXPECT_EQ(got.load(), 0);
    b.release.count_down();
    waiter.join();
    ZTH_EXPECT_EQ(got.load(), 99);
}
ZTH_TEST_TIMEOUT("robust", "several threads in wait_idle at once all come back", 6) {
    Scheduler s(2);
    for (int i = 0; i < 6; ++i) s.submit(Priority::Normal, [] { std::this_thread::sleep_for(10ms); });
    std::atomic<int> back{0};
    {
        std::vector<std::jthread> waiters;
        for (int i = 0; i < 3; ++i) waiters.emplace_back([&] {
            s.wait_idle();
            ++back;
        });
    }
    ZTH_EXPECT_EQ(back.load(), 3);
}

// ================================================ Fast ========================================================
ZTH_TEST_TIMEOUT("fast", "threads are reused: 200 slow jobs on 3 workers never put more than 3 extra threads in the process", 8) {
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
        Scheduler s(3);
        for (int i = 0; i < 200; ++i) s.submit(Priority::Normal, [] { std::this_thread::sleep_for(2ms); });
        s.wait_idle();
    }
    done = true;
    sampler.join();
    ZTH_EXPECT_TRUE(most.load() <= base + 3);
}
ZTH_TEST_TIMEOUT("fast", "waiting is free: idle workers, and a thread in wait_idle, use almost no processor time while a job runs for a quarter of a second", 6) {
    Scheduler s(3);
    std::latch started(1);
    auto longjob = s.submit(Priority::Normal, [&started] {
        started.count_down();
        std::this_thread::sleep_for(300ms);
    });
    started.wait();
    std::jthread idle_waiter([&] { s.wait_idle(); });
    std::this_thread::sleep_for(60ms);
    const double before = ::zth::cpu_ms();
    std::this_thread::sleep_for(200ms);
    const double used = ::zth::cpu_ms() - before;
    longjob.get();
    idle_waiter.join();
    ZTH_EXPECT_TRUE(used < 100.0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no detach, no std::async: the workers are std::jthread members that are joined") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("scheduler.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "detach").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "async").empty());
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "jthread").empty() || !::zth::find_word(code, "join").empty());
}
ZTH_TEST("clean", "every wait has a predicate, and nothing sleeps, yields or spins") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("scheduler.hpp"));
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
    ZTH_EXPECT_TRUE(waits >= 2);
}
ZTH_TEST("clean", "no raw new/delete, no volatile, no manual lock()/unlock() calls on a mutex") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("scheduler.hpp"));
    ZTH_EXPECT_NO_RAW_MEMORY("scheduler.hpp");
    ZTH_EXPECT_TRUE(::zth::find_word(code, "volatile").empty());
    ZTH_EXPECT_TRUE(code.find("mutex_.lock(") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find("mutex_.unlock(") == std::string::npos);
}
