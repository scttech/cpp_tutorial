// MUTANT: equal totals are not ordered: the winner depends on the hash table.
#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "types.hpp"

namespace detail {

struct Parsed {
    int level;
    std::string_view user;  // points into the line: nothing is copied
    long ms;
};

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

// Exactly three fields, found in one pass over the line.
inline std::optional<Parsed> parse_line(std::string_view line) {
    std::string_view fields[3];
    std::size_t count = 0;
    std::size_t i = 0;
    while (true) {
        while (i < line.size() && is_space(line[i])) ++i;
        if (i == line.size()) break;
        const std::size_t start = i;
        while (i < line.size() && !is_space(line[i])) ++i;
        if (count == 3) return std::nullopt;  // a fourth field
        fields[count++] = line.substr(start, i - start);
    }
    if (count != 3) return std::nullopt;

    int level = -1;
    for (std::size_t k = 0; k < kLevels.size(); ++k) {
        if (fields[0] == kLevels[k]) level = static_cast<int>(k);
    }
    if (level < 0) return std::nullopt;

    const std::string_view ms_text = fields[2];
    if (ms_text.size() > 9 || ms_text[0] < '0' || ms_text[0] > '9') return std::nullopt;  // 1 to 9 digits (and no sign)
    long ms = 0;
    const auto result = std::from_chars(ms_text.data(), ms_text.data() + ms_text.size(), ms);
    if (result.ec != std::errc() || result.ptr != ms_text.data() + ms_text.size()) return std::nullopt;  // letters after the digits
    return Parsed{level, fields[1], ms};
}

}  // namespace detail

inline Report analyze(const std::vector<std::string>& lines) {
    Report report;
    std::unordered_map<UserId, long long> totals;  // finding a user costs O(1), not a walk over all of them
    for (const std::string& line : lines) {
        const auto parsed = detail::parse_line(line);
        if (!parsed) {
            ++report.skipped;
            continue;
        }
        ++report.lines;
        ++report.by_level[static_cast<std::size_t>(parsed->level)];
        totals[UserId(parsed->user)] += parsed->ms;  // the temporary key is MOVED into the map when the user is new
    }
    report.distinct_users = totals.size();

    // The three best, kept in a tiny sorted array: no copy of the totals, no sort of all of them.
    std::vector<std::pair<const std::string*, long long>> best;
    auto better = [](const std::pair<const std::string*, long long>& a, const std::pair<const std::string*, long long>& b) {
        if (a.second != b.second) return a.second > b.second;
        return false;
    };
    for (const auto& entry : totals) {
        const std::pair<const std::string*, long long> candidate(&entry.first.name, entry.second);
        if (best.size() == 3 && !better(candidate, best.back())) continue;
        best.insert(std::upper_bound(best.begin(), best.end(), candidate, better), candidate);
        if (best.size() > 3) best.pop_back();
    }
    for (const auto& b : best) report.top.emplace_back(*b.first, b.second);
    return report;
}
