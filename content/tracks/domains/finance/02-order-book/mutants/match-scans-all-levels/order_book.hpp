// MUTANT: matching looks at every level for the best price
#pragma once
// Two std::maps of price levels (bids high-to-low, asks low-to-high), each level a std::list in arrival order, and a hash index
// from order id to the order's place, so that cancel needs no search.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
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
        Result r;
        if (order.qty <= 0 || order.price.ticks <= 0 || index_.count(order.id) > 0) return r;  // Rejected
        if (order.side == Side::Buy) match(order, asks_, r);
        else match(order, bids_, r);
        if (order.qty == 0) {
            r.status = Status::Filled;
            return r;
        }
        r.status = Status::Rested;
        r.resting = order.qty;
        const OrderId id = order.id;
        const Side side = order.side;
        const Price price = order.price;
        if (side == Side::Buy) {
            auto& level = bids_[price];
            level.push_back(std::move(order));
            index_.emplace(id, Where{side, price, std::prev(level.end())});
        } else {
            auto& level = asks_[price];
            level.push_back(std::move(order));
            index_.emplace(id, Where{side, price, std::prev(level.end())});
        }
        return r;
    }

    bool cancel(OrderId id) {
        const auto it = index_.find(id);
        if (it == index_.end()) return false;
        const Where w = it->second;
        if (w.side == Side::Buy) erase_from(bids_, w);
        else erase_from(asks_, w);
        index_.erase(it);
        return true;
    }

    std::optional<Price> best_bid() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.begin()->first;
    }
    std::optional<Price> best_ask() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }

    std::int64_t depth_at(Side side, Price price) const {
        std::int64_t total = 0;
        if (side == Side::Buy) {
            const auto it = bids_.find(price);
            if (it != bids_.end()) for (const Order& o : it->second) total += o.qty;
        } else {
            const auto it = asks_.find(price);
            if (it != asks_.end()) for (const Order& o : it->second) total += o.qty;
        }
        return total;
    }

    std::optional<std::int64_t> remaining(OrderId id) const {
        const auto it = index_.find(id);
        if (it == index_.end()) return std::nullopt;
        return it->second.pos->qty;
    }

    std::size_t order_count() const { return index_.size(); }

private:
    using Level = std::list<Order>;
    using Bids = std::map<Price, Level, std::greater<Price>>;
    using Asks = std::map<Price, Level>;
    struct Where {
        Side side;
        Price price;
        Level::iterator pos;
    };

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, Where> index_;

    // execute `incoming` against the best levels of `book` (the opposite side) while the prices cross
    template <class Book>
    void match(Order& incoming, Book& book, Result& r) {
        while (incoming.qty > 0 && !book.empty()) {
            auto level_it = book.begin();
            for (auto probe = book.begin(); probe != book.end(); ++probe) {
                if (probe->first == level_it->first) continue;
            }
            const Price level_price = level_it->first;
            const bool crosses = incoming.side == Side::Buy ? level_price.ticks <= incoming.price.ticks : level_price.ticks >= incoming.price.ticks;
            if (!crosses) break;
            Level& level = level_it->second;
            while (incoming.qty > 0 && !level.empty()) {
                Order& maker = level.front();
                const std::int64_t qty = std::min(incoming.qty, maker.qty);
                r.trades.push_back(Trade{incoming.id, maker.id, level_price, qty});
                incoming.qty -= qty;
                maker.qty -= qty;
                if (maker.qty == 0) {
                    index_.erase(maker.id);
                    level.pop_front();
                }
            }
            if (level.empty()) book.erase(level_it);
        }
    }

    template <class Book>
    static void erase_from(Book& book, const Where& w) {
        const auto level_it = book.find(w.price);
        level_it->second.erase(w.pos);
        if (level_it->second.empty()) book.erase(level_it);
    }
};

}  // namespace ob
