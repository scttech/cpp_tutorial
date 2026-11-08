#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

// A block of ints on the heap that can be resized, with VALUE semantics: a copy is independent of the original, a move is
// cheap and leaves the source empty. An empty Buffer owns nothing (a null pointer).
//
// The members below compile, but most do nothing useful yet, and the compiler's own copy and move just copy the pointer.
// Make every member do its job (read the table in lesson.md). Decide who owns the memory, and do not leak it, free it twice, or
// read past its end.
class Buffer {
public:
    Buffer() = default;

    // TODO: `n` ints, every one equal to `fill`. Allocate with new int[n] (but nothing at all when n == 0).
    explicit Buffer(std::size_t n, int fill = 0) { (void)n; (void)fill; }

    // TODO: the destructor, the copy constructor and copy assignment (deep copy, safe for `a = a`), and the move
    // constructor and move assignment (steal the buffer, leave the source empty, noexcept). The rule of five.

    // TODO: exchange the contents of two Buffers without copying (noexcept), plus a friend swap(a, b).
    void swap(Buffer& other) noexcept { (void)other; }
    friend void swap(Buffer& a, Buffer& b) noexcept { a.swap(b); }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    // TODO: operator[] is unchecked (like an array); at() throws std::out_of_range for an index >= size().
    int& operator[](std::size_t i) { return data_[i]; }
    const int& operator[](std::size_t i) const { return data_[i]; }
    int& at(std::size_t i) { return data_[i]; }
    const int& at(std::size_t i) const { return data_[i]; }

    int* begin() { return data_; }
    int* end() { return data_ + size_; }
    const int* begin() const { return data_; }
    const int* end() const { return data_ + size_; }

    // TODO: keep the first min(n, size()) values; new elements are 0. n == size() does nothing.
    void resize(std::size_t n) { (void)n; }

    // TODO: equal when the sizes are equal and every element is equal.
    friend bool operator==(const Buffer& a, const Buffer& b) { (void)a; (void)b; return false; }

private:
    std::size_t size_ = 0;
    int* data_ = nullptr;  // null when empty
};
