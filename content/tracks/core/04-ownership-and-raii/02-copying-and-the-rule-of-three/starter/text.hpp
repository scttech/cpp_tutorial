#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>

// A tiny string that OWNS a heap buffer. It has a constructor and a destructor, but the compiler's own copy simply
// copies the POINTER: two Texts then share one buffer, and both destructors free it. Press Run to see the report,
// then give the class the rest of the "big three".
class Text {
public:
    Text() : size_(0), data_(new char[1]) { data_[0] = '\0'; }

    Text(const char* s) : size_(s ? std::strlen(s) : 0), data_(new char[size_ + 1]) {
        if (s) std::memcpy(data_, s, size_);
        data_[size_] = '\0';
    }

    ~Text() { delete[] data_; }

    // TODO: a copy constructor `Text(const Text& other)`: allocate a NEW buffer (size_ + 1 bytes, for the final '\0')
    //       and copy the characters, so each Text owns its own buffer.
    // TODO: a copy assignment `Text& operator=(const Text& other)`: do nothing for `a = a`; otherwise build the copy
    //       FIRST, then delete[] the old buffer, then take the new one. Return *this (by reference).

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

    // TODO: append(s) adds the characters of `s` at the end. It is currently missing: allocate a bigger buffer, copy
    //       the old characters and then `s`, release the old buffer, and update size_. (`s` may point into this very
    //       Text: `a.append(a.c_str())`. Read it before you release the old buffer.)
    void append(const char* s) { (void)s; }

private:
    std::size_t size_;
    char* data_;
};
