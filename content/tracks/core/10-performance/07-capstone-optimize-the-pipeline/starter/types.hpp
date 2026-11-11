#pragma once
// READ-ONLY. The types of the pipeline, and the counters that measure it.
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace probe {
inline unsigned long long compares = 0;  // UserId == and UserId <
inline unsigned long long copies = 0;    // UserId copied (moves are free)
inline void reset() { compares = copies = 0; }
}  // namespace probe

// A user name that COUNTS comparisons and copies. Build one from a string_view; move it into a container instead of copying it.
struct UserId {
    std::string name;
    explicit UserId(std::string_view n) : name(n) {}
    UserId(const UserId& o) : name(o.name) { ++probe::copies; }
    UserId(UserId&&) noexcept = default;
    UserId& operator=(const UserId& o) {
        name = o.name;
        ++probe::copies;
        return *this;
    }
    UserId& operator=(UserId&&) noexcept = default;
    friend bool operator==(const UserId& a, const UserId& b) {
        ++probe::compares;
        return a.name == b.name;
    }
    friend bool operator<(const UserId& a, const UserId& b) {
        ++probe::compares;
        return a.name < b.name;
    }
};

template <>
struct std::hash<UserId> {
    std::size_t operator()(const UserId& u) const noexcept { return std::hash<std::string_view>()(u.name); }
};

// The three log levels, in the order of Report::by_level.
inline constexpr std::array<std::string_view, 3> kLevels = {"INFO", "WARN", "ERROR"};

// What analyze() returns.
struct Report {
    std::size_t lines = 0;                                // lines that were understood
    std::size_t skipped = 0;                              // lines that were not (malformed)
    std::array<std::size_t, 3> by_level{};                // understood lines per level: INFO, WARN, ERROR
    std::size_t distinct_users = 0;                       // different user names among the understood lines
    std::vector<std::pair<std::string, long long>> top;   // the (at most 3) users with the biggest total of milliseconds, biggest first;
                                                          // equal totals: the name that sorts first (std::string <) comes first
};

inline std::string describe(const Report& r) {
    std::string s = "lines=" + std::to_string(r.lines) + " skipped=" + std::to_string(r.skipped) + " levels=" + std::to_string(r.by_level[0]) + "/" +
                    std::to_string(r.by_level[1]) + "/" + std::to_string(r.by_level[2]) + " users=" + std::to_string(r.distinct_users) + " top=[";
    for (const auto& t : r.top) s += t.first + ":" + std::to_string(t.second) + " ";
    return s + "]";
}

// A made-up log with n lines and about n / 10 different users (the same every time for the same n and seed). About 1 line in 30 is malformed.
inline std::vector<std::string> make_log(std::size_t n, unsigned seed = 1) {
    std::vector<std::string> log;
    log.reserve(n);
    unsigned x = seed * 2654435761u + 99u;
    auto next = [&x] {
        x = x * 1664525u + 1013904223u;
        return x >> 8;
    };
    const unsigned users = static_cast<unsigned>(n / 10 + 1);
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned r = next();
        if (r % 30 == 0) {
            static const char* const bad[] = {"BAD user1 5", "INFO user1", "INFO user1 5 extra", "WARN user2 x5", ""};
            log.push_back(bad[next() % 5]);
            continue;
        }
        log.push_back(std::string(kLevels[next() % 3]) + " user" + std::to_string(next() % users) + " " + std::to_string(next() % 10000));
    }
    return log;
}
