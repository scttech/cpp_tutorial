// Base64 (standard and URL-safe) and hex, with strict decoding. Read-only. Press Run.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "codec64.hpp"

int main() {
    const std::string text = "foobar";
    const std::vector<std::uint8_t> bytes(text.begin(), text.end());
    std::printf("hex      %s\n", hex::encode(bytes, true).c_str());
    for (std::size_t n = 0; n <= 6; ++n) {
        const std::vector<std::uint8_t> prefix(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n));
        std::printf("base64(%-6s) = %-9s unpadded: %s\n", text.substr(0, n).c_str(), b64::encode(prefix).c_str(), b64::encode(prefix, b64::Alphabet::Standard, false).c_str());
    }
    const std::vector<std::uint8_t> tricky = {0xFB, 0xFF, 0xBF};
    std::printf("bytes FB FF BF: standard %s, URL-safe %s\n", b64::encode(tricky).c_str(), b64::encode(tricky, b64::Alphabet::UrlSafe).c_str());
    for (const char* in : {"Zm9vYmFy", "Zm9vYg==", "Zm9vYg=", "Zh==", "Zm9v YmFy", "Zm9v!mFy"}) {
        const auto out = b64::decode(in);
        std::printf("decode(\"%s\") -> %s\n", in, out ? std::string(out->begin(), out->end()).c_str() : "(rejected)");
    }
}
