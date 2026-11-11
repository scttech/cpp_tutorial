// MUTANT: empty text is a different error
#pragma once
// A single pass over the text. Fields are views into it; nothing is copied. Numbers come from std::from_chars.
#include <charconv>
#include <cstddef>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace fix {
namespace detail {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

// all of `s` as a non-negative int; false if empty, not all digits, or too big
inline bool to_int(std::string_view s, int& out) {
    if (s.empty()) return false;
    for (const char c : s) {
        if (!is_digit(c)) return false;
    }
    const auto result = std::from_chars(s.data(), s.data() + s.size(), out);
    return result.ec == std::errc() && result.ptr == s.data() + s.size();
}

}  // namespace detail

inline int checksum_of(std::string_view text) {
    unsigned sum = 0;
    for (const char c : text) sum += static_cast<unsigned char>(c);
    return static_cast<int>(sum % 256);
}

inline ParseResult parse(std::string_view text) {
    ParseResult r;
    auto fail = [&r](Error e, std::size_t pos) {
        r.fields.clear();
        r.error = e;
        r.pos = pos;
        return r;
    };
    if (text.empty()) return fail(Error::BadBeginString, 0);

    std::vector<std::size_t> starts;  // where each field starts
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t end = text.find(kSoh, at);
        if (end == std::string_view::npos) return fail(Error::MissingDelimiter, at);  // the last field has no separator
        const std::string_view field = text.substr(at, end - at);
        const std::size_t eq = field.find('=');
        if (eq == std::string_view::npos) return fail(Error::MissingDelimiter, at);
        int tag = 0;
        if (!detail::to_int(field.substr(0, eq), tag) || tag <= 0) return fail(Error::BadTag, at);
        r.fields.push_back(Field{tag, field.substr(eq + 1)});
        starts.push_back(at);
        at = end + 1;
    }

    // the frame: 8=..., 9=..., ..., 10=...
    if (r.fields[0].tag != 8 || r.fields[0].value.empty()) return fail(Error::BadBeginString, 0);
    if (r.fields.size() < 2 || r.fields[1].tag != 9) return fail(Error::BadBodyLength, starts.size() > 1 ? starts[1] : text.size());
    int declared = 0;
    if (!detail::to_int(r.fields[1].value, declared)) return fail(Error::BadBodyLength, starts[1]);
    const std::size_t last = r.fields.size() - 1;
    if (last < 2 || r.fields[last].tag != 10) {
        // a "10=" field somewhere in the middle means there is data after it
        for (std::size_t i = 2; i < last; ++i) {
            if (r.fields[i].tag == 10) return fail(Error::TrailingData, starts[i + 1]);
        }
        return fail(Error::BadChecksum, starts[last]);
    }
    for (std::size_t i = 2; i < last; ++i) {
        if (r.fields[i].tag == 10) return fail(Error::TrailingData, starts[i + 1]);
    }
    const std::size_t body_start = starts[2 <= last ? 2 : last];
    const std::size_t body_end = starts[last];
    if (body_end - body_start != static_cast<std::size_t>(declared)) return fail(Error::BadBodyLength, body_start);
    int declared_sum = 0;
    if (r.fields[last].value.size() != 3 || !detail::to_int(r.fields[last].value, declared_sum)) return fail(Error::BadChecksum, starts[last]);
    if (declared_sum != checksum_of(text.substr(0, body_end))) return fail(Error::BadChecksum, starts[last]);
    return r;
}

inline const Field* find(const ParseResult& r, int tag) {
    for (const Field& f : r.fields) {
        if (f.tag == tag) return &f;
    }
    return nullptr;
}

}  // namespace fix
