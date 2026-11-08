#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <utility>

// A growable array of ints: your own tiny std::vector. It owns a heap block (`data_`) with room for `capacity_` ints, of which
// the first `size_` are in use. The invariant: size_ <= capacity_, and data_ is null exactly when capacity_ is 0.
//
// The names and signatures are fixed (the tests call exactly these); every stub compiles but does nothing useful yet.
// Read lesson.md for the rules of each member. Use new[] and delete[] only: no malloc, and no standard container inside.
class IntVector {
public:
    IntVector() = default;

    // TODO: `count` copies of `value`. explicit: IntVector(3, 7) is three sevens.
    explicit IntVector(std::size_t count, int value = 0) { (void)count; (void)value; }

    // TODO: the values in the list, capacity == size. IntVector{3, 7} is the two values 3 and 7.
    IntVector(std::initializer_list<int> init) { (void)init; }

    // TODO: the rule of five. Destructor; copy constructor and copy assignment (deep copy, capacity == size, safe for
    // `a = a`); move constructor and move assignment (steal the block, leave the source with size 0, capacity 0, no block;
    // noexcept; safe for `a = std::move(a)`).

    // TODO: exchange everything without copying (noexcept), plus a friend swap(a, b).
    void swap(IntVector& other) noexcept { (void)other; }
    friend void swap(IntVector& a, IntVector& b) noexcept { a.swap(b); }

    // ---- size and capacity ----
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    // ---- element access ----
    // TODO: operator[] is unchecked; at() throws std::out_of_range for i >= size(); front() and back() throw
    // std::out_of_range when empty. (const and non-const versions.)
    int& operator[](std::size_t i) { return data_[i]; }
    const int& operator[](std::size_t i) const { return data_[i]; }
    int& at(std::size_t i) { return data_[i]; }
    const int& at(std::size_t i) const { return data_[i]; }
    int& front() { return data_[0]; }
    const int& front() const { return data_[0]; }
    int& back() { return data_[size_ - 1]; }
    const int& back() const { return data_[size_ - 1]; }
    int* data() { return data_; }
    const int* data() const { return data_; }
    int* begin() { return data_; }
    int* end() { return data_ + size_; }
    const int* begin() const { return data_; }
    const int* end() const { return data_ + size_; }

    // ---- changing the contents ----
    // TODO: add `value` at the end. When full, the capacity DOUBLES (0 -> 1 -> 2 -> 4 -> 8 ...): a new block, copy the
    // elements, release the old one. Careful: `value` is a reference and may be an element of this very vector
    // (`v.push_back(v[0])`), so read it before you release the old block.
    void push_back(const int& value) { (void)value; }

    // TODO: remove the last element; std::out_of_range if empty. The capacity does not change.
    void pop_back() {}

    // TODO: insert before position `pos` (0..size(); std::out_of_range for a bigger one); the elements from pos on move up
    // by one. Grow like push_back when full. `value` may be one of the elements that is about to move: copy it first.
    void insert(std::size_t pos, const int& value) { (void)pos; (void)value; }

    // TODO: remove the element at `pos` (< size(); std::out_of_range otherwise); later elements move down. Capacity unchanged.
    void erase(std::size_t pos) { (void)pos; }

    // TODO: size becomes 0; the capacity (and the block) stay for reuse.
    void clear() {}

    // TODO: make room for `n` elements without changing size(): if n > capacity(), capacity() becomes exactly n. Never shrinks.
    void reserve(std::size_t n) { (void)n; }

    // TODO: capacity() becomes size() (no block at all if the vector is empty). No change if they are already equal.
    void shrink_to_fit() {}

    // TODO: size becomes n: new elements are copies of `value`; capacity() is at least n. (Shrinking keeps the capacity.)
    void resize(std::size_t n, const int& value = 0) { (void)n; (void)value; }

    // TODO: equal sizes and equal elements.
    friend bool operator==(const IntVector& a, const IntVector& b) { (void)a; (void)b; return false; }

private:
    // TODO: helpers if you want them, for example `void reallocate(std::size_t new_capacity)`.

    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
    int* data_ = nullptr;
};
