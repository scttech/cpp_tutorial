#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

// Runs a function when it goes out of scope: on a normal exit, an early `return`, a `break`, or an exception.
class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> on_exit) : on_exit_(std::move(on_exit)) {}

    // TODO: a destructor that runs on_exit_ (if there is one). It must never let an exception escape:
    // wrap the call in try { ... } catch (...) { }.

    // TODO: copying a guard would run the cleanup twice: delete the copy constructor and the copy assignment.

    // TODO: dismiss() cancels the cleanup (the guard becomes inactive); active() says whether it would still run.
    void dismiss() noexcept {}
    bool active() const noexcept { return false; }

private:
    std::function<void()> on_exit_;
};

// A session acquires something in its constructor (a heap-allocated token) and must release it in its destructor.
// It also writes "open <name>" / "close <name>" to a log so that the order of events is visible.
class Session {
public:
    Session(std::vector<std::string>& log, std::string name) : log_(log), name_(std::move(name)), token_(new int(++issued_)) {
        log_.push_back("open " + name_);
    }

    // TODO: a destructor that logs "close <name>" and frees the token (`delete token_;`).
    // TODO: a session owns its token, so it must not be copied: delete the copy constructor and copy assignment.

    const std::string& name() const { return name_; }
    int token() const { return *token_; }

private:
    inline static int issued_ = 0;  // how many sessions have ever been opened: the tokens are 1, 2, 3, ...
    std::vector<std::string>& log_;
    std::string name_;
    int* token_;
};
