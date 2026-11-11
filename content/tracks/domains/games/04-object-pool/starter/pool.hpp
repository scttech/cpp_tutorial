#pragma once
// YOUR OBJECT POOL: N objects of type T in a fixed block of memory inside the pool itself (no heap).
//
//   create(args...)   construct a T in a free slot, forwarding the arguments; the returned Handle names it.
//                     If the pool is full: kInvalidHandle, nothing is constructed.
//                     A destroyed slot is reused first (most recently freed first). If T's constructor throws, the pool is unchanged.
//   destroy(h)        destroy the object and free its slot: true. A stale, invalid or already destroyed handle: false, nothing happens.
//   get(h)            pointer to the object, or nullptr for a stale / invalid handle
//   alive(h)          is the handle current?
//   size(), capacity(), full()
//   for_each(f)       f(Handle, T&) for every live object, in slot order
//   ~Pool()           destroys the objects that are still alive. The pool cannot be copied or moved.
//
// create and destroy take constant time. Use std::construct_at / std::destroy_at (no `new`); the storage must be correctly aligned for T.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "types.hpp"

namespace pool {

template <class T, std::size_t N>
class Pool {
public:
    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    ~Pool() {}  // TODO: destroy the live objects

    template <class... Args>
    Handle create(Args&&... args) {
        (void)sizeof...(args);
        return kInvalidHandle;  // TODO
    }
    bool destroy(Handle) { return false; }  // TODO
    T* get(Handle) { return nullptr; }       // TODO
    const T* get(Handle) const { return nullptr; }
    bool alive(Handle) const { return false; }
    std::size_t size() const { return 0; }
    static constexpr std::size_t capacity() { return N; }
    bool full() const { return false; }
    template <class F>
    void for_each(F&&) {}  // TODO

private:
    // TODO: the storage (N slots, correctly aligned), a generation per slot, the free list
};

}  // namespace pool
