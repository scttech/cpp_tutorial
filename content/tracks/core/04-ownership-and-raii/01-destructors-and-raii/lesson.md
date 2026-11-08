# Destructors and RAII

Some things must be given back: memory from `new`, a file you opened, a lock, a network connection. Forgetting is the classic
bug, and "remember to clean up on every path" does not work in real code: there is an early `return`, a `continue`, an exception
thrown three calls down.

C++ has a better answer, and it is the most important idea in the language: **RAII**, *Resource Acquisition Is Initialization*.

> Tie the lifetime of a resource to the lifetime of an **object**. The constructor acquires it, the **destructor** releases it.
> The compiler runs the destructor when the object goes out of scope, **on every way out**: normal exit, `return`, `break`,
> `continue`, or an exception. Nobody has to remember anything.

## The destructor

A destructor is a member function named `~ClassName()`, with no parameters and no return type. It runs automatically when an
object's lifetime ends:

```cpp
class Session {
public:
    Session(Log& log, std::string name) : name_(std::move(name)), token_(new int(42)) { log.push_back("open " + name_); }
    ~Session() { delete token_; }          // runs at the closing brace of the scope that declared it
private:
    std::string name_;
    int* token_;                           // the resource this object OWNS
};

void f() {
    Session s(log, "db");                  // acquired here
    if (bad()) return;                     // released here ...
    work();
}                                          // ... or here, or when work() throws: the same destructor call
```

Objects are destroyed in the **reverse order of their construction**, like unstacking plates. That matters when one resource
depends on another: the later one is closed first.

`std::vector`, `std::string`, `std::ifstream` and `std::unique_ptr` are all RAII classes: that is why this course keeps saying
"let the container own it". You are now learning how they do it.

## A scope guard

`ScopeGuard` is the smallest useful RAII class: it owns *an action* instead of a resource, and runs it in its destructor.

```cpp
{
    ScopeGuard cleanup([&] { std::cout << "done\n"; });
    work();                  // even if this throws, "done" is printed
}
```

## Destructors must not throw

If an exception is already travelling up the stack (*stack unwinding*), the destructors of the objects being cleaned up run on
the way. If one of those destructors throws a **second** exception, the program is ended on the spot (`std::terminate`).
That is why destructors are `noexcept` by default. If the thing you call from a destructor can throw, catch it there:

```cpp
~ScopeGuard() {
    try { on_exit_(); } catch (...) { /* nothing sensible to do: swallow it */ }
}
```

## Owners are not copyable (yet)

Suppose `Session` is copied. Now two `Session`s hold the same `token_`, both destructors `delete` it, and the second is a
**double free**. The compiler-generated copy simply copies the pointer. For now, forbid it:

```cpp
Session(const Session&) = delete;
Session& operator=(const Session&) = delete;
```

`= delete` makes any attempt to copy a compile error. The next lessons show how to make copies correct, or cheap to move.

## Your task

Complete `guard.hpp`. The two classes are partly written, and the starter has two problems: **press Run** and the
sanitizer's report names the first of them (LeakSanitizer finds a block that is never freed, and says where it was allocated).

| Class | Behavior |
|---|---|
| `ScopeGuard(std::function<void()>)` | Runs the function **exactly once**, in its destructor, however the scope ends, unless it was dismissed. An exception from the function must not escape the destructor. |
| `dismiss()`, `active()` | `dismiss()` cancels the cleanup. `active()` says whether it will still run. A guard built from an empty function is not active. |
| `ScopeGuard` copies | Deleted. |
| `Session` | Logs `open <name>` when made and `close <name>` when destroyed, and **frees its token** in the destructor (no leak, no double free). Not copyable. |

The tests check each of the ways a scope can end, the order of destruction, that a throwing cleanup does not end the program, and
(with the sanitizers and allocation tracking on) that nothing is leaked or freed twice.
