#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

// An ARENA (also called a bump allocator): one big block, and an allocation is just "move the marker forward". No search for a free block, no
// bookkeeping per allocation, and everything is freed at once by reset() or by destroying the arena. It plugs into the standard containers
// as a std::pmr::memory_resource:   std::pmr::vector<int> v(&arena);   std::pmr::string s("text", &arena);
//
// TASK: make Arena do what its comments say. The stub below just passes every request to the upstream resource (the heap), which is correct but
// is exactly the cost the arena should save. Things to get right:
//   * The block comes from `upstream` ONCE (in the constructor) and goes back in the destructor. Ask for alignof(std::max_align_t) alignment.
//   * do_allocate(size, align): return an address that is a multiple of `align`. Align the ADDRESS (block start + offset), not just the offset.
//     used() is the offset after the allocation, so it includes the padding that was needed for the alignment.
//   * A request that does not fit in what is left (or is absurdly big: beware of `offset + size` overflowing) goes to `upstream` and is counted by
//     fallbacks(). Such a pointer is not inside the block: do_deallocate gives it back to upstream; a pointer inside the block is ignored.
//   * A request for 0 bytes is treated as a request for 1, so that every pointer is different.
//   * reset() forgets everything handed out from the block (they are all invalid now) and starts again at the beginning of the block.
//   * create<T>(args...) builds one T in the arena. An arena never runs destructors, so T must be trivially destructible (the `requires` is already there).
class Arena : public std::pmr::memory_resource {
public:
    explicit Arena(std::size_t capacity, std::pmr::memory_resource* upstream = std::pmr::new_delete_resource()) : capacity_(capacity), upstream_(upstream) {}
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    std::size_t used() const { return 0; }          // bytes of the block handed out so far, padding included
    std::size_t capacity() const { return capacity_; }
    std::size_t fallbacks() const { return 0; }     // requests that did not fit and went to upstream
    void reset() {}

    template <class T, class... Args>
        requires std::is_trivially_destructible_v<T>
    T* create(Args&&... args) {
        ((void)args, ...);
        return nullptr;
    }

protected:
    void* do_allocate(std::size_t size, std::size_t align) override { return upstream_->allocate(size, align); }
    void do_deallocate(void* p, std::size_t size, std::size_t align) override { upstream_->deallocate(p, size, align); }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }

private:
    std::size_t capacity_;
    std::pmr::memory_resource* upstream_;
};
