// A demo of your library (read-only). Press Run to see what it prints.
#include <iostream>

#include "library.hpp"

int main() {
    lib::Library library;
    const int ada = library.add_member("Ada");
    const int bob = library.add_member("Bob");
    const int dune = library.add_book("Dune", "Frank Herbert");
    const int emma = library.add_book("Emma", "Jane Austen");

    std::cout << "ada borrows dune on day 1: " << (library.checkout(ada, dune, 1) ? "ok" : "refused") << "\n";
    std::cout << "bob borrows dune on day 2: " << (library.checkout(bob, dune, 2) ? "ok" : "refused") << "\n";
    std::cout << library.book(dune);
    std::cout << "\n";
    std::cout << "overdue on day 20: " << library.overdue(20).size() << " book(s)\n";

    const long long fine = library.return_book(dune, 20);
    std::cout << "ada returns dune on day 20: fine " << fine << " cents; she owes " << library.fines_owed(ada) << "\n";
    std::cout << "ada borrows emma while she owes money: " << (library.checkout(ada, emma, 20) ? "ok" : "refused") << "\n";
    library.pay_fine(ada, fine);
    std::cout << "after paying: " << (library.checkout(ada, emma, 20) ? "ok" : "refused") << "\n";
    std::cout << library.book(emma);
    std::cout << "\n";
}
