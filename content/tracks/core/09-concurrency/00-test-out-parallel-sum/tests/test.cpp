#include <chrono>
#include <climits>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#include "parallel_sum.hpp"
#include "zth_test.hpp"

namespace {

long long reference(const std::vector<int>& v) { return std::accumulate(v.begin(), v.end(), 0LL); }

// The number of threads of this process right now (the "Threads:" line of /proc/self/status).
int thread_count_now() {
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line);) {
        if (line.rfind("Threads:", 0) == 0) return std::stoi(line.substr(8));
    }
    return 0;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "the sum of 1..100 is 5050, with 1, 2, 4 and 7 threads", 5) {
    std::vector<int> v(100);
    std::iota(v.begin(), v.end(), 1);
    for (unsigned threads : {1u, 2u, 4u, 7u}) ZTH_EXPECT_EQ(parallel_sum(v, threads), 5050LL);
}
ZTH_TEST_TIMEOUT("works", "it agrees with std::accumulate on 20000 random values", 5) {
    ::zth::Lcg rng(7);
    std::vector<int> v(20000);
    for (int& x : v) x = static_cast<int>(rng.next() % 2001) - 1000;
    for (unsigned threads : {2u, 3u, 8u}) ZTH_EXPECT_EQ(parallel_sum(v, threads), reference(v));
}
ZTH_TEST_TIMEOUT("works", "the sum may be bigger than INT_MAX: the result is a long long", 5) {
    std::vector<int> v(100, 2000000000);
    ZTH_EXPECT_EQ(parallel_sum(v, 3), 200000000000LL);
}
ZTH_TEST_TIMEOUT("works", "the work is really shared by several threads (the process has more threads while it runs)", 20) {
    std::vector<int> v(3000000, 1);
    std::atomic<bool> done{false};
    std::atomic<int> most{0};
    std::thread sampler([&] {
        while (!done.load()) {
            const int now = thread_count_now();
            int seen = most.load();
            while (now > seen && !most.compare_exchange_weak(seen, now)) {
            }
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        }
    });
    const int base = thread_count_now();  // main, the sampler, and the helper threads of the sanitizer
    long long total = 0;
    for (int round = 0; round < 3; ++round) total = parallel_sum(v, 4);
    done = true;
    sampler.join();
    ZTH_EXPECT_EQ(total, 3000000LL);
    ZTH_EXPECT_TRUE(most.load() >= base + 2);  // at least two workers were alive at the same moment
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "an empty vector gives 0 (and needs no thread)", 5) {
    ZTH_EXPECT_EQ(parallel_sum({}, 8), 0LL);
}
ZTH_TEST_TIMEOUT("robust", "zero threads is an error: std::invalid_argument", 5) {
    const std::vector<int> v = {1, 2, 3};
    ZTH_EXPECT_THROWS(std::invalid_argument, parallel_sum(v, 0));
}
ZTH_TEST_TIMEOUT("robust", "fewer values than threads: 1 value with 100000 threads, 3 values with 1000", 10) {
    ZTH_EXPECT_EQ(parallel_sum({5}, 100000), 5LL);
    ZTH_EXPECT_EQ(parallel_sum({1, 2, 3}, 1000), 6LL);
}
ZTH_TEST_TIMEOUT("robust", "every value counts when the length is not a multiple of the thread count (no remainder is dropped)", 5) {
    for (std::size_t n : {1u, 2u, 5u, 7u, 10u, 11u, 99u, 1001u}) {
        std::vector<int> v(n);
        std::iota(v.begin(), v.end(), 1);
        for (unsigned threads : {2u, 3u, 4u, 8u}) ZTH_EXPECT_EQ(parallel_sum(v, threads), reference(v));
    }
}
ZTH_TEST_TIMEOUT("robust", "negative values and zeros", 5) {
    const std::vector<int> v = {-5, 0, 7, -100, 0, 42, -1};
    ZTH_EXPECT_EQ(parallel_sum(v, 3), -57LL);
    const std::vector<int> mins(10, INT_MIN);
    ZTH_EXPECT_EQ(parallel_sum(mins, 4), 10LL * INT_MIN);
}
ZTH_TEST_TIMEOUT("robust", "the input is not changed, and repeated calls give the same answer", 10) {
    std::vector<int> v(5000);
    std::iota(v.begin(), v.end(), 1);
    const std::vector<int> copy = v;
    for (int i = 0; i < 20; ++i) ZTH_EXPECT_EQ(parallel_sum(v, 5), reference(copy));
    ZTH_EXPECT_TRUE(v == copy);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "parallel_sum.hpp never detaches a thread: every thread is joined before the function returns") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("parallel_sum.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "detach").empty());
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "join").empty() || !::zth::find_word(code, "jthread").empty());
}
ZTH_TEST("clean", "parallel_sum.hpp has no mutable global or static state") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("parallel_sum.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "static").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "thread_local").empty());
}
ZTH_TEST("clean", "parallel_sum.hpp does not use new, delete, malloc or free") {
    ZTH_EXPECT_NO_RAW_MEMORY("parallel_sum.hpp");
}
