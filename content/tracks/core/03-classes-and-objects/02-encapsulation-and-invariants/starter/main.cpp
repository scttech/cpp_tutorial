// A small demo of your BankAccount (read-only). Press Run to see what it prints.
#include <iostream>
#include <stdexcept>

#include "account.hpp"

void show(BankAccount& a) {
    std::cout << a.owner() << ": " << a.balance() << " cents, " << a.history().size() << " transactions\n";
}

int main() {
    BankAccount ada("Ada", 10000);
    BankAccount bob("Bob");

    ada.deposit(2500);
    std::cout << "withdraw 50000: " << (ada.withdraw(50000) ? "ok" : "refused") << "\n";
    std::cout << "transfer 4000 to Bob: " << (ada.transfer_to(bob, 4000) ? "ok" : "refused") << "\n";
    show(ada);
    show(bob);

    try {
        ada.deposit(-5);
        std::cout << "a negative deposit was accepted\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "rejected: " << e.what() << "\n";
    }

    // In your finished class the next two lines must NOT compile (try them!): the data is private and the getters are
    // read-only, so nobody can bypass the rules.
    //   ada.balance() = -1000000;
    //   ada.history().clear();
}
