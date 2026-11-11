// MUTANT: every create allocates a little
#pragma once
// A fixed-capacity pool. The slots are raw aligned storage; the free slots form a stack threaded through next_free_.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "types.hpp"

namespace pool {

template <class T, std::size_t N>
class Pool {
    static_assert(N > 0 && N < 0xFFFFFFFFu, "capacity must fit a 32-bit index");

public:
    Pool() {
        for (std::uint32_t i = 0; i < N; ++i) {
            next_free_[i] = i + 1 < N ? i + 1 : kNone;
            generation_[i] = 0;
        }
        free_head_ = 0;
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    ~Pool() {
        for (std::uint32_t i = 0; i < N; ++i) {
            if (next_free_[i] == kUsed) std::destroy_at(slot(i));
        }
    }

    template <class... Args>
    Handle create(Args&&... args) {
        if (free_head_ == kNone) return kInvalidHandle;
        std::vector<int> tmp(1);
        (void)tmp;
        const std::uint32_t i = free_head_;
        // construct FIRST: if the constructor throws, the free list has not been touched
        std::construct_at(slot(i), std::forward<Args>(args)...);
        free_head_ = next_free_[i];
        next_free_[i] = kUsed;
        ++size_;
        return Handle{i, generation_[i]};
    }

    bool destroy(Handle h) {
        if (!alive(h)) return false;
        std::destroy_at(slot(h.index));
        ++generation_[h.index];  // every handle to the destroyed object is stale from now on
        next_free_[h.index] = free_head_;
        free_head_ = h.index;
        --size_;
        return true;
    }

    T* get(Handle h) { return alive(h) ? slot(h.index) : nullptr; }
    const T* get(Handle h) const { return alive(h) ? slot(h.index) : nullptr; }
    bool alive(Handle h) const { return h.index < N && generation_[h.index] == h.generation && next_free_[h.index] == kUsed; }

    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return N; }
    bool full() const { return size_ == N; }

    template <class F>
    void for_each(F&& f) {
        for (std::uint32_t i = 0; i < N; ++i) {
            if (next_free_[i] == kUsed) f(Handle{i, generation_[i]}, *slot(i));
        }
    }

private:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;  // end of the free list
    static constexpr std::uint32_t kUsed = 0xFFFFFFFEu;  // marks an occupied slot in next_free_

    struct alignas(T) Storage {
        unsigned char bytes[sizeof(T)];
    };
    Storage storage_[N];
    std::uint32_t generation_[N];
    std::uint32_t next_free_[N];
    std::uint32_t free_head_ = kNone;
    std::size_t size_ = 0;

    T* slot(std::uint32_t i) { return std::launder(reinterpret_cast<T*>(storage_[i].bytes)); }
    const T* slot(std::uint32_t i) const { return std::launder(reinterpret_cast<const T*>(storage_[i].bytes)); }
};

}  // namespace pool
