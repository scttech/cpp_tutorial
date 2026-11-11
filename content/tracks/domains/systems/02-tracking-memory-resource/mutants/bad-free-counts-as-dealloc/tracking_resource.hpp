// MUTANT: a bad free also counts as a deallocation
#pragma once
// Forward to the upstream, remember every live block in a hash map, count everything.
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

    std::size_t live_count() const { return live_.size(); }
    std::size_t live_bytes() const { return live_bytes_; }
    std::size_t peak_bytes() const { return peak_bytes_; }
    std::size_t total_allocations() const { return total_allocations_; }
    std::size_t total_deallocations() const { return total_deallocations_; }
    std::size_t bad_frees() const { return bad_frees_; }
    std::size_t mismatched_frees() const { return mismatched_frees_; }

    std::vector<Allocation> leaks() const {
        std::vector<Allocation> out;
        out.reserve(live_.size());
        for (const auto& entry : live_) out.push_back(entry.second);
        std::sort(out.begin(), out.end(), [](const Allocation& a, const Allocation& b) { return a.id < b.id; });
        return out;
    }

    std::string report() const {
        std::string out;
        if (live_.empty()) {
            out = "no leaks (" + std::to_string(total_allocations_) + " allocations, peak " + std::to_string(peak_bytes_) + " bytes)\n";
        } else {
            out = "LEAK: " + std::to_string(live_.size()) + " allocation(s), " + std::to_string(live_bytes_) + " byte(s) not freed\n";
            for (const Allocation& a : leaks()) {
                out += "  #" + std::to_string(a.id) + ": " + std::to_string(a.bytes) + " bytes, alignment " + std::to_string(a.alignment) + "\n";
            }
        }
        if (bad_frees_ > 0) out += "BAD FREE: " + std::to_string(bad_frees_) + " deallocation(s) of unknown pointers\n";
        if (mismatched_frees_ > 0) out += "MISMATCH: " + std::to_string(mismatched_frees_) + " deallocation(s) with the wrong size or alignment\n";
        return out;
    }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        void* p = upstream_->allocate(bytes, alignment);  // may throw: then nothing has been recorded
        live_[p] = Allocation{p, bytes, alignment, ++total_allocations_};
        live_bytes_ += bytes;
        if (live_bytes_ > peak_bytes_) peak_bytes_ = live_bytes_;
        return p;
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        const auto it = live_.find(p);
        if (it == live_.end()) {
            ++bad_frees_;
            ++total_deallocations_;
            return;
        }
        if (it->second.bytes != bytes || it->second.alignment != alignment) ++mismatched_frees_;
        upstream_->deallocate(p, it->second.bytes, it->second.alignment);  // free it as it was allocated
        live_bytes_ -= it->second.bytes;
        ++total_deallocations_;
        live_.erase(it);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }

private:
    std::pmr::memory_resource* upstream_;
    std::unordered_map<void*, Allocation> live_;
    std::size_t live_bytes_ = 0;
    std::size_t peak_bytes_ = 0;
    std::size_t total_allocations_ = 0;
    std::size_t total_deallocations_ = 0;
    std::size_t bad_frees_ = 0;
    std::size_t mismatched_frees_ = 0;
};

}  // namespace memtrack
