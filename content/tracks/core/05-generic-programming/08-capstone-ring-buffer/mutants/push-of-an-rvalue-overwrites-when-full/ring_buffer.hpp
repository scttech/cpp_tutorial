// MUTANT: the full check is missing in push(T&&).
#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

// A fixed-capacity circular queue: the capacity N is part of the TYPE, the elements live inside the object (no heap), and the oldest
// element leaves first. Pushing onto a full buffer is refused (push_overwrite replaces the oldest instead).
//
//   head_ is where the OLDEST element is; the next free slot is (head_ + size_) % N. Both indices wrap around, so the slots are reused forever.
//
// The element type must be default constructible (the array is full of default values) and movable.
template <std::default_initializable T, std::size_t N>
    requires(N > 0) && std::movable<T>
class RingBuffer {
public:
    using value_type = T;
    using size_type = std::size_t;

private:
    // One iterator class for both iterator and const_iterator: Const says which. It walks LOGICAL positions 0..size()-1 (oldest first).
    template <bool Const>
    class Iter {
        using Owner = std::conditional_t<Const, const RingBuffer, RingBuffer>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<Const, const T*, T*>;
        using reference = std::conditional_t<Const, const T&, T&>;

        Iter() = default;
        Iter(Owner* owner, size_type index) : owner_(owner), index_(index) {}

        reference operator*() const { return (*owner_)[index_]; }
        pointer operator->() const { return &(*owner_)[index_]; }
        Iter& operator++() {
            ++index_;
            return *this;
        }
        Iter operator++(int) {
            Iter before = *this;
            ++index_;
            return before;
        }
        friend bool operator==(const Iter& a, const Iter& b) { return a.owner_ == b.owner_ && a.index_ == b.index_; }

        // an iterator converts to a const_iterator (never the other way round)
        operator Iter<true>() const
            requires(!Const)
        {
            return Iter<true>(owner_, index_);
        }

    private:
        Owner* owner_ = nullptr;
        size_type index_ = 0;
    };

public:
    using iterator = Iter<false>;
    using const_iterator = Iter<true>;

    // ---- size ----
    static constexpr size_type capacity() noexcept { return N; }
    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    bool full() const noexcept { return size_ == N; }

    // ---- adding: false (and no change) when the buffer is full ----
    bool push(const T& value) {
        if (full()) return false;
        data_[slot(size_)] = value;
        ++size_;
        return true;
    }
    bool push(T&& value) {
        data_[slot(size_)] = std::move(value);
        ++size_;
        return true;
    }
    // Builds the element from the arguments (perfect forwarding).
    template <class... Args>
    bool emplace(Args&&... args) {
        if (full()) return false;
        data_[slot(size_)] = T(std::forward<Args>(args)...);
        ++size_;
        return true;
    }
    // Always succeeds: when full, the OLDEST element is replaced.
    void push_overwrite(T value) {
        if (full()) {
            data_[head_] = std::move(value);
            head_ = (head_ + 1) % N;
        } else {
            data_[slot(size_)] = std::move(value);
            ++size_;
        }
    }

    // ---- removing the oldest ----
    std::optional<T> pop() {
        if (empty()) return std::nullopt;
        T& oldest = data_[head_];
        head_ = (head_ + 1) % N;
        --size_;
        return std::optional<T>(std::move(oldest));
    }
    // Empties the buffer and lets go of what the elements held (a shared_ptr, a string's memory...).
    void clear() {
        for (size_type i = 0; i < size_; ++i) data_[slot(i)] = T{};
        head_ = 0;
        size_ = 0;
    }

    // ---- looking: index 0 is the OLDEST element. No bounds checking (like std::vector::operator[]). ----
    T& operator[](size_type i) { return data_[slot(i)]; }
    const T& operator[](size_type i) const { return data_[slot(i)]; }
    T& front() { return data_[head_]; }
    const T& front() const { return data_[head_]; }
    T& back() { return data_[slot(size_ - 1)]; }
    const T& back() const { return data_[slot(size_ - 1)]; }

    // ---- iteration, oldest to newest ----
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size_); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size_); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

private:
    // the array index of the element at logical position i
    size_type slot(size_type i) const noexcept { return (head_ + i) % N; }

    std::array<T, N> data_{};
    size_type head_ = 0;
    size_type size_ = 0;
};
