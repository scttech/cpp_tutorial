// MUTANT: a = a destroys the data it copies
#pragma once
// Raw aligned storage, objects built with construct_at and ended with destroy_at; shifting is done with move assignment.
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace emb {

template <class T, std::size_t N>
class StaticVector {
    static_assert(N >= 1, "capacity must be at least 1");

public:
    StaticVector() noexcept = default;

    StaticVector(const StaticVector& other) {
        for (std::size_t i = 0; i < other.size_; ++i) emplace_back(other[i]);
    }

    StaticVector(StaticVector&& other) {
        for (std::size_t i = 0; i < other.size_; ++i) emplace_back(std::move(other[i]));
        other.clear();
    }

    StaticVector& operator=(const StaticVector& other) {
        if (true) {
            clear();
            for (std::size_t i = 0; i < other.size_; ++i) emplace_back(other[i]);
        }
        return *this;
    }

    StaticVector& operator=(StaticVector&& other) {
        if (this != &other) {
            clear();
            for (std::size_t i = 0; i < other.size_; ++i) emplace_back(std::move(other[i]));
            other.clear();
        }
        return *this;
    }

    ~StaticVector() { clear(); }

    template <class... Args>
    T* emplace_back(Args&&... args) {
        if (size_ == N) return nullptr;
        T* p = std::construct_at(slot(size_), std::forward<Args>(args)...);
        ++size_;
        return p;
    }

    bool push_back(const T& value) { return emplace_back(value) != nullptr; }
    bool push_back(T&& value) { return emplace_back(std::move(value)) != nullptr; }

    void pop_back() {
        if (size_ > 0) {
            --size_;
            std::destroy_at(slot(size_));
        }
    }

    bool insert(std::size_t index, T value) {
        if (size_ == N || index > size_) return false;
        if (index == size_) {
            emplace_back(std::move(value));
            return true;
        }
        std::construct_at(slot(size_), std::move(*slot(size_ - 1)));  // the new last slot is a fresh object
        for (std::size_t i = size_ - 1; i > index; --i) *slot(i) = std::move(*slot(i - 1));
        *slot(index) = std::move(value);
        ++size_;
        return true;
    }

    bool erase(std::size_t index) {
        if (index >= size_) return false;
        for (std::size_t i = index; i + 1 < size_; ++i) *slot(i) = std::move(*slot(i + 1));
        pop_back();
        return true;
    }

    bool swap_remove(std::size_t index) {
        if (index >= size_) return false;
        if (index + 1 != size_) *slot(index) = std::move(*slot(size_ - 1));
        pop_back();
        return true;
    }

    void clear() {
        while (size_ > 0) pop_back();
    }

    T& operator[](std::size_t i) { return *slot(i); }
    const T& operator[](std::size_t i) const { return *slot(i); }
    T* at(std::size_t i) { return i < size_ ? slot(i) : nullptr; }
    T& front() { return *slot(0); }
    T& back() { return *slot(size_ - 1); }
    T* data() { return slot(0); }
    T* begin() { return slot(0); }
    T* end() { return slot(size_); }
    const T* begin() const { return slot(0); }
    const T* end() const { return slot(size_); }

    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return N; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == N; }

private:
    T* slot(std::size_t i) { return std::launder(reinterpret_cast<T*>(storage_) + i); }
    const T* slot(std::size_t i) const { return std::launder(reinterpret_cast<const T*>(storage_) + i); }

    alignas(T) std::byte storage_[N * sizeof(T)];
    std::size_t size_ = 0;
};

}  // namespace emb
