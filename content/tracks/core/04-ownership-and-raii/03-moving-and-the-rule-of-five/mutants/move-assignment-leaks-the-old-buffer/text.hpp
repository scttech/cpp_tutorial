// MUTANT: the buffer the target owned is never released.
#pragma once

#include <cstddef>
#include <cstring>
#include <utility>

// The Text from the last lesson, now with MOVE operations: the "big five". An empty Text owns nothing at all (no buffer),
// so a moved-from Text can be left empty without allocating anything: that is what lets a move be `noexcept` and free.
class Text {
public:
    Text() = default;

    Text(const char* s) {
        if (s && *s) {
            size_ = std::strlen(s);
            data_ = new char[size_ + 1];
            std::memcpy(data_, s, size_ + 1);
        }
    }

    // 1. destructor (delete[] on a null pointer is fine: it does nothing)
    ~Text() { delete[] data_; }

    // 2. copy constructor: a deep copy
    Text(const Text& other) : size_(other.size_), data_(other.data_ ? new char[other.size_ + 1] : nullptr) {
        if (data_) std::memcpy(data_, other.data_, size_ + 1);
    }

    // 3. copy assignment: copy first, then swap the copy in; the old buffer leaves with the temporary
    Text& operator=(const Text& other) {
        if (this != &other) {
            Text copy(other);
            swap(copy);
        }
        return *this;
    }

    // 4. move constructor: STEAL the buffer and leave the source empty. Nothing is allocated, nothing can throw,
    //    so it is `noexcept`. (std::exchange(x, v) returns the old value of x and stores v.)
    Text(Text&& other) noexcept : size_(std::exchange(other.size_, 0)), data_(std::exchange(other.data_, nullptr)) {}

    // 5. move assignment: release what we own, then steal. `this != &other` guards `a = std::move(a)`.
    Text& operator=(Text&& other) noexcept {
        if (this != &other) {
            size_ = std::exchange(other.size_, 0);
            data_ = std::exchange(other.data_, nullptr);
        }
        return *this;
    }

    // Swapping two Texts is just swapping their pointers and sizes: no allocation, no copying.
    void swap(Text& other) noexcept {
        std::swap(size_, other.size_);
        std::swap(data_, other.data_);
    }
    friend void swap(Text& a, Text& b) noexcept { a.swap(b); }

    const char* c_str() const { return data_ ? data_ : ""; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void append(const char* s) {
        if (!s || !*s) return;
        const std::size_t add = std::strlen(s);
        char* bigger = new char[size_ + add + 1];
        if (size_) std::memcpy(bigger, data_, size_);
        std::memcpy(bigger + size_, s, add + 1);
        delete[] data_;
        data_ = bigger;
        size_ += add;
    }

private:
    std::size_t size_ = 0;
    char* data_ = nullptr;  // null for an empty Text
};
