# Fast path: what's different in C++

- **Parse with views, not copies.** `std::string_view` fields into the line (`find`, `substr`), numbers with `std::from_chars` (check the error code AND that it consumed the whole field). An `EntryView` of views allocates nothing; a struct of `std::string`s allocates for every field longer than ~15 characters.
- **A view is valid only while the text it views is.** That is why `analyze` can key a map by `string_view`s into the log (the log outlives the call), but the `Report` that it RETURNS must own its strings.
- **`<chrono>` calendar (C++20)**: `std::chrono::year_month_day{year{y}, month{m}, day{d}}`, `.ok()` validates the date (leap years included), `sys_days{ymd}` is a time point in days since 1970-01-01, `time_since_epoch()` its duration. The other way: `floor<days>(time_point)`, `year_month_day{day_point}`, `hh_mm_ss{time_of_day}`. `sys_seconds` is `time_point<system_clock, seconds>`.
- **One pass** over the data: count into hash maps as you go instead of building intermediate vectors, and extract only what you need (a top 3 with `std::partial_sort`, a maximum with a loop).
- **`std::unordered_map` order is unspecified**: any "first/most/least" result must have an explicit tie-break (compare counts, then names/keys).
- **Floor division**: `a / b` truncates toward zero in C++. For negative `a` the floor is one less (`-1800 / 3600 == 0` but the hour is `-1`).
- **`std::format`** with width/fill (`{:04}`, `{:02}`) writes fixed-width numbers; `{}` takes `size_t`, `int`, `std::string`, `const char*` without casts.
- **Do not reach for `<regex>`** to parse a fixed format: a handful of `find`s and `from_chars` is faster, shorter, and does not allocate.
- **Aggregate, then format**: keep the computation (`analyze`) and the presentation (`format_report`) separate; the tests can then check each exactly.
