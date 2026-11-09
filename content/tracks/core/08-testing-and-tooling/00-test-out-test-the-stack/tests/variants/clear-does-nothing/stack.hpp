#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

// A stack of ints with a fixed capacity. THIS is the code under test: you do not edit it, you test it.
//
//   BoundedStack s(3);     capacity 3; a capacity of 0 is std::invalid_argument
//   s.push(1);             std::length_error if the stack is already full
//   s.pop();               removes and returns the NEWEST item; std::out_of_range if the stack is empty
//   s.top();               returns the newest item without removing it; std::out_of_range if empty
//   s.size(), s.capacity(), s.empty(), s.full(), s.clear()
class BoundedStack {
public:
    explicit BoundedStack(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("capacity must be positive");
        items_.reserve(capacity);
    }

    void push(int value) {
        if (full()) throw std::length_error("stack is full");
        items_.push_back(value);
    }

    int pop() {
        if (items_.empty()) throw std::out_of_range("stack is empty");
        const int value = items_.back();
        items_.pop_back();
        return value;
    }

    int top() const {
        if (items_.empty()) throw std::out_of_range("stack is empty");
        return items_.back();
    }

    bool empty() const { return items_.empty(); }
    bool full() const { return items_.size() == capacity_; }
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }
    void clear() {}

private:
    std::size_t capacity_;
    std::vector<int> items_;
};
