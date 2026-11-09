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
//
// The types below are given. Every FUNCTION is a stub that compiles but is wrong: replace each body, in the order they appear.
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

// ---- 1. TODO: "DEBUG", "INFO", "WARN", "ERROR". ---------------------------------------------------------------------------------------------------
inline const char* level_name(Level level) {
    (void)level;
    return "?";
}

// ---- 2. TODO: YYYY-MM-DDTHH:MM:SS -> seconds since 1970-01-01T00:00:00 UTC. ------------------------------------------------------------------------------
// Nothing unless the text is EXACTLY that shape (19 characters, digits where the digits go, '-' ':' 'T' where they go) AND a real date and time
// (month 1-12, a day that exists in that month and year, hour < 24, minute < 60, second < 60).
// Hints: read the digit groups with std::from_chars (or by hand); build a std::chrono::year_month_day{year{y}, month{m}, day{d}} and check .ok();
// std::chrono::sys_days{ymd}.time_since_epoch() is the number of days since 1970-01-01; then add the hours, minutes and seconds.
inline std::optional<std::int64_t> parse_timestamp(std::string_view text) {
    (void)text;
    return std::nullopt;
}

// ---- 3. TODO: the reverse: seconds since the epoch -> "YYYY-MM-DDTHH:MM:SS". ---------------------------------------------------------------------------------
// Hints: std::chrono::sys_seconds, std::chrono::floor<std::chrono::days>(t), year_month_day{day}, hh_mm_ss{t - day}, std::format("{:04}-{:02}...").
inline std::string format_time(std::int64_t seconds_since_epoch) {
    (void)seconds_since_epoch;
    return "";
}

// ---- 4. TODO: one line -> an EntryView, or nothing if it is not a log line (see the format at the top). No allocation. ---------------------------------------
// Hints: trim the line, then peel off the fields with find(' ') and substr (string_views, no copies): timestamp, level, component, code (from_chars,
// the whole field must be the number), and the message is everything after the single space that follows the code (it may be empty).
inline std::optional<EntryView> parse_line(std::string_view line) {
    (void)line;
    return std::nullopt;
}

// ---- 5. TODO: the whole log (lines separated by '\n') -> a Report. ---------------------------------------------------------------------------------------
// Blank lines are skipped and not counted; malformed lines are counted. One pass over the text: no copy of the text, no string per line (only the few
// strings the Report keeps). Count with std::unordered_map<std::string_view, ...> keyed by views INTO the text; pick the top 3 with std::partial_sort.
// The hour of a timestamp t is floor(t / 3600) (use a floor division: t may be negative for dates before 1970).
inline Report analyze(std::string_view text) {
    (void)text;
    return Report{};
}

// ---- 6. TODO: the report as text, exactly in this shape (every line ends with '\n'): -------------------------------------------------------------------------
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
    (void)r;
    return "";
}
