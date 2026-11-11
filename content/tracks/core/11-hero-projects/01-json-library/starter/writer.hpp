#pragma once
// YOUR WRITER. to_string(v) gives the compact text; to_string(v, 2) the pretty text with 2 spaces per level.
// The skeleton writes null/true/false only: everything else is yours.
#include <string>

#include "value.hpp"

namespace json {
namespace detail {

inline void write_value(std::string& out, const Value& v, int indent, int level) {
    (void)indent;
    (void)level;
    if (v.is_bool()) {
        out += v.as_bool() ? "true" : "false";
    } else {
        out += "null";  // TODO: numbers, strings, arrays, objects
    }
}

}  // namespace detail

inline std::string to_string(const Value& v, int indent = -1) {
    std::string out;
    detail::write_value(out, v, indent, 0);
    return out;
}

}  // namespace json
