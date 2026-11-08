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
// Every function below is a stub (it compiles but does the wrong thing). Replace the bodies; the signatures are what the tests use.
// Read the tiers in the lesson text first: Fast counts copies, moves and heap allocations.

// TODO 1: constrain the class. T must be default constructible and movable (`std::default_initializable T`, `std::movable<T>`), and N > 0.
//         Write it as:   template <std::default_initializable T, std::size_t N> requires(N > 0) && std::movable<T> class RingBuffer
template <class T, std::size_t N>
class RingBuffer {
public:
    using value_type = T;
    using size_type = std::size_t;

private:
    // TODO 8: one iterator class for both iterator and const_iterator: Const says which. It walks LOGICAL positions 0..size()-1, oldest first,
    // by calling the owner's operator[]. It needs the five iterator typedefs (forward_iterator_tag), *, ->, prefix and postfix ++, ==,
    // and a conversion from iterator to const_iterator (`operator Iter<true>() const requires(!Const)`).
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

        reference operator*() const { return (*owner_)[0]; }
        pointer operator->() const { return &(*owner_)[0]; }
        Iter& operator++() { return *this; }
        Iter operator++(int) { return *this; }
        friend bool operator==(const Iter&, const Iter&) { return true; }

    private:
        Owner* owner_ = nullptr;
        size_type index_ = 0;
    };

public:
    using iterator = Iter<false>;
    using const_iterator = Iter<true>;

    // ---- size ----
    // TODO 2: capacity() is N; size(), empty() and full() use size_.
    static constexpr size_type capacity() noexcept { return 0; }
    size_type size() const noexcept { return 0; }
    bool empty() const noexcept { return true; }
    bool full() const noexcept { return false; }

    // ---- adding: false (and no change) when the buffer is full ----
    // TODO 3: store into the slot after the newest element: data_[slot(size_)], then ++size_. The `T&&` overload MOVES.
    bool push(const T&) { return false; }
    bool push(T&&) { return false; }
    // TODO 4: like push, but builds the element from the arguments: `T(std::forward<Args>(args)...)` (perfect forwarding).
    template <class... Args>
    bool emplace(Args&&...) {
        return false;
    }
    // TODO 5: always succeeds. When full, the OLDEST element is replaced (and head_ moves on); otherwise it is an ordinary push.
    void push_overwrite(T) {}

    // ---- removing the oldest ----
    // TODO 6: nullopt when empty; otherwise MOVE the oldest element out (data_[head_]), advance head_ (wrapping), shrink size_.
    //         `return std::optional<T>(std::move(oldest));` moves exactly once.
    std::optional<T> pop() { return std::nullopt; }
    // TODO 7: empties the buffer AND lets go of what the elements held (assign `T{}` to each live slot), so a shared_ptr, for example, is released.
    void clear() {}

    // ---- looking: index 0 is the OLDEST element. No bounds checking (like std::vector::operator[]). ----
    // TODO 2b: the element at logical position i lives in array slot slot(i); front() is slot(0) and back() is slot(size_ - 1).
    T& operator[](size_type) { return data_[0]; }
    const T& operator[](size_type) const { return data_[0]; }
    T& front() { return data_[0]; }
    const T& front() const { return data_[0]; }
    T& back() { return data_[0]; }
    const T& back() const { return data_[0]; }

    // ---- iteration, oldest to newest ----
    // TODO 9: begin() is position 0, end() is position size_.
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, 0); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, 0); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

private:
    // the array index of the element at logical position i: (head_ + i) % N
    size_type slot(size_type i) const noexcept {
        (void)i;
        return 0;
    }

    std::array<T, N> data_{};
    size_type head_ = 0;
    size_type size_ = 0;
};
