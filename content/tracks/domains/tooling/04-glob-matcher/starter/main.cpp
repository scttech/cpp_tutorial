// File globs: *.cpp, src/**/test_?.hpp, {a,b}, [a-f]. Read-only. Press Run.
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "glob.hpp"

int main() {
    struct Case { const char* pattern; const char* path; };
    const Case cases[] = {
        {"*.cpp", "main.cpp"},          {"*.cpp", "src/main.cpp"},         {"**/*.cpp", "src/main.cpp"},       {"**/*.cpp", "main.cpp"},
        {"src/**/test_?.hpp", "src/a/b/test_1.hpp"}, {"src/**", "src/a/b"}, {"src/**", "src"},                  {"*.{cpp,hpp}", "x.hpp"},
        {"[a-f]*", "deadbeef"},         {"[!a-f]*", "deadbeef"},           {"*", ".hidden"},                   {".*", ".hidden"},
        {"\\*literal", "*literal"},     {"a?c", "a/c"},                    {"data[0-9][0-9].csv", "data07.csv"},
    };
    for (const Case& c : cases) std::printf("%-24s %-22s %s\n", c.pattern, c.path, glob::match(c.pattern, c.path) ? "match" : "no match");

    // the classic trap: a pattern that makes a backtracking matcher take longer than the age of the universe
    const std::string pattern = std::string(25, '*').replace(0, 0, "") + "";
    std::string nasty;
    for (int i = 0; i < 40; ++i) nasty += "*a";
    nasty += "b";
    const std::string text(200, 'a');
    const auto t0 = std::chrono::steady_clock::now();
    const bool m = glob::match(nasty, text);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::printf("\n40 stars against 200 a's (no match expected): %s in %lld ms\n", m ? "match?!" : "no match", static_cast<long long>(ms));
    (void)pattern;

    for (const char* bad : {"[abc", "{a,b", "a\\", "[z-a]"}) {
        const auto e = glob::check(bad);
        std::printf("check(\"%s\") -> %s", bad, e ? e->message.c_str() : "fine");
        if (e) std::printf(" (at byte %zu)", e->position);
        std::printf("\n");
    }
}
