#pragma once
// READ-ONLY. The text type (it counts what is done to it), the commands, the reply, and the error codes.
#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace db {

namespace probe {
inline unsigned long long compares = 0;  // Text == and Text < / <=>
inline unsigned long long copies = 0;    // Text copied (moves are free)
inline void reset() { compares = copies = 0; }
}  // namespace probe

// A key or a value. A std::string that counts its comparisons and copies, so that the tests can see
// what your data structures do. Move a Text into a container instead of copying it.
struct Text {
    std::string s;

    Text() = default;
    explicit Text(std::string v) : s(std::move(v)) {}
    explicit Text(std::string_view v) : s(v) {}
    explicit Text(const char* v) : s(v) {}
    Text(const Text& other) : s(other.s) { ++probe::copies; }
    Text(Text&&) noexcept = default;
    Text& operator=(const Text& other) {
        s = other.s;
        ++probe::copies;
        return *this;
    }
    Text& operator=(Text&&) noexcept = default;

    friend bool operator==(const Text& a, const Text& b) {
        ++probe::compares;
        return a.s == b.s;
    }
    friend std::strong_ordering operator<=>(const Text& a, const Text& b) {
        ++probe::compares;
        return a.s <=> b.s;  // bytewise: "B" < "a"
    }
};

}  // namespace db

template <>
struct std::hash<db::Text> {
    std::size_t operator()(const db::Text& t) const noexcept { return std::hash<std::string>{}(t.s); }
};

namespace db {

// ---- the commands (what query.hpp produces and database.hpp executes) -----------------------
struct Set { Text key; Text value; };
struct Get { Text key; };
struct Del { Text key; };
struct Count { Text value; };
struct Range { Text lo; Text hi; };
struct Incr { Text key; };
struct Begin {};
struct Rollback {};
struct Commit {};
using Command = std::variant<Set, Get, Del, Count, Range, Incr, Begin, Rollback, Commit>;

enum class ParseError {
    None,
    Empty,               // nothing but whitespace (position: the end of the line)
    UnknownCommand,      // position: the first token
    WrongArity,          // wrong number of arguments (position: the first token)
    UnterminatedString,  // a quote that is never closed (position: the opening quote)
    BadEscape,           // a backslash escape other than \" \\ \n \t (position: the backslash)
    StrayQuote,          // a quote inside a bare word, or text glued to a closing quote (position: that character)
};

inline const char* name(ParseError e) {
    switch (e) {
        case ParseError::None: return "None";
        case ParseError::Empty: return "Empty";
        case ParseError::UnknownCommand: return "UnknownCommand";
        case ParseError::WrongArity: return "WrongArity";
        case ParseError::UnterminatedString: return "UnterminatedString";
        case ParseError::BadEscape: return "BadEscape";
        case ParseError::StrayQuote: return "StrayQuote";
    }
    return "?";
}

struct ParsedCommand {
    Command command;
    ParseError error = ParseError::None;
    std::size_t pos = 0;    // 0-based byte offset in the line
    std::string message;    // for people, never empty when there is an error
    bool ok() const { return error == ParseError::None; }
};

// ---- the reply -----------------------------------------------------------------------------------
struct Reply {
    enum class Kind { Ok, Value, Nil, Integer, List, Error };
    Kind kind = Kind::Ok;
    std::string text;                // Value: the value.  Error: the message.
    long long number = 0;            // Integer
    std::vector<std::string> items;  // List

    static Reply ok() { return Reply{}; }
    static Reply value(std::string v) { Reply r; r.kind = Kind::Value; r.text = std::move(v); return r; }
    static Reply nil() { Reply r; r.kind = Kind::Nil; return r; }
    static Reply integer(long long n) { Reply r; r.kind = Kind::Integer; r.number = n; return r; }
    static Reply list(std::vector<std::string> v) { Reply r; r.kind = Kind::List; r.items = std::move(v); return r; }
    static Reply error(std::string message) { Reply r; r.kind = Kind::Error; r.text = std::move(message); return r; }
};

inline std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out + "\"";
}

// How a reply is shown: OK   "text"   (nil)   (integer) 5   1) "a" 2) "b"   (empty list)   ERR message
inline std::string describe(const Reply& r) {
    switch (r.kind) {
        case Reply::Kind::Ok: return "OK";
        case Reply::Kind::Value: return quoted(r.text);
        case Reply::Kind::Nil: return "(nil)";
        case Reply::Kind::Integer: return "(integer) " + std::to_string(r.number);
        case Reply::Kind::List: {
            if (r.items.empty()) return "(empty list)";
            std::string s;
            for (std::size_t i = 0; i < r.items.size(); ++i) s += (i ? " " : "") + std::to_string(i + 1) + ") " + quoted(r.items[i]);
            return s;
        }
        case Reply::Kind::Error: return "ERR " + r.text;
    }
    return "?";
}

}  // namespace db
