# Constructors, initializer lists, and `explicit`

A **constructor** runs when an object is created. Its job is to make the object *valid from its very first moment*: no
half-built objects, no "call `init()` before you use it". If the constructor cannot produce a valid object, it throws,
and the object never exists.

```cpp
class Money {
public:
    explicit Money(long long cents) : cents_(cents) {}   // <- the member initializer list
    long long cents() const { return cents_; }
private:
    long long cents_;
};
```

## Member initializer lists

`: cents_(cents)` *initializes* the member. Writing `{ cents_ = cents; }` in the body instead would first leave `cents_`
uninitialized (or default-constructed) and then *assign* to it. For an `int` that is only a wasted step, but some members
**cannot** be assigned: `const` members, reference members, and types without a default constructor must be initialized
in the list.

Two rules to remember:

- Members are initialized **in the order they are declared in the class**, not in the order of your list. If the list
  disagrees, the compiler warns (`-Wreorder`) because the result is probably not what you meant:

  ```cpp
  struct Range {
      int last_;      // declared first, so initialized first...
      int size_;
      Range(int first, int size) : size_(size), last_(first + size_) {}  // ...so `size_` is still garbage when last_ reads it
  };
  ```

- A member you do not initialize anywhere holds **garbage** (for `int`, `long long`, pointers...). Give every member a
  value. The easiest way is a **default member initializer**:

  ```cpp
  long long cents_ = 0;      // every constructor that does not say otherwise starts it at 0
  ```

## The default constructor

`Money() = default;` asks the compiler to generate the "no arguments" constructor. Together with the default member
initializer above, `Money m;` is `$0.00`, never garbage. (If you write `Money() {}` instead and forget the initializer,
`m.cents()` is whatever bytes happened to be in that memory. The tests check exactly this.)

## Delegating constructors

When one constructor is just a different way to call another, **delegate** instead of copying code:

```cpp
Money(long long dollars, int cents) : Money(to_cents(dollars, cents)) {}
```

A delegating constructor's initializer list contains *only* the call to the other constructor. Here the call's argument is
a `static` helper that checks the numbers first. That is a handy idiom: you cannot put an `if` before a member initializer,
but you can call a function that validates and returns the value.

## `explicit`: no accidental conversions

A constructor that can be called with **one** argument also works as a silent conversion:

```cpp
class Money { public: Money(long long cents); /* ... */ };
void charge(Money price);

charge(500);        // compiles! Five dollars? Five hundred dollars? 500 cents? The reader cannot tell.
Money m = 500;      // compiles too
```

Marking it `explicit` makes the conversion something you must *ask for*:

```cpp
explicit Money(long long cents);
charge(500);          // error: no conversion from int to Money
charge(Money(500));   // clear: the caller wrote what they meant
```

Make single-argument constructors `explicit` **by default**; drop it only when the conversion really is natural (a
`std::string` from a `const char*`, a `Fraction` from a whole number). Constructors with two or more arguments rarely
need it, though brace initialization can use them: `Money m{12, 34};`.

## Validate in the constructor

If `Money(1, 100)` ("1 dollar and 100 cents") were allowed, every other function would have to wonder whether it was
looking at a nonsense amount. Reject it **once**, in the constructor, by throwing; afterwards every `Money` that exists
is valid. (Module 4 is all about what happens to resources when a constructor throws.)

## Your task

Complete `money.hpp`. `main.cpp` (read-only) is a demo: press **Run** to see it. Money is stored as whole **cents**
(never as a `double`).

| Member | Behavior |
|---|---|
| `Money()` | $0.00. Use `= default` and a default member initializer. |
| `explicit Money(long long cents)` | Any amount, negative allowed (a debt). |
| `Money(long long dollars, int cents)` | **Delegates** to the constructor above. `dollars` must be `>= 0` and `cents` in `0..99`, else throw `std::invalid_argument`. If `dollars * 100 + cents` does not fit in a `long long`, throw `std::overflow_error`. Argument errors win: `Money(LLONG_MAX, 100)` is an `invalid_argument`. |
| `cents()` | The amount in cents. |
| `to_string()` | `"$12.34"`, `"$0.05"`, `"$1.00"`; a debt has the sign in front of the `$`: `"-$0.05"`. |
| `plus(Money other)` | The sum. Throw `std::overflow_error` if it would not fit; do not let it wrap around. |

### Things to watch

- Check for overflow **before** you compute: signed overflow is undefined behavior, so you cannot compute first and look
  at the result afterwards. `dollars > (max - cents) / 100` is safe; `dollars * 100 + cents < 0` is not a reliable test.
- `-cents` is itself an overflow when `cents` is the smallest `long long`. For printing, work with the magnitude as an
  `unsigned long long`.
