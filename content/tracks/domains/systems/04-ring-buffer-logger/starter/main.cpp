// A flight recorder: the last few kilobytes of log messages, kept in memory, dumped when something goes wrong. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "ring_logger.hpp"
#include "types.hpp"

int main() {
    flightlog::RingLogger log(200);  // 200 bytes: room for only a few records
    log.set_min_level(flightlog::Level::Info);
    log.log(flightlog::Level::Debug, "this is filtered out");
    for (int i = 1; i <= 8; ++i) {
        const std::string msg = "request " + std::to_string(i) + " handled";
        log.log(i % 4 == 0 ? flightlog::Level::Warn : flightlog::Level::Info, msg);
    }
    log.log(flightlog::Level::Error, "disk on fire");
    std::printf("%s", log.dump().c_str());
    std::printf("stored %zu records using %zu of %zu bytes; accepted %llu, evicted %llu, filtered %llu\n", log.size(), log.bytes_used(), log.capacity(),
                static_cast<unsigned long long>(log.total_logged()), static_cast<unsigned long long>(log.evicted()), static_cast<unsigned long long>(log.filtered()));
}
