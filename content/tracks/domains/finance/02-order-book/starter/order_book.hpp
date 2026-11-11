#pragma once
// YOUR MATCHING ENGINE: a limit order book with price-time priority.
//
//   submit(order)   A LIMIT order (buy up to a price, or sell down to a price).
//                   - Rejected (nothing happens): qty <= 0, price <= 0, or an order with this id is already RESTING on the book.
//                   - A BUY executes against resting SELL orders whose price is <= the buy price (equal prices DO trade), lowest
//                     price first; among orders at the same price, the one that arrived first. A SELL executes against resting BUY
//                     orders with price >= its price, highest price first, then arrival order.
//                   - Every execution is a Trade at the RESTING order's price, for the smaller of the two remaining quantities.
//                   - Whatever is left of the incoming order rests on the book at its own price, behind the orders already there.
//                   - Result: the trades in the order they happened; status Filled (nothing left), Rested (something rests,
//                     `resting` = how much), or Rejected.
//   cancel(id)      remove a resting order; false if there is no such order (never existed, filled, or already cancelled).
//                   Must not search the book: use an index from id to the order.
//   best_bid(), best_ask()   the best price on each side, or nullopt. Levels that became empty must be gone.
//   depth_at(side, price)    the total resting quantity at that price on that side (0 if none).
//   remaining(id)            the resting quantity of an order, or nullopt.
//   order_count()            the number of resting orders.
//
// The book is never crossed after a submit: best_bid < best_ask. An id can be reused once its order is no longer resting.
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace ob {

class OrderBook {
public:
    struct Result {
        std::vector<Trade> trades;
        Status status = Status::Rejected;
        std::int64_t resting = 0;
    };

    Result submit(Order order) {
        (void)order;
        return {};  // TODO
    }
    bool cancel(OrderId id) {
        (void)id;
        return false;  // TODO
    }
    std::optional<Price> best_bid() const { return std::nullopt; }  // TODO
    std::optional<Price> best_ask() const { return std::nullopt; }  // TODO
    std::int64_t depth_at(Side side, Price price) const {
        (void)side;
        (void)price;
        return 0;  // TODO
    }
    std::optional<std::int64_t> remaining(OrderId id) const {
        (void)id;
        return std::nullopt;  // TODO
    }
    std::size_t order_count() const { return 0; }  // TODO

private:
    // TODO: for each side a std::map<Price, std::list<Order>> (the lists keep arrival order, and their iterators stay valid),
    // and a std::unordered_map<OrderId, ...> to find an order by id without searching.
};

}  // namespace ob
