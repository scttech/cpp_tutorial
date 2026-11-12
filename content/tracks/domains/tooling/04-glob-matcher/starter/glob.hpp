#pragma once
// YOUR GLOB MATCHER. namespace glob. A pattern is matched against a whole path (forward slashes separate segments). Bytes, not characters: '?' matches one BYTE.
//
// PATTERN SYNTAX
//   ?          any one character except '/'
//   *          any run of characters (also none) except '/'                       "*.cpp" matches "a.cpp" but not "dir/a.cpp"
//   **         when it is a WHOLE path segment (nothing but '**' between two '/' or the ends of the pattern): matches across segments
//                "**"       the whole pattern: everything, any number of characters including '/'
//                "**/x"     at the start: "x" in any directory (zero or more leading "segment/" pieces):  matches "x", "d/x", "d/e/x"
//                "a/**/x"   in the middle: zero or more whole directories between:                       matches "a/x", "a/d/x", "a/d/e/x"
//                "a/**"     at the end: ONE OR MORE characters after "a/":                                matches "a/x", "a/d/x", but neither "a" nor "a/"
//              '**' that is not a whole segment ("a**b", "**x") is just '*'.
//   [abc] [a-z] [!abc] [^abc]   one character from the set (or, with ! or ^, not from it). A ']' right after '[' (or after the negation) is a member of the set; '-' as the first or last member is itself.
//                A set never matches '/', not even a negated one or one that lists it. Backslash escapes inside a set. "[]" is not a set (the ']' is a member, so it is unterminated).
//   {a,b,c}    alternatives, each an arbitrary pattern (also empty, "{,x}", also with wildcards and nested braces); a ',' or '}' outside braces is an ordinary character.
//   \x         the character x, literally (\* \? \[ \{ \\ \,)
//   anything else matches itself. '/' matches only '/'.
// OPTIONS   Options { bool dot = false; bool ignore_case = false; }
//   dot == false: a path segment that starts with '.' (the first character of the path, or the one after a '/') can only be matched by a '.' written in the pattern: the wildcards * ? [..] and ** do not
//                 match it ("*" does not match ".git", ".*" does, "**/x" does not look inside ".hidden/"). dot == true: they do.
//   ignore_case: ASCII letters compare case-insensitively, in literals and in sets (ranges too: [a-c] matches 'B').
// check(pattern) -> optional<Error>      nullopt if the pattern is valid. Error { size_t position; std::string message }; position is a byte offset:
//     unterminated set: the '[' ; reversed range ("[z-a]"): the range's first character; unterminated brace: the '{' ; trailing backslash: the backslash ; braces nested deeper than 32: the 33rd '{'.
// Glob::compile(pattern, options) -> optional<Glob>     nullopt for an invalid pattern.      Glob::matches(path) const -> bool      the whole path must match.
// match(pattern, path, options = {}) -> bool            compile + matches; an invalid pattern matches nothing.
//
// REQUIREMENTS   matches() must run in O(length of path x size of pattern) time, whatever the pattern: simulate the pattern as an NFA (all possible positions at once), never backtrack.
//                ("*a*a*a*a*a*a*a*a*a*b" against 200 'a's must not take longer than a blink.) No recursion over the path or over a long pattern: a pattern of a million characters must not overflow the stack
//                (only nesting of { } is recursive, and it is limited to 32). matches() makes a bounded number of heap allocations, independent of the path length.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glob {

struct Options {
    bool dot = false;
    bool ignore_case = false;
};

struct Error {
    std::size_t position;
    std::string message;
};

inline std::optional<Error> check(std::string_view) { return std::nullopt; }   // TODO

class Glob {
public:
    static std::optional<Glob> compile(std::string_view, Options = {}) { return std::nullopt; }   // TODO
    bool matches(std::string_view) const { return false; }   // TODO
};

inline bool match(std::string_view pattern, std::string_view path, Options options = {}) {
    const auto g = Glob::compile(pattern, options);
    return g && g->matches(path);
}

}  // namespace glob
