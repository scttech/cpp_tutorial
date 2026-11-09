#include <gtest/gtest.h>

#include <climits>
#include <stdexcept>

#include "bank.hpp"

// The test suite of bank.hpp (read-only). Press Run: it prints GoogleTest's report. Your job is to READ it. Every FAILED test, and every
// sanitizer report, points at a bug in bank.hpp. Do not change this file: fix the code.

TEST(Account, StartsWithNoMoney) {
    Account a("ada");
    EXPECT_EQ(a.balance(), 0);
    EXPECT_EQ(a.owner(), "ada");
}

TEST(Account, DepositAddsToTheBalance) {
    Account a("ada");
    a.deposit(500);
    a.deposit(250);
    EXPECT_EQ(a.balance(), 750);
}

TEST(Account, WithdrawSubtractsFromTheBalance) {
    Account a("ada");
    a.deposit(500);
    a.withdraw(200);
    EXPECT_EQ(a.balance(), 300);
}

TEST(Account, WithdrawingEverythingLeavesZero) {
    Account a("ada");
    a.deposit(500);
    EXPECT_NO_THROW(a.withdraw(500));
    EXPECT_EQ(a.balance(), 0);
}

TEST(Account, WithdrawingMoreThanTheBalanceIsRefused) {
    Account a("ada");
    a.deposit(500);
    EXPECT_THROW(a.withdraw(501), std::runtime_error);
    EXPECT_EQ(a.balance(), 500);
}

TEST(Account, ADepositOfANegativeAmountIsRejected) {
    Account a("ada");
    EXPECT_THROW(a.deposit(-5), std::invalid_argument);
    EXPECT_EQ(a.balance(), 0);
}

TEST(Account, DepositingBeyondTheLimitIsReported) {
    Account a("ada");
    a.deposit(INT_MAX);
    EXPECT_THROW(a.deposit(1), std::overflow_error);
    EXPECT_EQ(a.balance(), INT_MAX);
}

TEST(Account, TheLastTransactionStaysValidWhileTheAccountGrows) {
    Account a("ada");
    a.deposit(100);
    const auto& first = a.last_transaction();
    a.deposit(50);
    a.deposit(25);
    EXPECT_EQ(first.kind, "deposit");
    EXPECT_EQ(first.cents, 100);
}

TEST(Account, ANewAccountHasNoLastTransaction) {
    Account a("ada");
    EXPECT_THROW(a.last_transaction(), std::out_of_range);
}

TEST(Account, TransferMovesTheMoney) {
    Account a("ada");
    Account b("bob");
    a.deposit(300);
    a.transfer_to(b, 120);
    EXPECT_EQ(a.balance(), 180);
    EXPECT_EQ(b.balance(), 120);
}

TEST(Account, AFailedTransferChangesNothing) {
    Account a("ada");
    Account b("bob");
    a.deposit(100);
    b.freeze();
    EXPECT_THROW(a.transfer_to(b, 30), std::runtime_error);
    EXPECT_EQ(a.balance(), 100);
    EXPECT_EQ(b.balance(), 0);
}

TEST(Account, EveryAccountHasItsOwnId) {
    Account a("ada");
    Account b("bob");
    EXPECT_NE(a.id(), b.id());
}
