#pragma once
// READ-ONLY. Prices, ids and orders, with counters so that the tests can see how much work the book does.
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace ob {

namespace stats {
inline unsigned long long price_compares = 0;  // comparisons of two Prices
inline unsigned long long id_compares = 0;     // comparisons of two OrderIds
inline unsigned long long order_copies = 0;    // Orders COPIED (moves are free)
inline void reset() { price_compares = id_compares = order_copies = 0; }
}  // namespace stats

// A price in ticks (say 1 tick = 1 cent). Every comparison is counted.
struct Price {
    std::int64_t ticks = 0;
    friend bool operator==(const Price& a, const Price& b) {
        ++stats::price_compares;
        return a.ticks == b.ticks;
    }
    friend std::strong_ordering operator<=>(const Price& a, const Price& b) {
        ++stats::price_compares;
        return a.ticks <=> b.ticks;
    }
};

// The id of an order. Every comparison is counted; there is a std::hash so it can be a key of an unordered_map.
struct OrderId {
    std::uint64_t value = 0;
    friend bool operator==(const OrderId& a, const OrderId& b) {
        ++stats::id_compares;
        return a.value == b.value;
    }
};

}  // namespace ob

template <>
struct std::hash<ob::OrderId> {
    std::size_t operator()(const ob::OrderId& id) const noexcept { return std::hash<std::uint64_t>{}(id.value); }
};

namespace ob {

enum class Side { Buy, Sell };

struct Order {
    OrderId id;
    Side side = Side::Buy;
    Price price;
    std::int64_t qty = 0;  // how many units
    Order() = default;
    Order(OrderId i, Side s, Price p, std::int64_t q) : id(i), side(s), price(p), qty(q) {}
    Order(const Order& o) : id(o.id), side(o.side), price(o.price), qty(o.qty) { ++stats::order_copies; }
    Order(Order&&) noexcept = default;
    Order& operator=(const Order& o) {
        id = o.id;
        side = o.side;
        price = o.price;
        qty = o.qty;
        ++stats::order_copies;
        return *this;
    }
    Order& operator=(Order&&) noexcept = default;
};

// One execution: the incoming ("taker") order met a resting ("maker") order.
struct Trade {
    OrderId taker;
    OrderId maker;
    Price price;       // always the price of the RESTING order
    std::int64_t qty;
};

enum class Status {
    Rested,    // accepted; some quantity (possibly all of it) now rests on the book
    Filled,    // accepted; completely executed, nothing rests
    Rejected,  // not accepted: nothing happened
};

}  // namespace ob
