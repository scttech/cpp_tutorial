// MUTANT: the summary carries the time of the original line
#pragma once
// YOUR STRUCTURED LOGGER (namespace slog; Level, Value, Field, Record, kv(), Sink and Clock are in types.hpp).
//
// level_name(level) -> string_view      "trace", "debug", "info", "warn", "error", "fatal"
// parse_level(text) -> optional<Level>   case-insensitive; also accepts "warning" for Warn; nullopt for anything else (including "" and " info")
// iso8601(ms) -> string                 "YYYY-MM-DDTHH:MM:SS.mmmZ" for milliseconds since the epoch, in UTC. Negative times are before 1970 (-1 is 1969-12-31T23:59:59.999Z: divide with a FLOOR).
//                                       Proleptic Gregorian calendar (leap years: divisible by 4, except centuries not divisible by 400). Years 0000 to 9999 only: otherwise "" (empty).
// sanitize_utf8(text) -> string          valid UTF-8 for any bytes: valid sequences are kept, and every INVALID one becomes U+FFFD (EF BF BD) by the WHATWG rule: at a bad or truncated sequence write ONE U+FFFD for the bytes
//                                       consumed so far and continue with the byte that broke it. Lead bytes: C2..DF + 1 continuation; E0..EF + 2 (after E0 the first must be A0..BF, after ED 80..9F: no overlongs, no surrogates);
//                                       F0..F4 + 3 (after F0 the first must be 90..BF, after F4 80..8F: nothing above U+10FFFF). C0, C1, F5..FF and a stray continuation byte (80..BF) are invalid on their own.
//                                       Examples: "\xFF" -> 1 U+FFFD;  "\xE2\x82" (cut off) -> 1;  "\xE2\x82\x41" -> U+FFFD then "A";  "\xC0\x80" -> 2;  "\xED\xA0\x80" -> 3.
// escape_json(out, text)                appends the JSON string BODY (no surrounding quotes) of text: sanitize first, then  " -> \"   \ -> \\   \b \f \n \r \t   other control characters below 0x20 as \u00xx
//                                       (lower-case hex, four digits); everything else untouched (so '/' and DEL and non-ASCII are written as they are).
// to_json(record) -> string             ONE line, no trailing newline, no spaces:  {"ts":"<iso8601>","level":"info","msg":"...","key":value,...}
//     * ts is null (the word, no quotes) when iso8601 gives "".
//     * fields follow in their given order. A key equal to "ts", "level" or "msg" is written with a leading underscore ("_ts") so that it can never replace the real one.
//       If two fields end up with the same key, there is ONE entry: it stays at the position of the FIRST and takes the value of the LAST. Keys are escaped like strings.
//     * values: String -> quoted and escaped. Int -> decimal digits (also the most negative value). Bool -> true/false. Null -> null.
//       Double -> the SHORTEST text that reads back as the same double (std::to_chars), with ".0" appended when that text has no '.', 'e' or 'E' (so 1.0 stays a float: "1.0", but 1e21 is "1e+21");
//       NaN and infinities are not JSON: null.
//
// struct Options { Level min_level = Level::Info; std::int64_t dedupe_window_ms = 0; std::size_t max_line_bytes = 0; }
// Logger(sink, clock, options)   log(level, message, fields)   trace/debug/info/warn/error/fatal(message, fields)   with(fields) -> Logger   flush()
//   * log(): records below min_level are dropped (nothing else happens: no clock reading needed, no state change). The timestamp is clock.now_ms() read once per log call.
//     The record's fields are the logger's context fields (from with(), outermost first) followed by the call's fields.
//   * with(fields): a child logger that adds those fields to every record (after its parent's). Parent and child share the sink, the clock, the options AND the duplicate-suppression state below.
//   * Duplicate suppression (only when dedupe_window_ms > 0): the "key" of a record is its level, message and complete field list (context + call). If a record has the same key as the last line that was
//     WRITTEN and now - (the time of that line) < dedupe_window_ms, it is not written; it is counted. When a record with a different key arrives (or the same key after the window), FIRST a summary line is written
//     if anything was counted: level and message of the suppressed record, the same fields, plus a final field "repeated" (Int) with the count, timestamp = now; THEN the record itself is written. flush() writes a pending summary
//     (timestamp = now) and writes nothing otherwise. The window runs from the last written ORIGINAL line, not from the summary.
//   * Line length (only when max_line_bytes > 0): a line longer than max_line_bytes is shortened. First the message is cut to the longest prefix of whole code points such that prefix + "…" (U+2026, 3 bytes)
//     makes the whole line fit. If even an empty prefix (so just "…") does not fit, user fields are removed from the END, one at a time; as soon as the COMPLETE message fits with the fields that are left, the line is written with the complete message,
//     and if instead the empty prefix fits first, the message is cut to the longest prefix that fits with the fields that are left. If nothing fits, the shortest form (all fields removed, msg "…") is written anyway.
//     Lines that fit are written unchanged. The limit counts the bytes of the finished JSON line.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace slog {

struct Options {
    Level min_level = Level::Info;
    std::int64_t dedupe_window_ms = 0;
    std::size_t max_line_bytes = 0;
};

inline std::string_view level_name(Level l) {
    switch (l) {
        case Level::Trace: return "trace";
        case Level::Debug: return "debug";
        case Level::Info: return "info";
        case Level::Warn: return "warn";
        case Level::Error: return "error";
        case Level::Fatal: return "fatal";
    }
    return "info";
}

inline std::optional<Level> parse_level(std::string_view text) {
    std::string t;
    for (char c : text) t.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    for (Level l : {Level::Trace, Level::Debug, Level::Info, Level::Warn, Level::Error, Level::Fatal})
        if (t == level_name(l)) return l;
    if (t == "warning") return Level::Warn;
    return std::nullopt;
}

inline std::string iso8601(std::int64_t ms) {
    constexpr std::int64_t kDay = 86400000;
    std::int64_t days = ms / kDay, rem = ms % kDay;
    if (rem < 0) {   // floor division: -1 ms is the last millisecond of the previous day
        rem += kDay;
        --days;
    }
    // days since 1970-01-01 -> civil date (Howard Hinnant's algorithm, proleptic Gregorian)
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    std::int64_t year = yoe + era * 400;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    const std::int64_t day = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t month = mp < 10 ? mp + 3 : mp - 9;
    if (month <= 2) ++year;
    if (year < 0 || year > 9999) return "";
    const auto digits = [](std::string& s, std::int64_t v, int width) {
        std::string d(static_cast<std::size_t>(width), '0');
        for (int k = width - 1; k >= 0; --k) {
            d[static_cast<std::size_t>(k)] = static_cast<char>('0' + v % 10);
            v /= 10;
        }
        s += d;
    };
    std::string out;
    digits(out, year, 4);
    out += '-';
    digits(out, month, 2);
    out += '-';
    digits(out, day, 2);
    out += 'T';
    digits(out, rem / 3600000, 2);
    out += ':';
    digits(out, rem / 60000 % 60, 2);
    out += ':';
    digits(out, rem / 1000 % 60, 2);
    out += '.';
    digits(out, rem % 1000, 3);
    out += 'Z';
    return out;
}

inline std::string sanitize_utf8(std::string_view t) {
    std::string out;
    out.reserve(t.size());
    std::size_t i = 0;
    const std::size_t n = t.size();
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
            continue;
        }
        std::size_t need = 0;
        unsigned lower = 0x80, upper = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) {
            need = 1;
        } else if (c >= 0xE0 && c <= 0xEF) {
            need = 2;
            if (c == 0xE0) lower = 0xA0;
            if (c == 0xED) upper = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3;
            if (c == 0xF0) lower = 0x90;
            if (c == 0xF4) upper = 0x8F;
        } else {
            out += "\xEF\xBF\xBD";
            ++i;
            continue;
        }
        std::size_t j = i + 1, got = 0;
        bool ok = true;
        while (got < need) {
            if (j >= n) {
                ok = false;
                break;
            }
            const unsigned char d = static_cast<unsigned char>(t[j]);
            if (d < lower || d > upper) {
                ok = false;
                break;
            }
            lower = 0x80;
            upper = 0xBF;
            ++j;
            ++got;
        }
        if (ok) out.append(t.substr(i, need + 1));
        else out += "\xEF\xBF\xBD";
        i = ok ? j : j;   // on failure the byte that broke the sequence is looked at again
    }
    return out;
}

inline void escape_json(std::string& out, std::string_view text) {
    const std::string clean = sanitize_utf8(text);
    static const char* kHex = "0123456789abcdef";
    for (char ch : clean) {
        const unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += kHex[c >> 4];
                    out += kHex[c & 15];
                } else {
                    out.push_back(ch);
                }
        }
    }
}

namespace detail {

inline void append_value(std::string& out, const Value& v) {
    switch (v.type) {
        case Value::Type::String:
            out += '"';
            escape_json(out, v.s);
            out += '"';
            break;
        case Value::Type::Int: {
            char buf[32];
            const auto r = std::to_chars(buf, buf + sizeof buf, v.i);
            out.append(buf, r.ptr);
            break;
        }
        case Value::Type::Bool: out += v.b ? "true" : "false"; break;
        case Value::Type::Null: out += "null"; break;
        case Value::Type::Double: {
            if (!std::isfinite(v.d)) {
                out += "null";
                break;
            }
            char buf[64];
            const auto r = std::to_chars(buf, buf + sizeof buf, v.d);
            const std::string_view text(buf, static_cast<std::size_t>(r.ptr - buf));
            out += text;
            if (text.find_first_of(".eE") == std::string_view::npos) out += ".0";
            break;
        }
    }
}

// ,"key":value,... for the fields, reserved keys renamed, duplicates merged
inline std::string fields_fragment(const std::vector<Field>& fields) {
    std::vector<std::string> keys;
    std::vector<const Value*> values;
    keys.reserve(fields.size());
    values.reserve(fields.size());
    std::unordered_map<std::string_view, std::size_t> seen;
    for (const Field& f : fields) {
        std::string key = (f.key == "ts" || f.key == "level" || f.key == "msg") ? "_" + f.key : f.key;
        const auto it = seen.find(key);
        if (it != seen.end()) {
            values[it->second] = &f.value;   // the last value wins, at the position of the first
            continue;
        }
        keys.push_back(std::move(key));
        values.push_back(&f.value);
        seen.emplace(std::string_view(keys.back()), keys.size() - 1);
    }
    std::string out;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        out += ",\"";
        escape_json(out, keys[i]);
        out += "\":";
        append_value(out, *values[i]);
    }
    return out;
}

inline std::string assemble(const Record& r, const std::string& message, const std::string& fragment) {
    std::string out = "{\"ts\":";
    const std::string ts = iso8601(r.timestamp_ms);
    if (ts.empty()) out += "null";
    else out += "\"" + ts + "\"";
    out += ",\"level\":\"";
    out += level_name(r.level);
    out += "\",\"msg\":\"";
    escape_json(out, message);
    out += "\"";
    out += fragment;
    out += "}";
    return out;
}

}  // namespace detail

inline std::string to_json(const Record& r) { return detail::assemble(r, r.message, detail::fields_fragment(r.fields)); }

namespace detail {

// to_json, shortened to max_line bytes as described in the header comment
inline std::string fit(const Record& r, std::size_t max_line) {
    std::string line = to_json(r);
    if (max_line == 0 || line.size() <= max_line) return line;
    const std::string clean = sanitize_utf8(r.message);
    std::vector<std::size_t> cuts;   // byte offsets of the code points, and the end
    for (std::size_t i = 0; i < clean.size(); ++i)
        if ((static_cast<unsigned char>(clean[i]) & 0xC0) != 0x80) cuts.push_back(i);
    cuts.push_back(clean.size());
    const std::size_t points = cuts.size() - 1;
    const auto render = [&](const Record& rec, std::size_t k) {
        return assemble(rec, clean.substr(0, cuts[k]) + "\xE2\x80\xA6", fields_fragment(rec.fields));
    };
    Record work = r;
    while (render(work, 0).size() > max_line) {   // not even the empty prefix fits
        if (work.fields.empty()) return render(work, 0);   // nothing fits: the shortest form
        work.fields.pop_back();
        const std::string whole = to_json(work);
        if (whole.size() <= max_line) return whole;   // with fewer fields the complete message fits
    }
    std::size_t lo = 0, hi = points == 0 ? 0 : points - 1;   // the largest k in [lo, hi] that fits (k = 0 fits)
    while (lo < hi) {
        const std::size_t mid = (lo + hi + 1) / 2;
        if (render(work, mid).size() <= max_line) lo = mid;
        else hi = mid - 1;
    }
    return render(work, lo);
}

}  // namespace detail

class Logger {
public:
    Logger(Sink& sink, Clock& clock, Options options = {}) : core_(std::make_shared<Core>()) {
        core_->sink = &sink;
        core_->clock = &clock;
        core_->options = options;
    }

    void log(Level level, std::string_view message, std::vector<Field> fields = {}) {
        Core& c = *core_;
        if (level < c.options.min_level) return;
        const std::int64_t now = c.clock->now_ms();
        std::vector<Field> all = base_;
        for (Field& f : fields) all.push_back(std::move(f));
        std::string key = std::string(level_name(level)) + '\0' + std::string(message) + '\0' + detail::fields_fragment(all);
        if (c.options.dedupe_window_ms > 0 && c.has_last && c.last_key == key && now - c.last_time < c.options.dedupe_window_ms) {
            ++c.suppressed;
            return;
        }
        summary(now);
        Record r;
        r.level = level;
        r.timestamp_ms = now;
        r.message = std::string(message);
        r.fields = all;
        c.sink->write(detail::fit(r, c.options.max_line_bytes));
        c.has_last = true;
        c.last_key = std::move(key);
        c.last_record = std::move(r);
        c.last_time = now;
    }

    void trace(std::string_view m, std::vector<Field> f = {}) { log(Level::Trace, m, std::move(f)); }
    void debug(std::string_view m, std::vector<Field> f = {}) { log(Level::Debug, m, std::move(f)); }
    void info(std::string_view m, std::vector<Field> f = {}) { log(Level::Info, m, std::move(f)); }
    void warn(std::string_view m, std::vector<Field> f = {}) { log(Level::Warn, m, std::move(f)); }
    void error(std::string_view m, std::vector<Field> f = {}) { log(Level::Error, m, std::move(f)); }
    void fatal(std::string_view m, std::vector<Field> f = {}) { log(Level::Fatal, m, std::move(f)); }

    Logger with(std::vector<Field> fields) const {
        Logger child = *this;
        for (Field& f : fields) child.base_.push_back(std::move(f));
        return child;
    }

    void flush() { summary(core_->clock->now_ms()); }

private:
    struct Core {
        Sink* sink = nullptr;
        Clock* clock = nullptr;
        Options options;
        bool has_last = false;
        std::string last_key;
        Record last_record;
        std::int64_t last_time = 0;
        std::size_t suppressed = 0;
    };

    void summary(std::int64_t now) {
        Core& c = *core_;
        if (c.suppressed == 0) return;
        Record r = c.last_record;
        r.fields.push_back(kv("repeated", static_cast<std::int64_t>(c.suppressed)));
        c.suppressed = 0;
        c.sink->write(detail::fit(r, c.options.max_line_bytes));
    }

    std::shared_ptr<Core> core_;
    std::vector<Field> base_;
};

}  // namespace slog
