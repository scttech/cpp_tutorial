#pragma once
// YOUR FIXED-CAPACITY VECTOR for firmware: up to N elements of type T stored INSIDE the object (no heap), constructed and destroyed one by one.
//
//   StaticVector<T, N>                 N >= 1. Holds `size()` live objects in raw, correctly aligned storage. It never default-constructs
//                                      unused slots (T may have no default constructor) and never allocates.
//   T* emplace_back(args...)           constructs a new last element in place; nullptr (and nothing constructed) if the vector is full
//   bool push_back(const T&) / push_back(T&&)    false if full
//   void pop_back()                    destroys the last element (no effect if empty)
//   bool insert(index, T value)        puts value at `index` (0..size()) and shifts the later elements up; false if full or index > size()
//   bool erase(index)                  removes the element and shifts the later ones down; false if index >= size()
//   bool swap_remove(index)            O(1) removal that does not keep the order: the last element moves into the hole; false if index >= size()
//   void clear()                       destroys all elements
//   operator[](i)                      unchecked          T* at(i)    nullptr if i >= size()
//   front(), back(), data(), begin(), end(), size(), capacity(), empty(), full()
//   copy constructor / copy assignment copy the elements; move constructor / move assignment MOVE the elements one by one and leave the source EMPTY.
//   The destructor destroys the elements. Self-assignment must be harmless.
//
// HOW: `alignas(T) std::byte storage_[N * sizeof(T)];` is raw memory. Build objects in it with std::construct_at(ptr, args...) and end their
// life with std::destroy_at(ptr). Reach slot i with std::launder(reinterpret_cast<T*>(storage_) + i). Shift elements with MOVE assignment, not copies.
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace emb {

template <class T, std::size_t N>
class StaticVector {
    static_assert(N >= 1, "capacity must be at least 1");

public:
    StaticVector() noexcept = default;
    StaticVector(const StaticVector& other) { (void)other; }   // TODO
    StaticVector(StaticVector&& other) { (void)other; }        // TODO
    StaticVector& operator=(const StaticVector& other) {
        (void)other;
        return *this;  // TODO
    }
    StaticVector& operator=(StaticVector&& other) {
        (void)other;
        return *this;  // TODO
    }
    ~StaticVector() {}  // TODO

    template <class... Args>
    T* emplace_back(Args&&... args) {
        (void)sizeof...(args);
        return nullptr;  // TODO
    }
    bool push_back(const T& value) {
        (void)value;
        return false;  // TODO
    }
    bool push_back(T&& value) {
        (void)value;
        return false;  // TODO
    }
    void pop_back() {}  // TODO
    bool insert(std::size_t index, T value) {
        (void)index;
        (void)value;
        return false;  // TODO
    }
    bool erase(std::size_t index) {
        (void)index;
        return false;  // TODO
    }
    bool swap_remove(std::size_t index) {
        (void)index;
        return false;  // TODO
    }
    void clear() {}  // TODO

    T& operator[](std::size_t i) { return *slot(i); }
    const T& operator[](std::size_t i) const { return *slot(i); }
    T* at(std::size_t i) {
        (void)i;
        return nullptr;  // TODO
    }
    T& front() { return *slot(0); }
    T& back() { return *slot(0); }  // TODO
    T* data() { return slot(0); }
    T* begin() { return slot(0); }
    T* end() { return slot(0); }  // TODO
    const T* begin() const { return slot(0); }
    const T* end() const { return slot(0); }  // TODO

    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return N; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }

private:
    T* slot(std::size_t i) { return std::launder(reinterpret_cast<T*>(storage_) + i); }
    const T* slot(std::size_t i) const { return std::launder(reinterpret_cast<const T*>(storage_) + i); }

    std::byte storage_[N * sizeof(T)];  // TODO: align it for T
    std::size_t size_ = 0;
};

}  // namespace emb
