#pragma once
// YOUR TRACKING MEMORY RESOURCE: a std::pmr::memory_resource that forwards every request to an "upstream" resource and keeps books.
//
//   TrackingResource(upstream = std::pmr::get_default_resource())
//   live_count()          allocations not yet freed             live_bytes()      their total size
//   peak_bytes()          the largest live_bytes() ever seen    total_allocations(), total_deallocations()
//   bad_frees()           deallocations of a pointer that is not live (never allocated here, or already freed)
//   mismatched_frees()    deallocations whose size or alignment differ from the allocation
//   leaks()               the live allocations as Allocation{ptr, bytes, alignment, id}, in allocation order (id 1, 2, 3 ...)
//   report()              a text report (exact format in the lesson)
//
// Rules: forward EXACTLY the size and alignment that were asked (no headers, no padding); give each successful allocation the next id
// (ids are never reused; an allocation the upstream refused leaves no trace); a deallocation of an unknown pointer is counted and NOT forwarded
// (forwarding it would corrupt the heap); a mismatched one is counted and the block is freed with the size and alignment it was ALLOCATED with.
// The book-keeping itself uses ordinary containers (std::unordered_map), not the upstream.
#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <string>
#include <unordered_map>
#include <vector>

namespace memtrack {

class TrackingResource : public std::pmr::memory_resource {
public:
    struct Allocation {
        void* ptr;
        std::size_t bytes;
        std::size_t alignment;
        std::size_t id;
    };

    explicit TrackingResource(std::pmr::memory_resource* upstream = std::pmr::get_default_resource()) : upstream_(upstream) {}

    std::size_t live_count() const { return 0; }           // TODO
    std::size_t live_bytes() const { return 0; }           // TODO
    std::size_t peak_bytes() const { return 0; }           // TODO
    std::size_t total_allocations() const { return 0; }    // TODO
    std::size_t total_deallocations() const { return 0; }  // TODO
    std::size_t bad_frees() const { return 0; }            // TODO
    std::size_t mismatched_frees() const { return 0; }     // TODO
    std::vector<Allocation> leaks() const { return {}; }   // TODO
    std::string report() const { return ""; }              // TODO

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        return upstream_->allocate(bytes, alignment);  // TODO: keep the books
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(p, bytes, alignment);  // TODO: keep the books
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        (void)other;
        return false;  // TODO
    }

private:
    std::pmr::memory_resource* upstream_;
    // TODO: the live allocations (an unordered_map from pointer to Allocation) and the counters
};

}  // namespace memtrack
