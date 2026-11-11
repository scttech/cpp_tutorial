// A tracked allocation scope: containers allocate through a TrackingResource, the report shows what leaked. Read-only. Press Run.
#include <cstdio>
#include <memory_resource>
#include <string>
#include <vector>

#include "tracking_resource.hpp"

int main() {
    memtrack::TrackingResource tracker;
    {
        std::pmr::vector<std::pmr::string> names(&tracker);
        for (int i = 0; i < 20; ++i) names.emplace_back("a name that is long enough to need heap memory #" + std::to_string(i));
        std::printf("while the vector lives: %zu live allocations, %zu bytes\n", tracker.live_count(), tracker.live_bytes());
    }
    std::printf("after it died: %zu live allocations, peak %zu bytes\n", tracker.live_count(), tracker.peak_bytes());
    std::printf("%s", tracker.report().c_str());

    void* forgotten = tracker.allocate(256, 16);  // a leak, on purpose
    (void)forgotten;
    std::printf("%s", tracker.report().c_str());
}
