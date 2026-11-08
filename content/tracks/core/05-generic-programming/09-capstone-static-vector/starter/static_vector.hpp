#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// A vector with a FIXED capacity N whose elements live INSIDE the object (no heap), and whose elements are created and destroyed
// one at a time, exactly when they are added and removed. That is the difference from std::array<T, N>, which constructs all N
// elements up front: here a StaticVector<T, 100> with 3 elements has constructed exactly 3 T objects, and T need not be default constructible.
//
//   storage_  raw, correctly aligned BYTES: room for N objects, but no object lives there yet
//   size_     how many of them have been constructed (placement new) and not yet destroyed
//
// Rules of the game: an element in [0, size_) is alive; a slot at or after size_ is raw memory. Never touch a raw slot as if it were a T.
//
// Every function below is a stub (it compiles but does the wrong thing). Replace the bodies; the signatures are what the tests use.

// TODO 1: constrain the class: `requires(N > 0) && std::destructible<T>` after the template parameter list.
template <class T, std::size_t N>
class StaticVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    // ---- the five special members (the rule of five, because we manage lifetimes by hand) ----
    StaticVector() noexcept = default;

    // TODO 6: push_back each element; stop at the first one that does not fit (the first N elements are kept).
    StaticVector(std::initializer_list<T>) {}

    // TODO 7: copy: copy-CONSTRUCT each of other's elements into the raw slots (emplace_back(other[i])).
    StaticVector(const StaticVector&) {}

    // TODO 7: move: move-construct each element, then clear() the source: a moved-from vector is EMPTY.
    StaticVector(StaticVector&&) noexcept(std::is_nothrow_move_constructible_v<T>) {}

    // TODO 7: copy assignment: guard against self-assignment, clear() what you hold, then copy-construct the elements.
    StaticVector& operator=(const StaticVector&) { return *this; }

    // TODO 7: move assignment: the same, moving; clear the source afterwards.
    StaticVector& operator=(StaticVector&&) noexcept(std::is_nothrow_move_constructible_v<T>) { return *this; }

    // TODO 3: the destructor must destroy every live element.
    ~StaticVector() {}

    // ---- size ----
    // TODO 2: capacity() is N; size(), empty() and full() use size_.
    static constexpr size_type capacity() noexcept { return 0; }
    size_type size() const noexcept { return 0; }
    bool empty() const noexcept { return true; }
    bool full() const noexcept { return false; }

    // ---- adding at the end: false (and no change) when full ----
    // TODO 4: push_back forwards to emplace_back (std::move for the && overload).
    bool push_back(const T&) { return false; }
    bool push_back(T&&) { return false; }
    // TODO 4: constructs the element IN PLACE in the next raw slot with placement new:
    //         ::new (static_cast<void*>(storage_ + size_ * sizeof(T))) T(std::forward<Args>(args)...);  then ++size_.
    template <class... Args>
    bool emplace_back(Args&&...) {
        return false;
    }

    // ---- removing ----
    // TODO 3: --size_ and destroy the element that was last (std::destroy_at(ptr(size_)) or ptr(size_)->~T()). Does nothing when empty.
    void pop_back() {}
    // TODO 3: destroys every element (pop_back until empty).
    void clear() noexcept {}

    // ---- in the middle: false when the position is out of range (or, for insert, when full) ----
    // TODO 8: insert before `index` (index == size() appends). Move-CONSTRUCT the last element into the first raw slot (emplace_back of
    //         std::move(last)), shift the others up one place by move ASSIGNMENT (from the back), then move-assign `value` into the gap.
    bool insert(size_type, T) { return false; }
    // TODO 8: erase the element at `index`: move-assign every following element one place down, then pop_back() the (moved-from) last one.
    bool erase(size_type) { return false; }

    // ---- access (no bounds checking) ----
    // TODO 5: operator[](i) is *ptr(i); front() is *ptr(0); back() is *ptr(size_ - 1); data() is a pointer to element 0
    //         (when empty, any pointer: begin() == end() is what matters).
    T& operator[](size_type i) { return *ptr(i); }
    const T& operator[](size_type i) const { return *ptr(i); }
    T& front() { return *ptr(0); }
    const T& front() const { return *ptr(0); }
    T& back() { return *ptr(0); }
    const T& back() const { return *ptr(0); }
    T* data() noexcept { return nullptr; }
    const T* data() const noexcept { return nullptr; }

    // ---- iteration: plain pointers (the elements are contiguous) ----
    // TODO 5: begin() is data(), end() is data() + size_.
    iterator begin() noexcept { return nullptr; }
    iterator end() noexcept { return nullptr; }
    const_iterator begin() const noexcept { return nullptr; }
    const_iterator end() const noexcept { return nullptr; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    // TODO 9: equal when the sizes are equal and std::equal says the elements are.
    friend bool operator==(const StaticVector&, const StaticVector&) { return false; }

private:
    // The live object in slot i. std::launder tells the compiler "there really is a T here, created by placement new".
    T* ptr(size_type i) noexcept { return std::launder(reinterpret_cast<T*>(storage_ + i * sizeof(T))); }
    const T* ptr(size_type i) const noexcept { return std::launder(reinterpret_cast<const T*>(storage_ + i * sizeof(T))); }

    alignas(T) unsigned char storage_[sizeof(T) * N];
    size_type size_ = 0;
};
