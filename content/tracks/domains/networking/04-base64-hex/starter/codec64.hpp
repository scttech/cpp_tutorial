#pragma once
// YOUR BASE64 AND HEX CODECS (RFC 4648), strict on input, exact on output, safe on every buffer.
//
// namespace b64
//   Alphabet::Standard  A-Z a-z 0-9 + /          Alphabet::UrlSafe   A-Z a-z 0-9 - _      (the two differ only in values 62 and 63)
//   DecodeOptions { require_padding = true, allow_whitespace = false }
//   encoded_size(n, pad = true)      exact length of the text for n bytes: padded 4 * ceil(n / 3); unpadded ceil(4 * n / 3)  (n % 3 == 1 -> 2 chars, 2 -> 3 chars)
//   max_decoded_size(len)            an upper bound on the bytes that len characters can decode to:  (len / 4) * 3 + (len % 4) * 3 / 4
//   encode(bytes, alphabet = Standard, pad = true) -> std::string
//   encode_to(bytes, out, alphabet, pad) -> optional<size_t>   writes EXACTLY encoded_size(...) characters into `out` and returns that number; if `out` is
//                      smaller it writes nothing and returns nullopt. It never writes past out.size().
//   decode(text, alphabet = Standard, options = {}) -> optional<vector<uint8_t>>
//   decode_to(text, out, alphabet, options) -> optional<size_t>   the decoded length; nullopt if the text is invalid or the decoded data does not fit in out.
//                      It never writes past out.size() (the contents of out are unspecified when it returns nullopt).
//   STRICT DECODING (nullopt for anything else):
//     * only characters of the chosen alphabet (a '-' is not valid in the Standard alphabet, a '+' not in UrlSafe), plus '=' as padding at the very end;
//       whitespace (space, tab, CR, LF) is skipped ONLY if allow_whitespace is set, and then it may appear anywhere, also between '=' signs.
//     * padding: with require_padding the character count (data + '=') must be a multiple of 4 and the padding exactly as long as needed: "Zg==" and "Zm8=" are
//       right, "Zg=", "Zg===" and "Zm9v=" are not. With require_padding == false the padding may be left out ("Zg" is right), but if it is present it must still be exactly right.
//     * no data character may follow a '=' ("Zm=v" is invalid); a group may not have a single character ("Z", "Zm9vY" are invalid).
//     * canonical form: the unused low bits of the last character must be ZERO ("Zh==" would decode to the same byte as "Zg==" and is refused; so is "Zm9=" for "Zm8=").
//
// namespace hex
//   encode(bytes, upper = false) -> std::string      two digits per byte, lower case by default
//   decode(text) -> optional<vector<uint8_t>>        an even number of hex digits, either case; nothing else (no spaces, no "0x")
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

constexpr std::size_t encoded_size(std::size_t n, bool pad = true) {
    (void)n;
    (void)pad;
    return 0;  // TODO
}
constexpr std::size_t max_decoded_size(std::size_t len) {
    (void)len;
    return 0;  // TODO
}
inline std::optional<std::size_t> encode_to(std::span<const std::uint8_t> in, std::span<char> out, Alphabet alphabet = Alphabet::Standard, bool pad = true) {
    (void)in;
    (void)out;
    (void)alphabet;
    (void)pad;
    return std::nullopt;  // TODO
}
inline std::string encode(std::span<const std::uint8_t> in, Alphabet alphabet = Alphabet::Standard, bool pad = true) {
    (void)in;
    (void)alphabet;
    (void)pad;
    return "";  // TODO
}
inline std::optional<std::size_t> decode_to(std::string_view text, std::span<std::uint8_t> out, Alphabet alphabet = Alphabet::Standard, DecodeOptions options = {}) {
    (void)text;
    (void)out;
    (void)alphabet;
    (void)options;
    return std::nullopt;  // TODO
}
inline std::optional<std::vector<std::uint8_t>> decode(std::string_view text, Alphabet alphabet = Alphabet::Standard, DecodeOptions options = {}) {
    (void)text;
    (void)alphabet;
    (void)options;
    return std::nullopt;  // TODO
}

}  // namespace b64

namespace hex {

inline std::string encode(std::span<const std::uint8_t> in, bool upper = false) {
    (void)in;
    (void)upper;
    return "";  // TODO
}
inline std::optional<std::vector<std::uint8_t>> decode(std::string_view text) {
    (void)text;
    return std::nullopt;  // TODO
}

}  // namespace hex
