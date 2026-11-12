// MUTANT: missing padding is always refused
#pragma once
// Table-driven RFC 4648 codecs. The decode tables are built at compile time; the encoder works on 3-byte groups; the decoder is a single strict pass.
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace b64 {

enum class Alphabet { Standard, UrlSafe };

struct DecodeOptions {
    bool require_padding = true;
    bool allow_whitespace = false;
};

namespace detail {

inline constexpr char kStandard[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
inline constexpr char kUrlSafe[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

constexpr std::array<std::uint8_t, 256> make_decode_table(const char* alphabet) {
    std::array<std::uint8_t, 256> t{};
    for (auto& v : t) v = 0xFF;
    for (int i = 0; i < 64; ++i) t[static_cast<unsigned char>(alphabet[i])] = static_cast<std::uint8_t>(i);
    return t;
}
inline constexpr std::array<std::uint8_t, 256> kStandardTable = make_decode_table(kStandard);
inline constexpr std::array<std::uint8_t, 256> kUrlSafeTable = make_decode_table(kUrlSafe);

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace detail

constexpr std::size_t encoded_size(std::size_t n, bool pad = true) {
    if (pad) return (n + 2) / 3 * 4;
    return n / 3 * 4 + (n % 3 == 0 ? 0 : n % 3 + 1);
}

constexpr std::size_t max_decoded_size(std::size_t len) { return len / 4 * 3 + (len % 4) * 3 / 4; }

inline std::optional<std::size_t> encode_to(std::span<const std::uint8_t> in, std::span<char> out, Alphabet alphabet = Alphabet::Standard, bool pad = true) {
    const std::size_t need = encoded_size(in.size(), pad);
    if (out.size() < need) return std::nullopt;
    const char* table = alphabet == Alphabet::Standard ? detail::kStandard : detail::kUrlSafe;
    std::size_t o = 0, i = 0;
    for (; i + 3 <= in.size(); i += 3) {
        const std::uint32_t v = (std::uint32_t{in[i]} << 16) | (std::uint32_t{in[i + 1]} << 8) | in[i + 2];
        out[o++] = table[(v >> 18) & 63];
        out[o++] = table[(v >> 12) & 63];
        out[o++] = table[(v >> 6) & 63];
        out[o++] = table[v & 63];
    }
    const std::size_t rest = in.size() - i;
    if (rest == 1) {
        const std::uint32_t v = std::uint32_t{in[i]} << 16;
        out[o++] = table[(v >> 18) & 63];
        out[o++] = table[(v >> 12) & 63];
        if (pad) {
            out[o++] = '=';
            out[o++] = '=';
        }
    } else if (rest == 2) {
        const std::uint32_t v = (std::uint32_t{in[i]} << 16) | (std::uint32_t{in[i + 1]} << 8);
        out[o++] = table[(v >> 18) & 63];
        out[o++] = table[(v >> 12) & 63];
        out[o++] = table[(v >> 6) & 63];
        if (pad) out[o++] = '=';
    }
    return o;
}

inline std::string encode(std::span<const std::uint8_t> in, Alphabet alphabet = Alphabet::Standard, bool pad = true) {
    std::string out(encoded_size(in.size(), pad), '\0');
    encode_to(in, std::span<char>(out.data(), out.size()), alphabet, pad);
    return out;
}

inline std::optional<std::size_t> decode_to(std::string_view text, std::span<std::uint8_t> out, Alphabet alphabet = Alphabet::Standard, DecodeOptions options = {}) {
    const auto& table = alphabet == Alphabet::Standard ? detail::kStandardTable : detail::kUrlSafeTable;
    std::size_t produced = 0, data_chars = 0, pad_chars = 0;
    std::uint32_t acc = 0;
    int bits = 0;
    for (char ch : text) {
        if (options.allow_whitespace && detail::is_space(ch)) continue;
        if (ch == '=') {
            ++pad_chars;
            continue;
        }
        if (pad_chars > 0) return std::nullopt;          // data after padding
        const std::uint8_t v = table[static_cast<unsigned char>(ch)];
        if (v == 0xFF) return std::nullopt;               // not in the alphabet (also catches whitespace when it is not allowed)
        ++data_chars;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (produced >= out.size()) return std::nullopt;
            out[produced++] = static_cast<std::uint8_t>((acc >> bits) & 0xFF);
        }
    }
    const std::size_t rem = data_chars % 4;
    if (rem == 1) return std::nullopt;
    if (bits > 0 && (acc & ((1u << bits) - 1)) != 0) return std::nullopt;   // non-canonical: the unused bits must be zero
    const std::size_t needed_pad = rem == 0 ? 0 : 4 - rem;
    if (pad_chars > 0 && pad_chars != needed_pad) return std::nullopt;
    if (pad_chars == 0 && needed_pad > 0 && true) return std::nullopt;
    return produced;
}

inline std::optional<std::vector<std::uint8_t>> decode(std::string_view text, Alphabet alphabet = Alphabet::Standard, DecodeOptions options = {}) {
    std::vector<std::uint8_t> out(max_decoded_size(text.size()) + 3);
    const auto n = decode_to(text, std::span<std::uint8_t>(out.data(), out.size()), alphabet, options);
    if (!n) return std::nullopt;
    out.resize(*n);
    return out;
}

}  // namespace b64

namespace hex {

inline std::string encode(std::span<const std::uint8_t> in, bool upper = false) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out(in.size() * 2, '\0');
    for (std::size_t i = 0; i < in.size(); ++i) {
        out[2 * i] = digits[in[i] >> 4];
        out[2 * i + 1] = digits[in[i] & 15];
    }
    return out;
}

inline std::optional<std::vector<std::uint8_t>> decode(std::string_view text) {
    if (text.size() % 2 != 0) return std::nullopt;
    auto value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::uint8_t> out(text.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int h = value(text[2 * i]), l = value(text[2 * i + 1]);
        if (h < 0 || l < 0) return std::nullopt;
        out[i] = static_cast<std::uint8_t>(h * 16 + l);
    }
    return out;
}

}  // namespace hex
