# A Structured Logger with a JSON Formatter

> **You're on the platform team.** Production logs are read by machines first: Elasticsearch, Loki, Datadog and CloudWatch ingest one JSON object per line and let you ask "all `error` lines for `request_id=r-42` in the last hour". That only works if every line is **valid JSON**, always. One unescaped quote in a user's name, one raw newline in a stack trace, one byte of Latin-1 in a UTF-8 field, and the line is rejected, or, worse, silently split in two. Alongside that, a service stuck in a retry loop can write a million identical lines a minute and bury everything else. You will write the formatter and the logger around it, with the edge cases that decide whether the logs survive production.

## The formatter

Each line is `{"ts":...,"level":...,"msg":...,<fields>}`. The rules that matter:

- **Escaping** is not optional and not "just the quotes": `"` and `\` must be escaped; every character below `0x20` must be escaped (the named ones as `\n`, `\t`, ..., the rest as `\u00xx`), because JSON forbids raw control characters in strings. `/` and non-ASCII characters are fine as they are.
- **Valid UTF-8.** JSON text must be UTF-8, but log messages come from user input, file names and network packets, and are often not. You cannot just copy the bytes. Replace invalid sequences with U+FFFD, using the WHATWG rule: *one* replacement character per maximal broken sequence, then go on with the byte that broke it. Overlong forms (`C0 80`), UTF-16 surrogates (`ED A0 80`) and values above U+10FFFF are invalid. Without this, one binary blob in one field makes a whole log pipeline reject the batch.
- **Numbers.** Integers are digits. A double must read back as the *same* double: use the shortest representation (`std::to_chars` gives exactly this) rather than `printf("%f")` (loses digits) or `%.17g` (ugly). A whole-valued double like `1.0` is written `1.0`, not `1`, so a consumer that sees `1` does not decide the field is an integer. NaN and infinity do not exist in JSON: write `null`.
- **Timestamps.** ISO 8601 in UTC with milliseconds. Converting milliseconds-since-epoch to a calendar date is classic: divide by the length of a day with a **floor** (so `-1 ms` is the last millisecond of 1969, not the first of 1970), then use Howard Hinnant's integer algorithm (days to year/month/day) which handles leap years (`2000` is, `1900` and `2100` are not) with no tables or loops.
- **Keys.** User fields must not be able to replace the real `ts`, `level` or `msg`, so those names get an underscore. Duplicate keys make JSON ambiguous (parsers disagree on which wins), so the formatter writes each key once: first position, last value.

## The logger

A `Logger` is a small handle: a sink to write lines to, a clock to read, options, and *context fields* (`with({request_id})` returns a child that adds them to every line). Two features beyond formatting:

- **Suppression of repeats.** If the same record (level, message, all fields) arrives again within the window of the last written line, count it instead of writing it. When the run ends, write one summary line carrying `"repeated": N`, then the new record. A retry loop becomes two lines instead of ten thousand, and nothing is lost: the count says how many there were. The window must be *strict* (`< window`), and shared by all child loggers, or a child could flood what its parent suppressed.
- **A line length cap.** Some pipelines drop or truncate lines over a limit (syslog: 1-8 KB, some agents 16 KB) in the *middle*, producing invalid JSON. So the logger shortens the line itself, cleanly: cut the message at a code-point boundary and add `…`; if the fields alone are too much, drop them from the end. The finished line is always valid JSON.

## Interfaces for testability

`Sink` and `Clock` are abstract classes (`virtual` functions). That is the standard way to make code that touches the outside world (files, time, network) testable: the program passes real implementations, a test passes a recording sink and a clock it can advance by hand, with no sleeping and no flakiness.

## The tiers

- **Works**: levels, timestamps from year 0 to 9999 with leap days and negative times, every escape, the WHATWG replacements, every value type, doubles, reserved and duplicate keys, filtering, context fields, suppression (window edges, what counts as "same", shared state, flush), truncation.
- **Robust**: 3000 random records parsed back by an independent strict JSON parser; 100000 random byte strings against an independent UTF-8 decoder; 200000 instants against `<chrono>`; 20000 doubles round-tripped; random runs of the whole logger; accounting for every suppressed record.
- **Fast**: 200000 lines and a 4 MB message; a bounded number of allocations per call and no quadratic duplicate handling; 300000 identical records.
- **Clean**: no raw memory or exceptions; no printf/streams/`std::format`, no JSON library, no `<chrono>` calendar or `gmtime`.
