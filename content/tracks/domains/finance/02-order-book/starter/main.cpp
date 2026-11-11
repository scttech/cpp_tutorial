// A few orders arrive; trades are printed as they happen. Read-only. Press Run.
#include <cstdio>

#include "order_book.hpp"

static void show(const ob::OrderBook::Result& r) {
    for (const ob::Trade& t : r.trades) std::printf("  trade: %lld @ %lld (taker %llu, maker %llu)\n", static_cast<long long>(t.qty), static_cast<long long>(t.price.ticks), static_cast<unsigned long long>(t.taker.value), static_cast<unsigned long long>(t.maker.value));
    std::printf("  %s, %lld resting\n", r.status == ob::Status::Filled ? "filled" : r.status == ob::Status::Rested ? "rested" : "rejected", static_cast<long long>(r.resting));
}

int main() {
    using namespace ob;
    OrderBook book;
    auto submit = [&](unsigned long long id, Side side, long long price, long long qty, const char* what) {
        std::printf("%s #%llu: %s %lld @ %lld\n", what, id, side == Side::Buy ? "buy" : "sell", qty, price);
        show(book.submit(Order{OrderId{id}, side, Price{price}, qty}));
    };
    submit(1, Side::Sell, 101, 50, "new");
    submit(2, Side::Sell, 101, 30, "new");  // same price, later: second in line
    submit(3, Side::Sell, 103, 40, "new");
    submit(4, Side::Buy, 99, 20, "new");    // no match: rests
    submit(5, Side::Buy, 102, 70, "new");   // takes 50 from #1 and 20 from #2, at 101
    std::printf("best bid %lld, best ask %lld\n", static_cast<long long>(book.best_bid()->ticks), static_cast<long long>(book.best_ask()->ticks));
    std::printf("cancel #3: %s\n", book.cancel(OrderId{3}) ? "done" : "unknown");
    std::printf("best ask now: %s\n", book.best_ask() ? "still there" : "none");
    submit(6, Side::Sell, 98, 100, "new");  // hits the bid of #4 (20 @ 99), the rest rests at 98
}
