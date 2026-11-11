// MUTANT: no done() check: asking again after the end resumes a coroutine that has finished (undefined behavior).
#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <utility>

template <class T>
class Generator {
public:
    struct promise_type {
        std::optional<T> current;

        Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }  // lazy: nothing runs until the first next()
        std::suspend_always final_suspend() noexcept { return {}; }    // stay suspended at the end: the Generator destroys the frame
        std::suspend_always yield_value(T value) {
            current = std::move(value);
            return {};  // suspend: control goes back to whoever resumed us
        }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    Generator() = default;
    explicit Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    ~Generator() { destroy(); }

    bool next() {
        if (!handle_) return false;
        handle_.resume();
        return !handle_.done();
    }

    const T& value() const { return *handle_.promise().current; }

    struct Sentinel {};
    struct Iterator {
        Generator* owner;
        bool at_end;
        bool operator==(Sentinel) const { return at_end; }
        const T& operator*() const { return owner->value(); }
        Iterator& operator++() {
            at_end = !owner->next();
            return *this;
        }
    };
    Iterator begin() { return Iterator{this, !next()}; }
    Sentinel end() { return {}; }

private:
    void destroy() {
        if (handle_) handle_.destroy();
        handle_ = {};
    }

    std::coroutine_handle<promise_type> handle_{};
};

inline Generator<int> range(int begin, int end) {
    for (int i = begin; i < end; ++i) co_yield i;
}

inline Generator<unsigned long long> fibonacci() {
    unsigned long long a = 0;
    unsigned long long b = 1;
    for (;;) {
        co_yield a;
        const unsigned long long next = a + b;
        a = b;
        b = next;
    }
}

inline Generator<int> take(Generator<int> source, int n) {
    if (n <= 0) co_return;
    for (int value : source) {
        co_yield value;
        if (--n == 0) co_return;  // stop BEFORE asking the source for another value
    }
}
inline Generator<unsigned long long> take(Generator<unsigned long long> source, int n) {
    if (n <= 0) co_return;
    for (unsigned long long value : source) {
        co_yield value;
        if (--n == 0) co_return;
    }
}

inline Generator<std::string> words(std::string text) {  // by value: the frame keeps its own copy
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = text.find_first_not_of(' ', pos);
        if (start == std::string::npos) co_return;
        const std::size_t end = text.find(' ', start);
        if (end == std::string::npos) {
            co_yield text.substr(start);
            co_return;
        }
        co_yield text.substr(start, end - start);
        pos = end;
    }
}
