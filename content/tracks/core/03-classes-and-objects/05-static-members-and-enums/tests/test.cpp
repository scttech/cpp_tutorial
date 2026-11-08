#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include "order.hpp"
#include "zth_test.hpp"

namespace {

// ---- the shape of the class, as templates: a missing `static` or constant is a clear failure, not a compile error here
template <class O> constexpr bool created_is_static = requires { { O::created() } -> std::convertible_to<int>; };
template <class O> constexpr bool allowed_is_static = requires { { O::allowed(Status::Created, Status::Paid) } -> std::convertible_to<bool>; };
template <class O> constexpr bool has_first_id = requires { { O::first_id } -> std::convertible_to<int>; };
template <class O> constexpr bool readable_when_const = requires(const O& o) { o.id(); o.status(); };

// The rules, written out independently of the learner's code.
bool model_allowed(Status from, Status to) {
    switch (from) {
        case Status::Created: return to == Status::Paid || to == Status::Cancelled;
        case Status::Paid: return to == Status::Shipped || to == Status::Cancelled;
        case Status::Shipped: return to == Status::Delivered;
        default: return false;
    }
}

const Status kAll[] = {Status::Created, Status::Paid, Status::Shipped, Status::Delivered, Status::Cancelled};

// An order brought to the given status through legal steps.
Order make(Status target) {
    Order o;
    switch (target) {
        case Status::Created: break;
        case Status::Paid: o.pay(); break;
        case Status::Shipped: o.pay(); o.ship(); break;
        case Status::Delivered: o.pay(); o.ship(); o.deliver(); break;
        case Status::Cancelled: o.cancel(); break;
    }
    return o;
}

bool step(Order& o, int action) {
    switch (action) {
        case 0: return o.pay();
        case 1: return o.ship();
        case 2: return o.deliver();
        default: return o.cancel();
    }
}
Status target_of(int action) {
    switch (action) {
        case 0: return Status::Paid;
        case 1: return Status::Shipped;
        case 2: return Status::Delivered;
        default: return Status::Cancelled;
    }
}

template <class O>
void created_test() {
    if constexpr (!created_is_static<O>) {
        ::zth::fail("created() must be a static member function, so that `Order::created()` works without an order");
    } else {
        const int before = O::created();
        O a;
        ZTH_EXPECT_EQ(O::created(), before + 1);
        O b;
        O c;
        ZTH_EXPECT_EQ(O::created(), before + 3);
        // asking, or changing an order, does not count as creating one
        a.pay();
        a.ship();
        ZTH_EXPECT_EQ(O::created(), before + 3);
    }
}

template <class O>
void ids_test() {
    if constexpr (!created_is_static<O>) {
        ::zth::fail("created() must be a static member function");
    } else if constexpr (!has_first_id<O>) {
        ::zth::fail("add the shared constant `static constexpr int first_id = 1;` to Order");
    } else {
        const int before = O::created();
        O a;
        O b;
        O c;
        ZTH_EXPECT_EQ(a.id(), O::first_id + before);
        ZTH_EXPECT_EQ(b.id(), a.id() + 1);
        ZTH_EXPECT_EQ(c.id(), b.id() + 1);
        ZTH_EXPECT_EQ(O::first_id, 1);
    }
}

template <class O>
void many_ids_test() {
    if constexpr (!created_is_static<O>) {
        ::zth::fail("created() must be a static member function");
    } else {
        const int before = O::created();
        std::vector<int> ids;
        for (int i = 0; i < 50; ++i) {
            O o;
            ids.push_back(o.id());
        }
        for (std::size_t i = 1; i < ids.size(); ++i) ZTH_EXPECT_EQ(ids[i], ids[i - 1] + 1);
        ZTH_EXPECT_TRUE(O::created() >= before + 50);
    }
}

template <class O>
void allowed_table_test() {
    if constexpr (!allowed_is_static<O>) {
        ::zth::fail("add `static bool allowed(Status from, Status to)` to Order (the rules in one place; static, so no order is needed)");
    } else {
        for (Status from : kAll)
            for (Status to : kAll)
                if (O::allowed(from, to) != model_allowed(from, to)) {
                    ::zth::fail("Order::allowed(" + to_string(from) + ", " + to_string(to) + ") should be " +
                                (model_allowed(from, to) ? "true" : "false"));
                    return;
                }
    }
}

template <class O>
void const_test() {
    if constexpr (!readable_when_const<O>) {
        ::zth::fail("id() and status() must be const member functions");
    } else {
        O o;
        o.pay();
        const O& c = o;
        ZTH_EXPECT_TRUE(c.status() == Status::Paid);
        ZTH_EXPECT_TRUE(c.id() >= 0);
    }
}

}  // namespace

// ---- Works ---------------------------------------------------------------------------------------
ZTH_TEST("works", "to_string gives each status its name") {
    ZTH_EXPECT_EQ(to_string(Status::Created), std::string("Created"));
    ZTH_EXPECT_EQ(to_string(Status::Paid), std::string("Paid"));
    ZTH_EXPECT_EQ(to_string(Status::Shipped), std::string("Shipped"));
    ZTH_EXPECT_EQ(to_string(Status::Delivered), std::string("Delivered"));
    ZTH_EXPECT_EQ(to_string(Status::Cancelled), std::string("Cancelled"));
}
ZTH_TEST("works", "a new order is Created") {
    Order o;
    ZTH_EXPECT_TRUE(o.status() == Status::Created);
}
ZTH_TEST("works", "the happy path: pay, ship, deliver") {
    Order o;
    ZTH_EXPECT_TRUE(o.pay());
    ZTH_EXPECT_TRUE(o.status() == Status::Paid);
    ZTH_EXPECT_TRUE(o.ship());
    ZTH_EXPECT_TRUE(o.status() == Status::Shipped);
    ZTH_EXPECT_TRUE(o.deliver());
    ZTH_EXPECT_TRUE(o.status() == Status::Delivered);
}
ZTH_TEST("works", "an order can be cancelled before it ships") {
    Order a;
    ZTH_EXPECT_TRUE(a.cancel());
    ZTH_EXPECT_TRUE(a.status() == Status::Cancelled);
    Order b;
    b.pay();
    ZTH_EXPECT_TRUE(b.cancel());
    ZTH_EXPECT_TRUE(b.status() == Status::Cancelled);
}
ZTH_TEST("works", "a step that is not allowed returns false and changes nothing") {
    Order o;
    ZTH_EXPECT_FALSE(o.ship());
    ZTH_EXPECT_FALSE(o.deliver());
    ZTH_EXPECT_TRUE(o.status() == Status::Created);
    o.pay();
    ZTH_EXPECT_FALSE(o.pay());
    ZTH_EXPECT_FALSE(o.deliver());
    ZTH_EXPECT_TRUE(o.status() == Status::Paid);
}
ZTH_TEST("works", "every order gets its own id, from one shared counter") { ids_test<Order>(); }
ZTH_TEST("works", "Order::created() (a static function) counts the orders ever made, and steps or failed steps do not count") { created_test<Order>(); }

// ---- Robust --------------------------------------------------------------------------------------
ZTH_TEST("robust", "Status is a scoped enum (no silent conversion to int) with a one-byte underlying type") {
    ZTH_EXPECT_FALSE((std::is_convertible_v<Status, int>));
    ZTH_EXPECT_TRUE((std::is_same_v<std::underlying_type_t<Status>, std::uint8_t>));
    ZTH_EXPECT_EQ(sizeof(Status), std::size_t{1});
    ZTH_EXPECT_EQ(static_cast<int>(Status::Created), 0);
    ZTH_EXPECT_EQ(static_cast<int>(Status::Cancelled), 4);
}
ZTH_TEST("robust", "from every status, every step does exactly what the rules say") {
    for (Status from : kAll) {
        for (int action = 0; action < 4; ++action) {
            Order o = make(from);
            const bool ok = step(o, action);
            const bool expected = model_allowed(from, target_of(action));
            if (ok != expected) {
                ::zth::fail("from " + to_string(from) + ", the step to " + to_string(target_of(action)) + " should return " +
                            (expected ? "true" : "false"));
                return;
            }
            const Status want = expected ? target_of(action) : from;
            if (o.status() != want) {
                ::zth::fail("from " + to_string(from) + ", the step to " + to_string(target_of(action)) + " should leave the order " +
                            to_string(want) + " but it is " + to_string(o.status()));
                return;
            }
        }
    }
}
ZTH_TEST("robust", "Delivered and Cancelled are final") {
    for (Status end : {Status::Delivered, Status::Cancelled}) {
        Order o = make(end);
        ZTH_EXPECT_FALSE(o.pay());
        ZTH_EXPECT_FALSE(o.ship());
        ZTH_EXPECT_FALSE(o.deliver());
        ZTH_EXPECT_FALSE(o.cancel());
        ZTH_EXPECT_TRUE(o.status() == end);
    }
}
ZTH_TEST("robust", "Order::allowed(from, to) is static and gives the whole transition table") { allowed_table_test<Order>(); }
ZTH_TEST("robust", "ids keep counting up for many orders made one at a time") { many_ids_test<Order>(); }
ZTH_TEST("robust", "id() and status() work on a const order") { const_test<Order>(); }
ZTH_TEST("robust", "a long random sequence of steps always follows the rules") {
    zth::Lcg rng(2024);
    auto next = [&] { return rng.next(); };
    for (int round = 0; round < 200; ++round) {
        Order o;
        Status model = Status::Created;
        for (int i = 0; i < 8; ++i) {
            const int action = static_cast<int>(next() % 4);
            const bool expected = model_allowed(model, target_of(action));
            ZTH_EXPECT_EQ(step(o, action), expected);
            if (expected) model = target_of(action);
            ZTH_EXPECT_TRUE(o.status() == model);
        }
    }
}
