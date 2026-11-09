#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// A plugin-style logger: a Logger owns a list of Sinks, and each Sink decides what to do with a message. New kinds of sink can be added
// without touching the Logger. Every TODO is a numbered step. The Clean tier reads this file as well.

enum class Level { Debug, Info, Warn, Error };

inline const char* to_string(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "?";
}

// ---- Two SMALL interfaces ---------------------------------------------------------------------------------------------------------------------
// TODO 1: Sink is an interface: ABSTRACT (write is pure virtual), with a VIRTUAL destructor, and NOT copyable (delete the copy constructor and
//         copy assignment; give it a protected default constructor so derived classes can still be built).
// TODO 2: format is pure virtual too, but ALSO has a body (a pure virtual function may be defined, outside the class): the default format is
//         "[WARN] message" (`[`, the level text from to_string, `] `, the message). Derived classes call it as `Sink::format(level, message)`.
class Sink {
public:
    virtual void write(Level level, std::string_view message) {
        (void)level;
        (void)message;
    }
    virtual std::string format(Level level, std::string_view message) const {
        (void)level;
        (void)message;
        return "";
    }
};

// TODO 3: Flushable is another interface (virtual destructor, one pure virtual function). Not every sink can be flushed, so it is NOT part of
//         Sink: clients that need only to write never see flush (interface segregation).
class Flushable {
public:
    virtual void flush() {}
};

// ---- Concrete sinks ----------------------------------------------------------------------------------------------------------------------------
// TODO 4: MemorySink keeps the FORMATTED lines (`format(level, message)`, a virtual call) in lines(); format is the default: Sink::format(...).
//         Mark everything that overrides with `override`.
class MemorySink : public Sink {
public:
    void write(Level level, std::string_view message) override {
        (void)level;
        (void)message;
    }
    std::string format(Level level, std::string_view message) const override { return Sink::format(level, message); }
    const std::vector<std::string>& lines() const { return lines_; }

private:
    std::vector<std::string> lines_;
};

// TODO 5: JsonSink is a MemorySink (a leaf class: final) that overrides ONLY format: {"level":"WARN","message":"disk low"} with `"`, `\`
//         and newline in the message escaped as `\"`, `\\` and `\n`. (MemorySink::write calls format virtually, so it stores JSON lines.)
class JsonSink : public MemorySink {};

// TODO 6: FilterSink(target, minimum) forwards write to the target sink, but only for levels >= minimum (Level compares in order Debug <
//         Info < Warn < Error); its format is the target's format. A decorator: it holds a reference, and the target must outlive it.
class FilterSink : public Sink {
public:
    FilterSink(Sink& target, Level minimum) : target_(target), minimum_(minimum) {}
    void write(Level level, std::string_view message) override {
        (void)level;
        (void)message;
    }
    std::string format(Level level, std::string_view message) const override { return target_.format(level, message); }

private:
    Sink& target_;
    Level minimum_;
};

// TODO 7: BufferedSink(target) IS a Sink AND IS Flushable (two bases). write queues the message (the level and a std::string COPY: the
//         string_view may not outlive the call); flush() writes everything queued to the target, in order, and empties the queue; its destructor
//         flushes too; pending() is the number of queued messages; format is the target's.
class BufferedSink : public Sink, public Flushable {
public:
    explicit BufferedSink(Sink& target) : target_(target) {}
    void write(Level level, std::string_view message) override {
        (void)level;
        (void)message;
    }
    std::string format(Level level, std::string_view message) const override { return target_.format(level, message); }
    std::size_t pending() const { return 0; }

private:
    Sink& target_;
};

// ---- The logger owns a list of sinks -----------------------------------------------------------------------------------------------------------
class Logger {
public:
    // TODO 8: add takes OWNERSHIP of the sink and returns a reference to it; a null pointer is std::invalid_argument.
    Sink& add(std::unique_ptr<Sink> sink) {
        static MemorySink unused;
        (void)sink;
        return unused;
    }

    // TODO 9: emplace<S>(args...) builds the sink in place and returns it with its REAL type (MemorySink&). S must derive from Sink:
    //         the signature already says so (a concept), you write the body.
    template <std::derived_from<Sink> S, class... Args>
    S& emplace(Args&&... args) {
        static S unused(std::forward<Args>(args)...);
        return unused;
    }

    // TODO 10: log writes the message to EVERY sink, in the order they were added; debug/info/warn/error are shorthands.
    void log(Level level, std::string_view message) {
        (void)level;
        (void)message;
    }
    void debug(std::string_view message) { log(Level::Debug, message); }
    void info(std::string_view message) { log(Level::Info, message); }
    void warn(std::string_view message) { log(Level::Warn, message); }
    void error(std::string_view message) { log(Level::Error, message); }

    // TODO 11: flush every sink that IS Flushable, and return how many it flushed. A Sink* does not convert to a Flushable* (they are
    //          unrelated interfaces): ask with a SIDEWAYS dynamic_cast<Flushable*>(sink_ptr).
    std::size_t flush_all() { return 0; }

    std::size_t size() const { return sinks_.size(); }

private:
    std::vector<std::unique_ptr<Sink>> sinks_;
};
