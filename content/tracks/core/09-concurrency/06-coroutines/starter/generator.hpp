#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <utility>

// A COROUTINE is a function that can stop in the middle (co_yield, co_await) and be resumed later, keeping its local variables. A GENERATOR is the simplest
// use: a function that produces a sequence of values ONE AT A TIME, when asked:
//
//     Generator<int> range(int begin, int end) { for (int i = begin; i < end; ++i) co_yield i; }
//     for (int x : range(0, 5)) ...        // 0 1 2 3 4: each value is computed when the loop asks for it
//
// The compiler turns such a function into a state machine whose local variables live in a heap block (the coroutine FRAME). What the compiler does at
// every step is decided by the return type, through its nested `promise_type`: this file is that return type, and four small coroutines that use it.
// THE STARTER IS A SKELETON THAT NEVER SUSPENDS AND NEVER PRODUCES A VALUE. The lesson text explains each piece.

template <class T>
class Generator {
public:
    struct promise_type {
        std::optional<T> current;  // the value that was yielded last

        Generator get_return_object() { return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }

        // TODO 1: a generator must not start running until the first value is asked for ("lazy start"): return std::suspend_always.
        std::suspend_never initial_suspend() noexcept { return {}; }

        // TODO 2: when the body ends, stay suspended (std::suspend_always), so that the Generator can still ask the handle whether it is done() and
        //         can destroy the frame itself. (If the coroutine destroys its own frame at the end, the handle would dangle.)
        std::suspend_never final_suspend() noexcept { return {}; }

        // TODO 3: `co_yield value;` calls this. Keep a copy of the value in `current`, and SUSPEND (std::suspend_always): that is what hands the
        //         value to the caller.
        std::suspend_never yield_value(T value) {
            current = std::move(value);
            return {};
        }

        void return_void() {}                          // `co_return;` or running off the end of the body
        void unhandled_exception() { std::terminate(); }  // (this lesson does not use exceptions inside coroutines)
    };

    Generator() = default;  // an empty generator: no values
    explicit Generator(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    // TODO 4: the Generator OWNS the coroutine frame, like a unique_ptr owns memory: the destructor destroys it (handle_.destroy()), copying is
    //         deleted, and moving transfers the handle and leaves the source empty (std::exchange(other.handle_, {})).
    Generator(const Generator&) = default;
    Generator& operator=(const Generator&) = default;

    // TODO 5: next() resumes the coroutine until its next co_yield (or until it ends) and says whether there is a value. It must work when called
    //         again after the end (resuming a finished coroutine is undefined behavior: check done() first), and on an empty Generator.
    bool next() { return false; }

    // The value that the last successful next() produced.
    const T& value() const { return *handle_.promise().current; }

    // TODO 6: range-for support. begin() calls next() once and returns an iterator; ++ calls next(); * gives value(); the iterator equals `end()`
    //         (a sentinel) when the generator has no more values.
    struct Sentinel {};
    struct Iterator {
        Generator* owner = nullptr;
        bool operator==(Sentinel) const { return true; }
        const T& operator*() const { return owner->value(); }
        Iterator& operator++() { return *this; }
    };
    Iterator begin() { return Iterator{this}; }
    Sentinel end() { return {}; }

private:
    std::coroutine_handle<promise_type> handle_{};
};

// TODO 7: write these four as COROUTINES (each needs co_yield or co_return in its body). The stubs below are ordinary functions that return an
// empty generator. Parameters of a coroutine are COPIED into its frame, so take them BY VALUE: a reference parameter would dangle as soon as the
// caller's temporary is gone.

// range(begin, end): begin, begin + 1, ..., end - 1 (nothing if begin >= end)
inline Generator<int> range(int begin, int end) {
    (void)begin;
    (void)end;
    return {};
}

// fibonacci(): 0, 1, 1, 2, 3, 5, 8, ... for ever (it never ends: the caller decides how much to take)
inline Generator<unsigned long long> fibonacci() { return {}; }

// take(source, n): the first n values of `source` (fewer if it has fewer). Lazy: after the n-th value it must NOT ask `source` for another one.
inline Generator<int> take(Generator<int> source, int n) {
    (void)source;
    (void)n;
    return {};
}
inline Generator<unsigned long long> take(Generator<unsigned long long> source, int n) {
    (void)source;
    (void)n;
    return {};
}

// words(text): the words of `text`, separated by spaces; several spaces in a row count as one, and leading or trailing spaces give no empty words
inline Generator<std::string> words(std::string text) {
    (void)text;
    return {};
}
