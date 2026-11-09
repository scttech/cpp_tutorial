#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

// A ring buffer of ints with a fixed capacity: it keeps the NEWEST `capacity` items. THIS is the code under test: you test it, you do not edit it.
//
//   RingBuffer b(3);       capacity 3; a capacity of 0 is std::invalid_argument
//   b.push(1);             adds the newest item; when the buffer is already full, the OLDEST item is overwritten
//   b.front()              the oldest item         (std::out_of_range if empty)
//   b.back()               the newest item         (std::out_of_range if empty)
//   b.pop()                removes and returns the oldest item  (std::out_of_range if empty)
//   b.at(i)                the i-th item counted from the oldest, 0-based  (std::out_of_range if i >= size())
//   b.size(), b.capacity(), b.empty(), b.full(), b.clear()
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity) : data_(capacity) {
    }

    void push(int value) {
        data_[(head_ + size_) % data_.size()] = value;
        if (size_ == data_.size()) {
            head_ = (head_ + 1) % data_.size();  // full: the oldest slot was just overwritten
        } else {
            ++size_;
        }
    }

    int pop() {
        if (size_ == 0) throw std::out_of_range("buffer is empty");
        const int value = data_[head_];
        head_ = (head_ + 1) % data_.size();
        --size_;
        return value;
    }

    int front() const {
        if (size_ == 0) throw std::out_of_range("buffer is empty");
        return data_[head_];
    }

    int back() const {
        if (size_ == 0) throw std::out_of_range("buffer is empty");
        return data_[(head_ + size_ - 1) % data_.size()];
    }

    int at(std::size_t index) const {
        if (index >= size_) throw std::out_of_range("index out of range");
        return data_[(head_ + index) % data_.size()];
    }

    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == data_.size(); }
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return data_.size(); }
    void clear() {
        head_ = 0;
        size_ = 0;
    }

private:
    std::vector<int> data_;
    std::size_t head_ = 0;
    std::size_t size_ = 0;
};
