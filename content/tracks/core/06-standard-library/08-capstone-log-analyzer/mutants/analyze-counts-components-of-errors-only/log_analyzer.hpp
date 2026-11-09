// MUTANT: only ERROR lines count.
#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

// ======================================================================================================================================
// Log Analyzer: parse a log, aggregate statistics, report. It uses the whole module: string_view and from_chars for parsing, <chrono> for
// the timestamps, unordered_map for counting, partial_sort for the top errors, optional for "no value", std::format for the report.
//
// A log line is:   TIMESTAMP LEVEL COMPONENT CODE MESSAGE
//   2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2
//   TIMESTAMP  exactly YYYY-MM-DDTHH:MM:SS (a real date and time, UTC)       LEVEL  DEBUG, INFO, WARN or ERROR (upper case)
//   COMPONENT  a word without spaces                                          CODE   an integer
//   MESSAGE    the rest of the line (it may be empty and may contain spaces); whitespace around the whole line is ignored
// ======================================================================================================================================

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

// One parsed line. The text views point INTO the log text: they are valid while that text is, and nothing is copied.
struct EntryView {
    std::int64_t time = 0;  // seconds since 1970-01-01T00:00:00
    Level level = Level::Info;
    std::string_view component;
    int code = 0;
    std::string_view message;
};

struct ErrorGroup {
    int code = 0;
    std::string message;  // the FIRST message seen with this code
    std::size_t count = 0;
};

struct Report {
    std::size_t lines = 0;      // lines that are not blank
    std::size_t parsed = 0;     // lines that are log lines
    std::size_t malformed = 0;  // lines - parsed
    std::array<std::size_t, 4> by_level{};              // indexed by Level
    std::optional<std::int64_t> first_time;             // the earliest and the latest timestamp (not necessarily the first and last LINE)
    std::optional<std::int64_t> last_time;
    std::vector<ErrorGroup> top_errors;                 // up to 3 codes of ERROR lines: most frequent first, equal counts by smaller code
    std::string busiest_component;                      // the component with the most lines (equal counts: the alphabetically first); "" if none
    std::optional<std::int64_t> peak_error_hour;        // the start (a multiple of 3600) of the hour with the most ERROR lines (equal: the earliest)
};

namespace detail {

inline std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\v\f";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

// `length` digits starting at `pos`, as a number; nothing if any of them is not a digit.
inline std::optional<int> digits(std::string_view text, std::size_t pos, std::size_t length) {
    if (pos + length > text.size()) return std::nullopt;
    int value = 0;
    for (std::size_t i = pos; i < pos + length; ++i) {
        if (text[i] < '0' || text[i] > '9') return std::nullopt;
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

inline std::optional<Level> parse_level(std::string_view text) {
    if (text == "DEBUG") return Level::Debug;
    if (text == "INFO") return Level::Info;
    if (text == "WARN") return Level::Warn;
    if (text == "ERROR") return Level::Error;
    return std::nullopt;
}

inline std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

}  // namespace detail

// "DEBUG", "INFO", "WARN", "ERROR".
inline const char* level_name(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "?";
}

// YYYY-MM-DDTHH:MM:SS -> seconds since 1970-01-01T00:00:00 UTC. Nothing unless the text is EXACTLY that shape (19 characters, digits where the digits
// go, '-' ':' 'T' where they go) AND a real date and time (month 1-12, a day that exists in that month and year, hour < 24, minute < 60, second < 60).
inline std::optional<std::int64_t> parse_timestamp(std::string_view text) {
    if (text.size() != 19 || text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' || text[16] != ':') return std::nullopt;
    const auto y = detail::digits(text, 0, 4);
    const auto mo = detail::digits(text, 5, 2);
    const auto d = detail::digits(text, 8, 2);
    const auto h = detail::digits(text, 11, 2);
    const auto mi = detail::digits(text, 14, 2);
    const auto s = detail::digits(text, 17, 2);
    if (!y || !mo || !d || !h || !mi || !s) return std::nullopt;
    if (*h > 23 || *mi > 59 || *s > 59) return std::nullopt;
    const std::chrono::year_month_day date{std::chrono::year{*y}, std::chrono::month{static_cast<unsigned>(*mo)}, std::chrono::day{static_cast<unsigned>(*d)}};
    if (!date.ok()) return std::nullopt;  // month 13, February 30, ...
    const std::chrono::sys_days days{date};
    return std::chrono::duration_cast<std::chrono::seconds>(days.time_since_epoch()).count() + *h * 3600LL + *mi * 60LL + *s;
}

// The reverse: seconds since the epoch -> "YYYY-MM-DDTHH:MM:SS".
inline std::string format_time(std::int64_t seconds_since_epoch) {
    const std::chrono::sys_seconds t{std::chrono::seconds{seconds_since_epoch}};
    const std::chrono::sys_days day = std::chrono::floor<std::chrono::days>(t);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss<std::chrono::seconds> time{t - day};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                       static_cast<unsigned>(date.day()), time.hours().count(), time.minutes().count(), time.seconds().count());
}

// One line -> an EntryView, or nothing if it is not a log line (see the format at the top). No allocation.
inline std::optional<EntryView> parse_line(std::string_view line) {
    line = detail::trim(line);
    const std::size_t s1 = line.find(' ');
    if (s1 == std::string_view::npos) return std::nullopt;
    const auto time = parse_timestamp(line.substr(0, s1));
    if (!time) return std::nullopt;

    std::string_view rest = line.substr(s1 + 1);
    const std::size_t s2 = rest.find(' ');
    if (s2 == std::string_view::npos) return std::nullopt;
    const auto level = detail::parse_level(rest.substr(0, s2));
    if (!level) return std::nullopt;

    rest = rest.substr(s2 + 1);
    const std::size_t s3 = rest.find(' ');
    if (s3 == std::string_view::npos || s3 == 0) return std::nullopt;  // a component is required
    const std::string_view component = rest.substr(0, s3);

    rest = rest.substr(s3 + 1);
    const std::size_t s4 = rest.find(' ');
    const std::string_view code_text = rest.substr(0, s4);
    int code = 0;
    const auto [stop, ec] = std::from_chars(code_text.data(), code_text.data() + code_text.size(), code);
    if (ec != std::errc{} || stop != code_text.data() + code_text.size()) return std::nullopt;

    EntryView entry;
    entry.time = *time;
    entry.level = *level;
    entry.component = component;
    entry.code = code;
    if (s4 != std::string_view::npos) entry.message = rest.substr(s4 + 1);
    return entry;
}

// The whole log (lines separated by '\n') -> a Report. Blank lines are skipped and not counted; malformed lines are counted. One pass over the text:
// no copy of the text, no string per line (only the few strings the Report keeps).
inline Report analyze(std::string_view text) {
    struct Group {
        std::size_t count = 0;
        std::string_view first_message;
    };
    Report report;
    std::unordered_map<std::string_view, std::size_t> components;
    std::unordered_map<int, Group> error_codes;
    std::unordered_map<std::int64_t, std::size_t> error_hours;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = detail::trim(text.substr(pos, end - pos));
        pos = end + 1;
        if (line.empty()) continue;

        ++report.lines;
        const auto entry = parse_line(line);
        if (!entry) {
            ++report.malformed;
            continue;
        }
        ++report.parsed;
        ++report.by_level[static_cast<std::size_t>(entry->level)];
        if (!report.first_time || entry->time < *report.first_time) report.first_time = entry->time;
        if (!report.last_time || entry->time > *report.last_time) report.last_time = entry->time;
        if (entry->level == Level::Error) ++components[entry->component];
        if (entry->level == Level::Error) {
            Group& g = error_codes[entry->code];
            if (g.count++ == 0) g.first_message = entry->message;
            ++error_hours[detail::floor_div(entry->time, 3600)];
        }
    }

    // the busiest component: most lines, then the alphabetically first
    std::string_view best_component;
    std::size_t best_count = 0;
    for (const auto& [name, count] : components) {
        if (count > best_count || (count == best_count && name < best_component)) {
            best_component = name;
            best_count = count;
        }
    }
    report.busiest_component = std::string(best_component);

    // the top 3 error codes
    std::vector<std::pair<int, Group>> groups(error_codes.begin(), error_codes.end());
    const auto more_frequent = [](const auto& a, const auto& b) {
        return a.second.count != b.second.count ? a.second.count > b.second.count : a.first < b.first;
    };
    const std::size_t keep = std::min<std::size_t>(3, groups.size());
    std::partial_sort(groups.begin(), groups.begin() + static_cast<std::ptrdiff_t>(keep), groups.end(), more_frequent);
    for (std::size_t i = 0; i < keep; ++i) report.top_errors.push_back(ErrorGroup{groups[i].first, std::string(groups[i].second.first_message), groups[i].second.count});

    // the hour with the most errors (equal: the earliest)
    std::size_t peak_count = 0;
    for (const auto& [hour, count] : error_hours) {
        if (count > peak_count || (count == peak_count && report.peak_error_hour && hour * 3600 < *report.peak_error_hour)) {
            peak_count = count;
            report.peak_error_hour = hour * 3600;
        }
    }
    return report;
}

// The report as text, exactly in this shape (every line ends with '\n'):
//   Lines: 10 (8 parsed, 2 malformed)
//   Levels: DEBUG 1, INFO 3, WARN 2, ERROR 2
//   Span: 2024-03-05T12:00:00 to 2024-03-05T14:30:10
//   Busiest component: db
//   Peak error hour: 2024-03-05T13:00:00
//   Top errors:
//     1. 503 x3 Connection refused
//     2. 404 x1 Not found
// With nothing parsed: "Span: none", "Busiest component: none", "Peak error hour: none" and "Top errors: none" (no list).
inline std::string format_report(const Report& r) {
    std::string out = std::format("Lines: {} ({} parsed, {} malformed)\n", r.lines, r.parsed, r.malformed);
    out += std::format("Levels: DEBUG {}, INFO {}, WARN {}, ERROR {}\n", r.by_level[0], r.by_level[1], r.by_level[2], r.by_level[3]);
    if (r.first_time && r.last_time) {
        out += std::format("Span: {} to {}\n", format_time(*r.first_time), format_time(*r.last_time));
    } else {
        out += "Span: none\n";
    }
    out += std::format("Busiest component: {}\n", r.busiest_component.empty() ? std::string("none") : r.busiest_component);
    out += std::format("Peak error hour: {}\n", r.peak_error_hour ? format_time(*r.peak_error_hour) : std::string("none"));
    if (r.top_errors.empty()) {
        out += "Top errors: none\n";
    } else {
        out += "Top errors:\n";
        for (std::size_t i = 0; i < r.top_errors.size(); ++i) {
            const ErrorGroup& g = r.top_errors[i];
            out += std::format("  {}. {} x{} {}\n", i + 1, g.code, g.count, g.message);
        }
    }
    return out;
}
