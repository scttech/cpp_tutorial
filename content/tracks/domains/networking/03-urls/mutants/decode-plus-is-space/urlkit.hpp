// MUTANT: plus signs are decoded to spaces in paths too
#pragma once
// Hand-written RFC 3986 pieces: a validating percent decoder, dot-segment removal with an output stack, a splitting parser, and reference resolution.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "types.hpp"

namespace urlkit {
namespace detail {

inline int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
inline bool is_unreserved(char c) { return is_alpha(c) || is_digit(c) || c == '-' || c == '.' || c == '_' || c == '~'; }
inline bool is_control(char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; }

// Checks the escapes of a component and writes it with upper-case hex digits; nullopt if an escape is invalid or a forbidden character is present.
inline std::optional<std::string> clean_component(std::string_view text, bool allow_space_and_control = false) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '%') {
            if (i + 2 >= text.size()) return std::nullopt;
            const int h = hex_value(text[i + 1]), l = hex_value(text[i + 2]);
            if (h < 0 || l < 0) return std::nullopt;
            out += '%';
            out += "0123456789ABCDEF"[h];
            out += "0123456789ABCDEF"[l];
            i += 2;
        } else {
            if (!allow_space_and_control && (c == ' ' || is_control(c))) return std::nullopt;
            out += c;
        }
    }
    return out;
}

inline std::uint16_t default_port(const std::string& scheme) {
    if (scheme == "http" || scheme == "ws") return 80;
    if (scheme == "https" || scheme == "wss") return 443;
    if (scheme == "ftp") return 21;
    return 0;
}
inline bool path_defaults_to_slash(const std::string& scheme) { return scheme == "http" || scheme == "https" || scheme == "ws" || scheme == "wss"; }

}  // namespace detail

inline std::optional<std::string> percent_decode(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '%') {
            out += text[i] == '+' ? ' ' : text[i];
            continue;
        }
        if (i + 2 >= text.size()) return std::nullopt;
        const int h = detail::hex_value(text[i + 1]), l = detail::hex_value(text[i + 2]);
        if (h < 0 || l < 0) return std::nullopt;
        out += static_cast<char>(h * 16 + l);
        i += 2;
    }
    return out;
}

inline std::string percent_encode(std::string_view text, std::string_view keep = "") {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (detail::is_unreserved(c) || keep.find(c) != std::string_view::npos) {
            out += c;
        } else {
            const unsigned char u = static_cast<unsigned char>(c);
            out += '%';
            out += "0123456789ABCDEF"[u >> 4];
            out += "0123456789ABCDEF"[u & 15];
        }
    }
    return out;
}

inline std::string remove_dot_segments(std::string_view path) {
    // RFC 3986 5.2.4: `rest` is the "input buffer" (a view, never copied or erased from the front), `out` the "output buffer".
    std::string out;
    out.reserve(path.size());
    std::string_view rest = path;
    auto starts = [&](std::string_view prefix) { return rest.substr(0, prefix.size()) == prefix; };
    auto drop_last_segment = [&] {
        const std::size_t slash = out.rfind('/');
        if (slash == std::string::npos) out.clear();
        else out.erase(slash);
    };
    while (!rest.empty()) {
        if (starts("../")) {
            rest.remove_prefix(3);
        } else if (starts("./")) {
            rest.remove_prefix(2);
        } else if (starts("/./")) {
            rest.remove_prefix(2);
        } else if (rest == "/.") {
            rest = "/";
        } else if (starts("/../")) {
            rest.remove_prefix(3);
            drop_last_segment();
        } else if (rest == "/..") {
            rest = "/";
            drop_last_segment();
        } else if (rest == "." || rest == "..") {
            rest = std::string_view();
        } else {
            const std::size_t next = rest.find('/', rest.front() == '/' ? 1 : 0);
            const std::size_t len = next == std::string_view::npos ? rest.size() : next;
            out.append(rest.data(), len);
            rest.remove_prefix(len);
        }
    }
    return out;
}

inline std::optional<Url> parse(std::string_view text) {
    Url url;
    // scheme
    std::size_t i = 0;
    if (text.empty() || !detail::is_alpha(text[0])) return std::nullopt;
    while (i < text.size() && (detail::is_alpha(text[i]) || detail::is_digit(text[i]) || text[i] == '+' || text[i] == '-' || text[i] == '.')) ++i;
    if (text.substr(i, 3) != "://") return std::nullopt;
    for (std::size_t k = 0; k < i; ++k) url.scheme += detail::lower(text[k]);
    i += 3;
    // authority
    const std::size_t auth_end = text.find_first_of("/?#", i);
    const std::string_view authority = text.substr(i, auth_end == std::string_view::npos ? std::string_view::npos : auth_end - i);
    std::string_view rest = auth_end == std::string_view::npos ? std::string_view() : text.substr(auth_end);
    std::string_view hostport = authority;
    const std::size_t at = authority.rfind('@');
    if (at != std::string_view::npos) {
        const auto ui = detail::clean_component(authority.substr(0, at));
        if (!ui) return std::nullopt;
        url.userinfo = *ui;
        hostport = authority.substr(at + 1);
    }
    std::string_view host = hostport, port_text;
    bool has_colon = false;
    if (!hostport.empty() && hostport.front() == '[') {
        const std::size_t close = hostport.find(']');
        if (close == std::string_view::npos) return std::nullopt;
        host = hostport.substr(0, close + 1);
        const std::string_view after = hostport.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':') return std::nullopt;
            has_colon = true;
            port_text = after.substr(1);
        }
    } else {
        const std::size_t colon = hostport.rfind(':');
        if (colon != std::string_view::npos) {
            host = hostport.substr(0, colon);
            has_colon = true;
            port_text = hostport.substr(colon + 1);
        }
        for (char c : host) {
            if (c == '[' || c == ']' || c == ':' || c == '@' || c == ' ' || detail::is_control(c)) return std::nullopt;
        }
    }
    if (host.empty()) return std::nullopt;
    const auto h = detail::clean_component(host);
    if (!h) return std::nullopt;
    for (char c : *h) url.host += detail::lower(c);
    if (has_colon && !port_text.empty()) {
        if (port_text.size() > 10) return std::nullopt;
        std::uint64_t value = 0;
        for (char c : port_text) {
            if (!detail::is_digit(c)) return std::nullopt;
            value = value * 10 + static_cast<std::uint64_t>(c - '0');
        }
        if (value > 65535) return std::nullopt;
        url.port = static_cast<std::uint16_t>(value);
    }
    // path, query, fragment
    const std::size_t hash = rest.find('#');
    if (hash != std::string_view::npos) {
        const auto f = detail::clean_component(rest.substr(hash + 1));
        if (!f) return std::nullopt;
        url.fragment = *f;
        rest = rest.substr(0, hash);
    }
    const std::size_t qm = rest.find('?');
    if (qm != std::string_view::npos) {
        const auto q = detail::clean_component(rest.substr(qm + 1));
        if (!q) return std::nullopt;
        url.query = *q;
        rest = rest.substr(0, qm);
    }
    const auto p = detail::clean_component(rest);
    if (!p) return std::nullopt;
    url.path = *p;
    return url;
}

inline std::string to_string(const Url& url) {
    std::string out = url.scheme + "://";
    if (!url.userinfo.empty()) out += url.userinfo + "@";
    out += url.host;
    if (url.port && *url.port != detail::default_port(url.scheme)) out += ":" + std::to_string(*url.port);
    if (url.path.empty() && detail::path_defaults_to_slash(url.scheme)) out += "/";
    else out += url.path;
    if (url.query) out += "?" + *url.query;
    if (url.fragment) out += "#" + *url.fragment;
    return out;
}

inline std::optional<Url> resolve(const Url& base, std::string_view reference) {
    // does the reference start with a scheme?
    std::size_t i = 0;
    if (!reference.empty() && detail::is_alpha(reference[0])) {
        while (i < reference.size() && (detail::is_alpha(reference[i]) || detail::is_digit(reference[i]) || reference[i] == '+' || reference[i] == '-' || reference[i] == '.')) ++i;
        if (i < reference.size() && reference[i] == ':') {
            auto url = parse(reference);
            if (!url) return std::nullopt;
            url->path = remove_dot_segments(url->path);
            return url;
        }
    }
    Url target;
    target.scheme = base.scheme;
    std::string_view rest = reference;
    std::optional<std::string> fragment, query;
    std::size_t hash = rest.find('#');
    if (hash != std::string_view::npos) {
        const auto f = detail::clean_component(rest.substr(hash + 1));
        if (!f) return std::nullopt;
        fragment = *f;
        rest = rest.substr(0, hash);
    }
    std::size_t qm = rest.find('?');
    if (qm != std::string_view::npos) {
        const auto q = detail::clean_component(rest.substr(qm + 1));
        if (!q) return std::nullopt;
        query = *q;
        rest = rest.substr(0, qm);
    }
    if (rest.substr(0, 2) == "//") {
        // network-path reference: take the authority from the reference
        std::string synthetic = base.scheme + ":" + std::string(reference);
        const auto parsed = parse(synthetic);
        if (!parsed) return std::nullopt;
        Url out = *parsed;
        out.path = remove_dot_segments(out.path);
        return out;
    }
    const auto path = detail::clean_component(rest);
    if (!path) return std::nullopt;
    target.userinfo = base.userinfo;
    target.host = base.host;
    target.port = base.port;
    if (path->empty()) {
        target.path = base.path;
        target.query = query ? query : base.query;
    } else {
        if (path->front() == '/') {
            target.path = remove_dot_segments(*path);
        } else {
            std::string merged;
            if (base.path.empty()) {
                merged = "/" + *path;
            } else {
                const std::size_t last = base.path.rfind('/');
                merged = (last == std::string::npos ? std::string() : base.path.substr(0, last + 1)) + *path;
            }
            target.path = remove_dot_segments(merged);
        }
        target.query = query;
    }
    target.fragment = fragment;
    return target;
}

inline std::vector<std::pair<std::string, std::string>> parse_query(std::string_view text) {
    std::vector<std::pair<std::string, std::string>> out;
    auto decode = [](std::string_view piece) {
        std::string plus(piece);
        for (char& c : plus) {
            if (c == '+') c = ' ';
        }
        // an invalid escape is kept as literal text
        std::string result;
        for (std::size_t i = 0; i < plus.size(); ++i) {
            if (plus[i] == '%' && i + 2 < plus.size() && detail::hex_value(plus[i + 1]) >= 0 && detail::hex_value(plus[i + 2]) >= 0) {
                result += static_cast<char>(detail::hex_value(plus[i + 1]) * 16 + detail::hex_value(plus[i + 2]));
                i += 2;
            } else {
                result += plus[i];
            }
        }
        return result;
    };
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t amp = text.find('&', pos);
        const std::size_t end = amp == std::string_view::npos ? text.size() : amp;
        const std::string_view piece = text.substr(pos, end - pos);
        if (!piece.empty()) {
            const std::size_t eq = piece.find('=');
            if (eq == std::string_view::npos) out.emplace_back(decode(piece), std::string());
            else out.emplace_back(decode(piece.substr(0, eq)), decode(piece.substr(eq + 1)));
        }
        if (amp == std::string_view::npos) break;
        pos = amp + 1;
    }
    return out;
}

inline std::string build_query(const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::string out;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) out += '&';
        out += percent_encode(pairs[i].first) + "=" + percent_encode(pairs[i].second);
    }
    return out;
}

}  // namespace urlkit
