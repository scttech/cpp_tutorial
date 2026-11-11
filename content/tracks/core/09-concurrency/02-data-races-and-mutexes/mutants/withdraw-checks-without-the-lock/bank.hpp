// MUTANT: the balance is tested before the lock is taken (check-then-act).
#pragma once

#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

class Bank {
public:
    Bank(std::size_t accounts, long long initial) : balances_(accounts, initial), mutexes_(accounts) {}

    std::size_t size() const { return balances_.size(); }

    long long balance(std::size_t account) const {
        check(account);
        std::lock_guard lock(mutexes_[account]);
        return balances_[account];
    }

    void deposit(std::size_t account, long long amount) {
        check(account);
        positive(amount);
        std::lock_guard lock(mutexes_[account]);
        balances_[account] += amount;
    }

    bool withdraw(std::size_t account, long long amount) {
        check(account);
        positive(amount);
        if (balances_[account] < amount) return false;
        std::lock_guard lock(mutexes_[account]);
        balances_[account] -= amount;
        return true;
    }

    bool transfer(std::size_t from, std::size_t to, long long amount) {
        check(from);
        check(to);
        positive(amount);
        if (from == to) {
            std::lock_guard lock(mutexes_[from]);  // locking the same mutex twice would deadlock
            return balances_[from] >= amount;
        }
        std::scoped_lock lock(mutexes_[from], mutexes_[to]);  // takes both mutexes without risking a deadlock, whatever the order
        if (balances_[from] < amount) return false;
        balances_[from] -= amount;
        balances_[to] += amount;
        return true;
    }

    long long total() const {
        // Lock every account (always in the same order, lowest first) so that no transfer is half done while we add up.
        std::vector<std::unique_lock<std::mutex>> locks;
        locks.reserve(mutexes_.size());
        for (std::mutex& m : mutexes_) locks.emplace_back(m);
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
    mutable std::vector<std::mutex> mutexes_;  // one mutex per account
};
