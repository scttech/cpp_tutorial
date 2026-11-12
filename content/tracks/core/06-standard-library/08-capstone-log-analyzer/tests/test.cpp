#define ZTH_TRACK_ALLOCATIONS
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "log_analyzer.hpp"
#include "zth_test.hpp"

namespace {

using S = std::string;

constexpr std::int64_t kBase = 1709596800;  // 2024-03-05T00:00:00

// the log used by several tests (it is also the one in main.cpp): 10 lines, 8 parsed, 2 malformed, 1 blank
const char* const kSampleLog =
    "2024-03-05T12:00:00 INFO web 200 Request served\n"
    "2024-03-05T12:15:30 WARN db 429 Slow query on the orders table\n"
    "2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2\n"
    "2024-03-05T13:02:40 ERROR db 503 Connection refused to replica-2\n"
    "this line is not a log line\n"
    "\n"
    "2024-03-05T13:40:00 ERROR web 404 Not found: /missing\n"
    "2024-03-05T14:30:10 DEBUG web 0 \r\n"
    "2024-02-30T10:00:00 INFO web 200 There is no February 30th\n"
    "2024-03-05T13:59:59 ERROR db 503 Connection refused again\n";

const char* const kSampleReport =
    "Lines: 9 (7 parsed, 2 malformed)\n"
    "Levels: DEBUG 1, INFO 1, WARN 1, ERROR 4\n"
    "Span: 2024-03-05T12:00:00 to 2024-03-05T14:30:10\n"
    "Busiest component: db\n"
    "Peak error hour: 2024-03-05T13:00:00\n"
    "Top errors:\n"
    "  1. 503 x3 Connection refused to replica-2\n"
    "  2. 404 x1 Not found: /missing\n";

// n lines of a realistic log: 6 components, 20 error codes, long messages, 7 seconds apart
S make_log(int n) {
    static const char* const components[] = {"web", "db", "cache", "auth", "queue", "search"};
    static const char* const levels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    S text;
    text.reserve(static_cast<std::size_t>(n) * 90);
    for (int i = 0; i < n; ++i) {
        const std::int64_t t = kBase + static_cast<std::int64_t>(i) * 7;
        text += format_time(t);
        text += ' ';
        text += levels[i % 4];
        text += ' ';
        text += components[(i / 3) % 6];
        text += ' ';
        text += std::to_string(400 + (i * 7) % 20);
        text += " A reasonably long message that does not fit in the small string buffer #";
        text += std::to_string(i);
        text += '\n';
    }
    return text;
}

}  // namespace

// ================================================ Works =======================================================
ZTH_TEST("works", "level_name") {
    ZTH_EXPECT_EQ(S(level_name(Level::Debug)), S("DEBUG"));
    ZTH_EXPECT_EQ(S(level_name(Level::Info)), S("INFO"));
    ZTH_EXPECT_EQ(S(level_name(Level::Warn)), S("WARN"));
    ZTH_EXPECT_EQ(S(level_name(Level::Error)), S("ERROR"));
}
ZTH_TEST("works", "parse_timestamp: seconds since 1970-01-01T00:00:00") {
    const auto epoch = parse_timestamp("1970-01-01T00:00:00");
    ZTH_EXPECT_TRUE(epoch.has_value());
    if (epoch) ZTH_EXPECT_EQ(*epoch, std::int64_t{0});
    const auto y2k = parse_timestamp("2000-01-01T00:00:00");
    if (y2k) ZTH_EXPECT_EQ(*y2k, std::int64_t{946684800});
    const auto t = parse_timestamp("2024-03-05T13:02:11");
    if (t) ZTH_EXPECT_EQ(*t, std::int64_t{1709643731});
    const auto last32 = parse_timestamp("2038-01-19T03:14:07");
    if (last32) ZTH_EXPECT_EQ(*last32, std::int64_t{2147483647});
}
ZTH_TEST("works", "format_time is the reverse of parse_timestamp") {
    ZTH_EXPECT_EQ(format_time(0), S("1970-01-01T00:00:00"));
    ZTH_EXPECT_EQ(format_time(946684800), S("2000-01-01T00:00:00"));
    ZTH_EXPECT_EQ(format_time(1709643731), S("2024-03-05T13:02:11"));
    ZTH_EXPECT_EQ(format_time(2147483647), S("2038-01-19T03:14:07"));
}
ZTH_TEST("works", "parse_line: all the fields") {
    const auto e = parse_line("2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2");
    ZTH_EXPECT_TRUE(e.has_value());
    if (e) {
        ZTH_EXPECT_EQ(e->time, std::int64_t{1709643731});
        ZTH_EXPECT_TRUE(e->level == Level::Error);
        ZTH_EXPECT_EQ(S(e->component), S("db"));
        ZTH_EXPECT_EQ(e->code, 503);
        ZTH_EXPECT_EQ(S(e->message), S("Connection refused to replica-2"));
    }
}
ZTH_TEST("works", "parse_line accepts every level") {
    const char* lines[] = {"2024-03-05T00:00:00 DEBUG a 1 m", "2024-03-05T00:00:00 INFO a 1 m", "2024-03-05T00:00:00 WARN a 1 m", "2024-03-05T00:00:00 ERROR a 1 m"};
    const Level expected[] = {Level::Debug, Level::Info, Level::Warn, Level::Error};
    for (int i = 0; i < 4; ++i) {
        const auto e = parse_line(lines[i]);
        if (!e || e->level != expected[i]) {
            ::zth::fail(S("level of ") + lines[i] + " was not read");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("works", "parse_line: the text fields are views into the line (no copy)") {
    const S line = "2024-03-05T13:02:11 WARN cache 17 hit ratio low";
    const auto e = parse_line(line);
    ZTH_EXPECT_TRUE(e.has_value());
    if (e) {
        ZTH_EXPECT_TRUE(e->component.data() >= line.data() && e->component.data() < line.data() + line.size());
        ZTH_EXPECT_TRUE(e->message.data() >= line.data() && e->message.data() < line.data() + line.size());
    }
}
ZTH_TEST("works", "analyze counts lines, parsed lines and malformed lines (blank lines do not count)") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_EQ(r.lines, std::size_t{9});
    ZTH_EXPECT_EQ(r.parsed, std::size_t{7});
    ZTH_EXPECT_EQ(r.malformed, std::size_t{2});
}
ZTH_TEST("works", "analyze counts lines per level") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_EQ(r.by_level[0], std::size_t{1});
    ZTH_EXPECT_EQ(r.by_level[1], std::size_t{1});
    ZTH_EXPECT_EQ(r.by_level[2], std::size_t{1});
    ZTH_EXPECT_EQ(r.by_level[3], std::size_t{4});
}
ZTH_TEST("works", "analyze: the span of the timestamps") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_TRUE(r.first_time.has_value());
    ZTH_EXPECT_TRUE(r.last_time.has_value());
    if (r.first_time && r.last_time) {
        ZTH_EXPECT_EQ(format_time(*r.first_time), S("2024-03-05T12:00:00"));
        ZTH_EXPECT_EQ(format_time(*r.last_time), S("2024-03-05T14:30:10"));
    }
}
ZTH_TEST("works", "analyze: the busiest component") {
    ZTH_EXPECT_EQ(analyze(kSampleLog).busiest_component, S("db"));
}
ZTH_TEST("works", "analyze: the most frequent error codes, with the first message seen") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_EQ(r.top_errors.size(), std::size_t{2});
    if (r.top_errors.size() == 2) {
        ZTH_EXPECT_EQ(r.top_errors[0].code, 503);
        ZTH_EXPECT_EQ(r.top_errors[0].count, std::size_t{3});
        ZTH_EXPECT_EQ(r.top_errors[0].message, S("Connection refused to replica-2"));
        ZTH_EXPECT_EQ(r.top_errors[1].code, 404);
        ZTH_EXPECT_EQ(r.top_errors[1].count, std::size_t{1});
    }
}
ZTH_TEST("works", "analyze: the hour with the most errors") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_TRUE(r.peak_error_hour.has_value());
    if (r.peak_error_hour) ZTH_EXPECT_EQ(format_time(*r.peak_error_hour), S("2024-03-05T13:00:00"));
}
ZTH_TEST("works", "format_report of the sample log") {
    ZTH_EXPECT_EQ(format_report(analyze(kSampleLog)), S(kSampleReport));
}

// ================================================ Robust ======================================================
ZTH_TEST("robust", "parse_timestamp: dates that do not exist") {
    for (const char* bad : {"2024-13-01T00:00:00", "2024-00-10T00:00:00", "2024-02-30T10:00:00", "2023-02-29T10:00:00", "2100-02-29T10:00:00", "2024-04-31T10:00:00",
                            "2024-01-00T10:00:00", "2024-01-32T10:00:00"}) {
        if (parse_timestamp(bad)) {
            ::zth::fail(S("parse_timestamp(\"") + bad + "\") should be nothing: that date does not exist");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "parse_timestamp: leap days that do exist") {
    const auto a = parse_timestamp("2024-02-29T23:59:59");
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_EQ(*a, std::int64_t{1709251199});
    ZTH_EXPECT_TRUE(parse_timestamp("2000-02-29T00:00:00").has_value());  // divisible by 400
}
ZTH_TEST("robust", "parse_timestamp: times that do not exist") {
    for (const char* bad : {"2024-03-05T24:00:00", "2024-03-05T00:60:00", "2024-03-05T00:00:60", "2024-03-05T99:99:99"}) {
        if (parse_timestamp(bad)) {
            ::zth::fail(S("parse_timestamp(\"") + bad + "\") should be nothing");
            return;
        }
    }
    ZTH_EXPECT_TRUE(parse_timestamp("2024-03-05T23:59:59").has_value());
}
ZTH_TEST("robust", "parse_timestamp: the shape must be exact") {
    for (const char* bad : {"", "2024-03-05", "2024-03-05 13:02:11", "2024-03-05t13:02:11", "2024/03/05T13:02:11", "2024-3-5T1:2:3", "2024-03-05T13:02:11Z", "2024-03-05T13:02:1",
                            " 2024-03-05T13:02:11", "2024-03-05T13:02:11 ", "20x4-03-05T13:02:11", "2024-03-05T13:02:+1", "-024-03-05T13:02:11"}) {
        if (parse_timestamp(bad)) {
            ::zth::fail(S("parse_timestamp(\"") + bad + "\") should be nothing");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "format_time round-trips through parse_timestamp, including times before 1970") {
    for (std::int64_t t : {std::int64_t{0}, std::int64_t{59}, std::int64_t{86399}, std::int64_t{86400}, std::int64_t{951782400}, std::int64_t{1709251199},
                           std::int64_t{4102444800}}) {
        const auto back = parse_timestamp(format_time(t));
        if (!back || *back != t) {
            ::zth::fail("format_time(" + std::to_string(t) + ") = " + format_time(t) + " does not parse back to the same time");
            return;
        }
    }
    ZTH_EXPECT_EQ(format_time(-1800), S("1969-12-31T23:30:00"));
    const auto before = parse_timestamp("1969-12-31T23:30:00");
    ZTH_EXPECT_TRUE(before.has_value());
    if (before) ZTH_EXPECT_EQ(*before, std::int64_t{-1800});
}
ZTH_TEST("robust", "parse_line: whitespace around the line, and a Windows line ending") {
    const auto a = parse_line("   2024-03-05T13:02:11 INFO web 200 ok   \r\n");
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_EQ(S(a->message), S("ok"));
    const auto b = parse_line("2024-03-05T13:02:11 INFO web 200 ok\r");
    ZTH_EXPECT_TRUE(b.has_value());
    if (b) ZTH_EXPECT_EQ(S(b->message), S("ok"));
}
ZTH_TEST("robust", "parse_line: an empty message, with and without a trailing space") {
    const auto a = parse_line("2024-03-05T13:02:11 DEBUG web 0");
    ZTH_EXPECT_TRUE(a.has_value());
    if (a) ZTH_EXPECT_TRUE(a->message.empty());
    const auto b = parse_line("2024-03-05T13:02:11 DEBUG web 0 ");
    ZTH_EXPECT_TRUE(b.has_value());
    if (b) ZTH_EXPECT_TRUE(b->message.empty());
}
ZTH_TEST("robust", "parse_line: a message keeps its inner spaces and punctuation") {
    const auto e = parse_line("2024-03-05T13:02:11 WARN web 5 disk  at   91%  (limit: 90%)");
    ZTH_EXPECT_TRUE(e.has_value());
    if (e) ZTH_EXPECT_EQ(S(e->message), S("disk  at   91%  (limit: 90%)"));
}
ZTH_TEST("robust", "parse_line: negative codes and large codes are fine, a code that is not a whole number is not") {
    const auto neg = parse_line("2024-03-05T13:02:11 WARN web -5 m");
    ZTH_EXPECT_TRUE(neg.has_value());
    if (neg) ZTH_EXPECT_EQ(neg->code, -5);
    const auto big = parse_line("2024-03-05T13:02:11 WARN web 2147483647 m");
    ZTH_EXPECT_TRUE(big.has_value());
    for (const char* bad : {"2024-03-05T13:02:11 WARN web 5x m", "2024-03-05T13:02:11 WARN web abc m", "2024-03-05T13:02:11 WARN web 99999999999 m",
                            "2024-03-05T13:02:11 WARN web +5 m", "2024-03-05T13:02:11 WARN web 1.5 m", "2024-03-05T13:02:11 WARN web  m"}) {
        if (parse_line(bad)) {
            ::zth::fail(S("parse_line(\"") + bad + "\") should be nothing");
            return;
        }
    }
}
ZTH_TEST("robust", "parse_line: malformed lines of every kind") {
    for (const char* bad : {"", "   ", "garbage", "2024-03-05T13:02:11", "2024-03-05T13:02:11 ERROR", "2024-03-05T13:02:11 ERROR db",
                            "2024-03-05T13:02:11 error db 5 m", "2024-03-05T13:02:11 FATAL db 5 m", "2024-03-05T13:02:11 Error db 5 m", "2024-03-05T13:02:11 ERROR  5 m",
                            "ERROR 2024-03-05T13:02:11 db 5 m", "2024-03-05 ERROR db 5 m", "2024-13-05T13:02:11 ERROR db 5 m", "2024-03-05T13:02:11ERROR db 5 m"}) {
        if (parse_line(bad)) {
            ::zth::fail(S("parse_line(\"") + bad + "\") should be nothing");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("robust", "analyze: an empty log, a log of blank lines, and a log of garbage") {
    const Report empty = analyze("");
    ZTH_EXPECT_EQ(empty.lines, std::size_t{0});
    ZTH_EXPECT_FALSE(empty.first_time.has_value());
    ZTH_EXPECT_TRUE(empty.busiest_component.empty());
    ZTH_EXPECT_TRUE(empty.top_errors.empty());
    ZTH_EXPECT_FALSE(empty.peak_error_hour.has_value());
    const Report blank = analyze("\n\n  \n\t\r\n");
    ZTH_EXPECT_EQ(blank.lines, std::size_t{0});
    const Report garbage = analyze("nope\nstill nope\n");
    ZTH_EXPECT_EQ(garbage.lines, std::size_t{2});
    ZTH_EXPECT_EQ(garbage.parsed, std::size_t{0});
    ZTH_EXPECT_EQ(garbage.malformed, std::size_t{2});
    ZTH_EXPECT_FALSE(garbage.first_time.has_value());
}
ZTH_TEST("robust", "analyze: the last line without a newline, and a log with Windows line endings") {
    const Report a = analyze("2024-03-05T10:00:00 INFO a 1 x\n2024-03-05T10:00:01 INFO a 1 y");
    ZTH_EXPECT_EQ(a.parsed, std::size_t{2});
    const Report b = analyze("2024-03-05T10:00:00 INFO a 1 x\r\n2024-03-05T10:00:01 INFO a 1 y\r\n");
    ZTH_EXPECT_EQ(b.parsed, std::size_t{2});
    ZTH_EXPECT_EQ(b.malformed, std::size_t{0});
}
ZTH_TEST("robust", "analyze: lines == parsed + malformed, and the level counts add up to parsed") {
    const Report r = analyze(kSampleLog);
    ZTH_EXPECT_EQ(r.lines, r.parsed + r.malformed);
    ZTH_EXPECT_EQ(r.by_level[0] + r.by_level[1] + r.by_level[2] + r.by_level[3], r.parsed);
}
ZTH_TEST("robust", "analyze: the span uses the earliest and latest TIMESTAMPS, whatever the order of the lines") {
    const Report r = analyze(
        "2024-03-05T12:00:00 INFO a 1 x\n"
        "2024-03-05T08:00:00 INFO a 1 x\n"
        "2024-03-05T20:00:00 INFO a 1 x\n"
        "2024-03-05T10:00:00 INFO a 1 x\n");
    ZTH_EXPECT_TRUE(r.first_time.has_value() && r.last_time.has_value());
    if (r.first_time && r.last_time) {
        ZTH_EXPECT_EQ(format_time(*r.first_time), S("2024-03-05T08:00:00"));
        ZTH_EXPECT_EQ(format_time(*r.last_time), S("2024-03-05T20:00:00"));
    }
}
ZTH_TEST("robust", "analyze: a single line gives the same first and last time") {
    const Report r = analyze("2024-03-05T10:00:00 INFO a 1 x\n");
    ZTH_EXPECT_TRUE(r.first_time.has_value() && r.last_time.has_value());
    if (r.first_time && r.last_time) ZTH_EXPECT_EQ(*r.first_time, *r.last_time);
}
ZTH_TEST("robust", "analyze: components with equal counts - the alphabetically first wins") {
    const Report r = analyze(
        "2024-03-05T10:00:00 INFO zeta 1 x\n"
        "2024-03-05T10:00:01 INFO alpha 1 x\n"
        "2024-03-05T10:00:02 INFO mid 1 x\n"
        "2024-03-05T10:00:03 INFO zeta 1 x\n"
        "2024-03-05T10:00:04 INFO alpha 1 x\n");
    ZTH_EXPECT_EQ(r.busiest_component, S("alpha"));
}
ZTH_TEST("robust", "analyze: components are compared exactly (case sensitive, no prefix matching)") {
    const Report r = analyze(
        "2024-03-05T10:00:00 INFO db 1 x\n"
        "2024-03-05T10:00:01 INFO DB 1 x\n"
        "2024-03-05T10:00:02 INFO db2 1 x\n"
        "2024-03-05T10:00:03 INFO db2 1 x\n");
    ZTH_EXPECT_EQ(r.busiest_component, S("db2"));
}
ZTH_TEST("robust", "analyze: only ERROR lines count as errors, and only the top 3 codes are kept") {
    S log;
    auto add = [&log](int n, int code, const char* level) {
        for (int i = 0; i < n; ++i) log += S("2024-03-05T10:00:00 ") + level + " a " + std::to_string(code) + " message " + std::to_string(code) + "\n";
    };
    add(5, 500, "ERROR");
    add(4, 501, "ERROR");
    add(3, 502, "ERROR");
    add(2, 503, "ERROR");
    add(10, 999, "WARN");  // frequent, but not an error
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.top_errors.size(), std::size_t{3});
    if (r.top_errors.size() == 3) {
        ZTH_EXPECT_EQ(r.top_errors[0].code, 500);
        ZTH_EXPECT_EQ(r.top_errors[1].code, 501);
        ZTH_EXPECT_EQ(r.top_errors[2].code, 502);
        ZTH_EXPECT_EQ(r.top_errors[2].count, std::size_t{3});
    }
}
ZTH_TEST("robust", "analyze: error codes with equal counts - the smaller code first; the first message of a code is kept") {
    const Report r = analyze(
        "2024-03-05T10:00:00 ERROR a 700 first seven\n"
        "2024-03-05T10:00:01 ERROR a 300 first three\n"
        "2024-03-05T10:00:02 ERROR a 700 second seven\n"
        "2024-03-05T10:00:03 ERROR a 300 second three\n"
        "2024-03-05T10:00:04 ERROR a 100 only one\n");
    ZTH_EXPECT_EQ(r.top_errors.size(), std::size_t{3});
    if (r.top_errors.size() == 3) {
        ZTH_EXPECT_EQ(r.top_errors[0].code, 300);
        ZTH_EXPECT_EQ(r.top_errors[0].message, S("first three"));
        ZTH_EXPECT_EQ(r.top_errors[1].code, 700);
        ZTH_EXPECT_EQ(r.top_errors[1].message, S("first seven"));
        ZTH_EXPECT_EQ(r.top_errors[2].code, 100);
    }
}
ZTH_TEST("robust", "analyze: the Report owns its strings (the log text may be destroyed afterwards)") {
    Report r;
    {
        S log = "2024-03-05T10:00:00 ERROR component-with-a-long-name 500 a message that is long enough to need the heap\n";
        r = analyze(log);
        log.assign(log.size(), 'x');
    }
    ZTH_EXPECT_EQ(r.busiest_component, S("component-with-a-long-name"));
    ZTH_EXPECT_EQ(r.top_errors.size(), std::size_t{1});
    if (!r.top_errors.empty()) ZTH_EXPECT_EQ(r.top_errors[0].message, S("a message that is long enough to need the heap"));
}
ZTH_TEST("robust", "analyze: the peak error hour - the boundary between hours, and ties go to the earlier hour (off-by-one)") {
    const Report a = analyze(
        "2024-03-05T13:59:59 ERROR a 1 x\n"
        "2024-03-05T14:00:00 ERROR a 1 x\n"
        "2024-03-05T14:30:00 ERROR a 1 x\n");
    ZTH_EXPECT_TRUE(a.peak_error_hour.has_value());
    if (a.peak_error_hour) ZTH_EXPECT_EQ(format_time(*a.peak_error_hour), S("2024-03-05T14:00:00"));
    const Report b = analyze(
        "2024-03-05T15:10:00 ERROR a 1 x\n"
        "2024-03-05T09:10:00 ERROR a 1 x\n"
        "2024-03-05T12:10:00 ERROR a 1 x\n");
    ZTH_EXPECT_TRUE(b.peak_error_hour.has_value());
    if (b.peak_error_hour) ZTH_EXPECT_EQ(format_time(*b.peak_error_hour), S("2024-03-05T09:00:00"));
}
ZTH_TEST("robust", "analyze: only ERROR lines count for the peak hour, and no errors means no peak") {
    const Report r = analyze(
        "2024-03-05T10:00:00 WARN a 1 x\n"
        "2024-03-05T10:01:00 WARN a 1 x\n");
    ZTH_EXPECT_FALSE(r.peak_error_hour.has_value());
    ZTH_EXPECT_TRUE(r.top_errors.empty());
}
ZTH_TEST("robust", "analyze: times before 1970 fall in the right hour (floor division, not truncation)") {
    const Report r = analyze("1969-12-31T23:30:00 ERROR a 1 x\n");
    ZTH_EXPECT_TRUE(r.peak_error_hour.has_value());
    if (r.peak_error_hour) {
        ZTH_EXPECT_EQ(*r.peak_error_hour, std::int64_t{-3600});
        ZTH_EXPECT_EQ(format_time(*r.peak_error_hour), S("1969-12-31T23:00:00"));
    }
}
ZTH_TEST("robust", "format_report with nothing parsed") {
    ZTH_EXPECT_EQ(format_report(analyze("")),
                  S("Lines: 0 (0 parsed, 0 malformed)\n"
                    "Levels: DEBUG 0, INFO 0, WARN 0, ERROR 0\n"
                    "Span: none\n"
                    "Busiest component: none\n"
                    "Peak error hour: none\n"
                    "Top errors: none\n"));
    ZTH_EXPECT_EQ(format_report(analyze("junk\n")),
                  S("Lines: 1 (0 parsed, 1 malformed)\n"
                    "Levels: DEBUG 0, INFO 0, WARN 0, ERROR 0\n"
                    "Span: none\n"
                    "Busiest component: none\n"
                    "Peak error hour: none\n"
                    "Top errors: none\n"));
}
ZTH_TEST("robust", "format_report without errors") {
    ZTH_EXPECT_EQ(format_report(analyze("2024-03-05T10:00:00 INFO web 200 ok\n2024-03-05T10:00:05 WARN web 1 hm\n")),
                  S("Lines: 2 (2 parsed, 0 malformed)\n"
                    "Levels: DEBUG 0, INFO 1, WARN 1, ERROR 0\n"
                    "Span: 2024-03-05T10:00:00 to 2024-03-05T10:00:05\n"
                    "Busiest component: web\n"
                    "Peak error hour: none\n"
                    "Top errors: none\n"));
}
ZTH_TEST("robust", "format_report lists three errors, numbered") {
    const Report r = analyze(
        "2024-03-05T10:00:00 ERROR a 1 one\n"
        "2024-03-05T10:00:00 ERROR a 1 one again\n"
        "2024-03-05T10:00:00 ERROR a 2 two\n"
        "2024-03-05T10:00:00 ERROR a 3 three\n"
        "2024-03-05T10:00:00 ERROR a 4 four\n");
    ZTH_EXPECT_EQ(format_report(r),
                  S("Lines: 5 (5 parsed, 0 malformed)\n"
                    "Levels: DEBUG 0, INFO 0, WARN 0, ERROR 5\n"
                    "Span: 2024-03-05T10:00:00 to 2024-03-05T10:00:00\n"
                    "Busiest component: a\n"
                    "Peak error hour: 2024-03-05T10:00:00\n"
                    "Top errors:\n"
                    "  1. 1 x2 one\n"
                    "  2. 2 x1 two\n"
                    "  3. 3 x1 three\n"));
}

// ================================================ Fast ========================================================
ZTH_TEST("fast", "parse_timestamp and parse_line allocate nothing") {
    long sum = 0;
    zth::mem::Scope scope;
    for (int i = 0; i < 2000; ++i) {
        const auto e = parse_line("2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2 with a long message");
        if (e) sum += e->code;
        const auto t = parse_timestamp("2024-03-05T13:02:11");
        if (t) sum += *t % 7;
    }
    ZTH_EXPECT_TRUE(sum > 0);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 0);
}
ZTH_TEST("fast", "analyze does not allocate a string per line (20000 lines with long messages)") {
    const S log = make_log(20000);
    zth::mem::Scope scope;
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.parsed, std::size_t{20000});
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 600);
}
ZTH_TEST("fast", "analyze of 20000 lines gives consistent totals") {
    const S log = make_log(20000);
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.lines, std::size_t{20000});
    ZTH_EXPECT_EQ(r.by_level[0], std::size_t{5000});
    ZTH_EXPECT_EQ(r.by_level[3], std::size_t{5000});
    ZTH_EXPECT_EQ(r.top_errors.size(), std::size_t{3});
    ZTH_EXPECT_TRUE(r.first_time.has_value());
    if (r.first_time) ZTH_EXPECT_EQ(*r.first_time, kBase);
}
ZTH_TEST("fast", "analyze handles 200000 lines quickly") {
    const S log = make_log(200000);
    const Report r = analyze(log);
    ZTH_EXPECT_EQ(r.parsed, std::size_t{200000});
}
ZTH_TEST("fast", "format_report allocates a few strings, not one per formatted piece (a short report)") {
    const Report r = analyze(kSampleLog);
    zth::mem::Scope scope;
    const S text = format_report(r);
    ZTH_EXPECT_TRUE(text.size() > 100);
    ZTH_EXPECT_ALLOCS_AT_MOST(scope, 60);
}

// ================================================ Clean =======================================================
ZTH_TEST("clean", "log_analyzer.hpp does not use <regex>, string streams, sscanf, atoi or stoi: from_chars and string_view do the parsing") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("log_analyzer.hpp"));
    for (const char* word : {"regex", "stringstream", "istringstream", "ostringstream", "sscanf", "atoi", "atol", "stoi", "stol", "stoll", "stod", "strtol"}) {
        if (!::zth::find_word(code, word).empty()) {
            ::zth::fail(S("log_analyzer.hpp uses ") + word + ": parse with std::from_chars and std::string_view");
            return;
        }
    }
    ZTH_EXPECT_TRUE(true);
}
ZTH_TEST("clean", "log_analyzer.hpp has no `using namespace` and no raw new/delete/malloc/free") {
    ZTH_EXPECT_FALSE(::zth::source_layout_of("log_analyzer.hpp").using_directive);
    ZTH_EXPECT_NO_RAW_MEMORY("log_analyzer.hpp");
}
ZTH_TEST("clean", "log_analyzer.hpp does not call std::sort on all the data: the top errors use std::partial_sort") {
    const std::string code = ::zth::strip_comments_and_literals(::zth::read_file("log_analyzer.hpp"));
    ZTH_EXPECT_TRUE(code.find("partial_sort") != std::string::npos || code.find("nth_element") != std::string::npos ||
                    code.find("max_element") != std::string::npos || code.find("min_element") != std::string::npos);
}
