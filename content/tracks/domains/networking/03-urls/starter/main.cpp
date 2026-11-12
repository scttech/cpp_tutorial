// Parse, normalise and resolve URLs; percent-encoding; query strings. Read-only. Press Run.
#include <cstdio>

#include "types.hpp"
#include "urlkit.hpp"

int main() {
    const char* inputs[] = {"HTTP://User@Example.COM:80/a/b/../c/./d?x=1&y=%7e#Frag", "https://[::1]:8443/", "http://example.com:99999/", "http://exa mple.com/", "ftp://host"};
    for (const char* in : inputs) {
        const auto u = urlkit::parse(in);
        std::printf("%-52s -> %s\n", in, u ? urlkit::to_string(*u).c_str() : "(invalid)");
    }
    const auto base = urlkit::parse("http://a/b/c/d;p?q");
    for (const char* ref : {"g", "../g", "/g", "//g", "?y", "#s", "../../../g", "g?y#s"})
        std::printf("resolve %-12s = %s\n", ref, base ? (urlkit::resolve(*base, ref) ? urlkit::to_string(*urlkit::resolve(*base, ref)).c_str() : "(error)") : "?");
    std::printf("decode: %s\n", urlkit::percent_decode("caf%C3%A9%20au%20lait").value_or("(invalid)").c_str());
    std::printf("encode: %s\n", urlkit::percent_encode("a b/c?d=é", "/").c_str());
    for (const auto& kv : urlkit::parse_query("name=J%C3%B6rg+M&tag=a&tag=b&flag")) std::printf("  %s = [%s]\n", kv.first.c_str(), kv.second.c_str());
}
