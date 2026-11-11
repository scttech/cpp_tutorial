#define ZTH_TRACK_ALLOCATIONS
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "order_book.hpp"
#include "zth_test.hpp"

using namespace ob;

namespace {

Order O(unsigned long long id, Side side, long long price, long long qty) { return Order{OrderId{id}, side, Price{price}, qty}; }
Order B(unsigned long long id, long long price, long long qty) { return O(id, Side::Buy, price, qty); }
Order S(unsigned long long id, long long price, long long qty) { return O(id, Side::Sell, price, qty); }

std::string show(const std::vector<Trade>& trades) {
    std::string s;
    for (const Trade& t : trades) s += "[" + std::to_string(t.taker.value) + "x" + std::to_string(t.maker.value) + " " + std::to_string(t.qty) + "@" + std::to_string(t.price.ticks) + "]";
    return s;
}

// The reference: a flat vector of resting orders, scanned for every operation. Slow and obviously right.
struct Naive {
    std::vector<Order> resting;  // in arrival order
    struct Out {
        std::vector<Trade> trades;
        Status status;
        std::int64_t rest;
    };
    Out submit(Order o) {
        Out out{{}, Status::Rejected, 0};
        if (o.qty <= 0 || o.price.ticks <= 0) return out;
        for (const Order& r : resting) if (r.id.value == o.id.value) return out;
        while (o.qty > 0) {
            int best = -1;
            for (std::size_t i = 0; i < resting.size(); ++i) {
                const Order& r = resting[i];
                if (r.side == o.side) continue;
                const bool crosses = o.side == Side::Buy ? r.price.ticks <= o.price.ticks : r.price.ticks >= o.price.ticks;
                if (!crosses) continue;
                if (best < 0) {
                    best = static_cast<int>(i);
                    continue;
                }
                const Order& b = resting[static_cast<std::size_t>(best)];
                const bool better = o.side == Side::Buy ? r.price.ticks < b.price.ticks : r.price.ticks > b.price.ticks;
                if (better) best = static_cast<int>(i);  // the first in arrival order wins ties because we only replace on strictly better
            }
            if (best < 0) break;
            Order& m = resting[static_cast<std::size_t>(best)];
            const std::int64_t q = std::min(o.qty, m.qty);
            out.trades.push_back(Trade{o.id, m.id, m.price, q});
            o.qty -= q;
            m.qty -= q;
            if (m.qty == 0) resting.erase(resting.begin() + best);
        }
        if (o.qty == 0) {
            out.status = Status::Filled;
        } else {
            out.status = Status::Rested;
            out.rest = o.qty;
            resting.push_back(o);
        }
        return out;
    }
    bool cancel(unsigned long long id) {
        for (std::size_t i = 0; i < resting.size(); ++i) {
            if (resting[i].id.value == id) {
                resting.erase(resting.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }
    std::optional<long long> best(Side side) const {
        std::optional<long long> b;
        for (const Order& r : resting) {
            if (r.side != side) continue;
            if (!b || (side == Side::Buy ? r.price.ticks > *b : r.price.ticks < *b)) b = r.price.ticks;
        }
        return b;
    }
    long long depth(Side side, long long price) const {
        long long d = 0;
        for (const Order& r : resting) if (r.side == side && r.price.ticks == price) d += r.qty;
        return d;
    }
};

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "an empty book; a non-crossing order rests") {
    OrderBook book;
    ZTH_EXPECT_FALSE(book.best_bid().has_value());
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{0});
    const auto r = book.submit(B(1, 100, 10));
    ZTH_EXPECT_TRUE(r.status == Status::Rested);
    ZTH_EXPECT_TRUE(r.trades.empty());
    ZTH_EXPECT_EQ(r.resting, 10LL);
    ZTH_EXPECT_EQ(book.best_bid()->ticks, 100LL);
    ZTH_EXPECT_EQ(book.depth_at(Side::Buy, Price{100}), 10LL);
    ZTH_EXPECT_EQ(*book.remaining(OrderId{1}), 10LL);
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{1});
    book.submit(S(2, 105, 4));
    ZTH_EXPECT_EQ(book.best_ask()->ticks, 105LL);
}
ZTH_TEST("works", "a crossing order trades at the resting order's price") {
    OrderBook book;
    book.submit(S(1, 100, 10));
    const auto r = book.submit(B(2, 105, 10));   // willing to pay 105, pays 100
    ZTH_EXPECT_TRUE(r.status == Status::Filled);
    ZTH_EXPECT_EQ(show(r.trades), std::string("[2x1 10@100]"));
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{0});
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
    OrderBook book2;
    book2.submit(B(1, 100, 10));
    const auto r2 = book2.submit(S(2, 95, 10));  // sells at 95, gets 100
    ZTH_EXPECT_EQ(show(r2.trades), std::string("[2x1 10@100]"));
}
ZTH_TEST("works", "equal prices trade") {
    OrderBook book;
    book.submit(S(1, 100, 5));
    ZTH_EXPECT_EQ(show(book.submit(B(2, 100, 5)).trades), std::string("[2x1 5@100]"));
    book.submit(B(3, 90, 5));
    ZTH_EXPECT_EQ(show(book.submit(S(4, 90, 5)).trades), std::string("[4x3 5@90]"));
}
ZTH_TEST("works", "price priority: the best price is taken first, whenever it arrived") {
    OrderBook book;
    book.submit(S(1, 103, 10));
    book.submit(S(2, 101, 10));
    book.submit(S(3, 102, 10));
    ZTH_EXPECT_EQ(show(book.submit(B(4, 103, 25)).trades), std::string("[4x2 10@101][4x3 10@102][4x1 5@103]"));
    OrderBook bids;
    bids.submit(B(1, 97, 10));
    bids.submit(B(2, 99, 10));
    bids.submit(B(3, 98, 10));
    ZTH_EXPECT_EQ(show(bids.submit(S(4, 97, 25)).trades), std::string("[4x2 10@99][4x3 10@98][4x1 5@97]"));
}
ZTH_TEST("works", "time priority: at the same price the order that arrived first trades first") {
    OrderBook book;
    book.submit(S(1, 100, 10));
    book.submit(S(2, 100, 10));
    book.submit(S(3, 100, 10));
    ZTH_EXPECT_EQ(show(book.submit(B(4, 100, 15)).trades), std::string("[4x1 10@100][4x2 5@100]"));
    ZTH_EXPECT_EQ(*book.remaining(OrderId{2}), 5LL);   // partly filled, keeps its place
    ZTH_EXPECT_EQ(show(book.submit(B(5, 100, 8)).trades), std::string("[5x2 5@100][5x3 3@100]"));
}
ZTH_TEST("works", "partial fill of the incoming order: the rest rests at its own price") {
    OrderBook book;
    book.submit(S(1, 100, 4));
    const auto r = book.submit(B(2, 101, 10));
    ZTH_EXPECT_TRUE(r.status == Status::Rested);
    ZTH_EXPECT_EQ(show(r.trades), std::string("[2x1 4@100]"));
    ZTH_EXPECT_EQ(r.resting, 6LL);
    ZTH_EXPECT_EQ(book.best_bid()->ticks, 101LL);
    ZTH_EXPECT_EQ(book.depth_at(Side::Buy, Price{101}), 6LL);
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{1});
}
ZTH_TEST("works", "partial fill of the resting order: it stays with the remaining quantity") {
    OrderBook book;
    book.submit(S(1, 100, 10));
    const auto r = book.submit(B(2, 100, 3));
    ZTH_EXPECT_TRUE(r.status == Status::Filled);
    ZTH_EXPECT_EQ(*book.remaining(OrderId{1}), 7LL);
    ZTH_EXPECT_EQ(book.depth_at(Side::Sell, Price{100}), 7LL);
}
ZTH_TEST("works", "an order sweeps several levels and the rest rests; later orders queue behind it") {
    OrderBook book;
    book.submit(S(1, 100, 5));
    book.submit(S(2, 101, 5));
    book.submit(S(3, 102, 5));
    const auto r = book.submit(B(4, 102, 20));
    ZTH_EXPECT_EQ(show(r.trades), std::string("[4x1 5@100][4x2 5@101][4x3 5@102]"));
    ZTH_EXPECT_EQ(r.resting, 5LL);
    ZTH_EXPECT_EQ(book.best_bid()->ticks, 102LL);
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
}
ZTH_TEST("works", "cancel removes a resting order; best prices and depth follow") {
    OrderBook book;
    book.submit(B(1, 100, 10));
    book.submit(B(2, 100, 5));
    book.submit(B(3, 99, 7));
    ZTH_EXPECT_TRUE(book.cancel(OrderId{1}));
    ZTH_EXPECT_EQ(book.depth_at(Side::Buy, Price{100}), 5LL);
    ZTH_EXPECT_TRUE(book.cancel(OrderId{2}));
    ZTH_EXPECT_EQ(book.best_bid()->ticks, 99LL);      // the level at 100 is gone
    ZTH_EXPECT_EQ(book.depth_at(Side::Buy, Price{100}), 0LL);
    ZTH_EXPECT_FALSE(book.cancel(OrderId{2}));
    ZTH_EXPECT_FALSE(book.cancel(OrderId{99}));
    ZTH_EXPECT_FALSE(book.remaining(OrderId{1}).has_value());
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{1});
}
ZTH_TEST("works", "invalid orders are rejected and change nothing: zero or negative quantity, zero or negative price, a duplicate id") {
    OrderBook book;
    book.submit(B(1, 100, 10));
    for (const Order& o : {B(2, 100, 0), B(3, 100, -5), B(4, 0, 10), B(5, -3, 10), S(1, 105, 10), B(1, 100, 1)}) {
        const auto r = book.submit(o);
        ZTH_EXPECT_TRUE(r.status == Status::Rejected);
        ZTH_EXPECT_TRUE(r.trades.empty());
    }
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{1});
    ZTH_EXPECT_EQ(*book.remaining(OrderId{1}), 10LL);
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "an id can be reused once the order is gone (filled or cancelled); not while it rests") {
    OrderBook book;
    book.submit(S(7, 100, 5));
    ZTH_EXPECT_TRUE(book.submit(S(7, 100, 5)).status == Status::Rejected);
    book.submit(B(8, 100, 5));                       // fills #7
    ZTH_EXPECT_TRUE(book.submit(S(7, 110, 3)).status == Status::Rested);
    book.cancel(OrderId{7});
    ZTH_EXPECT_TRUE(book.submit(S(7, 111, 3)).status == Status::Rested);
    ZTH_EXPECT_EQ(book.best_ask()->ticks, 111LL);
}
ZTH_TEST("robust", "an incoming order that fills exactly leaves no empty level and no stale index entry") {
    OrderBook book;
    book.submit(S(1, 100, 5));
    book.submit(S(2, 101, 5));
    book.submit(B(3, 101, 5));    // fills #1 exactly
    ZTH_EXPECT_EQ(book.best_ask()->ticks, 101LL);
    ZTH_EXPECT_FALSE(book.remaining(OrderId{1}).has_value());
    ZTH_EXPECT_FALSE(book.cancel(OrderId{1}));
    ZTH_EXPECT_EQ(book.depth_at(Side::Sell, Price{100}), 0LL);
    book.submit(B(4, 101, 5));
    ZTH_EXPECT_FALSE(book.best_ask().has_value());
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{0});
}
ZTH_TEST("robust", "cancel in the middle of a level keeps the order of the others") {
    OrderBook book;
    for (unsigned long long id = 1; id <= 5; ++id) book.submit(S(id, 100, 10));
    book.cancel(OrderId{3});
    book.cancel(OrderId{1});
    ZTH_EXPECT_EQ(show(book.submit(B(9, 100, 25)).trades), std::string("[9x2 10@100][9x4 10@100][9x5 5@100]"));
}
ZTH_TEST("robust", "the book is never crossed: after any submit the best bid is below the best ask") {
    ::zth::Lcg rng(17);
    OrderBook book;
    for (unsigned long long id = 1; id <= 4000; ++id) {
        const Side side = rng.below(2) ? Side::Buy : Side::Sell;
        book.submit(O(id, side, 90 + rng.below(21), 1 + rng.below(30)));
        if (book.best_bid() && book.best_ask()) ZTH_EXPECT_TRUE(book.best_bid()->ticks < book.best_ask()->ticks);
    }
}
ZTH_TEST_TIMEOUT("robust", "6000 random submits and cancels give exactly the trades, statuses and depths of a naive scanning book", 15) {
    ::zth::Lcg rng(2468);
    OrderBook book;
    Naive naive;
    unsigned long long next_id = 1;
    for (int step = 0; step < 6000; ++step) {
        if (rng.below(4) == 0 && next_id > 1) {
            const unsigned long long id = 1 + static_cast<unsigned long long>(rng.below(static_cast<int>(next_id)));
            ZTH_EXPECT_EQ(book.cancel(OrderId{id}), naive.cancel(id));
        } else {
            unsigned long long id = next_id++;
            if (rng.below(40) == 0 && id > 3) id -= 2;  // sometimes a repeated id (usually still resting or filled)
            Order o = O(id, rng.below(2) ? Side::Buy : Side::Sell, 95 + rng.below(11), rng.below(25) - 2);
            if (rng.below(60) == 0) o.price.ticks = 0;
            const Order copy = o;
            const auto got = book.submit(std::move(o));
            const auto want = naive.submit(copy);
            if (show(got.trades) != show(want.trades) || got.status != want.status || got.resting != want.rest) {
                ::zth::fail("step " + std::to_string(step) + ": got " + show(got.trades) + " status " + std::to_string(static_cast<int>(got.status)) + " resting " + std::to_string(got.resting) + ", expected " +
                            show(want.trades) + " status " + std::to_string(static_cast<int>(want.status)) + " resting " + std::to_string(want.rest));
                return;
            }
        }
        if (step % 50 == 0) {
            ZTH_EXPECT_EQ(book.order_count(), naive.resting.size());
            const auto bb = naive.best(Side::Buy), ba = naive.best(Side::Sell);
            ZTH_EXPECT_EQ(book.best_bid().has_value(), bb.has_value());
            ZTH_EXPECT_EQ(book.best_ask().has_value(), ba.has_value());
            if (bb) ZTH_EXPECT_EQ(book.best_bid()->ticks, *bb);
            if (ba) ZTH_EXPECT_EQ(book.best_ask()->ticks, *ba);
            for (long long p = 94; p <= 106; ++p) {
                ZTH_EXPECT_EQ(book.depth_at(Side::Buy, Price{p}), naive.depth(Side::Buy, p));
                ZTH_EXPECT_EQ(book.depth_at(Side::Sell, Price{p}), naive.depth(Side::Sell, p));
            }
        }
    }
    for (const Order& r : naive.resting) ZTH_EXPECT_EQ(*book.remaining(r.id), r.qty);
}
ZTH_TEST("robust", "large quantities: depth and fills work near the limits of int64") {
    OrderBook book;
    const long long big = 4000000000000000000LL;
    book.submit(S(1, 100, big));
    const auto r = book.submit(B(2, 100, big - 5));
    ZTH_EXPECT_EQ(show(r.trades), "[2x1 " + std::to_string(big - 5) + "@100]");
    ZTH_EXPECT_EQ(*book.remaining(OrderId{1}), 5LL);
}

// ================================================ Fast ========================================================
namespace {
// n resting orders on `levels` price levels on each side (bids below 10000, asks above)
void fill_book(OrderBook& book, int n, int levels) {
    ::zth::Lcg rng(5);
    for (int i = 1; i <= n; ++i) {
        const bool buy = i % 2 == 0;
        const long long offset = 1 + rng.below(levels);
        book.submit(O(static_cast<unsigned long long>(i), buy ? Side::Buy : Side::Sell, buy ? 10000 - offset : 10000 + offset, 5));
    }
}
}  // namespace

ZTH_TEST_TIMEOUT("fast", "submitting costs a few price comparisons: 20000 non-crossing orders over 1000 levels need at most 30 per order", 10) {
    OrderBook book;
    stats::reset();
    fill_book(book, 20000, 1000);
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{20000});
    ZTH_EXPECT_TRUE(stats::price_compares <= 20000ULL * 30);
}
ZTH_TEST_TIMEOUT("fast", "matching looks only at the best level: 20000 orders taking one resting order each cost at most 12 comparisons", 10) {
    OrderBook book;
    fill_book(book, 20000, 1000);
    stats::reset();
    for (unsigned long long id = 100001; id < 100001 + 5000; ++id) {
        // a small order at the best opposite price: trades a little, usually leaves the level
        book.submit(O(id, id % 2 ? Side::Buy : Side::Sell, id % 2 ? 10000 + 1000 : 10000 - 1000, 1));
    }
    ZTH_EXPECT_TRUE(stats::price_compares <= 5000ULL * 40);
}
ZTH_TEST_TIMEOUT("fast", "cancel does not search: 20000 cancels cost at most 6 id comparisons and 40 price comparisons each", 10) {
    OrderBook book;
    fill_book(book, 20000, 1000);
    stats::reset();
    for (unsigned long long id = 1; id <= 20000; ++id) ZTH_EXPECT_TRUE(book.cancel(OrderId{id}));
    ZTH_EXPECT_EQ(book.order_count(), std::size_t{0});
    ZTH_EXPECT_TRUE(stats::id_compares <= 20000ULL * 6);
    ZTH_EXPECT_TRUE(stats::price_compares <= 20000ULL * 40);
}
ZTH_TEST_TIMEOUT("fast", "orders are moved into the book: at most one copy per order in total (20000 orders)", 10) {
    OrderBook book;
    stats::reset();
    fill_book(book, 20000, 100);
    ZTH_EXPECT_TRUE(stats::order_copies <= 20000ULL);
}
ZTH_TEST_TIMEOUT("fast", "a deep book: 100000 resting orders, then 400000 mixed operations, in time", 10) {
    OrderBook book;
    fill_book(book, 100000, 5000);
    ::zth::Lcg rng(9);
    unsigned long long id = 200000;
    for (int i = 0; i < 400000; ++i) {
        if (rng.below(2)) book.cancel(OrderId{1 + static_cast<unsigned long long>(rng.below(100000))});
        else book.submit(O(++id, rng.below(2) ? Side::Buy : Side::Sell, 10000 + rng.below(40) - 20, 1 + rng.below(3)));
    }
    ZTH_EXPECT_TRUE(book.order_count() > 0);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "no raw memory, no exceptions, no goto") {
    ZTH_EXPECT_NO_RAW_MEMORY("order_book.hpp");
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("order_book.hpp"));
    for (const char* word : {"throw", "try", "catch", "goto"}) ZTH_EXPECT_TRUE(::zth::find_word(code, word).empty());
}
ZTH_TEST("clean", "the price levels are an ordered container and the book is never re-sorted: std::map (or std::set), no std::sort / std::stable_sort") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("order_book.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "map").empty() || !::zth::find_word(code, "set").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "sort").empty());
    ZTH_EXPECT_TRUE(::zth::find_word(code, "stable_sort").empty());
}
ZTH_TEST("clean", "cancel uses an index (an unordered_map or a map keyed by id): the id index is a hash table") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("order_book.hpp"));
    ZTH_EXPECT_TRUE(!::zth::find_word(code, "unordered_map").empty() || !::zth::find_word(code, "map").empty());
}
