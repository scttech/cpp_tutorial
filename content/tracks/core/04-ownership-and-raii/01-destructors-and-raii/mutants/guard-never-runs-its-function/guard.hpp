// MUTANT: the destructor does not call the function.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

// Runs a function when it goes out of scope: on a normal exit, an early `return`, a `break`, or an exception.
class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> on_exit) : on_exit_(std::move(on_exit)) {}

    // The destructor is where the cleanup lives: the compiler calls it on EVERY way out of the scope.
    // It must never let an exception escape (a destructor is noexcept): during stack unwinding a second exception
    // would end the program.
    ~ScopeGuard() {
        if (!on_exit_) return;  // dismissed, or built from an empty function
        try {
        } catch (...) {
            // nothing sensible can be done here; swallow it
        }
    }

    // Copying a guard would run the cleanup twice. Say so, and the compiler refuses to compile a copy.
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    // "Never mind": the cleanup will not run.
    void dismiss() noexcept { on_exit_ = nullptr; }
    bool active() const noexcept { return static_cast<bool>(on_exit_); }

private:
    std::function<void()> on_exit_;
};

// A session that acquires something in its constructor (a heap-allocated token) and releases it in its destructor,
// and writes "open <name>" / "close <name>" to a log so that the order of events is visible.
// The rule of RAII: constructor acquires, destructor releases, and nothing else ever has to remember.
class Session {
public:
    Session(std::vector<std::string>& log, std::string name) : log_(log), name_(std::move(name)), token_(new int(++issued_)) {
        log_.push_back("open " + name_);
    }

    ~Session() {
        log_.push_back("close " + name_);
        delete token_;
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    const std::string& name() const { return name_; }
    int token() const { return *token_; }

private:
    inline static int issued_ = 0;  // how many sessions have ever been opened: the tokens are 1, 2, 3, ...
    std::vector<std::string>& log_;
    std::string name_;
    int* token_;
};
