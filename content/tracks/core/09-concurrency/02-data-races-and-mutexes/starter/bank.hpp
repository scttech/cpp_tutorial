#pragma once

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

// A bank with a fixed number of accounts. Amounts are whole cents in a long long. THE STARTER IS NOT THREAD-SAFE: it works with one thread, and
// breaks when several threads use it at once. Make it safe, and keep it deadlock-free.
//
//   Bank(accounts, initial)       every account starts with `initial`
//   size()                        the number of accounts
//   balance(i)                    std::out_of_range if there is no account i
//   deposit(i, amount)            amount <= 0: std::invalid_argument
//   withdraw(i, amount)           false (and no change) if there is not enough money; amount <= 0: std::invalid_argument
//   transfer(from, to, amount)    all or nothing: false (and no change) if `from` has not enough money. from == to is allowed: true or false
//                                 as if the money moved, but nothing changes. Unknown account: std::out_of_range. amount <= 0: std::invalid_argument
//   total()                       the sum of ALL the balances, taken at ONE moment: while transfers run, it must always equal the money the bank started with
//
// Threads call everything at the same time: two transfers in OPPOSITE directions between the same two accounts must not deadlock, and total() must
// never see money "in flight" (taken from one account, not yet added to the other).
class Bank {
public:
    Bank(std::size_t accounts, long long initial) : balances_(accounts, initial) {}

    std::size_t size() const { return balances_.size(); }

    long long balance(std::size_t account) const {
        check(account);
        return balances_[account];
    }

    void deposit(std::size_t account, long long amount) {
        check(account);
        positive(amount);
        balances_[account] += amount;
    }

    bool withdraw(std::size_t account, long long amount) {
        check(account);
        positive(amount);
        if (balances_[account] < amount) return false;
        balances_[account] -= amount;
        return true;
    }

    bool transfer(std::size_t from, std::size_t to, long long amount) {
        check(from);
        check(to);
        positive(amount);
        if (balances_[from] < amount) return false;
        if (from == to) return true;
        balances_[from] -= amount;
        balances_[to] += amount;
        return true;
    }

    long long total() const {
        long long sum = 0;
        for (long long b : balances_) sum += b;
        return sum;
    }

private:
    void check(std::size_t account) const {
        if (account >= balances_.size()) throw std::out_of_range("no such account");
    }
    static void positive(long long amount) {
        if (amount <= 0) throw std::invalid_argument("amount must be positive");
    }

    std::vector<long long> balances_;
    // TODO: the mutex (or mutexes) that protect the balances. mutable, so that const functions such as balance() and total() can lock.
};
