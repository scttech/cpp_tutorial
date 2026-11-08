#pragma once

#include <cstddef>
#include <cstring>
#include <utility>

// The Text from the last lesson (copying works). An EMPTY Text owns nothing at all (no buffer), so a moved-from Text
// can be left empty without allocating. Right now `std::move(a)` quietly COPIES, because the class has no move
// operations: every transfer allocates and copies a buffer. Add the two "move" members of the rule of five.
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

    ~Text() { delete[] data_; }

    Text(const Text& other) : size_(other.size_), data_(other.data_ ? new char[other.size_ + 1] : nullptr) {
        if (data_) std::memcpy(data_, other.data_, size_ + 1);
    }

    Text& operator=(const Text& other) {
        if (this != &other) {
            char* copy = other.data_ ? new char[other.size_ + 1] : nullptr;
            if (copy) std::memcpy(copy, other.data_, other.size_ + 1);
            delete[] data_;
            data_ = copy;
            size_ = other.size_;
        }
        return *this;
    }

    // TODO: move constructor `Text(Text&& other) noexcept`: take other's buffer and size, and leave `other` EMPTY
    //       (size 0, no buffer). Nothing is allocated, so it can be noexcept.
    //       (std::exchange(other.size_, 0) returns the old value and stores 0.)
    // TODO: move assignment `Text& operator=(Text&& other) noexcept`: release the buffer you own, then take other's
    //       and leave `other` empty. Do nothing for `a = std::move(a)`. Return *this.

    // TODO: make this swap really cheap: exchange the pointers and sizes (std::swap on the members), do not copy.
    void swap(Text& other) noexcept {
        Text tmp(other);
        other = *this;
        *this = tmp;
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
