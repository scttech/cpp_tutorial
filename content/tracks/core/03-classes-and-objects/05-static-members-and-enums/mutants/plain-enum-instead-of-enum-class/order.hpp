// MUTANT: Status is an unscoped enum: it converts to int silently.
#pragma once

#include <cstdint>
#include <string>

// The life of an order. `enum class`: the names live inside Status (Status::Paid, never a bare Paid), and a Status does
// not silently turn into an int. The underlying type is fixed, so it is exactly one byte everywhere.
enum Status : std::uint8_t { Created, Paid, Shipped, Delivered, Cancelled };

// A switch over an enum with NO `default`: if someone adds a Status and forgets this function, the compiler warns
// (-Wswitch). A `default:` would hide exactly that mistake.
inline std::string to_string(Status s) {
    switch (s) {
        case Status::Created: return "Created";
        case Status::Paid: return "Paid";
        case Status::Shipped: return "Shipped";
        case Status::Delivered: return "Delivered";
        case Status::Cancelled: return "Cancelled";
    }
    return "Unknown";  // not reachable for a valid Status; keeps "control reaches end of non-void function" quiet
}

class Order {
public:
    static constexpr int first_id = 1;  // a constant shared by every Order (no object needed to read it)

    Order() : id_(first_id + created_++) {}  // created_ is ONE counter for the whole class, not one per order

    int id() const { return id_; }
    Status status() const { return status_; }

    // Each returns true if the order moved, false (and nothing changes) if that step is not allowed from where it is.
    bool pay() { return move_to(Status::Paid); }
    bool ship() { return move_to(Status::Shipped); }
    bool deliver() { return move_to(Status::Delivered); }
    bool cancel() { return move_to(Status::Cancelled); }

    // The rules in ONE place. Static: it does not look at any particular order, so you can ask `Order::allowed(a, b)`.
    //   Created -> Paid | Cancelled      Paid -> Shipped | Cancelled      Shipped -> Delivered
    //   Delivered and Cancelled are final.
    static bool allowed(Status from, Status to) {
        switch (from) {
            case Status::Created: return to == Status::Paid || to == Status::Cancelled;
            case Status::Paid: return to == Status::Shipped || to == Status::Cancelled;
            case Status::Shipped: return to == Status::Delivered;
            case Status::Delivered:
            case Status::Cancelled: return false;
        }
        return false;
    }

    // How many orders have ever been created.
    static int created() { return created_; }

private:
    bool move_to(Status next) {
        if (!allowed(status_, next)) return false;
        status_ = next;
        return true;
    }

    // `inline static`: defined right here in the header. (A plain `static int created_;` would also need a definition
    // in exactly one .cpp file, or the program fails to link.)
    inline static int created_ = 0;

    int id_;
    Status status_ = Status::Created;
};
