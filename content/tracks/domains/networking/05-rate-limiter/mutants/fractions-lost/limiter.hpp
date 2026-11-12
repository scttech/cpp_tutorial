// MUTANT: only whole tokens are added
#pragma once
// Exact integer token accounting (units of 1/period of a token), a deque of timestamps for the sliding window, a hash map of buckets for keys.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

namespace ratelimit {

using Tick = std::uint64_t;

class TokenBucket {
public:
    TokenBucket(std::uint64_t capacity, std::uint64_t refill_tokens, Tick period, bool start_full = true)
        : capacity_(capacity), refill_(refill_tokens), period_(period == 0 ? 1 : period), balance_(start_full ? capacity * (period == 0 ? 1 : period) : 0) {}

    bool try_acquire(Tick now, std::uint64_t n = 1) {
        update(now);
        if (n == 0) return true;
        if (n > capacity_) return false;
        const std::uint64_t need = n * period_;
        if (balance_ < need) return false;
        balance_ -= need;
        return true;
    }

    std::uint64_t available(Tick now) {
        update(now);
        return balance_ / period_;
    }

    std::optional<Tick> wait_time(Tick now, std::uint64_t n = 1) {
        update(now);
        if (n == 0) return Tick{0};
        if (n > capacity_) return std::nullopt;
        const std::uint64_t need = n * period_;
        if (balance_ >= need) return Tick{0};
        if (refill_ == 0) return std::nullopt;
        const std::uint64_t deficit = need - balance_;
        return (deficit + refill_ - 1) / refill_;   // round up
    }

    bool full(Tick now) {
        update(now);
        return balance_ == capacity_ * period_;
    }

private:
    void update(Tick now) {
        if (now <= last_) return;                     // no time has passed (or the clock went backwards)
        const Tick elapsed = now - last_;
        last_ = now;
        const std::uint64_t cap = capacity_ * period_;
        const std::uint64_t room = cap - balance_;    // balance_ never exceeds cap
        if (refill_ == 0) return;
        if (elapsed > room / refill_) balance_ = cap;  // would overflow or exceed the cap: saturate
        else balance_ += (elapsed * refill_ / period_) * period_;
        if (balance_ > cap) balance_ = cap;
    }

    std::uint64_t capacity_, refill_;
    Tick period_;
    std::uint64_t balance_;
    Tick last_ = 0;
};

class SlidingWindowLog {
public:
    SlidingWindowLog(std::uint64_t limit, Tick window) : limit_(limit), window_(window) {}

    bool try_acquire(Tick now) {
        now = clamp(now);
        evict(now);
        if (times_.size() >= limit_) return false;
        times_.push_back(now);
        return true;
    }

    std::size_t count(Tick now) {
        now = clamp(now);
        evict(now);
        return times_.size();
    }

    std::optional<Tick> retry_after(Tick now) {
        now = clamp(now);
        evict(now);
        if (limit_ == 0) return std::nullopt;
        if (times_.size() < limit_) return Tick{0};
        return times_.front() + window_ - now;
    }

private:
    Tick clamp(Tick now) const { return (!times_.empty() && now < times_.back()) ? times_.back() : now; }
    void evict(Tick now) {
        while (!times_.empty() && times_.front() + window_ <= now) times_.pop_front();
    }

    std::uint64_t limit_;
    Tick window_;
    std::deque<Tick> times_;
};

template <class Key>
class KeyedLimiter {
public:
    KeyedLimiter(std::uint64_t capacity, std::uint64_t refill_tokens, Tick period) : capacity_(capacity), refill_(refill_tokens), period_(period) {}

    bool try_acquire(const Key& key, Tick now, std::uint64_t n = 1) {
        auto it = buckets_.find(key);
        if (it == buckets_.end()) it = buckets_.emplace(key, TokenBucket(capacity_, refill_, period_)).first;
        return it->second.try_acquire(now, n);
    }

    std::size_t size() const { return buckets_.size(); }

    std::size_t evict_idle(Tick now) {
        std::size_t removed = 0;
        for (auto it = buckets_.begin(); it != buckets_.end();) {
            if (it->second.full(now)) {
                it = buckets_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

private:
    std::uint64_t capacity_, refill_;
    Tick period_;
    std::unordered_map<Key, TokenBucket> buckets_;
};

}  // namespace ratelimit
