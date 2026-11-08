# Operator overloading

`a + b` on two `int`s is built in. On your own types it is a **function call in disguise**: `a + b` means
`operator+(a, b)` (or `a.operator+(b)`), and you can write that function. Used well, it makes a type read like a
number. Used badly, it makes code nobody can predict. The rule: **overload an operator only when it does what everyone
expects**, and make it behave exactly like the same operator on `int`.

You already wrote some in the test-out (`Fraction`). This lesson is about the *shape* of each kind, and the mistakes
that go with it.

## Compound assignment: members that return `*this`

`+=`, `-=`, `*=` change the object, so they are **members**. By convention they return the object itself **by reference**:

```cpp
Vec2& operator+=(const Vec2& other) {
    x_ += other.x_;
    y_ += other.y_;
    return *this;                 // a reference to the object, not a copy
}
```

For `int`, `(a += b) += c` and `x = (y += z)` work because `+=` gives back the variable itself. If you return a *copy*
(`Vec2 operator+=`), the second `+= c` modifies a temporary and `a` silently misses it.

## Binary operators: non-members, built from the compound ones

`a + b` must **not** change `a` or `b`. Write it as a non-member that takes the left side **by value** (the copy you were
going to make anyway), uses `+=` on it, and returns it:

```cpp
inline Vec2 operator+(Vec2 a, const Vec2& b) { return a += b; }
inline Vec2 operator*(Vec2 v, int k)         { return v *= k; }
inline Vec2 operator*(int k, const Vec2& v)  { return v * k; }
```

Why non-members? **Symmetry.** `v * 3` could be a member (`v.operator*(3)`), but `3 * v` cannot: the left operand is an
`int`, and `int` has no members. If both orders should work, write both, as free functions. (A member `operator+` also
only converts its *right* operand, never its left, which gives surprising asymmetries.)

Why by value on the left? If `operator+` took `Vec2& a` it could not be used with a `const` vector or a temporary, and
it would be tempting to modify `a`. The flawed `operator+` in the starter *changes its left operand*: after `c = a + b`, `a` has
changed too. Run the demo to see it.

## Unary operators, `==`, and `<<`

```cpp
Vec2 operator-() const { return Vec2(-x_, -y_); }                         // -v: a new vector, v unchanged (const!)
friend bool operator==(const Vec2&, const Vec2&) = default;               // compares every member; != comes for free
inline std::ostream& operator<<(std::ostream& os, const Vec2& v) {        // a non-member: the stream is on the left
    return os << '(' << v.x() << ", " << v.y() << ')';                    // take AND return the stream by reference: chainable
}
```

## `operator[]`, `++`, and `bool`

- **`operator[]`** usually has two overloads, like `at()` last time: `int& operator[](size_t)` and
  `const int& operator[](size_t) const`.
- **Prefix and postfix `++`** are two different functions. Prefix `++v` changes and returns the object itself
  (`Vec2&`). Postfix `v++` has a dummy `int` parameter (that is the only way the compiler tells them apart), and must
  return the **old** value, so it has to make a copy: `Vec2 old = *this; ++*this; return old;`. That is why `++i` is
  never slower than `i++`, and the habit to have in loops over iterators.
- **`explicit operator bool() const`** lets `if (v)` work. Without `explicit`, a `Vec2` silently converts to `bool`, and
  then to `int` (`v + 1`, `int n = v;` compile and mean nothing). `if`, `&&`, `||`, `!` and `?:` still work with an
  explicit one: they are "contextual conversions".

## Your task

Complete `vec2.hpp`. `main.cpp` (read-only) is a small demo; the comment at the end lists things to try once your
operators exist.

| Operation | Behavior |
|---|---|
| `a + b`, `a - b` | A new `Vec2`; neither operand changes; works on `const` vectors. |
| `v * k`, `k * v` | Scale both parts (two functions). |
| `-v` | A new `Vec2`. |
| `a += b`, `a -= b`, `a *= k` | Members, change `a`, return `Vec2&`. Must work when `b` is `a` itself. |
| `a == b`, `a != b` | Both parts equal (a defaulted `friend operator==`). |
| `v[0]`, `v[1]` | x and y; a non-const overload for writing and a const one. Anything else throws `std::out_of_range`. |
| `explicit operator bool` | `true` unless the vector is `(0, 0)`. |
| `++v`, `v++` | Add 1 to both parts. Prefix returns `Vec2&`, postfix returns the old value. |
| `dot(a, b)` | `a.x*b.x + a.y*b.y`. |
| `os << v` | Prints `(3, -4)`; returns the stream. |

The tests check each operator's *shape* (what it returns, whether it works on `const`) as well as its behavior, and run
a few hundred random vectors through the algebra: `(a + b) - b == a`, `k * a == a * k`, and so on.
