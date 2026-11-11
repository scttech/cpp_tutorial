# Fast path: what's different in C++

- **Errors without exceptions.** A parser meets bad input all the time; that is not exceptional. Returning a small `Error` plus a position keeps the success path cheap and lets the caller decide. Other languages would throw `JSONDecodeError`; C++ libraries (simdjson, rapidjson, nlohmann's `accept`) offer the value-returning form for the same reason.
- **`std::variant` instead of a class hierarchy.** A JSON value is *one of* six things. `std::variant` stores exactly one of them inline, with no heap allocation for the tag, and `std::visit` / `std::get_if` make the type check explicit. Compare Java's `Object` or a Python dynamic value.
- **`std::string_view` for the input.** The parser never owns or copies the text; it walks an index over memory someone else holds. Take a `std::string` by value and you copy megabytes for nothing.
- **Moving values.** `array.push_back(std::move(element))` hands the element's storage over; `push_back(element)` copies every nested array and string. `Value` counts copies so the test can see the difference.
- **`strtod` and `snprintf("%.17g")`** are the portable way to convert numbers in C++17; `std::from_chars` for `double` only arrived in libstdc++ 11 and libc++ 20. Remember the C locale: a locale with a decimal comma would break both.
- **Stack depth is a resource.** Recursive descent uses stack per nesting level; an attacker can send `[[[[…` for free. Every serious parser has a depth limit.
