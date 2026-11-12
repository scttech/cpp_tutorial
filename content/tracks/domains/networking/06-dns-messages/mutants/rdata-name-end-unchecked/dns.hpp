// MUTANT: a name in rdata may end before or after rdlength
#pragma once
// A bounds-checked cursor over the packet; names are decoded with an iterative loop whose pointer targets strictly decrease; serialization keeps a table of suffix offsets.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace dns {
namespace detail {

inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool label_char_ok(char c) { return c >= 0x21 && c <= 0x7E && c != '.'; }

// The labels of a presentation name; nullopt if it is invalid. The root name has no labels.
inline std::optional<std::vector<std::string_view>> split_labels(std::string_view name) {
    if (!name.empty() && name.back() == '.') name.remove_suffix(1);
    std::vector<std::string_view> labels;
    if (name.empty()) return labels;
    std::size_t start = 0;
    std::size_t wire = 1;
    while (true) {
        const std::size_t dot = name.find('.', start);
        const std::string_view label = name.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (label.empty() || label.size() > 63) return std::nullopt;
        for (char c : label) {
            if (!label_char_ok(c)) return std::nullopt;
        }
        wire += label.size() + 1;
        if (wire > 255) return std::nullopt;
        labels.push_back(label);
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    return labels;
}

inline std::uint16_t be16(std::span<const std::uint8_t> m, std::size_t p) { return static_cast<std::uint16_t>((m[p] << 8) | m[p + 1]); }
inline std::uint32_t be32(std::span<const std::uint8_t> m, std::size_t p) { return (std::uint32_t{m[p]} << 24) | (std::uint32_t{m[p + 1]} << 16) | (std::uint32_t{m[p + 2]} << 8) | m[p + 3]; }
inline void put16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

inline bool has_name(std::uint16_t type) { return type == 2 || type == 5 || type == 12; }

}  // namespace detail

inline std::optional<std::vector<std::uint8_t>> encode_name(std::string_view name) {
    const auto labels = detail::split_labels(name);
    if (!labels) return std::nullopt;
    std::vector<std::uint8_t> out;
    for (std::string_view l : *labels) {
        out.push_back(static_cast<std::uint8_t>(l.size()));
        out.insert(out.end(), l.begin(), l.end());
    }
    out.push_back(0);
    return out;
}

inline std::optional<std::string> decode_name(std::span<const std::uint8_t> msg, std::size_t& pos) {
    std::size_t p = pos;
    std::size_t limit = pos;        // every pointer must land before this
    std::size_t after = 0;
    bool jumped = false;
    std::size_t wire = 1;           // the final zero byte
    std::string out;
    while (true) {
        if (p >= msg.size()) return std::nullopt;
        const std::uint8_t len = msg[p];
        if (len == 0) {
            ++p;
            break;
        }
        if ((len & 0xC0) == 0xC0) {
            if (p + 1 >= msg.size()) return std::nullopt;
            const std::size_t target = (static_cast<std::size_t>(len & 0x3F) << 8) | msg[p + 1];
            if (!jumped) {
                after = p + 2;
                jumped = true;
            }
            if (target >= limit || target < 12) return std::nullopt;
            limit = target;
            p = target;
            continue;
        }
        if ((len & 0xC0) != 0) return std::nullopt;   // the reserved label types 01 and 10
        if (p + 1 + len > msg.size()) return std::nullopt;
        wire += len + 1u;
        if (wire > 255) return std::nullopt;
        for (std::size_t i = 0; i < len; ++i) {
            if (!detail::label_char_ok(static_cast<char>(msg[p + 1 + i]))) return std::nullopt;
        }
        if (!out.empty()) out += '.';
        out.append(reinterpret_cast<const char*>(msg.data() + p + 1), len);
        p += 1u + len;
    }
    pos = jumped ? after : p;
    return out.empty() ? std::string(".") : out;
}

inline std::optional<Message> parse(std::span<const std::uint8_t> msg) {
    if (msg.size() < 12) return std::nullopt;
    Message m;
    m.id = detail::be16(msg, 0);
    m.flags = detail::be16(msg, 2);
    const std::size_t counts[4] = {detail::be16(msg, 4), detail::be16(msg, 6), detail::be16(msg, 8), detail::be16(msg, 10)};
    std::size_t pos = 12;
    for (std::size_t i = 0; i < counts[0]; ++i) {
        auto name = decode_name(msg, pos);
        if (!name || pos + 4 > msg.size()) return std::nullopt;
        m.questions.push_back(Question{*name, detail::be16(msg, pos), detail::be16(msg, pos + 2)});
        pos += 4;
    }
    std::vector<Record>* sections[3] = {&m.answers, &m.authorities, &m.additionals};
    for (int s = 0; s < 3; ++s) {
        for (std::size_t i = 0; i < counts[s + 1]; ++i) {
            auto name = decode_name(msg, pos);
            if (!name || pos + 10 > msg.size()) return std::nullopt;
            Record r;
            r.name = *name;
            r.type = detail::be16(msg, pos);
            r.cls = detail::be16(msg, pos + 2);
            r.ttl = detail::be32(msg, pos + 4);
            const std::size_t rdlen = detail::be16(msg, pos + 8);
            pos += 10;
            if (pos + rdlen > msg.size()) return std::nullopt;
            const std::size_t end = pos + rdlen;
            if (detail::has_name(r.type)) {
                std::size_t p = pos;
                const auto target = decode_name(msg, p);
                if (!target) return std::nullopt;
                const auto wire = encode_name(*target == "." ? std::string_view() : std::string_view(*target));
                if (!wire) return std::nullopt;
                r.rdata = *wire;
            } else if (r.type == 15) {
                if (rdlen < 3) return std::nullopt;
                std::size_t p = pos + 2;
                const auto target = decode_name(msg, p);
                if (!target || p != end) return std::nullopt;
                const auto wire = encode_name(*target == "." ? std::string_view() : std::string_view(*target));
                if (!wire) return std::nullopt;
                r.rdata.push_back(msg[pos]);
                r.rdata.push_back(msg[pos + 1]);
                r.rdata.insert(r.rdata.end(), wire->begin(), wire->end());
            } else {
                if ((r.type == 1 && rdlen != 4) || (r.type == 28 && rdlen != 16)) return std::nullopt;
                r.rdata.assign(msg.begin() + static_cast<std::ptrdiff_t>(pos), msg.begin() + static_cast<std::ptrdiff_t>(end));
            }
            pos = end;
            sections[s]->push_back(std::move(r));
        }
    }
    if (pos != msg.size()) return std::nullopt;   // trailing garbage
    return m;
}

inline std::optional<std::vector<std::uint8_t>> serialize(const Message& m) {
    if (m.questions.size() > 65535 || m.answers.size() > 65535 || m.authorities.size() > 65535 || m.additionals.size() > 65535) return std::nullopt;
    std::vector<std::uint8_t> out;
    std::unordered_map<std::string, std::uint16_t> known;   // lower-case suffix -> offset of its first occurrence
    detail::put16(out, m.id);
    detail::put16(out, m.flags);
    detail::put16(out, static_cast<std::uint16_t>(m.questions.size()));
    detail::put16(out, static_cast<std::uint16_t>(m.answers.size()));
    detail::put16(out, static_cast<std::uint16_t>(m.authorities.size()));
    detail::put16(out, static_cast<std::uint16_t>(m.additionals.size()));

    auto put_name = [&](std::string_view name) -> bool {
        const auto labels = detail::split_labels(name);
        if (!labels) return false;
        for (std::size_t i = 0; i < labels->size(); ++i) {
            std::string suffix;
            for (std::size_t k = i; k < labels->size(); ++k) {
                if (k > i) suffix += '.';
                for (char c : (*labels)[k]) suffix += detail::lower(c);
            }
            const auto it = known.find(suffix);
            if (it != known.end()) {
                detail::put16(out, static_cast<std::uint16_t>(0xC000 | it->second));
                return true;
            }
            if (out.size() < 0x4000) known.emplace(suffix, static_cast<std::uint16_t>(out.size()));
            out.push_back(static_cast<std::uint8_t>((*labels)[i].size()));
            out.insert(out.end(), (*labels)[i].begin(), (*labels)[i].end());
        }
        out.push_back(0);
        return true;
    };
    // a name in uncompressed wire form (the rdata convention), written compressed
    auto put_wire_name = [&](std::span<const std::uint8_t> wire) -> bool {
        std::size_t p = 0;
        const auto name = decode_name(wire, p);
        if (!name || p != wire.size()) return false;
        return put_name(*name == "." ? std::string_view() : std::string_view(*name));
    };

    for (const Question& q : m.questions) {
        if (!put_name(q.name)) return std::nullopt;
        detail::put16(out, q.type);
        detail::put16(out, q.cls);
    }
    for (const std::vector<Record>* section : {&m.answers, &m.authorities, &m.additionals}) {
        for (const Record& r : *section) {
            if (!put_name(r.name)) return std::nullopt;
            detail::put16(out, r.type);
            detail::put16(out, r.cls);
            for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>((r.ttl >> shift) & 0xFF));
            const std::size_t len_pos = out.size();
            out.push_back(0);
            out.push_back(0);
            if (detail::has_name(r.type)) {
                if (!put_wire_name(r.rdata)) return std::nullopt;
            } else if (r.type == 15) {
                if (r.rdata.size() < 3) return std::nullopt;
                out.push_back(r.rdata[0]);
                out.push_back(r.rdata[1]);
                if (!put_wire_name(std::span<const std::uint8_t>(r.rdata).subspan(2))) return std::nullopt;
            } else {
                if ((r.type == 1 && r.rdata.size() != 4) || (r.type == 28 && r.rdata.size() != 16)) return std::nullopt;
                out.insert(out.end(), r.rdata.begin(), r.rdata.end());
            }
            const std::size_t rdlen = out.size() - len_pos - 2;
            if (rdlen > 65535) return std::nullopt;
            out[len_pos] = static_cast<std::uint8_t>(rdlen >> 8);
            out[len_pos + 1] = static_cast<std::uint8_t>(rdlen & 0xFF);
        }
    }
    if (out.size() > 65535) return std::nullopt;
    return out;
}

}  // namespace dns
