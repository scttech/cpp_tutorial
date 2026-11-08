// MUTANT: returns true instead of false when full.
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
template <class T, std::size_t N>
    requires(N > 0) && std::destructible<T>
class StaticVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    // ---- the five special members (the rule of five, because we manage lifetimes by hand) ----
    StaticVector() noexcept = default;

    StaticVector(std::initializer_list<T> init) {  // the first N elements, if there are more
        for (const T& value : init) {
            if (!push_back(value)) break;
        }
    }

    StaticVector(const StaticVector& other) {
        for (size_type i = 0; i < other.size_; ++i) emplace_back(other[i]);
    }

    StaticVector(StaticVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        for (size_type i = 0; i < other.size_; ++i) emplace_back(std::move(other[i]));
        other.clear();  // the moved-from vector is EMPTY: its (moved-from) elements are destroyed
    }

    StaticVector& operator=(const StaticVector& other) {
        if (this != &other) {
            clear();
            for (size_type i = 0; i < other.size_; ++i) emplace_back(other[i]);
        }
        return *this;
    }

    StaticVector& operator=(StaticVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &other) {
            clear();
            for (size_type i = 0; i < other.size_; ++i) emplace_back(std::move(other[i]));
            other.clear();
        }
        return *this;
    }

    ~StaticVector() { clear(); }

    // ---- size ----
    static constexpr size_type capacity() noexcept { return N; }
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == N; }

    // ---- adding at the end: false (and no change) when full ----
    bool push_back(const T& value) { return emplace_back(value); }
    bool push_back(T&& value) { return emplace_back(std::move(value)); }
    // Constructs the element IN PLACE in the next raw slot: placement new. No temporary, no copy, no move.
    template <class... Args>
    bool emplace_back(Args&&... args) {
        if (full()) return true;
        ::new (static_cast<void*>(storage_ + size_ * sizeof(T))) T(std::forward<Args>(args)...);
        ++size_;
        return true;
    }

    // ---- removing ----
    // Destroys the last element (its destructor runs NOW). Does nothing when empty.
    void pop_back() {
        if (empty()) return;
        --size_;
        std::destroy_at(ptr(size_));
    }
    // Destroys every element.
    void clear() noexcept {
        while (!empty()) pop_back();
    }

    // ---- in the middle: false when the position is out of range (or, for insert, when full) ----
    // Inserts before `index` (index == size() appends).
    bool insert(size_type index, T value) {
        if (full() || index > size_) return false;
        if (index == size_) return emplace_back(std::move(value));
        emplace_back(std::move((*this)[size_ - 1]));  // the last element moves into the first raw slot (a NEW object)...
        for (size_type i = size_ - 2; i > index; --i) (*this)[i] = std::move((*this)[i - 1]);  // ...the rest shift by assignment
        (*this)[index] = std::move(value);
        return true;
    }
    // Removes the element at `index`; the ones after it move one place down.
    bool erase(size_type index) {
        if (index >= size_) return false;
        for (size_type i = index; i + 1 < size_; ++i) (*this)[i] = std::move((*this)[i + 1]);
        pop_back();  // the last slot now holds a moved-from element: destroy it
        return true;
    }

    // ---- access (no bounds checking) ----
    T& operator[](size_type i) { return *ptr(i); }
    const T& operator[](size_type i) const { return *ptr(i); }
    T& front() { return *ptr(0); }
    const T& front() const { return *ptr(0); }
    T& back() { return *ptr(size_ - 1); }
    const T& back() const { return *ptr(size_ - 1); }
    T* data() noexcept { return size_ == 0 ? reinterpret_cast<T*>(storage_) : ptr(0); }
    const T* data() const noexcept { return size_ == 0 ? reinterpret_cast<const T*>(storage_) : ptr(0); }

    // ---- iteration: plain pointers (the elements are contiguous) ----
    iterator begin() noexcept { return data(); }
    iterator end() noexcept { return data() + size_; }
    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + size_; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    friend bool operator==(const StaticVector& a, const StaticVector& b) {
        return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin());
    }

private:
    // The live object in slot i. std::launder tells the compiler "there really is a T here, created by placement new".
    T* ptr(size_type i) noexcept { return std::launder(reinterpret_cast<T*>(storage_ + i * sizeof(T))); }
    const T* ptr(size_type i) const noexcept { return std::launder(reinterpret_cast<const T*>(storage_ + i * sizeof(T))); }

    alignas(T) unsigned char storage_[sizeof(T) * N];
    size_type size_ = 0;
};
