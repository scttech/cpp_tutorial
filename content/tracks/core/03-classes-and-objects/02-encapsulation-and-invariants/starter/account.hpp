#pragma once

#include <string>
#include <vector>

// A bank account. As written it has NO protection: anyone can set the balance to -1000000, rename the account to "",
// or erase the history. Your job is to make the rules below true for every BankAccount, always:
//   1. the owner's name is never empty
//   2. the balance is never negative
//   3. balance() is always the sum of history()  (deposits count +, withdrawals -; an opening balance is the first entry)
//   4. an operation that fails (throws, or returns false) changes nothing
class BankAccount {
public:
    // TODO: this is all public. Make the data private so that only the member functions can change it.
    std::string owner_;
    long long balance_;
    std::vector<long long> history_;

    // TODO: reject an empty owner and a negative opening balance (std::invalid_argument).
    // Record a non-zero opening balance as the first history entry.
    BankAccount(std::string owner, long long opening_balance = 0) : owner_(owner), balance_(opening_balance) {}

    // TODO: these hand out mutable references to the account's own data and are not const. Return a const reference
    // or a copy, and make them const member functions so they work on a const BankAccount too.
    std::string& owner() { return owner_; }
    long long& balance() { return balance_; }
    std::vector<long long>& history() { return history_; }

    // TODO: cents must be positive (else std::invalid_argument). Throw std::overflow_error if the balance would overflow.
    // Record the deposit in the history.
    void deposit(long long cents) { balance_ += cents; }

    // TODO: cents must be positive (else std::invalid_argument). Return false, changing nothing, if there is not
    // enough money. Record the withdrawal as a negative history entry.
    bool withdraw(long long cents) {
        balance_ -= cents;
        return true;
    }

    // TODO: move money to another account, all or nothing: if the withdrawal would fail, or the receiving account
    // would overflow, NEITHER account changes. Moving money to yourself changes nothing.
    bool transfer_to(BankAccount& to, long long cents) {
        balance_ -= cents;
        to.balance_ += cents;
        return true;
    }
};
