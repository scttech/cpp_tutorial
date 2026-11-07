# Fast path

- Read with `std::cin >> start >> end >> step` and test the stream (`if (!(std::cin >> a >> b >> c))`).
- `C * 9 / 5` with an `int` `C` truncates; use `9.0 / 5.0` or a `double`.
- `<iomanip>`: `std::setw(n)` applies to the **next** output only; `std::fixed` and `std::setprecision(n)` are sticky.
- Loop bound safety: do the loop arithmetic in a wider type (`long long`) so `c += step` cannot overflow `int` (UB) near `INT_MAX`.
