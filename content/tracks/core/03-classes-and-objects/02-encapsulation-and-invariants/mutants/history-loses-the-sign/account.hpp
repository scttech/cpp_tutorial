// MUTANT: records withdrawals as positive numbers.
#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A bank account whose invariants hold for the whole life of every object:
//   1. the owner's name is never empty
//   2. the balance is never negative
//   3. balance() is always the sum of history()    (the account keeps its own record: nobody else may edit it)
//   4. an operation that fails changes nothing
// All the data is private, and every way of changing it goes through code that keeps these true.
class BankAccount {
public:
    explicit BankAccount(std::string owner, long long opening_balance = 0) : owner_(std::move(owner)) {
        if (owner_.empty()) throw std::invalid_argument("BankAccount: the owner's name is empty");
        if (opening_balance < 0) throw std::invalid_argument("BankAccount: the opening balance is negative");
        if (opening_balance > 0) apply(opening_balance);  // the opening balance is the first history entry
    }

    // Read-only views. `const` member functions, and they return values or const references: a caller can look but
    // not touch (a `long long&` or a non-const `vector&` would let anyone break the invariants).
    const std::string& owner() const { return owner_; }
    long long balance() const { return balance_; }
    const std::vector<long long>& history() const { return history_; }

    void deposit(long long cents) {
        require_positive(cents);
        require_room(cents);
        apply(cents);
    }

    // false (and nothing changes) if there is not enough money
    bool withdraw(long long cents) {
        require_positive(cents);
        if (cents > balance_) return false;
        apply(-cents);
        return true;
    }

    // All or nothing: either both accounts change or neither does.
    bool transfer_to(BankAccount& to, long long cents) {
        require_positive(cents);
        if (cents > balance_) return false;
        if (&to == this) return true;  // moving money to yourself changes nothing (but you still need the funds)
        to.require_room(cents);        // check everything BEFORE changing anything
        apply(-cents);
        to.apply(cents);
        return true;
    }

private:
    static void require_positive(long long cents) {
        if (cents <= 0) throw std::invalid_argument("BankAccount: the amount must be positive");
    }

    void require_room(long long cents) const {
        if (balance_ > std::numeric_limits<long long>::max() - cents) {
            throw std::overflow_error("BankAccount: the balance would overflow");
        }
    }

    // The ONLY place that changes the balance, so the history can never disagree with it.
    void apply(long long delta) {
        history_.push_back(delta < 0 ? -delta : delta);
        balance_ += delta;
    }

    std::string owner_;
    long long balance_ = 0;
    std::vector<long long> history_;
};
