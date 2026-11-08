#include <climits>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "account.hpp"
#include "zth_test.hpp"

namespace {

using Hist = std::vector<long long>;

// true if `f()` throws an Ex (or something derived from it); any other outcome, including a different exception, is false
long long sum_of(const Hist& h) { return std::accumulate(h.begin(), h.end(), 0LL); }

// The three invariants that can be read through the public interface.
bool consistent(BankAccount& a) { return a.balance() >= 0 && a.balance() == sum_of(a.history()) && !a.owner().empty(); }

// ---- API-shape checks, as templates: false (instead of a compile error in this hidden file) when the shape is wrong
template <class A>
constexpr bool const_getters = requires(const A a) { a.owner(); a.balance(); a.history(); };

// "Tampering": try to change the account through what its getters return. Whatever the getters return, the account
// must not change. (If the getter hands out a mutable reference the tampering compiles and works; if it returns a
// copy or a const reference, there is nothing to do.)
template <class A>
void tamper(A& a) {
    if constexpr (requires { a.owner() += "!"; }) a.owner() += "!";
    if constexpr (requires { a.balance() = -999; }) a.balance() = -999;
    if constexpr (requires { a.history().push_back(7); }) a.history().push_back(7);
    if constexpr (requires { a.history().clear(); }) a.history().clear();
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "a new account has its owner and opening balance; the balance defaults to 0") {
    BankAccount a("Ada", 500);
    BankAccount b("Bob");
    ZTH_EXPECT_EQ(a.owner(), std::string("Ada"));
    ZTH_EXPECT_EQ(a.balance(), 500LL);
    ZTH_EXPECT_EQ(b.owner(), std::string("Bob"));
    ZTH_EXPECT_EQ(b.balance(), 0LL);
}
ZTH_TEST("works", "deposit adds to the balance") {
    BankAccount a("Ada", 100);
    a.deposit(250);
    ZTH_EXPECT_EQ(a.balance(), 350LL);
}
ZTH_TEST("works", "withdraw subtracts and returns true") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_TRUE(a.withdraw(30));
    ZTH_EXPECT_EQ(a.balance(), 70LL);
}
ZTH_TEST("works", "withdrawing more than the balance returns false and changes nothing") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_FALSE(a.withdraw(101));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
}
ZTH_TEST("works", "withdrawing exactly the balance is allowed") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_TRUE(a.withdraw(100));
    ZTH_EXPECT_EQ(a.balance(), 0LL);
    ZTH_EXPECT_FALSE(a.withdraw(1));
}
ZTH_TEST("works", "the history records the opening balance, deposits (+) and withdrawals (-) in order") {
    BankAccount a("Ada", 100);
    a.deposit(50);
    ZTH_EXPECT_TRUE(a.withdraw(30));
    ZTH_EXPECT_EQ(a.history(), (Hist{100, 50, -30}));
    BankAccount b("Bob");  // no opening balance: nothing to record
    ZTH_EXPECT_EQ(b.history().size(), std::size_t{0});
}
ZTH_TEST("works", "transfer_to moves money between two accounts") {
    BankAccount a("Ada", 100), b("Bob", 20);
    ZTH_EXPECT_TRUE(a.transfer_to(b, 40));
    ZTH_EXPECT_EQ(a.balance(), 60LL);
    ZTH_EXPECT_EQ(b.balance(), 60LL);
}
ZTH_TEST("works", "a transfer without enough money returns false and changes nothing") {
    BankAccount a("Ada", 30), b("Bob", 20);
    ZTH_EXPECT_FALSE(a.transfer_to(b, 31));
    ZTH_EXPECT_EQ(a.balance(), 30LL);
    ZTH_EXPECT_EQ(b.balance(), 20LL);
}

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "an empty owner or a negative opening balance is rejected with std::invalid_argument") {
    ZTH_EXPECT_THROWS(std::invalid_argument, BankAccount a(""));
    ZTH_EXPECT_THROWS(std::invalid_argument, BankAccount a("", 100));
    ZTH_EXPECT_THROWS(std::invalid_argument, BankAccount a("Ada", -1));
    ZTH_EXPECT_NO_THROW(BankAccount a("Ada", 0));
    ZTH_EXPECT_NO_THROW(BankAccount a("A", LLONG_MAX));
}
ZTH_TEST("robust", "a deposit or withdrawal of zero or less throws std::invalid_argument and changes nothing") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_THROWS(std::invalid_argument, a.deposit(0));
    ZTH_EXPECT_THROWS(std::invalid_argument, a.deposit(-5));
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)a.withdraw(0));
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)a.withdraw(-5));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
    ZTH_EXPECT_EQ(a.history(), (Hist{100}));
}
ZTH_TEST("robust", "a refused withdrawal leaves no trace in the history") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_FALSE(a.withdraw(500));
    ZTH_EXPECT_EQ(a.history(), (Hist{100}));
    ZTH_EXPECT_TRUE(consistent(a));
}
ZTH_TEST("robust", "a deposit that would overflow throws std::overflow_error and changes nothing") {
    BankAccount a("Ada", LLONG_MAX - 5);
    ZTH_EXPECT_THROWS(std::overflow_error, a.deposit(10));
    ZTH_EXPECT_EQ(a.balance(), LLONG_MAX - 5);
    ZTH_EXPECT_EQ(a.history().size(), std::size_t{1});
    a.deposit(5);  // exactly reaching the maximum is fine
    ZTH_EXPECT_EQ(a.balance(), LLONG_MAX);
    ZTH_EXPECT_TRUE(consistent(a));
}
ZTH_TEST("robust", "the getters cannot be used to change the account") {
    BankAccount a("Ada", 100);
    a.deposit(50);
    tamper(a);
    ZTH_EXPECT_EQ(a.owner(), std::string("Ada"));
    ZTH_EXPECT_EQ(a.balance(), 150LL);
    ZTH_EXPECT_EQ(a.history(), (Hist{100, 50}));
}
ZTH_TEST("robust", "owner(), balance() and history() are const member functions (they work on a const account)") {
    if (!const_getters<BankAccount>) {
        ::zth::fail("owner(), balance() and history() must be `const` member functions, so they can be called on a const BankAccount");
        return;
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "a transfer of zero or less throws std::invalid_argument and changes nothing") {
    BankAccount a("Ada", 100), b("Bob", 50);
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)a.transfer_to(b, 0));
    ZTH_EXPECT_THROWS(std::invalid_argument, (void)a.transfer_to(b, -30));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
    ZTH_EXPECT_EQ(b.balance(), 50LL);
    ZTH_EXPECT_EQ(a.history(), (Hist{100}));
    ZTH_EXPECT_EQ(b.history(), (Hist{50}));
}
ZTH_TEST("robust", "a transfer is recorded as a withdrawal for one account and a deposit for the other") {
    BankAccount a("Ada", 100), b("Bob");
    ZTH_EXPECT_TRUE(a.transfer_to(b, 40));
    ZTH_EXPECT_EQ(a.history(), (Hist{100, -40}));
    ZTH_EXPECT_EQ(b.history(), (Hist{40}));
}
ZTH_TEST("robust", "a transfer is all or nothing: if the receiver would overflow, neither account changes") {
    BankAccount a("Ada", 100), b("Bob", LLONG_MAX - 1);
    ZTH_EXPECT_THROWS(std::overflow_error, (void)a.transfer_to(b, 5));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
    ZTH_EXPECT_EQ(b.balance(), LLONG_MAX - 1);
    ZTH_EXPECT_EQ(a.history(), (Hist{100}));
    ZTH_EXPECT_EQ(b.history(), (Hist{LLONG_MAX - 1}));
}
ZTH_TEST("robust", "transferring to the same account changes nothing (but still needs the funds)") {
    BankAccount a("Ada", 100);
    ZTH_EXPECT_TRUE(a.transfer_to(a, 30));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
    ZTH_EXPECT_EQ(a.history(), (Hist{100}));
    ZTH_EXPECT_FALSE(a.transfer_to(a, 101));
    ZTH_EXPECT_EQ(a.balance(), 100LL);
}
ZTH_TEST("robust", "the invariants survive a long, messy sequence of operations") {
    BankAccount a("Ada", 1000), b("Bob", 10);
    zth::Lcg rng(12345);
    auto next = [&](long long mod) { return static_cast<long long>(rng.next() % static_cast<std::uint64_t>(mod)); };
    for (int i = 0; i < 2000; ++i) {
        BankAccount& from = next(2) ? a : b;
        BankAccount& to = (&from == &a) ? b : a;
        const long long amount = 1 + next(1500);
        switch (next(4)) {
            case 0: from.deposit(amount); break;
            case 1: (void)from.withdraw(amount); break;
            case 2: (void)from.transfer_to(to, amount); break;
            default: (void)from.transfer_to(from, amount); break;
        }
        if (!consistent(a) || !consistent(b)) {
            ::zth::fail("an invariant was broken after operation " + std::to_string(i) + " (a negative balance, or a balance that disagrees with the history)");
            return;
        }
    }
}
ZTH_TEST("robust", "transfers neither create nor destroy money") {
    BankAccount a("Ada", 700), b("Bob", 300);
    ZTH_EXPECT_TRUE(a.transfer_to(b, 250));
    ZTH_EXPECT_FALSE(b.transfer_to(a, 10000));
    ZTH_EXPECT_TRUE(b.transfer_to(a, 550));
    ZTH_EXPECT_EQ(a.balance() + b.balance(), 1000LL);
}
