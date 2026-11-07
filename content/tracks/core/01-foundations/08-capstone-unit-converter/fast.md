# Fast path: what you need

- Line-based stdin loop: `while (std::getline(std::cin, line))`; split on whitespace with `std::istringstream`.
- Unit table: `std::unordered_map<std::string, Unit>` (or a sorted array + `std::lower_bound`); convert through a base unit (m, kg, K).
- Numbers: `std::from_chars(first, last, double&)`; check `ec == std::errc{}`, `ptr == last`, and `std::isfinite(v)` (it parses `nan`/`inf`).
- Output: `std::cout << std::fixed << std::setprecision(4)` (the settings are *sticky* on the stream). Normalize `-0.0` results.
- Everything (including errors) goes to stdout so the tests can capture it.
