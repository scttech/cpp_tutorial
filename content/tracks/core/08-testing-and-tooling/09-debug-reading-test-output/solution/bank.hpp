#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Transaction {
    std::string kind;  // "deposit" or "withdraw"
    int cents;
};

class Account {
public:
    explicit Account(std::string owner) : id_(next_id_++), owner_(std::move(owner)) {}

    int id() const { return id_; }
    const std::string& owner() const { return owner_; }
    int balance() const { return balance_; }
    void freeze() { frozen_ = true; }

    void deposit(int cents) {
        if (cents <= 0) throw std::invalid_argument("amount must be positive");
        if (frozen_) throw std::runtime_error("account is frozen");
        if (balance_ > std::numeric_limits<int>::max() - cents) throw std::overflow_error("balance limit reached");
        balance_ += cents;
        history_.push_back({"deposit", cents});
    }

    void withdraw(int cents) {
        if (cents <= 0) throw std::invalid_argument("amount must be positive");
        if (cents > balance_) throw std::runtime_error("insufficient funds");
        balance_ -= cents;
        history_.push_back({"withdraw", cents});
    }

    Transaction last_transaction() const {
        if (history_.empty()) throw std::out_of_range("no transactions yet");
        return history_.back();
    }

    void transfer_to(Account& other, int cents) {
        withdraw(cents);
        try {
            other.deposit(cents);
        } catch (...) {
            balance_ += cents;  // put the money back, and forget the withdrawal
            history_.pop_back();
            throw;
        }
    }

private:
    static inline int next_id_ = 1;

    int id_;
    std::string owner_;
    int balance_ = 0;
    bool frozen_ = false;
    std::vector<Transaction> history_;
};
