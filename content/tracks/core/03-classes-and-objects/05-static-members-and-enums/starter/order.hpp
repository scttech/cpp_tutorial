#pragma once

#include <cstdint>
#include <string>

// TODO: this is a plain (unscoped) enum: its names leak into the surrounding scope (a bare `Paid`), and it converts to
// int silently. Make it `enum class Status : std::uint8_t` (a scoped enum with a fixed one-byte underlying type).
enum Status { Created, Paid, Shipped, Delivered, Cancelled };

// (Already written. Notice that there is no `default:`: with an `enum class` the compiler warns if a case is missing.)
inline std::string to_string(Status s) {
    switch (s) {
        case Created: return "Created";
        case Paid: return "Paid";
        case Shipped: return "Shipped";
        case Delivered: return "Delivered";
        case Cancelled: return "Cancelled";
    }
    return "Unknown";
}

class Order {
public:
    // TODO: a constant shared by the whole class: `static constexpr int first_id = 1;`

    // TODO: every order gets the next id (first_id, first_id + 1, ...) from ONE counter shared by all orders.
    // Make created_ a `static` member (`inline static int created_ = 0;`) and use it here.
    Order() {}

    int id() const { return id_; }
    Status status() const { return status_; }

    // TODO: each step may only be taken from the right state, and returns false (changing nothing) otherwise.
    // Put the rules in ONE function, `allowed(from, to)`, and have move_to() ask it.
    //   Created -> Paid | Cancelled      Paid -> Shipped | Cancelled      Shipped -> Delivered
    //   Delivered and Cancelled are final.
    bool pay() { return move_to(Paid); }
    bool ship() { return move_to(Shipped); }
    bool deliver() { return move_to(Delivered); }
    bool cancel() { return move_to(Cancelled); }

    // TODO: `static bool allowed(Status from, Status to)`: the transition rules, callable as Order::allowed(a, b).

    // TODO: `static int created()`: how many orders have ever been created (it must be static: no order needed to ask).
    int created() const { return 0; }

private:
    bool move_to(Status next) {
        status_ = next;
        return true;
    }

    int created_ = 0;  // TODO: one counter for the whole class, not one per order
    int id_ = 0;
    Status status_ = Created;
};
