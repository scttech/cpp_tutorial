// MUTANT: deallocate ignores everything: fallback memory leaks.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

class Arena : public std::pmr::memory_resource {
public:
    explicit Arena(std::size_t capacity, std::pmr::memory_resource* upstream = std::pmr::new_delete_resource())
        : capacity_(capacity), upstream_(upstream), block_(static_cast<std::byte*>(upstream->allocate(capacity, alignof(std::max_align_t)))) {}
    ~Arena() override { upstream_->deallocate(block_, capacity_, alignof(std::max_align_t)); }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    std::size_t used() const { return used_; }
    std::size_t capacity() const { return capacity_; }
    std::size_t fallbacks() const { return fallbacks_; }
    void reset() { used_ = 0; }

    template <class T, class... Args>
        requires std::is_trivially_destructible_v<T>
    T* create(Args&&... args) {
        return std::construct_at(static_cast<T*>(allocate(sizeof(T), alignof(T))), std::forward<Args>(args)...);
    }

protected:
    void* do_allocate(std::size_t size, std::size_t align) override {
        if (size == 0) size = 1;  // every pointer is different
        const auto base = reinterpret_cast<std::uintptr_t>(block_);
        const std::uintptr_t aligned = (base + used_ + (align - 1)) & ~(static_cast<std::uintptr_t>(align) - 1);  // align the ADDRESS
        const std::size_t offset = static_cast<std::size_t>(aligned - base);
        if (block_ != nullptr && offset <= capacity_ && size <= capacity_ - offset) {  // subtraction, so nothing can overflow
            used_ = offset + size;
            return block_ + offset;
        }
        ++fallbacks_;
        return upstream_->allocate(size, align);
    }
    void do_deallocate(void* p, std::size_t size, std::size_t align) override {
        if (size == 0) size = 1;
        (void)p;
        (void)size;
        (void)align;
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this == &other; }

private:
    bool inside_block(const void* p) const {
        const auto a = reinterpret_cast<std::uintptr_t>(p);
        const auto base = reinterpret_cast<std::uintptr_t>(block_);
        return a >= base && a < base + capacity_;
    }

    std::size_t capacity_;
    std::pmr::memory_resource* upstream_;
    std::byte* block_;
    std::size_t used_ = 0;
    std::size_t fallbacks_ = 0;
};
