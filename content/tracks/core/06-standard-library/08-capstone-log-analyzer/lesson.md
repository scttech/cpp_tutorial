# Capstone: Log Analyzer

The last project of the module uses all of it. You write the core of a tool that reads a server log and answers: *how many lines, how many are broken, how many of each level, over what period, which component is busiest, which errors are most frequent, and in which hour did things go wrong?*

```
2024-03-05T12:00:00 INFO web 200 Request served
2024-03-05T13:02:11 ERROR db 503 Connection refused to replica-2
this line is not a log line
2024-02-30T10:00:00 INFO web 200 There is no February 30th
```

becomes

```
Lines: 9 (7 parsed, 2 malformed)
Levels: DEBUG 1, INFO 1, WARN 1, ERROR 4
Span: 2024-03-05T12:00:00 to 2024-03-05T14:30:10
Busiest component: db
Peak error hour: 2024-03-05T13:00:00
Top errors:
  1. 503 x3 Connection refused to replica-2
  2. 404 x1 Not found: /missing
```

## The log format

`TIMESTAMP LEVEL COMPONENT CODE MESSAGE`, separated by single spaces:

| Field | Rule |
|---|---|
| TIMESTAMP | exactly `YYYY-MM-DDTHH:MM:SS` (UTC), and a **real** date and time: month 1-12, a day that exists in that month and year (`2024-02-29` yes, `2023-02-29` and `2100-02-29` no), hour < 24, minute < 60, second < 60 |
| LEVEL | `DEBUG`, `INFO`, `WARN` or `ERROR`, upper case |
| COMPONENT | a word without spaces (not empty) |
| CODE | an integer (negative is fine; `5x`, `+5`, `1.5` and a number too big for an `int` are not) |
| MESSAGE | the rest of the line: may be empty, may contain any spaces. Whitespace around the **whole line** (a `\r` of a Windows line ending) is ignored |

## What you write

`log_analyzer.hpp` gives you the types (`Level`, `EntryView`, `ErrorGroup`, `Report`); you write six functions, in this order:

1. **`level_name`**: `Level` to text.
2. **`parse_timestamp`**: text to seconds since 1970. This is `<chrono>`'s calendar: build a `std::chrono::year_month_day`, ask `.ok()` (it knows leap years), convert with `std::chrono::sys_days`.
3. **`format_time`**: seconds back to `YYYY-MM-DDTHH:MM:SS` (`floor<days>`, `year_month_day`, `hh_mm_ss`, `std::format`).
4. **`parse_line`**: one line to an `EntryView`, whose text fields are **`string_view`s into the line**: no copies, no allocation.
5. **`analyze`**: the whole log to a `Report`, in **one pass**, counting with `std::unordered_map` keyed by `string_view`s into the log text; only the few strings the `Report` keeps are copied (the `Report` outlives the log text, the views do not).
6. **`format_report`**: the exact text above, with `std::format`.

## Decisions the tests check

- **Equal counts**: the busiest component is the alphabetically first; error codes with equal counts are ordered by the smaller code; the peak hour is the earliest. An `unordered_map` iterates in no particular order, so ties must be broken **explicitly**.
- **The hour of a timestamp** is `floor(t / 3600)`. For a time before 1970 (`t` negative) `t / 3600` *truncates toward zero* and gives the wrong hour: use a floor division.
- **First and last time** are the minimum and the maximum, not the first and last *line* (logs are not always in order).
- **Blank lines** are skipped and not counted; **malformed lines** are counted; `lines == parsed + malformed`.
- **A log may end without a newline**, may use `\r\n`, and may be empty.

## Tiers

- **Works**: the sample log, field by field and as a report.
- **Robust**: dates and times that do not exist, the exact shape of the timestamp, every kind of malformed line, whitespace and CRLF, empty and garbage logs, all the tie-breaks, a report that owns its strings, times before 1970, and the exact output of `format_report` in every shape.
- **Fast**: `parse_line` allocates nothing, and `analyze` of 20000 lines with long messages allocates only a few hundred blocks (not one per line); 200000 lines are analyzed quickly.
- **Clean**: no `<regex>`, no string streams, no `sscanf`/`atoi`/`stoi`: `from_chars` and `string_view` do the parsing; no `using namespace`; no raw `new`/`delete`; the top errors use a partial sort, not a sort of everything.

## Order of work

1. `level_name`, `parse_timestamp` and `format_time` (test each against the examples), then `parse_line`: the first group of tests passes.
2. `analyze`: add the counters one at a time (lines, levels, span, components, errors, hours); run the tests after each.
3. `format_report` last: it is only formatting.
