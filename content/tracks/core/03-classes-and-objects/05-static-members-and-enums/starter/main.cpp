// A demo of your Order (read-only). Press Run to see what it prints.
#include <iostream>

#include "order.hpp"

void show(Order& o) { std::cout << "order " << o.id() << ": " << to_string(o.status()) << "\n"; }

int main() {
    Order a;
    Order b;
    show(a);
    show(b);

    std::cout << "ship before paying: " << (a.ship() ? "ok" : "refused") << "\n";
    std::cout << "pay: " << (a.pay() ? "ok" : "refused") << "\n";
    std::cout << "pay again: " << (a.pay() ? "ok" : "refused") << "\n";
    std::cout << "ship: " << (a.ship() ? "ok" : "refused") << "\n";
    std::cout << "cancel after shipping: " << (a.cancel() ? "ok" : "refused") << "\n";
    show(a);

    // Once your class is finished, these work too (try them):
    //   std::cout << Order::created() << " orders so far\n";
    //   std::cout << Order::allowed(Status::Paid, Status::Shipped) << "\n";
    //   Status s = Status::Paid;   // `Paid` on its own, and `int n = s;`, are errors
}
