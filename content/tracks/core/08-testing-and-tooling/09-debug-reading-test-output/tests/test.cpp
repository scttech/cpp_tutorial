#include <climits>
#include <stdexcept>
#include <string>

#include "bank.hpp"
#include "zth_test.hpp"

// ================================================ Works =======================================================
ZTH_TEST("works", "a new account is empty and keeps its owner") {
    Account a("ada");
    ZTH_EXPECT_EQ(a.balance(), 0);
    ZTH_EXPECT_EQ(a.owner(), std::string("ada"));
}
ZTH_TEST("works", "deposit and withdraw change the balance") {
    Account a("ada");
    a.deposit(500);
    a.deposit(250);
    a.withdraw(200);
    ZTH_EXPECT_EQ(a.balance(), 550);
}
ZTH_TEST("works", "withdrawing exactly the balance is allowed and leaves zero") {
    Account a("ada");
    a.deposit(500);
    ZTH_EXPECT_NO_THROW(a.withdraw(500));
    ZTH_EXPECT_EQ(a.balance(), 0);
}
ZTH_TEST("works", "withdrawing more than the balance throws std::runtime_error and changes nothing") {
    Account a("ada");
    a.deposit(500);
    ZTH_EXPECT_THROWS(std::runtime_error, a.withdraw(501));
    ZTH_EXPECT_EQ(a.balance(), 500);
}
ZTH_TEST("works", "an amount that is zero or negative throws std::invalid_argument (deposit and withdraw)") {
    Account a("ada");
    a.deposit(10);
    ZTH_EXPECT_THROWS(std::invalid_argument, a.deposit(-5));
    ZTH_EXPECT_THROWS(std::invalid_argument, a.deposit(0));
    ZTH_EXPECT_THROWS(std::invalid_argument, a.withdraw(-5));
    ZTH_EXPECT_THROWS(std::invalid_argument, a.withdraw(0));
    ZTH_EXPECT_EQ(a.balance(), 10);
}
ZTH_TEST("works", "transfer_to moves the money from one account to the other") {
    Account a("ada");
    Account b("bob");
    a.deposit(300);
    a.transfer_to(b, 120);
    ZTH_EXPECT_EQ(a.balance(), 180);
    ZTH_EXPECT_EQ(b.balance(), 120);
}
ZTH_TEST("works", "every account has its own id, and they count up by one") {
    Account a("ada");
    Account b("bob");
    Account c("cy");
    ZTH_EXPECT_EQ(b.id(), a.id() + 1);
    ZTH_EXPECT_EQ(c.id(), b.id() + 1);
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "a deposit beyond INT_MAX throws std::overflow_error and leaves the account untouched") {
    Account a("ada");
    a.deposit(INT_MAX);
    ZTH_EXPECT_THROWS(std::overflow_error, a.deposit(1));
    ZTH_EXPECT_EQ(a.balance(), INT_MAX);
    ZTH_EXPECT_EQ(a.last_transaction().cents, INT_MAX);
}
ZTH_TEST("robust", "depositing into a frozen account throws std::runtime_error") {
    Account a("ada");
    a.freeze();
    ZTH_EXPECT_THROWS(std::runtime_error, a.deposit(5));
    ZTH_EXPECT_EQ(a.balance(), 0);
}
ZTH_TEST("robust", "last_transaction reports the newest entry; a new account has none") {
    Account a("ada");
    ZTH_EXPECT_THROWS(std::out_of_range, a.last_transaction());
    a.deposit(100);
    ZTH_EXPECT_EQ(a.last_transaction().kind, std::string("deposit"));
    a.withdraw(40);
    ZTH_EXPECT_EQ(a.last_transaction().kind, std::string("withdraw"));
    ZTH_EXPECT_EQ(a.last_transaction().cents, 40);
}
ZTH_TEST("robust", "a transaction that was read earlier stays valid while the account keeps growing (no dangling reference)") {
    Account a("ada");
    a.deposit(100);
    const auto& first = a.last_transaction();
    for (int i = 0; i < 20; ++i) a.deposit(1);
    ZTH_EXPECT_EQ(first.cents, 100);
    ZTH_EXPECT_EQ(first.kind, std::string("deposit"));
}
ZTH_TEST("robust", "last_transaction of two accounts do not share storage") {
    Account a("ada");
    Account b("bob");
    a.deposit(11);
    b.deposit(22);
    const auto& la = a.last_transaction();
    const auto& lb = b.last_transaction();
    ZTH_EXPECT_EQ(la.cents, 11);
    ZTH_EXPECT_EQ(lb.cents, 22);
}
ZTH_TEST("robust", "a failed transfer (frozen receiver) changes nothing: balances and histories are as before, and the exception reaches the caller") {
    Account a("ada");
    Account b("bob");
    a.deposit(100);
    a.withdraw(10);
    b.freeze();
    ZTH_EXPECT_THROWS(std::runtime_error, a.transfer_to(b, 30));
    ZTH_EXPECT_EQ(a.balance(), 90);
    ZTH_EXPECT_EQ(b.balance(), 0);
    ZTH_EXPECT_EQ(a.last_transaction().kind, std::string("withdraw"));
    ZTH_EXPECT_EQ(a.last_transaction().cents, 10);
}
ZTH_TEST("robust", "a transfer that fails at the first step (not enough money) changes nothing either") {
    Account a("ada");
    Account b("bob");
    a.deposit(50);
    ZTH_EXPECT_THROWS(std::runtime_error, a.transfer_to(b, 80));
    ZTH_EXPECT_EQ(a.balance(), 50);
    ZTH_EXPECT_EQ(b.balance(), 0);
}
ZTH_TEST("robust", "a transfer into an account that would overflow changes nothing (std::overflow_error)") {
    Account a("ada");
    Account b("bob");
    a.deposit(100);
    b.deposit(INT_MAX);
    ZTH_EXPECT_THROWS(std::overflow_error, a.transfer_to(b, 1));
    ZTH_EXPECT_EQ(a.balance(), 100);
    ZTH_EXPECT_EQ(b.balance(), INT_MAX);
    ZTH_EXPECT_EQ(a.last_transaction().kind, std::string("deposit"));
}
