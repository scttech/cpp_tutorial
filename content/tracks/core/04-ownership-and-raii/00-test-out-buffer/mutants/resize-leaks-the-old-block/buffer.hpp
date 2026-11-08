// MUTANT: the old block is not released.
#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

// A fixed block of ints on the heap that can be resized, with value semantics: a copy is independent, a move is cheap.
// An empty Buffer owns nothing (a null pointer), so moving from one never has to allocate.
class Buffer {
public:
    Buffer() = default;

    explicit Buffer(std::size_t n, int fill = 0) : size_(n), data_(n ? new int[n] : nullptr) { std::fill(data_, data_ + size_, fill); }

    ~Buffer() { delete[] data_; }

    Buffer(const Buffer& other) : size_(other.size_), data_(other.size_ ? new int[other.size_] : nullptr) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            Buffer copy(other);  // build the copy first: if the allocation throws, *this is unchanged
            swap(copy);          // the old contents leave with the temporary
        }
        return *this;
    }

    Buffer(Buffer&& other) noexcept : size_(std::exchange(other.size_, 0)), data_(std::exchange(other.data_, nullptr)) {}

    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            size_ = std::exchange(other.size_, 0);
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    void swap(Buffer& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }
    friend void swap(Buffer& a, Buffer& b) noexcept { a.swap(b); }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    int& operator[](std::size_t i) { return data_[i]; }  // unchecked, like the built-in array
    const int& operator[](std::size_t i) const { return data_[i]; }

    int& at(std::size_t i) {  // checked
        if (i >= size_) throw std::out_of_range("Buffer: index out of range");
        return data_[i];
    }
    const int& at(std::size_t i) const {
        if (i >= size_) throw std::out_of_range("Buffer: index out of range");
        return data_[i];
    }

    int* begin() { return data_; }
    int* end() { return data_ + size_; }
    const int* begin() const { return data_; }
    const int* end() const { return data_ + size_; }

    // Keeps the first min(n, size()) values; new elements are 0. Resizing to the same size does nothing.
    void resize(std::size_t n) {
        if (n == size_) return;
        int* bigger = n ? new int[n] : nullptr;
        const std::size_t keep = std::min(n, size_);
        std::copy(data_, data_ + keep, bigger);
        std::fill(bigger + keep, bigger + n, 0);
        data_ = bigger;
        size_ = n;
    }

    friend bool operator==(const Buffer& a, const Buffer& b) { return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin()); }

private:
    std::size_t size_ = 0;
    int* data_ = nullptr;  // null when empty
};
