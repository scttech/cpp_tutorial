#pragma once
// READ-ONLY. A market event that crosses between the two threads of the gateway.
#include <cstdint>

namespace evq {

// An event of the feed: four fields that must always be seen TOGETHER. A queue that hands one over before it is completely
// written (a "torn" event) breaks `consistent()`.
struct Event {
    std::uint64_t seq = 0;
    std::uint64_t price = 0;
    std::uint64_t qty = 0;
    std::uint64_t check = 0;  // always seq ^ price ^ qty

    static Event make(std::uint64_t seq) {
        Event e;
        e.seq = seq;
        e.price = seq * 31 + 7;
        e.qty = seq % 1000 + 1;
        e.check = e.seq ^ e.price ^ e.qty;
        return e;
    }
    bool consistent() const { return check == (seq ^ price ^ qty); }
};

}  // namespace evq
