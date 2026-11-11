#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "types.hpp"

// CAPSTONE: Optimize the Pipeline.
//
// analyze() reads log lines like "ERROR user42 1250": a level (INFO, WARN or ERROR), a user name and a number of milliseconds, separated by spaces or
// tabs (any amount of whitespace, also around the line). It counts lines per level, the different users, and the three users with the most milliseconds.
// A line that is not exactly "level user milliseconds" is SKIPPED: an unknown level (levels are case-sensitive), a missing or extra field, or
// milliseconds that are not 1 to 9 digits (no sign, no letters, no decimal point).
//
// The code below is CORRECT and SLOW. Make it fast WITHOUT changing a single result. Press Run: main.cpp shows what it costs (UserId comparisons, UserId copies,
// heap allocations). Find out which of those numbers is out of proportion and why (the previous lessons name the usual suspects), and fix them one by one.
//   Works / Robust: the right report for every kind of line.
//   Fast:           budgets for comparisons, copies and allocations, and scaling (twice the log must cost about twice as much).
//   Clean:          the modern tools: std::string_view, std::from_chars, no streams, no raw new/delete.

struct Parsed {
    int level;
    std::string user;
    long ms;
};

inline std::optional<Parsed> parse_line(const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> parts;
    std::string part;
    while (in >> part) parts.push_back(part);
    if (parts.size() != 3) return std::nullopt;

    int level = -1;
    for (std::size_t i = 0; i < kLevels.size(); ++i) {
        if (parts[0] == kLevels[i]) level = static_cast<int>(i);
    }
    if (level < 0) return std::nullopt;

    const std::string& ms = parts[2];
    if (ms.empty() || ms.size() > 9) return std::nullopt;
    for (char c : ms) {
        if (c < '0' || c > '9') return std::nullopt;
    }
    return Parsed{level, parts[1], std::stol(ms)};
}

inline Report analyze(const std::vector<std::string>& lines) {
    Report report;
    std::vector<std::pair<UserId, long long>> totals;  // one entry per user
    for (const std::string& line : lines) {
        const std::optional<Parsed> parsed = parse_line(line);
        if (!parsed) {
            ++report.skipped;
            continue;
        }
        ++report.lines;
        ++report.by_level[static_cast<std::size_t>(parsed->level)];

        const UserId id(parsed->user);
        bool found = false;
        for (auto& entry : totals) {
            if (entry.first == id) {
                entry.second += parsed->ms;
                found = true;
                break;
            }
        }
        if (!found) totals.emplace_back(id, parsed->ms);
    }
    report.distinct_users = totals.size();

    std::vector<std::pair<UserId, long long>> sorted = totals;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first.name < b.first.name;
    });
    for (std::size_t i = 0; i < sorted.size() && i < 3; ++i) report.top.emplace_back(sorted[i].first.name, sorted[i].second);
    return report;
}
