#pragma once
// Read-only: what a log call is made of, and the two interfaces the logger talks to.
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slog {

enum class Level { Trace, Debug, Info, Warn, Error, Fatal };

struct Value {
    enum class Type { String, Int, Double, Bool, Null };
    Type type = Type::Null;
    std::string s;
    std::int64_t i = 0;
    double d = 0;
    bool b = false;
};

struct Field {
    std::string key;
    Value value;
};

// building fields: kv("user", "ann"), kv("attempt", 3), kv("ratio", 0.25), kv("ok", true), kv_null("error")
inline Field kv(std::string key, std::string value) {
    Value v;
    v.type = Value::Type::String;
    v.s = std::move(value);
    return Field{std::move(key), std::move(v)};
}
inline Field kv(std::string key, const char* value) { return kv(std::move(key), std::string(value)); }
inline Field kv(std::string key, std::int64_t value) {
    Value v;
    v.type = Value::Type::Int;
    v.i = value;
    return Field{std::move(key), std::move(v)};
}
inline Field kv(std::string key, int value) { return kv(std::move(key), static_cast<std::int64_t>(value)); }
inline Field kv(std::string key, double value) {
    Value v;
    v.type = Value::Type::Double;
    v.d = value;
    return Field{std::move(key), std::move(v)};
}
inline Field kv(std::string key, bool value) {
    Value v;
    v.type = Value::Type::Bool;
    v.b = value;
    return Field{std::move(key), std::move(v)};
}
inline Field kv_null(std::string key) { return Field{std::move(key), Value{}}; }

// One log event, as the formatter sees it.
struct Record {
    Level level = Level::Info;
    std::int64_t timestamp_ms = 0;   // milliseconds since 1970-01-01T00:00:00Z (negative: before)
    std::string message;
    std::vector<Field> fields;
};

// Where finished lines go. `line` has no trailing newline; the sink adds it if it wants one.
class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(std::string_view line) = 0;
};

// The time source, so that tests can control it.
class Clock {
public:
    virtual ~Clock() = default;
    virtual std::int64_t now_ms() = 0;
};

}  // namespace slog
