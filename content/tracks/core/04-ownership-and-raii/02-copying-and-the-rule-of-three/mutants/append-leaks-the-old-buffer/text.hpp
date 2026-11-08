// MUTANT: append never releases the old buffer.
#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>

// A tiny string that OWNS a heap buffer. Because it owns a raw resource it needs the "big three": a destructor, a copy
// constructor and a copy assignment operator. Write one of them and you almost always need all three.
class Text {
public:
    Text() : size_(0), data_(new char[1]) { data_[0] = '\0'; }

    Text(const char* s) : size_(s ? std::strlen(s) : 0), data_(new char[size_ + 1]) {
        if (s) std::memcpy(data_, s, size_);
        data_[size_] = '\0';
    }

    // 1. destructor: release what the constructors acquired. (new[] pairs with delete[].)
    ~Text() { delete[] data_; }

    // 2. copy constructor: a NEW buffer with a copy of the characters (a "deep" copy), not a second pointer to the same one.
    Text(const Text& other) : size_(other.size_), data_(new char[other.size_ + 1]) { std::memcpy(data_, other.data_, size_ + 1); }

    // 3. copy assignment: the object already owns a buffer. Build the copy FIRST (if `new` throws, *this is untouched),
    // then release the old buffer, then take the new one.
    Text& operator=(const Text& other) {
        if (this == &other) return *this;  // `a = a` must be harmless
        char* copy = new char[other.size_ + 1];
        std::memcpy(copy, other.data_, other.size_ + 1);
        delete[] data_;
        data_ = copy;
        size_ = other.size_;
        return *this;  // by reference: lets `a = b = c` and `(a = b).append("!")` work like they do for int
    }

    const char* c_str() const { return data_; }
    std::size_t size() const { return size_; }

    char& at(std::size_t i) {
        if (i >= size_) throw std::out_of_range("Text: index out of range");
        return data_[i];
    }
    const char& at(std::size_t i) const {
        if (i >= size_) throw std::out_of_range("Text: index out of range");
        return data_[i];
    }

    void append(const char* s) {
        if (!s) return;
        const std::size_t add = std::strlen(s);
        char* bigger = new char[size_ + add + 1];
        std::memcpy(bigger, data_, size_);
        std::memcpy(bigger + size_, s, add + 1);  // `s` may point INTO data_ (a.append(a.c_str())): read it before freeing
        data_ = bigger;
        size_ += add;
    }

private:
    std::size_t size_;
    char* data_;
};
