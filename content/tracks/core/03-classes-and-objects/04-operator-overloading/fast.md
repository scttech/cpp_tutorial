# Fast path: what's different in C++

- An operator is a function (`operator+`). Overload it only to do what it does for `int`; keep its meaning predictable.
- **Compound assignment (`+=`)**: a **member**, changes the object, returns **`T&`** (`return *this;`) so it chains. Returning a copy is a classic bug.
- **Binary `+ - *`**: **non-members** taking the left operand **by value**, built on the compound operator: `inline T operator+(T a, const T& b) { return a += b; }`. Must not modify operands. Non-member means `3 * v` can work too (write both orders).
- **Unary `-`**: a `const` member returning a new object. **`==`**: `friend bool operator==(const T&, const T&) = default;` (C++20 also gives `!=`).
- **`<<`**: non-member `std::ostream& operator<<(std::ostream&, const T&)`, **returns the stream** so it chains.
- **`operator[]`**: a const and a non-const overload. **`++`**: prefix `T& operator++()`, postfix `T operator++(int)` returns the *old* value (a copy).
- **`explicit operator bool() const`**: `if (x)` works, `int n = x;` does not.
