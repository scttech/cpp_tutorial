#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// A tiny bank account. Money is counted in whole cents (an int). The rules:
//
//   Account(owner)           a new account: balance 0, an id that no other account has
//   deposit(cents)           cents <= 0: std::invalid_argument. A frozen account: std::runtime_error. A balance that would exceed INT_MAX:
//                            std::overflow_error (and nothing changes)
//   withdraw(cents)          cents <= 0: std::invalid_argument. More than the balance: std::runtime_error. Exactly the balance is allowed.
//   last_transaction()       the newest entry of the history (deposits and withdrawals); std::out_of_range if there is none
//   transfer_to(other, c)    withdraw from this account and deposit into `other`. ALL OR NOTHING: if anything fails, both accounts (balances
//                            AND histories) are exactly as they were before, and the exception reaches the caller.
//
// The test suite in bank_test.cpp finds six bugs here. Run it, read what it prints, fix them one by one.

struct Transaction {
    std::string kind;  // "deposit" or "withdraw"
    int cents;
};

class Account {
public:
    explicit Account(std::string owner) : id_(next_id_), owner_(std::move(owner)) {}

    int id() const { return id_; }
    const std::string& owner() const { return owner_; }
    int balance() const { return balance_; }
    void freeze() { frozen_ = true; }

    void deposit(int cents) {
        if (cents <= 0) throw std::logic_error("amount must be positive");
        if (frozen_) throw std::runtime_error("account is frozen");
        balance_ += cents;
        history_.push_back({"deposit", cents});
    }

    void withdraw(int cents) {
        if (cents <= 0) throw std::invalid_argument("amount must be positive");
        if (cents >= balance_) throw std::runtime_error("insufficient funds");
        balance_ -= cents;
        history_.push_back({"withdraw", cents});
    }

    const Transaction& last_transaction() const {
        if (history_.empty()) throw std::out_of_range("no transactions yet");
        return history_.back();
    }

    void transfer_to(Account& other, int cents) {
        withdraw(cents);
        other.deposit(cents);
    }

private:
    static inline int next_id_ = 1;

    int id_;
    std::string owner_;
    int balance_ = 0;
    bool frozen_ = false;
    std::vector<Transaction> history_;
};
