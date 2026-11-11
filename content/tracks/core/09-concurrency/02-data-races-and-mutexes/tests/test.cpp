#include <atomic>
#include <latch>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "bank.hpp"
#include "zth_test.hpp"

namespace {

// Runs `body(thread_index)` on `n` threads that all start together.
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
}  // joined here

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST_TIMEOUT("works", "a new bank: every account has the initial balance", 5) {
    Bank bank(3, 100);
    ZTH_EXPECT_EQ(bank.size(), std::size_t{3});
    ZTH_EXPECT_EQ(bank.balance(2), 100LL);
    ZTH_EXPECT_EQ(bank.total(), 300LL);
}
ZTH_TEST_TIMEOUT("works", "deposit, withdraw and transfer, with one thread", 5) {
    Bank bank(3, 100);
    bank.deposit(0, 50);
    ZTH_EXPECT_TRUE(bank.withdraw(1, 30));
    ZTH_EXPECT_TRUE(bank.transfer(0, 2, 120));
    ZTH_EXPECT_EQ(bank.balance(0), 30LL);
    ZTH_EXPECT_EQ(bank.balance(1), 70LL);
    ZTH_EXPECT_EQ(bank.balance(2), 220LL);
    ZTH_EXPECT_EQ(bank.total(), 320LL);
}
ZTH_TEST_TIMEOUT("works", "not enough money: withdraw and transfer return false and change nothing", 5) {
    Bank bank(2, 100);
    ZTH_EXPECT_FALSE(bank.withdraw(0, 101));
    ZTH_EXPECT_FALSE(bank.transfer(0, 1, 101));
    ZTH_EXPECT_EQ(bank.balance(0), 100LL);
    ZTH_EXPECT_EQ(bank.balance(1), 100LL);
}
ZTH_TEST_TIMEOUT("works", "four threads depositing 1000 times each lose no update", 6) {
    Bank bank(2, 0);
    run_together(4, [&](unsigned) {
        for (int i = 0; i < 1000; ++i) bank.deposit(0, 1);
    });
    ZTH_EXPECT_EQ(bank.balance(0), 4000LL);
}
ZTH_TEST_TIMEOUT("works", "transfers between 8 accounts from 4 threads conserve the money, and nobody goes below zero", 6) {
    Bank bank(8, 1000);
    run_together(4, [&](unsigned t) {
        ::zth::Lcg rng(100 + t);
        for (int i = 0; i < 400; ++i) bank.transfer(static_cast<std::size_t>(rng.below(8)), static_cast<std::size_t>(rng.below(8)), 1 + rng.below(200));
    });
    ZTH_EXPECT_EQ(bank.total(), 8000LL);
    for (std::size_t i = 0; i < 8; ++i) ZTH_EXPECT_TRUE(bank.balance(i) >= 0);
}

// ================================================ Robust ======================================================
ZTH_TEST_TIMEOUT("robust", "two threads transferring in OPPOSITE directions between the same accounts do not deadlock", 6) {
    Bank bank(2, 1000000);
    run_together(2, [&](unsigned t) {
        for (int i = 0; i < 1500; ++i) bank.transfer(t, 1 - t, 1);
    });
    ZTH_EXPECT_EQ(bank.total(), 2000000LL);
}
ZTH_TEST_TIMEOUT("robust", "transfers in one direction and then the other, one after the other, are fine (no lock-order inversion)", 6) {
    Bank bank(2, 100);
    std::jthread a([&] { bank.transfer(0, 1, 10); });
    a.join();
    std::jthread b([&] { bank.transfer(1, 0, 5); });
    b.join();
    ZTH_EXPECT_EQ(bank.balance(0), 95LL);
}
ZTH_TEST_TIMEOUT("robust", "a transfer from an account to itself works and changes nothing", 5) {
    Bank bank(2, 100);
    ZTH_EXPECT_TRUE(bank.transfer(1, 1, 60));
    ZTH_EXPECT_FALSE(bank.transfer(1, 1, 160));
    ZTH_EXPECT_EQ(bank.balance(1), 100LL);
}
ZTH_TEST_TIMEOUT("robust", "total() never sees money in flight: while 3 threads transfer, every total() is exactly the money the bank started with", 8) {
    Bank bank(6, 1000);
    std::atomic<bool> stop{false};
    std::atomic<int> wrong{0};
    std::jthread auditor([&] {
        while (!stop.load()) {
            if (bank.total() != 6000) ++wrong;
        }
    });
    run_together(3, [&](unsigned t) {
        ::zth::Lcg rng(7 + t);
        for (int i = 0; i < 1500; ++i) bank.transfer(static_cast<std::size_t>(rng.below(6)), static_cast<std::size_t>(rng.below(6)), 1 + rng.below(50));
    });
    stop = true;
    auditor.join();
    ZTH_EXPECT_EQ(wrong.load(), 0);
}
ZTH_TEST_TIMEOUT("robust", "reading a balance while others write is safe", 6) {
    Bank bank(3, 100);
    std::atomic<long long> seen{0};
    run_together(3, [&](unsigned t) {  // thread 0 reads, threads 1 and 2 write: all of them start together and do a fixed amount of work
        if (t == 0) {
            for (int i = 0; i < 1000; ++i) seen += bank.balance(1);
        } else {
            for (int i = 0; i < 1000; ++i) bank.deposit(1, 1);
        }
    });
    ZTH_EXPECT_EQ(bank.balance(1), 2100LL);
    ZTH_EXPECT_TRUE(seen.load() >= 100LL * 1000);
}
ZTH_TEST_TIMEOUT("robust", "withdrawing while others deposit: the check and the change are one step", 6) {
    Bank bank(3, 1000);
    run_together(3, [&](unsigned t) {
        for (int i = 0; i < 500; ++i) {
            if (t == 0) bank.withdraw(2, 1);
            else bank.deposit(2, 1);
        }
    });
    ZTH_EXPECT_EQ(bank.balance(2), 1500LL);  // 1000 - 500 + 2 * 500
}
ZTH_TEST_TIMEOUT("robust", "invalid arguments: out_of_range for an unknown account, invalid_argument for an amount <= 0", 5) {
    Bank bank(2, 100);
    ZTH_EXPECT_THROWS(std::out_of_range, bank.balance(2));
    ZTH_EXPECT_THROWS(std::out_of_range, bank.transfer(0, 5, 1));
    ZTH_EXPECT_THROWS(std::invalid_argument, bank.deposit(0, 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, bank.withdraw(0, -1));
    ZTH_EXPECT_THROWS(std::invalid_argument, bank.transfer(0, 1, 0));
    ZTH_EXPECT_EQ(bank.total(), 200LL);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "locks are RAII objects: bank.hpp never calls lock() or unlock() by hand") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("bank.hpp"));
    ZTH_EXPECT_TRUE(code.find(".lock(") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find(".unlock(") == std::string::npos);
    ZTH_EXPECT_TRUE(code.find(".try_lock(") == std::string::npos);
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "lock_guard").empty() || !::zth::find_word(code, "scoped_lock").empty() || !::zth::find_word(code, "unique_lock").empty());
}
ZTH_TEST("clean", "no sleeping, spinning or volatile to 'fix' a race, and no raw new/delete") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("bank.hpp"));
    ZTH_EXPECT_TRUE(::zth::find_word(code, "volatile").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sleep_for").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "yield").empty());
    ZTH_EXPECT_NO_RAW_MEMORY("bank.hpp");
}
