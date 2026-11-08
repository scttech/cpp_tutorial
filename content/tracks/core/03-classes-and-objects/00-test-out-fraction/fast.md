# Fast path

Complete `Fraction` in `fraction.hpp`: lowest terms, positive denominator, zero is `0/1`, all done in the **constructor** (zero denominator throws `std::invalid_argument`). Then `+ - * /` (non-members built on `+= -= *= /=`, so `2 + f` works; divide by zero throws `std::domain_error`), unary `-`, `operator<=>` plus a defaulted `operator==`, `operator<<` (`3/4`, whole numbers without `/1`), and `explicit operator double()`. Avoid overflow: least common denominator when adding, cancel with `std::gcd` before multiplying. Compare by cross-multiplying, never through `double`.
