// MUTANT: v.push_back(v[0]) reads freed memory when it reallocates.
#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <utility>

// A growable array of ints: your own tiny std::vector. It owns a heap block (`data_`) with room for `capacity_` ints, of which
// the first `size_` are in use. The invariant: size_ <= capacity_, and data_ is null exactly when capacity_ is 0.
class IntVector {
public:
    IntVector() = default;

    // IntVector(3, 7) is THREE sevens. (explicit: an int must never silently turn into a vector.)
    explicit IntVector(std::size_t count, int value = 0)
        : size_(count), capacity_(count), data_(count ? new int[count] : nullptr) {
        std::fill(data_, data_ + size_, value);
    }

    // IntVector{3, 7} is the two values 3 and 7: braces prefer the initializer_list constructor.
    IntVector(std::initializer_list<int> init)
        : size_(init.size()), capacity_(init.size()), data_(init.size() ? new int[init.size()] : nullptr) {
        std::copy(init.begin(), init.end(), data_);
    }

    ~IntVector() { delete[] data_; }

    // A copy gets exactly as much room as it needs (capacity == size).
    IntVector(const IntVector& other) : size_(other.size_), capacity_(other.size_), data_(other.size_ ? new int[other.size_] : nullptr) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    IntVector& operator=(const IntVector& other) {
        if (this != &other) {
            IntVector copy(other);
            swap(copy);
        }
        return *this;
    }

    IntVector(IntVector&& other) noexcept
        : size_(std::exchange(other.size_, 0)), capacity_(std::exchange(other.capacity_, 0)), data_(std::exchange(other.data_, nullptr)) {}

    IntVector& operator=(IntVector&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    void swap(IntVector& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        std::swap(data_, other.data_);
    }
    friend void swap(IntVector& a, IntVector& b) noexcept { a.swap(b); }

    // ---- size and capacity ----
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    // ---- element access ----
    int& operator[](std::size_t i) { return data_[i]; }  // unchecked
    const int& operator[](std::size_t i) const { return data_[i]; }
    int& at(std::size_t i) {
        if (i >= size_) throw std::out_of_range("IntVector::at: index out of range");
        return data_[i];
    }
    const int& at(std::size_t i) const {
        if (i >= size_) throw std::out_of_range("IntVector::at: index out of range");
        return data_[i];
    }
    int& front() { return at(0); }  // std::out_of_range if empty
    const int& front() const { return at(0); }
    int& back() {
        if (size_ == 0) throw std::out_of_range("IntVector::back: empty");
        return data_[size_ - 1];
    }
    const int& back() const {
        if (size_ == 0) throw std::out_of_range("IntVector::back: empty");
        return data_[size_ - 1];
    }
    int* data() { return data_; }
    const int* data() const { return data_; }
    int* begin() { return data_; }
    int* end() { return data_ + size_; }
    const int* begin() const { return data_; }
    const int* end() const { return data_ + size_; }

    // ---- changing the contents ----
    // Doubles the capacity when full (0 -> 1 -> 2 -> 4 -> 8 ...), so n push_backs cost about log2(n) allocations, not n.
    // `value` is a reference and may be an element of this very vector (v.push_back(v[0])): it must be read before the
    // old block is released.
    void push_back(const int& value) {
        if (size_ < capacity_) {
            data_[size_++] = value;
            return;
        }
        const std::size_t new_capacity = capacity_ ? capacity_ * 2 : 1;
        int* bigger = new int[new_capacity];
        std::copy(data_, data_ + size_, bigger);
        delete[] data_;
        bigger[size_] = value;
        data_ = bigger;
        capacity_ = new_capacity;
        ++size_;
    }

    void pop_back() {
        if (size_ == 0) throw std::out_of_range("IntVector::pop_back: empty");
        --size_;
    }

    // Inserts before position `pos` (0..size()); std::out_of_range for a bigger pos. Existing elements move up.
    void insert(std::size_t pos, const int& value) {
        if (pos > size_) throw std::out_of_range("IntVector::insert: position out of range");
        const int copy = value;  // `value` may be an element that is about to be shifted: take its value now
        if (size_ == capacity_) reallocate(capacity_ ? capacity_ * 2 : 1);
        std::copy_backward(data_ + pos, data_ + size_, data_ + size_ + 1);
        data_[pos] = copy;
        ++size_;
    }

    // Removes the element at `pos` (< size()); std::out_of_range otherwise. The capacity does not change.
    void erase(std::size_t pos) {
        if (pos >= size_) throw std::out_of_range("IntVector::erase: position out of range");
        std::copy(data_ + pos + 1, data_ + size_, data_ + pos);
        --size_;
    }

    void clear() { size_ = 0; }  // the capacity stays: the block is kept for reuse

    // Makes room for n elements without changing size(). Never shrinks. capacity() is exactly n if it had to grow.
    void reserve(std::size_t n) {
        if (n > capacity_) reallocate(n);
    }

    void shrink_to_fit() {  // capacity() becomes size() (and the block disappears if the vector is empty)
        if (capacity_ != size_) reallocate(size_);
    }

    // Changes the size to n: new elements are copies of `value`. capacity() is at least n.
    void resize(std::size_t n, const int& value = 0) {
        const int copy = value;
        if (n > capacity_) reallocate(n);
        if (n > size_) std::fill(data_ + size_, data_ + n, copy);
        size_ = n;
    }

    friend bool operator==(const IntVector& a, const IntVector& b) {
        return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin());
    }

private:
    // A new block of exactly new_capacity ints (>= size_) holding the current elements.
    void reallocate(std::size_t new_capacity) {
        int* bigger = new_capacity ? new int[new_capacity] : nullptr;
        std::copy(data_, data_ + size_, bigger);
        delete[] data_;
        data_ = bigger;
        capacity_ = new_capacity;
    }

    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
    int* data_ = nullptr;
};
