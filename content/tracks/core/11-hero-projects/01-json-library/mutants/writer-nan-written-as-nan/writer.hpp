// MUTANT: NaN and infinity are written as nan / inf
#pragma once
// to_string(v): the compact text; to_string(v, n): pretty, n spaces per level. One output buffer, appended to.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "value.hpp"

namespace json {
namespace detail {

inline void write_string(std::string& out, const std::string& s) {
    out.push_back('"');
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
}

inline void write_number(std::string& out, double d) {
    char buf[40];
    if (d == std::floor(d) && std::fabs(d) < 1e15) {
        std::snprintf(buf, sizeof buf, "%.0f", d);  // whole numbers without a fraction ("-0" stays "-0")
    } else {
        for (int precision = 1; precision <= 17; ++precision) {  // the shortest text that reads back as exactly d
            std::snprintf(buf, sizeof buf, "%.*g", precision, d);
            if (std::strtod(buf, nullptr) == d) break;
        }
    }
    out += buf;
}

inline void newline(std::string& out, int indent, int level) {
    out.push_back('\n');
    out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(level), ' ');
}

inline void write_value(std::string& out, const Value& v, int indent, int level) {
    const bool pretty = indent >= 0;
    if (v.is_null()) {
        out += "null";
    } else if (v.is_bool()) {
        out += v.as_bool() ? "true" : "false";
    } else if (v.is_number()) {
        write_number(out, v.as_number());
    } else if (v.is_string()) {
        write_string(out, v.as_string());
    } else if (v.is_array()) {
        const Array& items = v.as_array();
        if (items.empty()) {
            out += "[]";
            return;
        }
        out.push_back('[');
        bool first = true;
        for (const Value& item : items) {
            if (!first) out.push_back(',');
            first = false;
            if (pretty) newline(out, indent, level + 1);
            write_value(out, item, indent, level + 1);
        }
        if (pretty) newline(out, indent, level);
        out.push_back(']');
    } else {
        const Object& members = v.as_object();
        if (members.empty()) {
            out += "{}";
            return;
        }
        out.push_back('{');
        bool first = true;
        for (const auto& [key, item] : members) {
            if (!first) out.push_back(',');
            first = false;
            if (pretty) newline(out, indent, level + 1);
            write_string(out, key);
            out += pretty ? ": " : ":";
            write_value(out, item, indent, level + 1);
        }
        if (pretty) newline(out, indent, level);
        out.push_back('}');
    }
}

}  // namespace detail

inline std::string to_string(const Value& v, int indent = -1) {
    std::string out;
    detail::write_value(out, v, indent, 0);
    return out;
}

}  // namespace json
