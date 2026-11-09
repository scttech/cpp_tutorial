# Abstract classes and interfaces

A class with a **pure virtual function** (`= 0`) is **abstract**: it says *what* must be done and leaves *how* to the derived classes. You cannot create an object of it, only of a concrete class that implements every pure virtual function. A class that is *nothing but* pure virtual functions (and a virtual destructor) is an **interface**, C++'s answer to Java's `interface`: there is no keyword, it is a convention.

```cpp
class Sink {                                   // an interface
public:
    virtual ~Sink() = default;                 // always: it will be deleted through a Sink*
    virtual void write(Level level, std::string_view message) = 0;   // pure virtual
};

class MemorySink : public Sink {               // concrete: implements everything
public:
    void write(Level level, std::string_view message) override { lines_.push_back(...); }
};

Sink s;                       // ERROR: Sink is abstract
std::unique_ptr<Sink> p = std::make_unique<MemorySink>();   // fine: a Sink pointer to a concrete object
```

That is a **plugin architecture**: `Logger` knows only the `Sink` interface, so a new kind of sink (a file, a network socket, a test spy) needs no change to the logger.

## Design rules that go with it

- **Virtual destructor**, as always: an interface is deleted through a pointer to itself.
- **Not copyable.** `Sink(const Sink&) = delete;` An interface copied by value is the slicing bug of lesson 1; deleting the copy operations makes the mistake a compile error (and every derived class inherits the deletion). The default constructor must then be declared (`protected: Sink() = default;`) so derived classes can still be built. Real polymorphic copying uses a virtual `clone()`.
- **Small interfaces (interface segregation).** `Sink` is only about *writing*; the ability to be *flushed* is a separate interface, `Flushable`. A client that only writes never sees `flush()`, and a sink that cannot be flushed does not have to pretend. A class can **implement several interfaces** with multiple inheritance: `class BufferedSink : public Sink, public Flushable`. (Multiple inheritance of *interfaces*, which hold no data, is the uncontroversial kind.)
- **A pure virtual function may still have a body** (defined outside the class). A derived class must override it anyway, and may *call* the body as its default: `Sink::format(level, message)`. This is how you offer a default and still force every derived class to make an explicit choice. (A pure virtual **destructor** *must* have a body, because every derived destructor calls it.)

## Decorators and ownership

A **decorator** wraps another object that has the same interface: `FilterSink` *is a* `Sink`, takes a `Sink&` and forwards the messages it likes; `BufferedSink` holds them until `flush()`. Decorators can be stacked (`BufferedSink` → `FilterSink` → `MemorySink`) because each one needs only the interface. Two lifetime rules:

- The wrapped object must **outlive** the decorator when the decorator holds a *reference*. Declare the target first (it is destroyed last). `BufferedSink` flushes in its destructor, which writes into the target one last time.
- A `std::string_view` parameter is only valid during the call: **a sink that stores the message must copy it** into a `std::string`.

The `Logger` owns its sinks as `std::vector<std::unique_ptr<Sink>>`. Its `emplace<S>(args...)` builds one in place and returns an `S&` with the **real type**, so the caller can still use `BufferedSink::pending()`; `std::derived_from<S, Sink>` in the template head rejects types that are not sinks with a readable error.

## Asking sideways: `dynamic_cast` between interfaces

`Logger::flush_all()` holds `Sink`s, but wants to flush the ones that are also `Flushable`. `Sink` and `Flushable` are unrelated, so no implicit conversion exists, but `dynamic_cast<Flushable*>(sink_ptr)` checks the **real** object at run time and succeeds if it has a `Flushable` base: a *cross-cast*. (A virtual function in `Sink` that returns `Flushable*`, or a `can_flush()` query, is the alternative that needs no RTTI.)

## Your task

Fill in `logging.hpp` (numbered TODOs): the two interfaces, `MemorySink`, `JsonSink`, `FilterSink`, `BufferedSink` and the `Logger`. `main.cpp` (read-only) wires several of them together and prints the result.

The tiers: **Works** (the sinks and the Logger behave), **Robust** (the interfaces really are abstract, non-copyable and segregated; ownership and destruction; copies of messages; the decorators), **Clean** (no raw `new`/`delete`, `override` on every override, `virtual` only in the two interfaces).
