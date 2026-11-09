// MUTANT: a quote breaks the JSON.
#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// ---- Two SMALL interfaces (interface segregation) --------------------------------------------------------------------------------------------
// Something that accepts log messages. Abstract: it has pure virtual functions, so no Sink object can exist, only derived ones.
class Sink {
public:
    virtual ~Sink() = default;
    Sink(const Sink&) = delete;  // an interface is never copied: copying through a base would slice
    Sink& operator=(const Sink&) = delete;

    virtual void write(Level level, std::string_view message) = 0;
    // Pure virtual WITH a body (defined below): every derived class must say what its format is, and may use this one as its default.
    virtual std::string format(Level level, std::string_view message) const = 0;

protected:
    Sink() = default;
};

inline std::string Sink::format(Level level, std::string_view message) const {
    return std::string("[") + to_string(level) + "] " + std::string(message);
}

// Something that holds data back until it is told to push it on. Not every sink does: that is why it is a separate interface.
class Flushable {
public:
    virtual ~Flushable() = default;
    virtual void flush() = 0;
};

// ---- Concrete sinks ----------------------------------------------------------------------------------------------------------------------------
// Keeps every formatted line in memory.
class MemorySink : public Sink {
public:
    void write(Level level, std::string_view message) override { lines_.push_back(format(level, message)); }
    std::string format(Level level, std::string_view message) const override { return Sink::format(level, message); }
    const std::vector<std::string>& lines() const { return lines_; }

private:
    std::vector<std::string> lines_;
};

// A MemorySink that formats as one JSON object per line: {"level":"WARN","message":"disk low"} (", \ and newlines escaped).
class JsonSink final : public MemorySink {
public:
    std::string format(Level level, std::string_view message) const override {
        std::string out = std::string("{\"level\":\"") + to_string(level) + "\",\"message\":\"";
        for (const char c : message) {
            if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else out += c;
        }
        out += "\"}";
        return out;
    }
};

// A decorator: forwards to another sink, but only messages at or above a minimum level.
class FilterSink final : public Sink {
public:
    FilterSink(Sink& target, Level minimum) : target_(target), minimum_(minimum) {}
    void write(Level level, std::string_view message) override {
        if (level >= minimum_) target_.write(level, message);
    }
    std::string format(Level level, std::string_view message) const override { return target_.format(level, message); }

private:
    Sink& target_;
    Level minimum_;
};

// A decorator that holds messages back until flush() (or its own destruction) passes them on, in order.
// Two interfaces: it IS a Sink and it IS Flushable.
class BufferedSink final : public Sink, public Flushable {
public:
    explicit BufferedSink(Sink& target) : target_(target) {}
    ~BufferedSink() override { flush(); }

    void write(Level level, std::string_view message) override { pending_.emplace_back(level, std::string(message)); }
    std::string format(Level level, std::string_view message) const override { return target_.format(level, message); }
    void flush() override {
        for (const auto& [level, message] : pending_) target_.write(level, message);
        pending_.clear();
    }
    std::size_t pending() const { return pending_.size(); }

private:
    Sink& target_;
    std::vector<std::pair<Level, std::string>> pending_;
};

// ---- The logger owns a list of sinks (the "plugins") -----------------------------------------------------------------------------------------
class Logger {
public:
    // Takes ownership. A null pointer is a mistake: std::invalid_argument. Returns the sink so the caller can keep a reference.
    Sink& add(std::unique_ptr<Sink> sink) {
        if (!sink) throw std::invalid_argument("a null sink");
        sinks_.push_back(std::move(sink));
        return *sinks_.back();
    }

    // Builds the sink in place and returns it with its REAL type: `MemorySink& m = logger.emplace<MemorySink>();`
    template <std::derived_from<Sink> S, class... Args>
    S& emplace(Args&&... args) {
        auto sink = std::make_unique<S>(std::forward<Args>(args)...);
        S& ref = *sink;
        sinks_.push_back(std::move(sink));
        return ref;
    }

    void log(Level level, std::string_view message) {
        for (const auto& s : sinks_) s->write(level, message);
    }
    void debug(std::string_view message) { log(Level::Debug, message); }
    void info(std::string_view message) { log(Level::Info, message); }
    void warn(std::string_view message) { log(Level::Warn, message); }
    void error(std::string_view message) { log(Level::Error, message); }

    // Flushes the sinks that ARE Flushable (a sideways dynamic_cast from Sink to Flushable); returns how many it flushed.
    std::size_t flush_all() {
        std::size_t flushed = 0;
        for (const auto& s : sinks_) {
            if (auto* f = dynamic_cast<Flushable*>(s.get())) {
                f->flush();
                ++flushed;
            }
        }
        return flushed;
    }

    std::size_t size() const { return sinks_.size(); }

private:
    std::vector<std::unique_ptr<Sink>> sinks_;
};
