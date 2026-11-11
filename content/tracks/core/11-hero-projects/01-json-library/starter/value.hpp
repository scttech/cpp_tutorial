#pragma once
// READ-ONLY. The JSON value type and the result of parsing (you write parser.hpp and writer.hpp).
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace json {

namespace stats {
inline unsigned long long copies = 0;  // how many times a Value was COPIED (moves are free)
inline void reset() { copies = 0; }
}  // namespace stats

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;  // keys come out sorted

// null | bool | number | string | array | object
struct Value {
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data;

    Value() : data(nullptr) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(bool b) : data(b) {}
    Value(int n) : data(static_cast<double>(n)) {}
    Value(double d) : data(d) {}
    Value(const char* s) : data(std::string(s)) {}
    Value(std::string s) : data(std::move(s)) {}
    Value(Array a) : data(std::move(a)) {}
    Value(Object o) : data(std::move(o)) {}

    Value(const Value& other) : data(other.data) { ++stats::copies; }
    Value(Value&&) noexcept = default;
    Value& operator=(const Value& other) {
        data = other.data;
        ++stats::copies;
        return *this;
    }
    Value& operator=(Value&&) noexcept = default;

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<Array>(data); }
    bool is_object() const { return std::holds_alternative<Object>(data); }

    bool as_bool() const { return std::get<bool>(data); }
    double as_number() const { return std::get<double>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    const Array& as_array() const { return std::get<Array>(data); }
    const Object& as_object() const { return std::get<Object>(data); }

    friend bool operator==(const Value& a, const Value& b) { return a.data == b.data; }
};

enum class Error {
    None,
    UnexpectedEnd,        // the text ended where something was still needed
    UnexpectedCharacter,  // a character that cannot be here
    BadNumber,            // the number grammar is broken (the position is where a digit was needed)
    NumberOutOfRange,     // a well-formed number that does not fit in a double (position: its first character)
    BadString,            // a raw control character (below U+0020) inside a string
    BadEscape,            // a broken \ escape (position: its backslash)
    TooDeep,              // more than kMaxDepth arrays/objects inside each other
    TrailingContent,      // something other than whitespace after the document
};

inline constexpr int kMaxDepth = 128;

inline const char* name(Error e) {
    switch (e) {
        case Error::None: return "None";
        case Error::UnexpectedEnd: return "UnexpectedEnd";
        case Error::UnexpectedCharacter: return "UnexpectedCharacter";
        case Error::BadNumber: return "BadNumber";
        case Error::NumberOutOfRange: return "NumberOutOfRange";
        case Error::BadString: return "BadString";
        case Error::BadEscape: return "BadEscape";
        case Error::TooDeep: return "TooDeep";
        case Error::TrailingContent: return "TrailingContent";
    }
    return "?";
}

struct ParseResult {
    Value value;                  // the document (null when there is an error)
    Error error = Error::None;
    std::size_t line = 0;         // 1-based, only when there is an error
    std::size_t column = 0;       // 1-based, counted in bytes, only when there is an error
    std::string message;          // for people: never empty when there is an error

    bool ok() const { return error == Error::None; }
};

}  // namespace json
